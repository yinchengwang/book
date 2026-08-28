#ifndef DB_HASH_INDEX_H
#define DB_HASH_INDEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hash_index_s hash_index_t;

hash_index_t *hash_index_create(uint32_t n_buckets);
void hash_index_destroy(hash_index_t *idx);

/* O(1) put/get/del */
int hash_index_put(hash_index_t *idx, const void *key, size_t klen,
                  uint64_t value);
int hash_index_get(hash_index_t *idx, const void *key, size_t klen,
                  uint64_t *out_value);
int hash_index_del(hash_index_t *idx, const void *key, size_t klen);

#ifdef __cplusplus
}
#endif

#endif
