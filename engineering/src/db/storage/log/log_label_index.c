/**
 * @file log_label_index.c
 * @brief 标签倒排索引实装（C6.1）
 */
#include "db/storage/log/log_label_index.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_BUCKETS 1024
#define MAX_LABELS_PER_KEY 32

typedef struct label_key_s {
    char *label;
    char *value;
    uint64_t ids[MAX_LABELS_PER_KEY];
    int n_ids;
    struct label_key_s *next;
} label_key_t;

struct log_label_index_s {
    label_key_t *buckets[HASH_BUCKETS];
    int total_keys;
    int high_card_threshold;  /* 默认 10000 */
    char data_dir[512];
};

static uint32_t fnv_hash(const char *a, const char *b) {
    uint32_t h = 2166136261u;
    while (*a) { h = (h ^ (uint8_t)*a) * 16777619u; a++; }
    h = (h ^ '=') * 16777619u;
    while (*b) { h = (h ^ (uint8_t)*b) * 16777619u; b++; }
    return h;
}

static label_key_t *find_or_create(label_key_t **buckets,
                                   const char *label, const char *value,
                                   int *created) {
    uint32_t h = fnv_hash(label, value) % HASH_BUCKETS;
    label_key_t *k = buckets[h];
    while (k) {
        if (strcmp(k->label, label) == 0 && strcmp(k->value, value) == 0) {
            *created = 0;
            return k;
        }
        k = k->next;
    }
    /* 创建 */
    k = calloc(1, sizeof(*k));
    if (!k) return NULL;
    k->label = strdup(label);
    k->value = strdup(value);
    k->next = buckets[h];
    buckets[h] = k;
    *created = 1;
    return k;
}

log_label_index_t *log_label_index_create(const char *data_dir) {
    log_label_index_t *idx = calloc(1, sizeof(*idx));
    if (!idx) return NULL;
    idx->high_card_threshold = 10000;
    if (data_dir) strncpy(idx->data_dir, data_dir, sizeof(idx->data_dir) - 1);
    return idx;
}

void log_label_index_destroy(log_label_index_t *idx) {
    if (!idx) return;
    for (int i = 0; i < HASH_BUCKETS; ++i) {
        label_key_t *k = idx->buckets[i];
        while (k) {
            label_key_t *n = k->next;
            free(k->label); free(k->value);
            free(k);
            k = n;
        }
    }
    free(idx);
}

int log_label_index_put(log_label_index_t *idx,
                       const char *label, const char *value,
                       uint64_t stream_id) {
    if (!idx || !label || !value) return -1;
    int created = 0;
    label_key_t *k = find_or_create(idx->buckets, label, value, &created);
    if (!k) return -1;
    if (created) idx->total_keys++;
    if (k->n_ids < MAX_LABELS_PER_KEY) {
        /* 查重 */
        for (int i = 0; i < k->n_ids; ++i) {
            if (k->ids[i] == stream_id) return 0;
        }
        k->ids[k->n_ids++] = stream_id;
    }
    return 0;
}

int log_label_index_query(log_label_index_t *idx,
                          const char *labels[], const char *values[], int n,
                          log_label_index_match_cb cb, void *ctx) {
    if (!idx || !cb) return -1;
    /* 简化：取第一个 (label, value) → ids 集合 → 调用 cb */
    /* 完整实现：AND 语义需遍历所有 (label, value) 找交集 */
    if (n <= 0) return 0;
    uint32_t h = fnv_hash(labels[0], values[0]) % HASH_BUCKETS;
    label_key_t *k = idx->buckets[h];
    while (k) {
        if (strcmp(k->label, labels[0]) == 0 && strcmp(k->value, values[0]) == 0) {
            for (int i = 0; i < k->n_ids; ++i) {
                cb(k->ids[i], ctx);
            }
            return k->n_ids;
        }
        k = k->next;
    }
    return 0;
}

void log_label_index_set_threshold(log_label_index_t *idx, int threshold) {
    if (!idx) return;
    idx->high_card_threshold = threshold;
}

bool log_label_index_is_high_cardinality(log_label_index_t *idx,
                                         const char *label) {
    if (!idx || !label) return false;
    uint32_t h = fnv_hash(label, "") % HASH_BUCKETS;
    int n = 0;
    for (label_key_t *k = idx->buckets[h]; k; k = k->next) {
        if (strcmp(k->label, label) == 0) n += k->n_ids;
        if (n > idx->high_card_threshold) return true;
    }
    return false;
}
