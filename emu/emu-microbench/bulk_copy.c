#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include <emu_c_utils/emu_c_utils.h>
#include "common.h"

typedef struct bulk_copy_data {
    long * src;
    long * dst;
    long n;
    long num_threads;
} bulk_copy_data;

replicated bulk_copy_data data;

// Initialize a long* with mw_replicated_init
void
init_replicated_ptr(long ** loc, long * ptr)
{
    mw_replicated_init((long*)loc, (long)ptr);
}

void
bulk_copy_data_init(bulk_copy_data * data, const char* alloc_mode, long n, long num_threads)
{
    mw_replicated_init(&data->n, n);
    mw_replicated_init(&data->num_threads, num_threads);

    long * local_to = mw_malloc1dlong(NODELETS());

    // Allocate an array on nodelet 0, and replicate the pointer
    init_replicated_ptr(&data->src,
        mw_localmalloc(n * sizeof(long), &local_to[0])
    );

    long remote_nodelet = 0;
    if (!strcmp(alloc_mode, "intra_nodelet")) {
        remote_nodelet = 0;
    } else if (!strcmp(alloc_mode, "intra_node")) {
        remote_nodelet = 1;
    } else if (!strcmp(alloc_mode, "intra_chick")) {
        remote_nodelet = 8;
    } else {
        LOG("Invalid alloc_mode, must be one of ['intra_nodelet', 'intra_node', 'intra_chick']\n");
        exit(-1);
    }
    // Allocate an array on nodelet 1, and replicate the pointer
    init_replicated_ptr(&data->dst,
        mw_localmalloc(n * sizeof(long), &local_to[remote_nodelet])
    );
#ifndef NO_VALIDATE
    // Initialize the arrays
    emu_local_for_set_long(data->src, n, 1);
    emu_local_for_set_long(data->dst, n, 2);
#endif
    mw_free(local_to);
}

void
bulk_copy_data_deinit(bulk_copy_data * data)
{
    mw_localfree(data->src);
    mw_localfree(data->dst);
}

noinline void
bulk_copy_memcpy(bulk_copy_data * data)
{
    memcpy(data->dst, data->src, data->n * sizeof(long));
}

noinline void
bulk_copy_serial(bulk_copy_data * data)
{
    for (long i = 0; i < data->n; ++i) {
        data->dst[i] = data->src[i];
    }
}

static noinline void
emu_local_for_copy_long_worker(long begin, long end, void * arg1, void * arg2)
{
    long * dst = arg1;
    long * src = arg2;
    for (long i = begin; i < end; ++i) {
        dst[i] = src[i];
    }
}

noinline void
bulk_copy_emu_for(bulk_copy_data * data)
{
    emu_local_for_copy_long(data->dst, data->src, data->n);
//    emu_local_for_v2(0, data->n, data->n / data->num_threads,
//        emu_local_for_copy_long_worker, data->dst, data->src
//    );
}


void
bulk_copy_validate(bulk_copy_data* data)
{
    for (long i = 0; i < data->n; ++i) {
        if (data->dst[i] != 1) {
            LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 1)\n", i, data->dst[i]);
            exit(1);
        }
    }
}

void bulk_copy_run(
    bulk_copy_data * data,
    void (*benchmark)(bulk_copy_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("bulk_copy");
        benchmark(data);
        double time_ms = hooks_region_end();
        double bytes_per_second = time_ms == 0 ? 0 :
            (data->n * sizeof(long) * 2) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}

replicated bulk_copy_data data;

int main(int argc, char** argv)
{
    struct {
        const char* spawn_mode;
        const char* alloc_mode;
        long log2_num_elements;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 6) {
        LOG("Usage: %s spawn_mode alloc_mode log2_num_elements num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.spawn_mode = argv[1];
        args.alloc_mode = argv[2];
        args.log2_num_elements = atol(argv[3]);
        args.num_threads = atol(argv[4]);
        args.num_trials = atol(argv[5]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("spawn_mode", args.spawn_mode);
    hooks_set_attr_str("alloc_mode", args.alloc_mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long));

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    LOG("Initializing arrays with %li elements each (%li MiB)\n", n, mbytes);
    bulk_copy_data_init(&data, args.alloc_mode, n, args.num_threads);
    LOG("Doing bulk copy (%s) using %s\n", args.alloc_mode, args.spawn_mode);

    #define RUN_BENCHMARK(X) bulk_copy_run(&data, X, args.num_trials)

    if (!strcmp(args.spawn_mode, "memcpy")) {
        RUN_BENCHMARK(bulk_copy_memcpy);
    } else if (!strcmp(args.spawn_mode, "serial")) {
        RUN_BENCHMARK(bulk_copy_serial);
    } else if (!strcmp(args.spawn_mode, "emu_for")) {
        RUN_BENCHMARK(bulk_copy_emu_for);
    } else {
        LOG("Spawn mode %s not implemented!", args.spawn_mode);
    }
#ifndef NO_VALIDATE
    LOG("Validating results...");
    bulk_copy_validate(&data);
    LOG("OK\n");
#endif

    bulk_copy_data_deinit(&data);
    return 0;
}
