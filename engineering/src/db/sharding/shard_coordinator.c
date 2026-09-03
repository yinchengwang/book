// engineering/src/db/sharding/shard_coordinator.c
#include "db/sharding/shard_coordinator.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct shard_coordinator {
    shard_balance_config_t *config;
    shard_router_t *router;
    load_collector_t *collector;
    bool running;
    pthread_t monitor_thread;
};

static void *monitor_thread_func(void *arg) {
    shard_coordinator_t *coord = (shard_coordinator_t *)arg;

    while (coord->running) {
        sleep(coord->config->check_interval_ms / 1000);

        if (!coord->config->auto_rebalance) continue;

        double skew = load_collector_calculate_skew(coord->collector);
        if (skew > coord->config->skew_threshold) {
            // 阈值超限，触发再平衡检查
            shard_coordinator_check_and_rebalance(coord);
        }
    }
    return NULL;
}

shard_coordinator_t *shard_coordinator_create(const shard_balance_config_t *config,
                                               shard_router_t *router) {
    if (!config || !router) return NULL;

    shard_coordinator_t *coord = (shard_coordinator_t *)calloc(1,
        sizeof(shard_coordinator_t));
    if (!coord) return NULL;

    coord->config = (shard_balance_config_t *)malloc(sizeof(shard_balance_config_t));
    if (!coord->config) {
        free(coord);
        return NULL;
    }
    memcpy(coord->config, config, sizeof(shard_balance_config_t));

    coord->router = router;
    coord->collector = load_collector_create(16);
    if (!coord->collector) {
        free(coord->config);
        free(coord);
        return NULL;
    }

    coord->running = false;
    return coord;
}

void shard_coordinator_destroy(shard_coordinator_t *coord) {
    if (!coord) return;
    shard_coordinator_stop(coord);
    if (coord->collector) load_collector_destroy(coord->collector);
    if (coord->config) free(coord->config);
    free(coord);
}

int shard_coordinator_start(shard_coordinator_t *coord) {
    if (!coord || coord->running) return -1;
    coord->running = true;
    if (pthread_create(&coord->monitor_thread, NULL, monitor_thread_func, coord) != 0) {
        coord->running = false;
        return -1;
    }
    return 0;
}

void shard_coordinator_stop(shard_coordinator_t *coord) {
    if (!coord || !coord->running) return;
    coord->running = false;
    pthread_join(coord->monitor_thread, NULL);
}

int shard_coordinator_check_and_rebalance(shard_coordinator_t *coord) {
    if (!coord) return -1;
    // TODO: Task 4 实现再平衡逻辑
    return 0;
}

int shard_coordinator_select_least_load(shard_coordinator_t *coord,
                                         const int *candidate_shards,
                                         int count) {
    if (!coord || !candidate_shards || count <= 0) return -1;

    int best_shard = -1;
    double min_load = 1e100;  // DBL_MAX 近似

    for (int i = 0; i < count; i++) {
        const shard_load_t *load = load_collector_get(coord->collector,
                                                       candidate_shards[i]);
        if (!load) continue;

        // 负载计算公式：load = 0.4 * row_count + 0.3 * qps + 0.3 * latency_ms
        double load_value = 0.4 * load->row_count +
                      0.3 * load->qps +
                      0.3 * load->latency_ms;

        if (load_value < min_load) {
            min_load = load_value;
            best_shard = candidate_shards[i];
        }
    }

    return best_shard;
}

shard_router_t *shard_coordinator_get_router(shard_coordinator_t *coord) {
    if (!coord) return NULL;
    return coord->router;
}