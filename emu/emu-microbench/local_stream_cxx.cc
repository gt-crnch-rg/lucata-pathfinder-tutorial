#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include <emu_c_utils/emu_c_utils.h>
#include <emu_cxx_utils/spawn_templates.h>
#include "common.h"

using namespace emu;

struct local_stream {
    long * a;
    long * b;
    long * c;
    long n;
    long num_threads;

    local_stream(long n) : n(n)
    {
        a = (long*)malloc(n * sizeof(long));
        assert(a);
        b = (long*)malloc(n * sizeof(long));
        assert(b);
        c = (long*)malloc(n * sizeof(long));
        assert(c);

        // TODO init in parallel
        memset(a, 0, n * sizeof(long));
        memset(b, 0, n * sizeof(long));
        memset(c, 0, n * sizeof(long));
    }

    ~local_stream()
    {
        free(a);
        free(b);
        free(c);
    }

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
    run(const char * name, long num_trials)
    {
        for (long trial = 0; trial < num_trials; ++trial) {
            hooks_set_attr_i64("trial", trial);

            double time_ms = 0;
            #define RUN_BENCHMARK(X)            \
            do {                                \
                hooks_region_begin(name);       \
                X();                            \
                time_ms = hooks_region_end();   \
            } while(false)

            if (!strcmp(name, "cilk_for")) {
                RUN_BENCHMARK(add_cilk_for);
            } else if (!strcmp(name, "serial_spawn")) {
                RUN_BENCHMARK(add_serial_spawn);
            } else if (!strcmp(name, "recursive_spawn")) {
                RUN_BENCHMARK(add_recursive_spawn);
            } else if (!strcmp(name, "serial")) {
                RUN_BENCHMARK(add_serial);
            } else {
                printf("Mode %s not implemented!", name);
            }
            double bytes_per_second = (n * sizeof(long) * 3) / (time_ms/1000);
            LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
        }
    }
};


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
    LOG("Doing vector addition using %s\n", args.mode); fflush(stdout);

    local_stream benchmark(n);
    benchmark.num_threads = args.num_threads;

    printf("Doing vector addition using %s\n", args.mode); fflush(stdout);

    benchmark.run(args.mode, args.num_trials);

    return 0;
}
