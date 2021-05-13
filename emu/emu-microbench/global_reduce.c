#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <emu_c_utils/emu_c_utils.h>

#include "common.h"

typedef struct global_reduce_data {
    emu_chunked_array array_a;
    long ** a;
    long n;
    long num_threads;
} global_reduce_data;


// #define INDEX(PTR, BLOCK, I) (PTR[I/BLOCK][I%BLOCK])
#define INDEX(PTR, BLOCK, I) (PTR[I >> PRIORITY(BLOCK)][I&(BLOCK-1)])

void
global_reduce_init(global_reduce_data * data, long n)
{
    data->n = n;
    emu_chunked_array_replicated_init(&data->array_a, n, sizeof(long));
    data->a = (long**)data->array_a.data;

#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        global_reduce_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(global_reduce_data));
    }
#endif

    emu_chunked_array_set_long(&data->array_a, 1);
}

void
global_reduce_deinit(global_reduce_data * data)
{
    emu_chunked_array_replicated_deinit(&data->array_a);
}

// serial - just a regular for loop
long
global_reduce_add_serial(global_reduce_data * data)
{
    long sum = 0;
    long block_sz = data->n / NODELETS();
    for (long i = 0; i < data->n; ++i) {
        sum += INDEX(data->a, block_sz, i);
    }
    return sum;
}

static noinline void
global_reduce_add_emu_apply_worker(emu_chunked_array * array, long begin, long end, va_list args)
{
    long * sum = va_arg(args, long*);
    long * a = emu_chunked_array_index(array, begin);
    long local_sum = 0;
    for (long i = 0; i < end-begin; ++i) {
        local_sum += a[i];
    }
    REMOTE_ADD(sum, local_sum);
}

// Use the apply from the library
// Sums are accumulated within each thread, then remote-added to the global sum
long
global_reduce_add_emu_apply(global_reduce_data * data)
{
    long sum = 0;
    emu_chunked_array_apply(&data->array_a, GLOBAL_GRAIN(data->n),
        global_reduce_add_emu_apply_worker, &sum
    );
    return sum;
}

// Use emu_c_utils library function
// Sums are accumulated within each thread, then remote-added to the nodelet-local sum
// Finally, the nodelet-local sums are accumulated into the global sum
long
global_reduce_add_emu_reduce(global_reduce_data * data)
{
    return emu_chunked_array_reduce_sum_long(&data->array_a);
}

void global_reduce_run(
    global_reduce_data * data,
    const char * name,
    long (*benchmark)(global_reduce_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        long sum = benchmark(data);
        double time_ms = hooks_region_end();
        runtime_assert(sum == data->n, "Validation FAILED!");
        double bytes_per_second = time_ms == 0 ? 0 :
            (data->n * sizeof(long)) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}



replicated global_reduce_data data;

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        long log2_num_elements;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 5) {
        LOG("Usage: %s mode log2_num_elements num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_elements = atol(argv[2]);
        args.num_threads = atol(argv[3]);
        args.num_trials = atol(argv[4]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", n, mbytes, mbytes_per_nodelet);
    fflush(stdout);
    data.num_threads = args.num_threads;
    global_reduce_init(&data, n);
    LOG("Doing vector addition using %s\n", args.mode); fflush(stdout);

    #define RUN_BENCHMARK(X) global_reduce_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(global_reduce_add_serial);
    } else if (!strcmp(args.mode, "per_thread_remote")) {
        RUN_BENCHMARK(global_reduce_add_emu_apply);
    } else if (!strcmp(args.mode, "per_nodelet_remote")) {
        RUN_BENCHMARK(global_reduce_add_emu_reduce);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }

    global_reduce_deinit(&data);
    return 0;
}
