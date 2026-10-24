/**
 * @file index_manager.h
 * @brief Index manager for unified index lifecycle management
 *
 * The index manager provides centralized management of all index lifecycle
 * operations including creation, deletion, rebuilding, and lookup of indexes.
 * It wraps the index catalog and provides a higher-level API for index management.
 */

#ifndef DB_INDEX_MANAGER_H
#define DB_INDEX_MANAGER_H

#include "index_catalog.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Type Definitions
 * ============================================================ */

/** Index manager structure */
typedef struct index_manager {
    index_catalog_t *catalog;
    int next_index_id;
    pthread_mutex_t id_mutex;
    bool owns_catalog;
} index_manager_t;

/* ============================================================
 * Index Manager API
 * ============================================================ */

/**
 * @brief Create a new index manager
 * @return New index manager or NULL on failure
 */
index_manager_t *index_manager_create(void);

/**
 * @brief Destroy an index manager and free all resources
 * @param mgr Index manager to destroy
 */
void index_manager_destroy(index_manager_t *mgr);

/**
 * @brief Create a new index and add it to the catalog
 * @param mgr Index manager
 * @param name Index name (must be unique)
 * @param type Index type (BTREE, HASH, HNSW, etc.)
 * @param table_id ID of the table this index belongs to
 * @param columns Array of column IDs to index
 * @param column_count Number of columns in the index
 * @param config Index configuration (use index_config_default for defaults)
 * @return 0 on success, -1 on failure
 */
int index_manager_create_index(index_manager_t *mgr,
                               const char *name,
                               index_type_t type,
                               int table_id,
                               const int *columns,
                               int column_count,
                               const index_config_t *config);

/**
 * @brief Drop an index from the catalog
 * @param mgr Index manager
 * @param index_id ID of the index to drop
 * @return 0 on success, -1 if not found
 */
int index_manager_drop_index(index_manager_t *mgr, int index_id);

/**
 * @brief Rebuild an existing index
 * @param mgr Index manager
 * @param index_id ID of the index to rebuild
 * @return 0 on success, -1 if not found
 */
int index_manager_rebuild_index(index_manager_t *mgr, int index_id);

/**
 * @brief Get an index entry by ID
 * @param mgr Index manager
 * @param index_id ID of the index to retrieve
 * @return Index entry or NULL if not found
 */
const index_entry_t *index_manager_get_index(index_manager_t *mgr, int index_id);

/**
 * @brief Get all indexes for a specific table
 * @param mgr Index manager
 * @param table_id Table ID to query
 * @param results Array to store matching index entries
 * @param max_results Maximum number of results to return
 * @return Number of matching entries, or -1 on failure
 */
int index_manager_get_table_indexes(index_manager_t *mgr,
                                    int table_id,
                                    index_entry_t **results,
                                    int max_results);

/**
 * @brief Get the underlying index catalog
 * @param mgr Index manager
 * @return Index catalog or NULL if mgr is NULL
 */
index_catalog_t *index_manager_get_catalog(index_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_MANAGER_H */
