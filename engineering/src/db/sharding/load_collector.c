// engineering/src/db/sharding/load_collector.c
#include "db/sharding/shard_coordinator.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct load_collector {
    shard_load_t *shards;
    int capacity;
    int count;
    pthread_mutex_t mutex;
};

load_collector_t *load_collector_create(int initial_capacity) {
    load_collector_t *c = (load_collector_t *)calloc(1, sizeof(load_collector_t));
    if (!c) return NULL;

    c->shards = (shard_load_t *)calloc(initial_capacity, sizeof(shard_load_t));
    if (!c->shards) {
        free(c);
        return NULL;
    }

    c->capacity = initial_capacity;
    c->count = 0;
    pthread_mutex_init(&c->mutex, NULL);
    return c;
}

void load_collector_destroy(load_collector_t *c) {
    if (!c) return;
    pthread_mutex_destroy(&c->mutex);
    free(c->shards);
    free(c);
}

int load_collector_update(load_collector_t *c, const shard_load_t *load) {
    if (!c || !load) return -1;

    pthread_mutex_lock(&c->mutex);

    // 查找或插入
    for (int i = 0; i < c->count; i++) {
        if (c->shards[i].shard_id == load->shard_id) {
            c->shards[i] = *load;
            pthread_mutex_unlock(&c->mutex);
            return 0;
        }
    }

    // 需要扩容
    if (c->count >= c->capacity) {
        int new_cap = c->capacity * 2;
        shard_load_t *new_shards = (shard_load_t *)realloc(c->shards,
            new_cap * sizeof(shard_load_t));
        if (!new_shards) {
            pthread_mutex_unlock(&c->mutex);
            return -1;
        }
        c->shards = new_shards;
        c->capacity = new_cap;
    }

    c->shards[c->count++] = *load;
    pthread_mutex_unlock(&c->mutex);
    return 0;
}

const shard_load_t *load_collector_get(load_collector_t *c, int shard_id) {
    if (!c) return NULL;

    pthread_mutex_lock(&c->mutex);
    for (int i = 0; i < c->count; i++) {
        if (c->shards[i].shard_id == shard_id) {
            pthread_mutex_unlock(&c->mutex);
            return &c->shards[i];
        }
    }
    pthread_mutex_unlock(&c->mutex);
    return NULL;
}

double load_collector_calculate_skew(load_collector_t *c) {
    if (!c || c->count == 0) return 0.0;

    pthread_mutex_lock(&c->mutex);

    // 计算总行数和最大值
    uint64_t total = 0;
    uint64_t max_rows = 0;
    for (int i = 0; i < c->count; i++) {
        total += c->shards[i].row_count;
        if (c->shards[i].row_count > max_rows) {
            max_rows = c->shards[i].row_count;
        }
    }

    pthread_mutex_unlock(&c->mutex);

    if (total == 0) return 0.0;
    double avg = (double)total / c->count;
    if (avg == 0) return 0.0;
    return (double)max_rows / avg;
}