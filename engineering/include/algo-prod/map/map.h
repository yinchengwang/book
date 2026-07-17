#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct map map_t;
typedef struct map_entry map_entry_t;
typedef uint64_t (*map_hash_fn)(const void *key);
typedef bool (*map_equals_fn)(const void *lhs, const void *rhs);

map_t *map_create(size_t key_size, size_t value_size, map_hash_fn hash, map_equals_fn equals);
map_t *map_create_ex(size_t key_size,
                     size_t value_size,
                     size_t initial_bucket_count,
                     map_hash_fn hash,
                     map_equals_fn equals,
                     void (*free_key)(void *key),
                     void (*free_value)(void *value));
void map_destroy(map_t *map);
int map_put(map_t *map, const void *key, const void *value);
int map_get(const map_t *map, const void *key, void *out_value);
const void *map_get_ptr(const map_t *map, const void *key);
bool map_contains(const map_t *map, const void *key);
int map_remove(map_t *map, const void *key, void *out_value);
const map_entry_t *map_first(const map_t *map);
const map_entry_t *map_entry_next(const map_entry_t *entry);
const void *map_entry_key(const map_entry_t *entry);
const void *map_entry_value(const map_entry_t *entry);
size_t map_size(const map_t *map);
bool map_empty(const map_t *map);
void map_clear(map_t *map);

#ifdef __cplusplus
}
#endif

#endif /* MAP_H */