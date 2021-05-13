#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <emu_c_utils/emu_c_utils.h>

#include "recursive_spawn.h"
#include "common.h"

typedef struct local_sort_data {
    long * array;
    long n;
} local_sort_data;

static int
compare_long (const void * a, const void * b)
{
    return ( *(long*)a - *(long*)b );
}

void
init_array_worker(long begin, long end, va_list args)
{
    long * array = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        array[i] = rand();
    }
}

void
local_sort_init(local_sort_data * data, long n)
{
    data->n = n;
    data->array = malloc(n * sizeof(long));
    assert(data->array);

    emu_local_for(0, n, LOCAL_GRAIN_MIN(n, 256),
        init_array_worker, data->array
    );
}

void
local_sort_deinit(local_sort_data * data)
{
    free(data->array);
}

void
local_sort_qsort(local_sort_data * data)
{
    qsort(data->array, data->n, sizeof(long), compare_long);
}

void
local_sort_parallel(local_sort_data * data)
{
    emu_sort_local(data->array, data->n, sizeof(long), compare_long);
}

void local_sort_run(
    local_sort_data * data,
    const char * name,
    void (*benchmark)(local_sort_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark(data);
        double time_ms = hooks_region_end();
        double bytes_per_second = (data->n * sizeof(long)) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        long log2_num_elements;
        long num_trials;
    } args;

    if (argc != 4) {
        LOG("Usage: %s mode log2_num_elements num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_elements = atol(argv[2]);
        args.num_trials = atol(argv[3]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    long n = 1L << args.log2_num_elements;
    LOG("Initializing array with %li elements (%li MiB)\n",
        n, (n * sizeof(long)) / (1024*1024)); fflush(stdout);
    local_sort_data data;
    local_sort_init(&data, n);
    LOG("Sorting using %s\n", args.mode); fflush(stdout);

    #define RUN_BENCHMARK(X) local_sort_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "qsort")) {
        RUN_BENCHMARK(local_sort_qsort);
    } else if (!strcmp(args.mode, "parallel")) {
        RUN_BENCHMARK(local_sort_parallel);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }

    // TODO validate that elements are in order now

    local_sort_deinit(&data);
    return 0;
}
