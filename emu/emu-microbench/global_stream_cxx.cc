#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <emu_c_utils/emu_c_utils.h>
#include <emu_cxx_utils/striped_array.h>
#include <emu_cxx_utils/spawn_templates.h>
#include <emu_cxx_utils/chunked_array.h>
#include <emu_cxx_utils/mirrored.h>
#include "common.h"

using namespace emu;

struct benchmark {
    virtual void initialize() = 0;
    virtual void run(const char * name, long num_trials) = 0;
    virtual void validate() = 0;
    virtual ~benchmark() {};
};

template<template <typename> class array_type>
struct global_stream : public benchmark, public repl_new
{
    repl_copy<array_type<long>> a;
    repl_copy<array_type<long>> b;
    repl_copy<array_type<long>> c;
    repl<long> n;
    repl<long> num_threads;

    global_stream(long n, long num_threads)
    : a(n), b(n), c(n), n(n), num_threads(num_threads)
    {
    }

    void
    initialize() override
    {
        // TODO initialize in parallel
        for (long i = 0; i < n; ++i) {
            a[i] = 1;
            b[i] = 2;
        }
    }

    void
    validate() override
    {
        // TODO check in parallel
        for (long i = 0; i < n; ++i) {
            if (c[i] != 3) {
                LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 3)\n", i, c[i]);
                exit(1);
            }
        }
    }

    // serial - just a regular for loop
    void
    add_serial()
    {
        for (long i = 0; i < n; ++i) {
            c[i] = a[i] + b[i];
        }
    }

    void
    add_cilk_for()
    {
        #pragma cilk grainsize = n / num_threads
        cilk_for (long i = 0; i < n; ++i) {
            c[i] = a[i] + b[i];
        }
    }

    void
    add_serial_spawn()
    {
        local_serial_spawn(0, n, n/num_threads, [this](long i) {
            c[i] = a[i] + b[i];
        });
    }

    void
    add_recursive_spawn()
    {
        local_recursive_spawn(0, n, n/num_threads, [this](long i) {
            c[i] = a[i] + b[i];
        });
    }

    void
    add_serial_remote_spawn()
    {
        c.parallel_apply([this](long i) {
            c[i] = a[i] + b[i];
        }, n/num_threads);
    }

    void
    add_recursive_remote_spawn()
    {
        c.parallel_apply([this](long i) {
            c[i] = a[i] + b[i];
        }, n/num_threads);
    }

    void
    run(const char * name, long num_trials) override
    {
        LOG("In run(%s, %li)\n", name, num_trials);
        for (long trial = 0; trial < num_trials; ++trial) {
            hooks_set_attr_i64("trial", trial);

            #define RUN_BENCHMARK(X)            \
            do {                                \
                hooks_region_begin(name);       \
                X();                            \
                time_ms = hooks_region_end();   \
            } while(false)

            double time_ms = 0;
            if (!strcmp(name, "cilk_for")) {
                RUN_BENCHMARK(add_cilk_for);
            } else if (!strcmp(name, "serial_spawn")) {
                RUN_BENCHMARK(add_serial_spawn);
            } else if (!strcmp(name, "serial_remote_spawn")) {
                runtime_assert(num_threads >= NODELETS(), "serial_remote_spawn mode will always use at least one thread per nodelet");
                RUN_BENCHMARK(add_serial_remote_spawn);
            } else if (!strcmp(name, "recursive_spawn")) {
                RUN_BENCHMARK(add_recursive_spawn);
            } else if (!strcmp(name, "recursive_remote_spawn")) {
                runtime_assert(num_threads >= NODELETS(), "recursive_remote_spawn mode will always use at least one thread per nodelet");
                RUN_BENCHMARK(add_recursive_remote_spawn);
            } else if (!strcmp(name, "serial")) {
                runtime_assert(num_threads == 1, "serial mode can only use one thread");
                RUN_BENCHMARK(add_serial);
            } else {
                printf("Mode %s not implemented!", name);
                exit(1);
            }

            #undef RUN_BENCHMARK

            double bytes_per_second = time_ms == 0 ? 0 :
                (n * sizeof(long) * 3) / (time_ms/1000);
            LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
        }
    }
    
};

benchmark *
make_benchmark(const char * layout, long n, long num_threads)
{
    if (!strcmp(layout, "striped")) {
        return new global_stream<striped_array>(n, num_threads);
    } else if (!strcmp(layout, "chunked")) {
        return new global_stream<chunked_array>(n, num_threads);
    } else {
        printf("Layout %s not implemented!", layout);
        exit(1);
        return nullptr;
    }
}


int main(int argc, char** argv)
{
    struct {
        const char* mode;
        const char* layout;
        long log2_num_elements;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 6) {
        LOG("Usage: %s mode layout log2_num_elements num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.layout = argv[2];
        args.log2_num_elements = atol(argv[3]);
        args.num_threads = atol(argv[4]);
        args.num_trials = atol(argv[5]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("spawn_mode", args.mode);
    hooks_set_attr_str("layout", args.layout);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long) * 3);

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", 3 * n, 3 * mbytes, 3 * mbytes_per_nodelet);
    auto * benchmark = make_benchmark(args.layout, n, args.num_threads);
#ifndef NO_VALIDATE
    benchmark->initialize();
#endif
    LOG("Doing vector addition using %s\n", args.mode);
    benchmark->run(args.mode, args.num_trials);
#ifndef NO_VALIDATE
    LOG("Validating results...");
    benchmark->validate();
    LOG("OK\n");
#endif

    delete benchmark;
    return 0;
}
