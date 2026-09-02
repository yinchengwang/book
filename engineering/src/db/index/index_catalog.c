/**
 * @file index_catalog.c
 * @brief Index catalog implementation
 */

#include "index_catalog.h"

#include <errno.h>
#include <string.h>

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static index_entry_t **_index_catalog_find_entry(index_catalog_t *catalog,
                                                  int index_id)
{
    for (int i = 0; i < catalog->count; i++) {
        if (catalog->entries[i]->index_id == index_id) {
            return &catalog->entries[i];
        }
    }
    return NULL;
}

static int _index_catalog_grow(index_catalog_t *catalog)
{
    int new_capacity = catalog->capacity * 2;
    index_entry_t **new_entries = realloc(catalog->entries,
                                           new_capacity * sizeof(index_entry_t *));
    if (!new_entries) {
        return -1;
    }
    catalog->entries = new_entries;
    catalog->capacity = new_capacity;
    return 0;
}

/* ============================================================
 * Index Type String Conversion
 * ============================================================ */

static const char *_index_type_strings[] = {
    "BTREE",
    "HASH",
    "HNSW",
    "IVF",
    "FULLTEXT",
    "GIN"
};

const char *index_type_to_string(index_type_t type)
{
    if (type < 0 || type >= INDEX_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return _index_type_strings[type];
}

index_type_t index_type_from_string(const char *str)
{
    if (!str) {
        return INDEX_TYPE_COUNT;
    }
    for (int i = 0; i < INDEX_TYPE_COUNT; i++) {
        if (strcmp(str, _index_type_strings[i]) == 0) {
            return (index_type_t)i;
        }
    }
    return INDEX_TYPE_COUNT;
}

/* ============================================================
 * Index Catalog API
 * ============================================================ */

index_catalog_t *index_catalog_create(int initial_capacity)
{
    index_catalog_t *catalog;

    if (initial_capacity <= 0) {
        initial_capacity = 16;
    }

    catalog = malloc(sizeof(index_catalog_t));
    if (!catalog) {
        return NULL;
    }

    catalog->entries = calloc(initial_capacity, sizeof(index_entry_t *));
    if (!catalog->entries) {
        free(catalog);
        return NULL;
    }

    catalog->capacity = initial_capacity;
    catalog->count = 0;

    if (pthread_rwlock_init(&catalog->rwlock, NULL) != 0) {
        free(catalog->entries);
        free(catalog);
        return NULL;
    }

    return catalog;
}

void index_catalog_destroy(index_catalog_t *catalog)
{
    if (!catalog) {
        return;
    }

    pthread_rwlock_wrlock(&catalog->rwlock);

    for (int i = 0; i < catalog->count; i++) {
        index_entry_t *entry = catalog->entries[i];
        if (entry) {
            free(entry->columns);
            free(entry);
        }
    }
    free(catalog->entries);

    pthread_rwlock_unlock(&catalog->rwlock);
    pthread_rwlock_destroy(&catalog->rwlock);

    free(catalog);
}

int index_catalog_add(index_catalog_t *catalog, index_entry_t *entry)
{
    int result = -1;

    if (!catalog || !entry) {
        return -1;
    }

    pthread_rwlock_wrlock(&catalog->rwlock);

    /* Check for duplicate index_id */
    if (_index_catalog_find_entry(catalog, entry->index_id) != NULL) {
        goto unlock;
    }

    /* Grow if needed */
    if (catalog->count >= catalog->capacity) {
        if (_index_catalog_grow(catalog) != 0) {
            goto unlock;
        }
    }

    catalog->entries[catalog->count++] = entry;
    result = 0;

unlock:
    pthread_rwlock_unlock(&catalog->rwlock);
    return result;
}

int index_catalog_remove(index_catalog_t *catalog, int index_id)
{
    int result = -1;

    if (!catalog) {
        return -1;
    }

    pthread_rwlock_wrlock(&catalog->rwlock);

    index_entry_t **slot = _index_catalog_find_entry(catalog, index_id);
    if (!slot) {
        goto unlock;
    }

    /* Move last entry to removed slot */
    index_entry_t *entry = *slot;
    int last_idx = catalog->count - 1;
    if (last_idx > 0) {
        *slot = catalog->entries[last_idx];
    }
    catalog->count--;

    free(entry->columns);
    free(entry);
    result = 0;

unlock:
    pthread_rwlock_unlock(&catalog->rwlock);
    return result;
}

index_entry_t *index_catalog_get(index_catalog_t *catalog, int index_id)
{
    index_entry_t *result = NULL;

    if (!catalog) {
        return NULL;
    }

    pthread_rwlock_rdlock(&catalog->rwlock);

    index_entry_t **slot = _index_catalog_find_entry(catalog, index_id);
    if (slot) {
        result = *slot;
    }

    pthread_rwlock_unlock(&catalog->rwlock);
    return result;
}

int index_catalog_get_by_table(index_catalog_t *catalog, int table_id,
                               index_entry_t **results, int max_results)
{
    int count = 0;

    if (!catalog || !results || max_results <= 0) {
        return -1;
    }

    pthread_rwlock_rdlock(&catalog->rwlock);

    for (int i = 0; i < catalog->count && count < max_results; i++) {
        if (catalog->entries[i]->table_id == table_id) {
            results[count++] = catalog->entries[i];
        }
    }

    pthread_rwlock_unlock(&catalog->rwlock);
    return count;
}
