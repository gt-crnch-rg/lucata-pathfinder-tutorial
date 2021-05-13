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

typedef struct local_stream_data {
    long * a;
    long * b;
    long * c;
    long n;
    long num_threads;
} local_stream_data;

void
local_stream_init(local_stream_data * data, long n)
{
    data->n = n;
    data->a = mw_localmalloc(n * sizeof(long), data);
    assert(data->a);
    data->b = mw_localmalloc(n * sizeof(long), data);
    assert(data->b);
    data->c = mw_localmalloc(n * sizeof(long), data);
    assert(data->c);
#ifndef NO_VALIDATE
    emu_local_for_set_long(data->a, n, 1);
    emu_local_for_set_long(data->b, n, 2);
    emu_local_for_set_long(data->c, n, 0);
#endif
}

void
local_stream_deinit(local_stream_data * data)
{
    free(data->a);
    free(data->b);
    free(data->c);
}

void
local_stream_add_serial(local_stream_data * data)
{
    for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

void
local_stream_add_cilk_for(local_stream_data * data)
{
    #pragma cilk grainsize = data->n / data->num_threads
    cilk_for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

static void
recursive_spawn_add_worker(long begin, long end, local_stream_data *data)
{
    for (long i = begin; i < end; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

static void
recursive_spawn_add(long begin, long end, long grain, local_stream_data *data)
{
    RECURSIVE_CILK_SPAWN(begin, end, grain, recursive_spawn_add, data);
}

void
local_stream_add_recursive_spawn(local_stream_data * data)
{
    recursive_spawn_add(0, data->n, data->n / data->num_threads, data);
}

void
local_stream_add_serial_spawn(local_stream_data * data)
{
    long grain = data->n / data->num_threads;
    for (long i = 0; i < data->n; i += grain) {
        long begin = i;
        long end = begin + grain <= data->n ? begin + grain : data->n;
        cilk_spawn recursive_spawn_add_worker(begin, end, data);
    }
    cilk_sync;
}

void
local_stream_add_library_worker(long begin, long end, va_list args)
{
    long *a = va_arg(args, long*);
    long *b = va_arg(args, long*);
    long *c = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        c[i] = a[i] + b[i];
    }
}

void local_stream_add_library(local_stream_data * data)
{
    emu_local_for(0, data->n, data->n / data->num_threads,
        local_stream_add_library_worker, data->a, data->b, data->c
    );
}

void local_stream_run(
    local_stream_data * data,
    const char * name,
    void (*benchmark)(local_stream_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark(data);
        double time_ms = hooks_region_end();
        double bytes_per_second = time_ms == 0 ? 0 :
            (data->n * sizeof(long) * 3) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}

static void
local_stream_validate_worker(long begin, long end, va_list args)
{
    long * c = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        if (c[i] != 3) {
            LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 3)\n", i, c[i]);
            exit(1);
        }
    }
}

void
local_stream_validate(local_stream_data * data)
{
    emu_local_for(0, data->n, LOCAL_GRAIN(data->n),
        local_stream_validate_worker, data->c
    );
}


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
    LOG("Initializing arrays with %li elements each (%li MiB)\n",
        n, (n * sizeof(long)) / (1024*1024)); fflush(stdout);
    local_stream_data data;
    data.num_threads = args.num_threads;
    local_stream_init(&data, n);
    LOG("Doing vector addition using %s\n", args.mode); fflush(stdout);

    #define RUN_BENCHMARK(X) local_stream_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(local_stream_add_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(local_stream_add_serial_spawn);
    } else if (!strcmp(args.mode, "recursive_spawn")) {
        RUN_BENCHMARK(local_stream_add_recursive_spawn);
    } else if (!strcmp(args.mode, "library")) {
        RUN_BENCHMARK(local_stream_add_library);
    } else if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(local_stream_add_serial);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }
#ifndef NO_VALIDATE
    LOG("Validating results...");
    local_stream_validate(&data);
    LOG("OK\n");
#endif
    local_stream_deinit(&data);
    return 0;
}
