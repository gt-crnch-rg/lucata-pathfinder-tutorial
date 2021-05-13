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

/*
 * Goal: Quantify the thread spawn overhead in different circumstances:
 * - recursive vs serial spawn tree
 * - number of arguments passed to worker thread
 * - worker thread has stack or is stackless
 * Secondary goal: reproduce issues with va_arg in a minimal test case
 */

typedef struct spawn_rate_data {
    long * array;
    long n;
    long num_threads;
} spawn_rate_data;

replicated spawn_rate_data data;

#define DO_WORK(begin, end) \
    do {                                        \
        for (long *p = begin; p < end; ++p) {   \
            *p = 1;                             \
        }                                       \
    } while (0)

// Stackless worker function
noinline void
light_worker(long * begin, long * end)
{
    DO_WORK(begin, end);
}

// Delegates to light_worker, so must allocate a stack frame
noinline void
heavy_worker(long * begin, long * end)
{
    light_worker(begin, end);
}

// Recursive spawn, work function is inline
noinline void
recursive_spawn_inline_worker(long * begin, long * end, long grain)
{
    for (;;) {
        long count = end - begin;
        if (count < grain) break;
        long * mid = begin + count / 2;
        cilk_spawn recursive_spawn_inline_worker(begin, mid, grain);
        begin = mid;
    }
    DO_WORK(begin, end);
}

// Recursive spawn, work function is stackless
noinline void
recursive_spawn_light_worker(long * begin, long * end, long grain)
{
    for (;;) {
        long count = end - begin;
        if (count < grain) break;
        long * mid = begin + count / 2;
        cilk_spawn recursive_spawn_light_worker(begin, mid, grain);
        begin = mid;
    }
    light_worker(begin, end);
}

// Recursive spawn, work function has stack
noinline void
recursive_spawn_heavy_worker(long * begin, long * end, long grain)
{
    for (;;) {
        long count = end - begin;
        if (count < grain) break;
        long * mid = begin + count / 2;
        cilk_spawn recursive_spawn_heavy_worker(begin, mid, grain);
        begin = mid;
    }
    heavy_worker(begin, end);
}

// Serial spawn, work function is stackless
noinline void
serial_spawn_light_worker(long * begin, long * end, long grain)
{
    for (long * first = begin; first < end; first += grain) {
        long * last = first + grain <= end ? first + grain : end;
        cilk_spawn light_worker(first, last);
    }
}

// Recursive spawn, work function has stack
noinline void
serial_spawn_heavy_worker(long * begin, long * end, long grain)
{
    for (long * first = begin; first < end; first += grain) {
        long * last = first + grain <= end ? first + grain : end;
        cilk_spawn heavy_worker(first, last);
    }
}

// Worker used by emu_c_utils
noinline void
library_inline_worker(long begin, long end, va_list args)
{
    // NOTE begin and end are passed as indices, not pointers
    long* array = va_arg(args, long*);
    long * first = array + begin;
    long * last = array + end;
    DO_WORK(first, last);
}

noinline void
library_light_worker(long begin, long end, va_list args)
{
    // NOTE begin and end are passed as indices, not pointers
    long* array = va_arg(args, long*);
    long * first = array + begin;
    long * last = array + end;
    light_worker(first, last);
}

noinline void
library_heavy_worker(long begin, long end, va_list args)
{
    // NOTE begin and end are passed as indices, not pointers
    long* array = va_arg(args, long*);
    long * first = array + begin;
    long * last = array + end;
    heavy_worker(first, last);
}




void
init(long n, long num_threads)
{
    data.n = n;
    data.num_threads = num_threads;
    data.array = malloc(n * sizeof(long));
    assert(data.array);
}

void
deinit()
{
    free(data.array);
}

void
clear()
{
    memset(data.array, 0, data.n * sizeof(long));
}

void
validate()
{
    bool success = true;
    for (long i = 0; i < data.n; ++i) {
        if (data.array[i] != 1) {
            success = false;
            break;
        }
    }
    if (success) {
        LOG("PASSED\n");
    } else {
        LOG("FAILED\n");
        exit(1);
    }
}

noinline void
do_serial()
{
    DO_WORK(data.array, data.array + data.n);
}

noinline void
do_light()
{
    light_worker(data.array, data.array + data.n);
}

// Delegate to heavy_worker
noinline void
do_heavy()
{
    heavy_worker(data.array, data.array + data.n);
}

noinline void
do_serial_spawn_light() {
    serial_spawn_light_worker(data.array, data.array + data.n, data.n / data.num_threads);
}

noinline void
do_serial_spawn_heavy() {
    serial_spawn_heavy_worker(data.array, data.array + data.n, data.n / data.num_threads);
}

noinline void
do_recursive_spawn_inline() {
    recursive_spawn_inline_worker(data.array, data.array + data.n, data.n / data.num_threads);
}


noinline void
do_recursive_spawn_light() {
    recursive_spawn_light_worker(data.array, data.array + data.n, data.n / data.num_threads);
}

noinline void
do_recursive_spawn_heavy() {
    recursive_spawn_heavy_worker(data.array, data.array + data.n, data.n / data.num_threads);
}

// Do the work with an emu_c_utils library call
noinline void
do_library_inline()
{
    emu_local_for(0, data.n, data.n / data.num_threads, library_inline_worker, data.array);
}

noinline void
do_library_light()
{
    emu_local_for(0, data.n, data.n / data.num_threads, library_light_worker, data.array);
}

noinline void
do_library_heavy()
{
    emu_local_for(0, data.n, data.n / data.num_threads, library_heavy_worker, data.array);
}



void run(const char * name, void (*benchmark)(), long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark();
        double time_ms = hooks_region_end();
        double bytes_per_second = time_ms == 0 ? 0 :
            (data.n * sizeof(long)) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
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
    LOG("Initializing array with %li elements each (%li MiB)\n",
        n, (n * sizeof(long)) / (1024*1024)); fflush(stdout);

    init(n, args.num_threads);
    clear();

    LOG("Running with %s\n", args.mode);

    hooks_set_attr_str("mode", args.mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long));

#define RUN_BENCHMARK(X) run(args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(do_serial);
    } else if (!strcmp(args.mode, "light_worker")) {
        RUN_BENCHMARK(do_light);
    } else if (!strcmp(args.mode, "heavy_worker")) {
        RUN_BENCHMARK(do_heavy);
    } else if (!strcmp(args.mode, "serial_spawn_light")) {
        RUN_BENCHMARK(do_serial_spawn_light);
    } else if (!strcmp(args.mode, "serial_spawn_heavy")) {
        RUN_BENCHMARK(do_serial_spawn_heavy);
    } else if (!strcmp(args.mode, "recursive_spawn_inline")) {
        RUN_BENCHMARK(do_recursive_spawn_inline);
    } else if (!strcmp(args.mode, "recursive_spawn_light")) {
        RUN_BENCHMARK(do_recursive_spawn_light);
    } else if (!strcmp(args.mode, "recursive_spawn_heavy")) {
        RUN_BENCHMARK(do_recursive_spawn_heavy);
    } else if (!strcmp(args.mode, "library_inline")) {
        RUN_BENCHMARK(do_library_inline);
    } else if (!strcmp(args.mode, "library_light")) {
        RUN_BENCHMARK(do_library_light);
    } else if (!strcmp(args.mode, "library_heavy")) {
        RUN_BENCHMARK(do_library_heavy);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }
#ifndef NO_VALIDATE
    LOG("Validating results...");
    validate();
    LOG("OK\n");
#endif
    deinit();
    return 0;
}
