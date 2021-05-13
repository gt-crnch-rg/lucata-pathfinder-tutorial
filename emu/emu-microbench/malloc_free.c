#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include "common.h"

#include <emu_c_utils/emu_c_utils.h>


typedef struct malloc_free_data {
    // Total number of malloc/free pairs
    long n;
    // Number of threads
    long num_threads;
    // Size of each allocation in bytes
    long sz;
} malloc_free_data;


void
malloc_free_worker(long n, long sz)
{
    for (long i = 0; i < n; ++i){
        void * ptr = malloc(sz);
        free(ptr);
    }
}

void
malloc_free_spawner(malloc_free_data * data)
{
    long mallocs_per_thread = data->n / data->num_threads;
    for (long i = 0; i < data->num_threads; ++i){
        cilk_spawn malloc_free_worker(mallocs_per_thread, data->sz);
    }
}

void malloc_free_run(
    malloc_free_data * data,
    void (*benchmark)(malloc_free_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("malloc_free");
        benchmark(data);
        double time_ms = hooks_region_end();
        double mallocs_per_second = time_ms == 0 ? 0 :
            (data->n) / (time_ms/1000);
        LOG("%3.2f million mallocs per second\n", mallocs_per_second / (1000000));
    }
}

int main(int argc, char** argv)
{
    struct {
        long log2_num_mallocs;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 4) {
        LOG("Usage: %s log2_num_mallocs num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.log2_num_mallocs = atol(argv[1]);
        args.num_threads = atol(argv[2]);
        args.num_trials = atol(argv[3]);

        if (args.log2_num_mallocs <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_i64("log2_num_mallocs", args.log2_num_mallocs);
    hooks_set_attr_i64("num_threads", args.num_threads);

    malloc_free_data data;
    data.n = 1L << args.log2_num_mallocs;
    data.num_threads = args.num_threads;
    data.sz = 4096;

    LOG("Spawning %li threads to do %li malloc/free operations\n", data.num_threads, data.n);
    malloc_free_run(&data, malloc_free_spawner, args.num_trials);

    return 0;
}
