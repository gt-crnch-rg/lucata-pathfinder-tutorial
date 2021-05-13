#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <emu_c_utils/emu_c_utils.h>

#include "common.h"

typedef struct node {
    struct node * next;
    long weight;
} node;

enum sort_mode {
    ORDERED,
    INTRA_BLOCK_SHUFFLE,
    BLOCK_SHUFFLE,
    FULL_BLOCK_SHUFFLE
} sort_mode;

typedef struct pointer_chase_data {
    long n;
    long block_size;
    long num_threads;
    // Threads accumulate result into this field, to prevent over-optimization
    long sum;
    enum sort_mode sort_mode;
    // One pointer per thread
    node ** heads;
    // Actual array pointer
    node ** pool;
    // Ordering of linked list nodes
    long * indices;
} pointer_chase_data;

replicated pointer_chase_data data;

#define LCG_MUL64 6364136223846793005ULL
#define LCG_ADD64 1

void
lcg_init(unsigned long * x, unsigned long step)
{
    unsigned long mul_k, add_k, ran, un;

    mul_k = LCG_MUL64;
    add_k = LCG_ADD64;

    ran = 1;
    for (un = step; un; un >>= 1) {
        if (un & 1)
            ran = mul_k * ran + add_k;
        add_k *= (mul_k + 1);
        mul_k *= mul_k;
    }

    *x = ran;
}

unsigned long
lcg_rand(unsigned long * x) {
    *x = LCG_MUL64 * *x + LCG_ADD64;
    return *x;
}

//https://benpfaff.org/writings/clc/shuffle.html
/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(long *array, size_t n)
{
    unsigned long rand_state;
    lcg_init(&rand_state, (unsigned long)array);
    if (n > 1)
    {
        size_t i;
        for (i = 0; i < n - 1; i++)
        {
            size_t j = i + lcg_rand(&rand_state) / (ULONG_MAX / (n - i) + 1);
            long t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

// Initializes a list with 0, 1, 2, ...
noinline void
index_init_worker(long begin, long end, va_list args)
{
    long * list = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        list[i] = i;
    }
}

// Initializes a list with  0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
// This will transform malloc2D address mode to sequential
static noinline void
strided_index_init_worker(long begin, long end, va_list args)
{
    long * list = va_arg(args, long*);
    const long n = va_arg(args, long);
    const long num_nodelets = NODELETS();
    for (long i = begin; i < end; ++i) {
        // = i * NODELETS() % n
        // + i * NODELETS() / n
        list[i] = ((i * num_nodelets) & (n-1)) + ((i * num_nodelets) >> PRIORITY(n));
    }
}

static inline node *
get_node_ptr(pointer_chase_data* data, long i) {
    return mw_arrayindex((long*)data->pool, (size_t)i, (size_t)data->n, sizeof(node));
}

static void
relink_worker_1d(long * array, long begin, long end, va_list args)
{
    pointer_chase_data* data = va_arg(args, pointer_chase_data *);
    long * indices = data->indices;
    node** pool = data->pool;
    long n = data->n;
    for (long i = begin; i < end; i += NODELETS()) {
        // String pointers together according to the index
        long a = indices[i];
        long b = indices[i == n - 1 ? 0 : i + 1];

        node* node_a = get_node_ptr(data, a);
        node* node_b = get_node_ptr(data, b);

        node_a->next = node_b;
        // Initialize payload
        node_a->weight = i;
    }
}

void
memcpy_long_worker_var(long begin, long end, va_list args)
{
    long * dst = va_arg(args, long*);
    long * src = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        dst[i] = src[i];
    }
}

void
memcpy_long_worker(long begin, long end, ...)
{
    va_list args;
    va_start(args, end);
    memcpy_long_worker_var(begin, end, args);
    va_end(args);
}


// Shuffles the index array at a block level
noinline void
block_shuffle_worker(long begin, long end, va_list args)
{
    long * block_indices = va_arg(args, long*);
    long * old_indices = va_arg(args, long*);
    long * new_indices = va_arg(args, long*);
    long block_size = va_arg(args, long);

    for (long src_block = begin; src_block < end; ++src_block) {
        long dst_block = block_indices[src_block];
        long * dst_block_ptr = new_indices + dst_block * block_size;
        long * src_block_ptr = old_indices + src_block * block_size;
        // memcpy(dst_block_ptr, src_block_ptr, block_size * sizeof(long));
        for (long i = 0; i < block_size; ++i) {
            dst_block_ptr[i] = src_block_ptr[i];
        }
    }

}


// Shuffles the index array within each block
noinline void
intra_block_shuffle_worker(long begin, long end, va_list args)
{
    pointer_chase_data* data = va_arg(args, pointer_chase_data *);
    long block_size = va_arg(args, long);
    for (long block_id = begin; block_id < end; ++block_id) {
        shuffle(data->indices + block_id * block_size, block_size);
    }
}

void
pointer_chase_data_init(pointer_chase_data * data, long n, long block_size, long num_threads, enum sort_mode sort_mode)
{
    data->n = n;
    data->block_size = block_size;
    data->num_threads = num_threads;
    data->sort_mode = sort_mode;
    mw_replicated_init(&data->sum, 0);
    // Allocate N nodes, striped across nodelets
    data->pool = mw_malloc2d(n, sizeof(node));
    runtime_assert(data->pool != NULL, "Failed to allocate element pool");
    // Store a pointer for this thread's head of the list
    data->heads = (node**)mw_malloc1dlong(num_threads);
    runtime_assert(data->heads != NULL, "Failed to allocate pointers for each thread");
    // Make an array with entries 1 through n
    data->indices = mw_mallocrepl(n * sizeof(long));
    runtime_assert(data->indices != NULL, "Failed to allocate local index array");

    LOG("Replicating pointers...\n");
    // Replicate pointers to all other nodelets
    pointer_chase_data * data0 = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        pointer_chase_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data0, sizeof(pointer_chase_data));
    }

    // Initialize with striped index pattern (i.e. 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15)
    // This will transform malloc2D address mode to sequential
    LOG("Initializing indices...\n");
    emu_local_for(0, n, LOCAL_GRAIN(n),
        strided_index_init_worker, data->indices, (void*)n
    );

    bool do_block_shuffle = false, do_intra_block_shuffle = false;
    switch (data->sort_mode) {
        case ORDERED:
            do_block_shuffle = false;
            do_intra_block_shuffle = false;
            break;
        case INTRA_BLOCK_SHUFFLE:
            do_block_shuffle = false;
            do_intra_block_shuffle = true;
            break;
        case BLOCK_SHUFFLE:
            do_block_shuffle = true;
            do_intra_block_shuffle = false;
            break;
        case FULL_BLOCK_SHUFFLE:
            do_block_shuffle = true;
            do_intra_block_shuffle = true;
            break;
    }


    runtime_assert((n % block_size) == 0, "Block size must evenly divide number of elements");
    long num_blocks = n / block_size;

    if (do_block_shuffle) {
        LOG("Beginning block shuffle...\n");

        // Make an array with an element for each block
        long * block_indices = mw_localmalloc(sizeof(long) * num_blocks, data);
        runtime_assert(block_indices != NULL, "Failed to allocate array for block indices");
        emu_local_for(0, num_blocks, LOCAL_GRAIN(num_blocks),
            index_init_worker, block_indices
        );

        LOG("shuffle block_indices...\n");
        // Randomly shuffle it
        shuffle(block_indices, num_blocks);

        LOG("copy old_indices...\n");
        // Make a copy of the indices array
        long * old_indices = mw_localmalloc(sizeof(long) * n, data);
        runtime_assert(old_indices != NULL, "Failed to allocate copy of local index array");
        emu_local_for(0, n, LOCAL_GRAIN(n),
            memcpy_long_worker_var, old_indices, data->indices
        );

        LOG("apply block_indices to indices...\n");
        emu_local_for(0, num_blocks, LOCAL_GRAIN(num_blocks),
            block_shuffle_worker, block_indices, old_indices, data->indices, (void*)block_size
        );

        // Clean up
        mw_localfree(block_indices);
        mw_localfree(old_indices);
    }

    if (do_intra_block_shuffle) {
        LOG("Beginning intra-block shuffle\n");
        emu_local_for(0, num_blocks, LOCAL_GRAIN(num_blocks),
            intra_block_shuffle_worker, data, (void*)block_size
        );
    }

    LOG("Scattering index array...\n");
    // emu_replicated_array_init(data->indices, sizeof(long), n);
    long * local_indices = mw_get_nth(data->indices, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        long * remote_indices = mw_get_nth(data->indices, i);
        cilk_spawn_at(local_indices) memcpy(remote_indices, local_indices, sizeof(long) * n);
    }
    cilk_sync;

    LOG("Linking nodes together...\n");
    long grain = GLOBAL_GRAIN_MIN(data->n, 64);
    // long nthreads = data->n / grain;
    // LOG("Grain size %li, should spawn %li threads...\n", grain, nthreads);
    emu_1d_array_apply((long*)data->pool, data->n, grain,
        relink_worker_1d, data
    );

    LOG("Chop\n");
    // Chop up the list so there is one chunk per thread
    long chunk_size = n/num_threads;
    LOG("Each thread will traverse %li elements\n", chunk_size);
    for (long i = 0; i < data->num_threads; ++i) {
        long first_index = i * chunk_size;
        long last_index = (i+1) * chunk_size - 1;

       // LOG("Thread %li will start at element %li and end at element %li\n", i,
       //     data->indices[first_index],
       //     data->indices[last_index]
       // );

        // Store a pointer for this thread's head of the list
        data->heads[i] = get_node_ptr(data, data->indices[first_index]);
        // Set this thread's tail to null so it knows where to stop
        get_node_ptr(data, data->indices[last_index])->next = NULL;
    }
}

void
pointer_chase_data_deinit(pointer_chase_data * data)
{
    mw_free(data->pool);
    mw_free(data->heads);
    free(data->indices);
}

static noinline void
chase_pointers(node * head, long * sum)
{
    long local_sum = 0;
    for (node * p = head; p != NULL; p = p->next) {
        local_sum += p->weight;
    }
    REMOTE_ADD(sum, local_sum);
}

void
pointer_chase_serial_spawn(pointer_chase_data * data)
{
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn chase_pointers(data->heads[i], &data->sum);
    }
}

void
serial_spawn_local(pointer_chase_data * data)
{
    // Spawn a thread for each list head located at this nodelet
    // Using striped indexing to avoid migrations
    for (long i = NODE_ID(); i < data->num_threads; i += NODELETS()) {
        cilk_spawn chase_pointers(data->heads[i], &data->sum);
    }
}

void
pointer_chase_serial_remote_spawn(pointer_chase_data * data)
{
    // Spawn a thread at each nodelet
    for (long nodelet_id = 0; nodelet_id < NODELETS(); ++nodelet_id ) {
        if (nodelet_id >= data->num_threads) { break; }
        cilk_spawn_at(&data->heads[nodelet_id]) serial_spawn_local(data);
    }
}

// TODO make this an emu_c_utils library function
long
emu_replicated_reduce_sum_long(long * x)
{
    long sum = 0;
    for (long i = 0; i < NODELETS(); ++i) {
        sum += *(long*)mw_get_nth(x, i);
    }
    return sum;
}

void pointer_chase_run(
    pointer_chase_data * data,
    const char * name,
    void (*benchmark)(pointer_chase_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        mw_replicated_init(&data->sum, 0);
        hooks_region_begin("chase_pointers");
        benchmark(data);
        double time_ms = hooks_region_end();
#ifndef NO_VALIDATE
        // Sum of all integers from 0 to n
        long expected_sum = (data->n * (data->n - 1)) / 2;
        long actual_sum = emu_replicated_reduce_sum_long(&data->sum);
        LOG("expected_sum = %li, actual_sum = %li\n", expected_sum, actual_sum);
        runtime_assert(actual_sum == expected_sum, "Validation FAILED!");
#endif
        double bytes_per_second = time_ms == 0 ? 0 :
            (data->n * sizeof(node)) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}


static const struct option long_options[] = {
    {"log2_num_elements" , required_argument},
    {"num_threads"  , required_argument},
    {"block_size"   , required_argument},
    {"spawn_mode"   , required_argument},
    {"sort_mode"    , required_argument},
    {"num_trials"   , required_argument},
    {"help"         , no_argument},
    {NULL}
};

static void
print_help(const char* argv0)
{
    LOG( "Usage: %s [OPTIONS]\n", argv0);
    LOG("\t--log2_num_elements  Number of elements in the list\n");
    LOG("\t--num_threads        Number of threads traversing the list\n");
    LOG("\t--block_size         Number of elements to swap at a time\n");
    LOG("\t--spawn_mode         How to spawn the threads\n");
    LOG("\t--sort_mode          How to shuffle the array\n");
    LOG("\t--num_trials         Number of times to repeat the benchmark\n");
    LOG("\t--help               Print command line help\n");
}

typedef struct pointer_chase_args {
    long log2_num_elements;
    long num_threads;
    long block_size;
    const char* spawn_mode;
    const char* sort_mode;
    long num_trials;
} pointer_chase_args;

static struct pointer_chase_args
parse_args(int argc, char *argv[])
{
    pointer_chase_args args;
    args.log2_num_elements = 20;
    args.num_threads = 1;
    args.block_size = 1;
    args.spawn_mode = "serial_spawn";
    args.sort_mode = "block_shuffle";
    args.num_trials = 1;

    int option_index;
    while (true)
    {
        int c = getopt_long(argc, argv, "", long_options, &option_index);
        // Done parsing
        if (c == -1) { break; }
        // Parse error
        if (c == '?') {
            LOG( "Invalid arguments\n");
            print_help(argv[0]);
            exit(1);
        }
        const char* option_name = long_options[option_index].name;

        if (!strcmp(option_name, "log2_num_elements")) {
            args.log2_num_elements = atol(optarg);
        } else if (!strcmp(option_name, "num_threads")) {
            args.num_threads = atol(optarg);
        } else if (!strcmp(option_name, "block_size")) {
            args.block_size = atol(optarg);
        } else if (!strcmp(option_name, "spawn_mode")) {
            args.spawn_mode = optarg;
        } else if (!strcmp(option_name, "sort_mode")) {
            args.sort_mode = optarg;
        } else if (!strcmp(option_name, "num_trials")) {
            args.num_trials = atol(optarg);
        } else if (!strcmp(option_name, "help")) {
            print_help(argv[0]);
            exit(1);
        }
    }
    if (args.log2_num_elements <= 0) { LOG( "log2_num_elements must be > 0"); exit(1); }
    if (args.block_size <= 0) { LOG( "block_size must be > 0"); exit(1); }
    if (args.num_threads <= 0) { LOG( "num_threads must be > 0"); exit(1); }
    return args;
}

int main(int argc, char** argv)
{
    // There are two regions of interest, "init" and "chase_pointers"
    // By default, timing will be on for both
    // Can reduce simulation time by setting HOOKS_ACTIVE_REGION=chase_pointers
    const char* active_region = getenv("HOOKS_ACTIVE_REGION");
    if (active_region != NULL) {
        hooks_set_active_region(active_region);
    }

    pointer_chase_args args = parse_args(argc, argv);

    enum sort_mode sort_mode;
    if (!strcmp(args.sort_mode, "block_shuffle")) {
        sort_mode = BLOCK_SHUFFLE;
    } else if (!strcmp(args.sort_mode, "ordered")) {
        sort_mode = ORDERED;
    } else if (!strcmp(args.sort_mode, "intra_block_shuffle")) {
        sort_mode = INTRA_BLOCK_SHUFFLE;
    } else if (!strcmp(args.sort_mode, "full_block_shuffle")) {
        sort_mode = FULL_BLOCK_SHUFFLE;
    } else {
        LOG( "Sort mode %s not implemented!\n", args.sort_mode);
        exit(1);
    }

    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("block_size", args.block_size);
    hooks_set_attr_str("spawn_mode", args.spawn_mode);
    hooks_set_attr_str("sort_mode", args.sort_mode);
    hooks_set_attr_i64("num_nodelets", NODELETS());

    long n = 1L << args.log2_num_elements;
    long bytes = n * (sizeof(node));
    long mbytes = bytes / (1000000);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing %s array with %li elements (%li MB total, %li MB per nodelet)\n",
        args.sort_mode, n, mbytes, mbytes_per_nodelet);

    hooks_region_begin("init");
    pointer_chase_data_init(&data,
        n, args.block_size, args.num_threads, sort_mode);
    hooks_region_end();
    LOG( "Launching %s with %li threads...\n", args.spawn_mode, args.num_threads);

    #define RUN_BENCHMARK(X) pointer_chase_run(&data, args.spawn_mode, X, args.num_trials)

    if (!strcmp(args.spawn_mode, "serial_spawn")) {
        RUN_BENCHMARK(pointer_chase_serial_spawn);
    } else if (!strcmp(args.spawn_mode, "serial_remote_spawn")) {
        RUN_BENCHMARK(pointer_chase_serial_remote_spawn);
    } else {
        LOG( "Spawn mode %s not implemented!", args.spawn_mode);
        exit(1);
    }

    pointer_chase_data_deinit(&data);
    return 0;
}
