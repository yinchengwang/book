/**
 * @file index_catalog.h
 * @brief Index catalog for managing index metadata
 *
 * The index catalog provides centralized management of all index metadata
 * in the database system, including index type, state, columns, and configuration.
 */

#ifndef DB_INDEX_CATALOG_H
#define DB_INDEX_CATALOG_H

#include "index_config.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#ifndef MAX_INDEX_NAME_LEN
#define MAX_INDEX_NAME_LEN 128
#endif

/* ============================================================
 * Type Definitions
 * ============================================================ */

/** Index type enumeration */
typedef enum {
    INDEX_TYPE_BTREE = 0,
    INDEX_TYPE_HASH,
    INDEX_TYPE_HNSW,
    INDEX_TYPE_IVF,
    INDEX_TYPE_FULLTEXT,
    INDEX_TYPE_GIN,
    INDEX_TYPE_COUNT
} index_type_t;

/** Index state enumeration */
typedef enum {
    INDEX_STATE_BUILDING = 0,
    INDEX_STATE_READY,
    INDEX_STATE_DELETING
} index_state_t;

/** Index entry structure */
typedef struct index_entry {
    int index_id;
    char name[MAX_INDEX_NAME_LEN];
    index_type_t type;
    index_state_t state;
    int table_id;
    int *columns;
    int column_count;
    void *index_impl;
    index_config_t config;
    time_t created_at;
    size_t size_bytes;
} index_entry_t;

/** Index catalog structure */
typedef struct index_catalog {
    index_entry_t **entries;
    int capacity;
    int count;
    pthread_rwlock_t rwlock;
} index_catalog_t;

/* ============================================================
 * Index Catalog API
 * ============================================================ */

/**
 * @brief Create a new index catalog
 * @param initial_capacity Initial capacity for index entries
 * @return New index catalog or NULL on failure
 */
index_catalog_t *index_catalog_create(int initial_capacity);

/**
 * @brief Destroy an index catalog and free all resources
 * @param catalog Index catalog to destroy
 */
void index_catalog_destroy(index_catalog_t *catalog);

/**
 * @brief Add an index entry to the catalog
 * @param catalog Index catalog
 * @param entry Index entry to add
 * @return 0 on success, -1 on failure
 */
int index_catalog_add(index_catalog_t *catalog, index_entry_t *entry);

/**
 * @brief Remove an index entry from the catalog
 * @param catalog Index catalog
 * @param index_id ID of the index to remove
 * @return 0 on success, -1 if not found
 */
int index_catalog_remove(index_catalog_t *catalog, int index_id);

/**
 * @brief Get an index entry by ID
 * @param catalog Index catalog
 * @param index_id ID of the index to retrieve
 * @return Index entry or NULL if not found
 */
index_entry_t *index_catalog_get(index_catalog_t *catalog, int index_id);

/**
 * @brief Get all index entries for a specific table
 * @param catalog Index catalog
 * @param table_id Table ID to query
 * @param results Array to store matching index entries
 * @param max_results Maximum number of results to return
 * @return Number of matching entries, or -1 on failure
 */
int index_catalog_get_by_table(index_catalog_t *catalog, int table_id,
                               index_entry_t **results, int max_results);

/**
 * @brief Convert index type to string representation
 * @param type Index type
 * @return String representation or "UNKNOWN" if invalid
 */
const char *index_type_to_string(index_type_t type);

/**
 * @brief Convert string to index type
 * @param str String representation
 * @return Index type or INDEX_TYPE_COUNT if invalid
 */
index_type_t index_type_from_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_CATALOG_H */
