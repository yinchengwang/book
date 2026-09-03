#include <algo-prod/map/map.h>

#include <stdlib.h>
#include <string.h>

#define MAP_DEFAULT_BUCKET_COUNT 16u
#define MAP_MAX_LOAD_NUMERATOR 3u
#define MAP_MAX_LOAD_DENOMINATOR 4u

struct map_entry {
    struct map_entry *bucket_next;
    struct map_entry *iter_prev;
    struct map_entry *iter_next;
    uint64_t hash;
    size_t key_size;
    unsigned char data[];
};

struct map {
    size_t key_size;
    size_t value_size;
    size_t size;
    size_t bucket_count;
    map_entry_t **buckets;
    map_entry_t *head;
    map_entry_t *tail;
    map_hash_fn hash;
    map_equals_fn equals;
    void (*free_key)(void *key);
    void (*free_value)(void *value);
};

static void *map_entry_key_ptr(map_entry_t *entry)
{
    return entry->data;
}

static void *map_entry_value_ptr(const map_t *map, map_entry_t *entry)
{
    return entry->data + map->key_size;
}

static const void *map_entry_key_ptr_const(const map_entry_t *entry)
{
    return entry->data;
}

static const void *map_entry_value_ptr_const(const map_t *map, const map_entry_t *entry)
{
    return entry->data + map->key_size;
}

static size_t map_bucket_index(const map_t *map, uint64_t hash)
{
    return (size_t)(hash % map->bucket_count);
}

static map_entry_t *map_find_entry(const map_t *map, const void *key, uint64_t hash)
{
    map_entry_t *entry;

    if (!map || !key || map->bucket_count == 0u) {
        return NULL;
    }

    entry = map->buckets[map_bucket_index(map, hash)];
    while (entry) {
        if (entry->hash == hash && map->equals(map_entry_key_ptr_const(entry), key)) {
            return entry;
        }
        entry = entry->bucket_next;
    }

    return NULL;
}

static void map_unlink_entry(map_t *map, map_entry_t *entry, size_t bucket_index)
{
    map_entry_t **link;

    link = &map->buckets[bucket_index];
    while (*link && *link != entry) {
        link = &(*link)->bucket_next;
    }
    if (*link == entry) {
        *link = entry->bucket_next;
    }

    if (entry->iter_prev) {
        entry->iter_prev->iter_next = entry->iter_next;
    } else {
        map->head = entry->iter_next;
    }
    if (entry->iter_next) {
        entry->iter_next->iter_prev = entry->iter_prev;
    } else {
        map->tail = entry->iter_prev;
    }
}

static void map_release_entry(map_t *map, map_entry_t *entry, int transfer_value, void *out_value)
{
    if (!map || !entry) {
        return;
    }

    if (out_value) {
        memcpy(out_value, map_entry_value_ptr(map, entry), map->value_size);
    } else if (!transfer_value && map->free_value) {
        map->free_value(map_entry_value_ptr(map, entry));
    }

    if (map->free_key) {
        map->free_key(map_entry_key_ptr(entry));
    }

    free(entry);
}

static int map_rehash(map_t *map, size_t new_bucket_count)
{
    map_entry_t **new_buckets;
    map_entry_t *entry;

    new_buckets = (map_entry_t **)calloc(new_bucket_count, sizeof(map_entry_t *));
    if (!new_buckets) {
        return -1;
    }

    entry = map->head;
    while (entry) {
        size_t bucket_index = (size_t)(entry->hash % new_bucket_count);
        entry->bucket_next = new_buckets[bucket_index];
        new_buckets[bucket_index] = entry;
        entry = entry->iter_next;
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;
    return 0;
}

static int map_maybe_grow(map_t *map)
{
    size_t threshold;

    threshold = (map->bucket_count * MAP_MAX_LOAD_NUMERATOR) / MAP_MAX_LOAD_DENOMINATOR;
    if (map->size + 1u <= threshold) {
        return 0;
    }

    return map_rehash(map, map->bucket_count * 2u);
}

map_t *map_create(size_t key_size,
                  size_t value_size,
                  map_hash_fn hash,
                  map_equals_fn equals)
{
    return map_create_ex(key_size, value_size, 0u, hash, equals, NULL, NULL);
}

map_t *map_create_ex(size_t key_size,
                     size_t value_size,
                     size_t initial_bucket_count,
                     map_hash_fn hash,
                     map_equals_fn equals,
                     void (*free_key)(void *key),
                     void (*free_value)(void *value))
{
    map_t *map;

    if (key_size == 0u || value_size == 0u || !hash || !equals) {
        return NULL;
    }

    if (initial_bucket_count == 0u) {
        initial_bucket_count = MAP_DEFAULT_BUCKET_COUNT;
    }

    map = (map_t *)calloc(1, sizeof(map_t));
    if (!map) {
        return NULL;
    }

    map->buckets = (map_entry_t **)calloc(initial_bucket_count, sizeof(map_entry_t *));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->key_size = key_size;
    map->value_size = value_size;
    map->bucket_count = initial_bucket_count;
    map->hash = hash;
    map->equals = equals;
    map->free_key = free_key;
    map->free_value = free_value;
    return map;
}

void map_destroy(map_t *map)
{
    if (!map) {
        return;
    }

    map_clear(map);
    free(map->buckets);
    free(map);
}

int map_put(map_t *map, const void *key, const void *value)
{
    map_entry_t *entry;
    uint64_t hash_value;
    size_t bucket_index;

    if (!map || !key || !value) {
        return -1;
    }

    hash_value = map->hash(key);
    entry = map_find_entry(map, key, hash_value);
    if (entry) {
        if (map->free_value) {
            map->free_value(map_entry_value_ptr(map, entry));
        }
        memcpy(map_entry_value_ptr(map, entry), value, map->value_size);
        return 0;
    }

    if (map_maybe_grow(map) != 0) {
        return -1;
    }

    entry = (map_entry_t *)malloc(sizeof(map_entry_t) + map->key_size + map->value_size);
    if (!entry) {
        return -1;
    }

    entry->hash = hash_value;
    entry->key_size = map->key_size;
    entry->iter_prev = map->tail;
    entry->iter_next = NULL;
    memcpy(map_entry_key_ptr(entry), key, map->key_size);
    memcpy(map_entry_value_ptr(map, entry), value, map->value_size);

    if (map->tail) {
        map->tail->iter_next = entry;
    } else {
        map->head = entry;
    }
    map->tail = entry;

    bucket_index = map_bucket_index(map, hash_value);
    entry->bucket_next = map->buckets[bucket_index];
    map->buckets[bucket_index] = entry;
    map->size += 1u;
    return 0;
}

int map_get(const map_t *map, const void *key, void *out_value)
{
    map_entry_t *entry;

    if (!map || !key || !out_value) {
        return -1;
    }

    entry = map_find_entry(map, key, map->hash(key));
    if (!entry) {
        return -1;
    }

    memcpy(out_value, map_entry_value_ptr_const(map, entry), map->value_size);
    return 0;
}

const void *map_get_ptr(const map_t *map, const void *key)
{
    map_entry_t *entry;

    if (!map || !key) {
        return NULL;
    }

    entry = map_find_entry(map, key, map->hash(key));
    return entry ? map_entry_value_ptr_const(map, entry) : NULL;
}

bool map_contains(const map_t *map, const void *key)
{
    return map_get_ptr(map, key) != NULL;
}

int map_remove(map_t *map, const void *key, void *out_value)
{
    map_entry_t *entry;
    uint64_t hash_value;
    size_t bucket_index;

    if (!map || !key) {
        return -1;
    }

    hash_value = map->hash(key);
    entry = map_find_entry(map, key, hash_value);
    if (!entry) {
        return -1;
    }

    bucket_index = map_bucket_index(map, hash_value);
    map_unlink_entry(map, entry, bucket_index);
    map->size -= 1u;
    map_release_entry(map, entry, out_value != NULL, out_value);
    return 0;
}

const map_entry_t *map_first(const map_t *map)
{
    return map ? map->head : NULL;
}

const map_entry_t *map_entry_next(const map_entry_t *entry)
{
    return entry ? entry->iter_next : NULL;
}

const void *map_entry_key(const map_entry_t *entry)
{
    return entry ? entry->data : NULL;
}

const void *map_entry_value(const map_entry_t *entry)
{
    return entry ? entry->data + entry->key_size : NULL;
}

size_t map_size(const map_t *map)
{
    return map ? map->size : 0u;
}

bool map_empty(const map_t *map)
{
    return !map || map->size == 0u;
}

void map_clear(map_t *map)
{
    map_entry_t *entry;
    map_entry_t *next;
    size_t bucket_index;

    if (!map) {
        return;
    }

    entry = map->head;
    while (entry) {
        next = entry->iter_next;
        bucket_index = map_bucket_index(map, entry->hash);
        map_unlink_entry(map, entry, bucket_index);
        map_release_entry(map, entry, 0, NULL);
        entry = next;
    }

    memset(map->buckets, 0, map->bucket_count * sizeof(map_entry_t *));
    map->head = NULL;
    map->tail = NULL;
    map->size = 0u;
}