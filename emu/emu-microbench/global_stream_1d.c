#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include <emu_c_utils/emu_c_utils.h>
#include "common.h"
#include "recursive_spawn.h"

typedef struct global_stream_data {
    long * a;
    long * b;
    long * c;
    long n;
    long num_threads;
} global_stream_data;


static void init_worker(long * array, long begin, long end, va_list args)
{
    global_stream_data * data = va_arg(args, global_stream_data *);
    long nodelets = NODELETS();
    for (long i = begin; i < end; i += nodelets) {
        data->a[i] = 1;
        data->b[i] = 2;
        data->c[i] = 0;
    }
}

void
replicated_init_ptr(long** ptr, long* val)
{
    mw_replicated_init((long*)ptr, (long)val);
}

void
global_stream_init(global_stream_data * data, long n)
{
    data->n = n;

    replicated_init_ptr(&data->a, mw_malloc1dlong(n));
    replicated_init_ptr(&data->b, mw_malloc1dlong(n));
    replicated_init_ptr(&data->c, mw_malloc1dlong(n));
#ifndef NO_VALIDATE
    emu_1d_array_apply(data->a, n, GLOBAL_GRAIN(n),
        init_worker, data
    );
#endif
}

void
global_stream_deinit(global_stream_data * data)
{
    mw_free(data->a);
    mw_free(data->b);
    mw_free(data->c);
}

static void
global_stream_validate_worker(long * array, long begin, long end, va_list args)
{
    const long nodelets = NODELETS();
    for (long i = begin; i < end; i += nodelets) {
        if (array[i] != 3) {
            LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 3)\n", i, array[i]);
            exit(1);
        }
    }
}

void
global_stream_validate(global_stream_data * data)
{
    emu_1d_array_apply(data->c, data->n, GLOBAL_GRAIN_MIN(data->n, 64),
        global_stream_validate_worker
    );
}

// serial - just a regular for loop
void
global_stream_add_serial(global_stream_data * data)
{
    for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

// cilk_for - cilk_for loop with grainsize set to control number of threads
void
global_stream_add_cilk_for(global_stream_data * data)
{
    #pragma cilk grainsize = data->n / data->num_threads
    cilk_for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

noinline void
serial_spawn_add_worker(long begin, long end, global_stream_data *data)
{
    for (long i = begin; i < end; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

// serial_spawn - spawn one thread to handle each grain-sized chunk of the range
void
global_stream_add_serial_spawn(global_stream_data * data)
{
    long grain = data->n / data->num_threads;
    for (long i = 0; i < data->n; i += grain) {
        long begin = i;
        long end = begin + grain <= data->n ? begin + grain : data->n;
        cilk_spawn serial_spawn_add_worker(begin, end, data);
    }
    cilk_sync;
}


static void
global_stream_add_library_worker(long * array, long begin, long end, va_list args)
{
    (void)array;
    global_stream_data * data = va_arg(args, global_stream_data *);
    const long nodelets = NODELETS();
    for (long i = begin; i < end; i += nodelets) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

void
global_stream_add_library(global_stream_data * data)
{
    emu_1d_array_apply(data->a, data->n, data->n / data->num_threads,
        global_stream_add_library_worker, data
    );
}

void global_stream_run(
    global_stream_data * data,
    const char * name,
    void (*benchmark)(global_stream_data *),
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

replicated global_stream_data data;

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

    hooks_set_attr_str("spawn_mode", args.mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long) * 3);

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", 3 * n, 3 * mbytes, 3 * mbytes_per_nodelet);
    fflush(stdout);
    data.num_threads = args.num_threads;
    global_stream_init(&data, n);
    LOG("Doing vector addition using %s\n", args.mode); fflush(stdout);

    #define RUN_BENCHMARK(X) global_stream_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(global_stream_add_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(global_stream_add_serial_spawn);
    } else if (!strcmp(args.mode, "library")) {
        runtime_assert(data.num_threads >= NODELETS(), "will always use at least one thread per nodelet");
        RUN_BENCHMARK(global_stream_add_library);
    } else if (!strcmp(args.mode, "serial")) {
        runtime_assert(data.num_threads == 1, "serial mode can only use one thread");
        RUN_BENCHMARK(global_stream_add_serial);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }
#ifndef NO_VALIDATE
    LOG("Validating results...");
    global_stream_validate(&data);
    LOG("OK\n");
#endif

    global_stream_deinit(&data);
    return 0;
}
