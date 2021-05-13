#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <emu_c_utils/emu_c_utils.h>

#include "common.h"

typedef struct ping_pong_data {
    long * a;
    long num_migrations;
    long num_threads;
} ping_pong_data;

void
ping_pong_init(ping_pong_data * data, long num_migrations, long num_threads)
{
    data->num_migrations = num_migrations;
    data->num_threads = num_threads;
    data->a = mw_malloc1dlong(NODELETS());
}

void
ping_pong_deinit(ping_pong_data * data)
{
    mw_free(data->a);
}

// Migrate back and forth between two adjacent nodelets
void
ping_pong_local(ping_pong_data * data)
{
    long * a = data->a;
    // Each iteration forces four migrations
    long n = data->num_migrations / 4;
    for (long i = 0; i < n; ++i) {
        MIGRATE(&a[1]);
        MIGRATE(&a[0]);
        MIGRATE(&a[1]);
        MIGRATE(&a[0]);
    }
}

// Migrate back and forth between two adjacent nodes
void
ping_pong_global(ping_pong_data * data)
{
    long * a = data->a;
    // Each iteration forces four migrations
    long n = data->num_migrations / 4;
    for (long i = 0; i < n; ++i) {
        MIGRATE(&a[8]);
        MIGRATE(&a[0]);
        MIGRATE(&a[8]);
        MIGRATE(&a[0]);
    }
}

void
ping_pong_global_sweep(ping_pong_data * data, long src_node, long dst_node)
{
    long * a = data->a;
    // Each iteration forces four migrations
    long n = data->num_migrations / 4;

    // Pick a nodelet on each node
    const long nlets_per_node = 8;
    long src_nlet = src_node * nlets_per_node;
    long dst_nlet = dst_node * nlets_per_node;

    for (long i = 0; i < n; ++i) {
        MIGRATE(&a[dst_nlet]);
        MIGRATE(&a[src_nlet]);
        MIGRATE(&a[dst_nlet]);
        MIGRATE(&a[src_nlet]);
    }
}

void
ping_pong_global_sweep_nlets(ping_pong_data * data, long src_nlet, long dst_nlet)
{
    long * a = data->a;
    // Each iteration forces four migrations
    long n = data->num_migrations / 4;

    for (long i = 0; i < n; ++i) {
        MIGRATE(&a[dst_nlet]);
        MIGRATE(&a[src_nlet]);
        MIGRATE(&a[dst_nlet]);
        MIGRATE(&a[src_nlet]);
    }
}

void
ping_pong_spawn_local(ping_pong_data * data)
{
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn ping_pong_local(data);
    }
}

void
ping_pong_spawn_global(ping_pong_data * data)
{
    runtime_assert(NODELETS() > 8,
        "Global ping pong requires a configuration with more than one node (more than 8 nodelets)"
    );
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn ping_pong_global(data);
    }
}

void
ping_pong_spawn_global_sweep(ping_pong_data * data)
{
    runtime_assert(NODELETS() > 8,
        "Global ping pong requires a configuration with more than one node (more than 8 nodelets)"
    );

    const long nlets_per_node = 8;
    const long num_nodes = NODELETS() / nlets_per_node;

    for (long src_node = 0; src_node < num_nodes; ++src_node) {
        for (long dst_node = 0; dst_node < num_nodes; ++dst_node) {
            if (dst_node <= src_node) { continue; }

            LOG("Migrating between node %li and node %li\n", src_node, dst_node);

            for (long i = 0; i < data->num_threads; ++i) {
                cilk_spawn ping_pong_global_sweep(data, src_node, dst_node);
            }
            cilk_sync;
        }
    }
}

void
ping_pong_spawn_global_sweep_nlets(ping_pong_data * data)
{
    const long nlets_per_node = 8;
    const long num_nodes = NODELETS() / nlets_per_node;

    for (long src_nlet = 0; src_nlet < NODELETS(); ++src_nlet) {
        for (long dst_nlet = 0; dst_nlet < NODELETS(); ++dst_nlet) {
            // Skip duplicate trials
            if (dst_nlet <= src_nlet) { continue; }
            // Only intra-node migration trials
            if (dst_nlet / num_nodes == src_nlet / num_nodes) { continue; }

            LOG("Migrating between nlet %li and nlet %li\n", src_nlet, dst_nlet);

            hooks_set_attr_i64("src_nlet", src_nlet);
            hooks_set_attr_i64("dst_nlet", dst_nlet);
            hooks_region_begin("ping_pong");

            for (long i = 0; i < data->num_threads; ++i) {
                cilk_spawn ping_pong_global_sweep_nlets(data, src_nlet, dst_nlet);
            }
            cilk_sync;

            double time_ms = hooks_region_end();
            if (time_ms != 0) { // simulator was run without timing mode enabled
                double migrations_per_second = (data->num_migrations) / (time_ms/1e3);
                LOG("%3.2f million migrations per second\n", migrations_per_second / (1e6));
                LOG("Latency (amortized): %3.2f us\n", (1.0 / migrations_per_second) * 1e6);
            }
        }
    }
}

void ping_pong_run(
    ping_pong_data * data,
    const char * name,
    void (*benchmark)(ping_pong_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark(data);
        double time_ms = hooks_region_end();
        if (time_ms == 0) return; // simulator was run without timing mode enabled
        double migrations_per_second = (data->num_migrations) / (time_ms/1e3);
        LOG("%3.2f million migrations per second\n", migrations_per_second / (1e6));
        LOG("Latency (amortized): %3.2f us\n", (1.0 / migrations_per_second) * 1e6);
    }
}

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        long log2_num_migrations;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 5) {
        LOG("Usage: %s mode log2_num_migrations num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_migrations = atol(argv[2]);
        args.num_threads = atol(argv[3]);
        args.num_trials = atol(argv[4]);

        if (args.log2_num_migrations <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("mode", args.mode);
    hooks_set_attr_i64("log2_num_migrations", args.log2_num_migrations);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());

    long n = 1L << args.log2_num_migrations;
    ping_pong_data data;
    data.num_threads = args.num_threads;
    ping_pong_init(&data, n, args.num_threads);
    LOG("Doing %s ping pong \n", args.mode);

    #define RUN_BENCHMARK(X) ping_pong_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "local")) {
        RUN_BENCHMARK(ping_pong_spawn_local);
    } else if (!strcmp(args.mode, "global")) {
        RUN_BENCHMARK(ping_pong_spawn_global);
    } else if (!strcmp(args.mode, "global_sweep")) {
        RUN_BENCHMARK(ping_pong_spawn_global_sweep);
    } else if (!strcmp(args.mode, "global_sweep_nlets")) {
        RUN_BENCHMARK(ping_pong_spawn_global_sweep_nlets);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }

    ping_pong_deinit(&data);
    return 0;
}
