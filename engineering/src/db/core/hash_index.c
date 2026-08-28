#include "db/hash_index.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

#define HASH_BUCKET_DEFAULT 1024

typedef struct hash_entry_s {
    void *key;
    size_t klen;
    uint64_t value;
    struct hash_entry_s *next;
} hash_entry_t;

struct hash_index_s {
    hash_entry_t **buckets;
    uint32_t n_buckets;
    size_t size;
};

static uint32_t fnv1a(const void *key, size_t klen) {
    const uint8_t *p = key;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < klen; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

hash_index_t *hash_index_create(uint32_t n_buckets) {
    hash_index_t *idx = calloc(1, sizeof(*idx));
    if (!idx) return NULL;
    idx->n_buckets = n_buckets > 0 ? n_buckets : HASH_BUCKET_DEFAULT;
    idx->buckets = calloc(idx->n_buckets, sizeof(hash_entry_t *));
    if (!idx->buckets) { free(idx); return NULL; }
    return idx;
}

void hash_index_destroy(hash_index_t *idx) {
    if (!idx) return;
    for (uint32_t i = 0; i < idx->n_buckets; ++i) {
        hash_entry_t *e = idx->buckets[i];
        while (e) {
            hash_entry_t *n = e->next;
            free(e->key); free(e);
            e = n;
        }
    }
    free(idx->buckets);
    free(idx);
}

int hash_index_put(hash_index_t *idx, const void *key, size_t klen,
                  uint64_t value) {
    if (!idx || !key) return -1;
    uint32_t h = fnv1a(key, klen) % idx->n_buckets;
    hash_entry_t *e = idx->buckets[h];
    while (e) {
        if (e->klen == klen && memcmp(e->key, key, klen) == 0) {
            e->value = value;
            return 0;
        }
        e = e->next;
    }
    e = calloc(1, sizeof(*e));
    if (!e) return -1;
    e->key = malloc(klen);
    if (!e->key) { free(e); return -1; }
    memcpy(e->key, key, klen);
    e->klen = klen;
    e->value = value;
    e->next = idx->buckets[h];
    idx->buckets[h] = e;
    idx->size++;
    return 0;
}

int hash_index_get(hash_index_t *idx, const void *key, size_t klen,
                  uint64_t *out_value) {
    if (!idx || !key) return -1;
    uint32_t h = fnv1a(key, klen) % idx->n_buckets;
    hash_entry_t *e = idx->buckets[h];
    while (e) {
        if (e->klen == klen && memcmp(e->key, key, klen) == 0) {
            *out_value = e->value;
            return 0;
        }
        e = e->next;
    }
    return -1;
}

int hash_index_del(hash_index_t *idx, const void *key, size_t klen) {
    if (!idx || !key) return -1;
    uint32_t h = fnv1a(key, klen) % idx->n_buckets;
    hash_entry_t **pp = &idx->buckets[h];
    while (*pp) {
        hash_entry_t *e = *pp;
        if (e->klen == klen && memcmp(e->key, key, klen) == 0) {
            *pp = e->next;
            free(e->key); free(e);
            idx->size--;
            return 0;
        }
        pp = &e->next;
    }
    return -1;
}
