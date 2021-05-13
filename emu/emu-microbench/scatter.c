#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include <emu_c_utils/emu_c_utils.h>
#include "common.h"

typedef struct scatter_data {
    long * buffer;
    long n;
    long num_threads;
} scatter_data;

replicated scatter_data data;

// Initialize a long* with mw_replicated_init
void
init_replicated_ptr(long ** loc, long * ptr)
{
    mw_replicated_init((long*)loc, (long)ptr);
}

void
scatter_data_init(scatter_data * data, long n, long num_threads)
{
    mw_replicated_init(&data->n, n);
    mw_replicated_init(&data->num_threads, num_threads);

    // Allocate an array on nodelet 0, and replicate the pointer
    init_replicated_ptr(&data->buffer,
        mw_mallocrepl(n * sizeof(long))
    );
}

void
scatter_data_deinit(scatter_data * data)
{
    mw_free(data->buffer);
}

// Scatter the buffer using a memcpy for each nodelet
noinline void
scatter_memcpy(scatter_data * data)
{
    long * local = mw_get_nth(data->buffer, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        long * remote = mw_get_nth(data->buffer, i);
        memcpy(remote, local, data->n * sizeof(long));
    }
}

// Scatter the buffer using a serial for loop for each nodelet
static noinline void
copy_long_worker_var(long begin, long end, va_list args)
{
    long * dst = va_arg(args, long*);
    long * src = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        dst[i] = src[i];
    }
}

static void
copy_long_worker(long begin, long end, ...)
{
    va_list args;
    va_start(args, end);
    copy_long_worker_var(begin, end, args);
    va_end(args);
}


noinline void
scatter_serial(scatter_data * data)
{
    long * local = mw_get_nth(data->buffer, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        long * remote = mw_get_nth(data->buffer, i);
        for (long i = 0; i < data->n; ++i) {
            remote[i] = local[i];
        }
    }
}

// Scatter by spawning one thread for each remote nodelet
noinline void
scatter_parallel(scatter_data * data)
{
    long * local = mw_get_nth(data->buffer, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        long * remote = mw_get_nth(data->buffer, i);
        cilk_spawn copy_long_worker(0, data->n, remote, local);
    }
}

static void
scatter_emu_for_worker_level2(long begin, long end, void * arg1, void * arg2)
{
    long * dst = arg2;
    long * src = arg1;
    for (long i = begin; i < end; ++i) {
        dst[i] = src[i];
    }
}

static noinline void
scatter_emu_for_worker_level1(long begin, long end, long grain, void * arg1, void * arg2)
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn scatter_emu_for_worker_level2(first, last, arg1, arg2);
    }
}

// Spawn a parallel copy for each nodelet, being careful not to exhaust total # of threads
noinline void
scatter_emu_for(scatter_data * data)
{
    long grain = data->n / data->num_threads;
    long per_nodelet_grain = grain * NODELETS();
    long * local = mw_get_nth(data->buffer, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        long * remote = mw_get_nth(data->buffer, i);
        // TODO why doesn't this work on HW?
//        cilk_spawn emu_local_for_v2(0, data->n, per_nodelet_grain,
//            scatter_emu_for_worker_level2, local, remote
//        );
        cilk_spawn scatter_emu_for_worker_level1(0, data->n, per_nodelet_grain,
            local, remote
        );

    }
}

// TODO - Scatter with recursive tree
static void
scatter_tree(long * buffer, long n, long nlet_begin, long nlet_end)
{
    long num_nodelets = nlet_end - nlet_begin;
    // TODO detect this condition before spawning
    if (num_nodelets == 1) { return; }

    long nlet_mid = nlet_begin + (num_nodelets / 2);

    long * local = mw_get_nth(buffer, NODE_ID());
    long * remote = mw_get_nth(buffer, nlet_mid);

//    LOG("nlet[%li]: Copy from %li to %li\n", NODE_ID(), NODE_ID(), nlet_mid);
    // Copy to nlet_mid
    emu_local_for(0, n, LOCAL_GRAIN_MIN(n, 64),
        copy_long_worker_var, remote, local
    );

//    LOG("nlet[%li]: Spawn scatter_tree(%li - %li)\n", NODE_ID(), nlet_mid, nlet_end);

    // Spawn at target and recurse through my range
    cilk_spawn scatter_tree(remote, n, nlet_mid, nlet_end);
    scatter_tree(local, n, nlet_begin, nlet_mid);
}

noinline void scatter_recursive_tree(scatter_data * data)
{
    scatter_tree(data->buffer, data->n, 0, NODELETS());
}


void scatter_run(
    scatter_data * data,
    void (*benchmark)(scatter_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("scatter");
        benchmark(data);
        double time_ms = hooks_region_end();
        double bytes_per_second = time_ms == 0 ? 0 :
            (data->n * sizeof(long) * (NODELETS()-1)) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}

replicated scatter_data data;

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

    hooks_set_attr_str("mode", args.mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long));

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    LOG("Initializing arrays with %li elements each (%li MiB)\n", n, mbytes);
    scatter_data_init(&data, n, args.num_threads);
    LOG("Scattering with %s\n", args.mode);

    #define RUN_BENCHMARK(X) scatter_run(&data, X, args.num_trials)

    if (!strcmp(args.mode, "memcpy")) {
        RUN_BENCHMARK(scatter_memcpy);
    } else if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(scatter_serial);
    } else if (!strcmp(args.mode, "parallel_simple")) {
        RUN_BENCHMARK(scatter_parallel);
    } else if (!strcmp(args.mode, "emu_for")) {
        RUN_BENCHMARK(scatter_emu_for);
    } else if (!strcmp(args.mode, "tree")) {
        RUN_BENCHMARK(scatter_recursive_tree);
    } else {
        LOG("Spawn mode %s not implemented!", args.mode);
    }

    scatter_data_deinit(&data);
    return 0;
}
