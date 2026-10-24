/**
 * @file index_manager.c
 * @brief Index manager implementation
 */

#include "index_manager.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static int _index_manager_allocate_id(index_manager_t *mgr)
{
    int id;
    pthread_mutex_lock(&mgr->id_mutex);
    id = mgr->next_index_id++;
    pthread_mutex_unlock(&mgr->id_mutex);
    return id;
}

static index_entry_t *_index_manager_create_entry(
    int index_id,
    const char *name,
    index_type_t type,
    int table_id,
    const int *columns,
    int column_count,
    const index_config_t *config)
{
    index_entry_t *entry;

    entry = malloc(sizeof(index_entry_t));
    if (!entry) {
        return NULL;
    }

    memset(entry, 0, sizeof(index_entry_t));

    entry->index_id = index_id;

    if (name) {
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
    }

    entry->type = type;
    entry->state = INDEX_STATE_BUILDING;
    entry->table_id = table_id;
    entry->column_count = column_count;

    if (column_count > 0 && columns) {
        entry->columns = malloc(column_count * sizeof(int));
        if (!entry->columns) {
            free(entry);
            return NULL;
        }
        memcpy(entry->columns, columns, column_count * sizeof(int));
    }

    if (config) {
        entry->config = *config;
    }

    entry->created_at = time(NULL);

    return entry;
}

/* ============================================================
 * Index Manager API
 * ============================================================ */

index_manager_t *index_manager_create(void)
{
    index_manager_t *mgr;

    mgr = malloc(sizeof(index_manager_t));
    if (!mgr) {
        return NULL;
    }

    memset(mgr, 0, sizeof(index_manager_t));

    mgr->catalog = index_catalog_create(16);
    if (!mgr->catalog) {
        free(mgr);
        return NULL;
    }

    mgr->owns_catalog = true;
    mgr->next_index_id = 1;

    if (pthread_mutex_init(&mgr->id_mutex, NULL) != 0) {
        index_catalog_destroy(mgr->catalog);
        free(mgr);
        return NULL;
    }

    return mgr;
}

void index_manager_destroy(index_manager_t *mgr)
{
    if (!mgr) {
        return;
    }

    pthread_mutex_destroy(&mgr->id_mutex);

    if (mgr->owns_catalog && mgr->catalog) {
        index_catalog_destroy(mgr->catalog);
    }

    free(mgr);
}

int index_manager_create_index(index_manager_t *mgr,
                               const char *name,
                               index_type_t type,
                               int table_id,
                               const int *columns,
                               int column_count,
                               const index_config_t *config)
{
    index_entry_t *entry;
    int id;
    int result;

    if (!mgr || !name || column_count <= 0 || !columns) {
        return -1;
    }

    if (type < 0 || type >= INDEX_TYPE_COUNT) {
        return -1;
    }

    id = _index_manager_allocate_id(mgr);

    entry = _index_manager_create_entry(id, name, type, table_id,
                                        columns, column_count, config);
    if (!entry) {
        return -1;
    }

    result = index_catalog_add(mgr->catalog, entry);
    if (result != 0) {
        free(entry->columns);
        free(entry);
        return -1;
    }

    entry->state = INDEX_STATE_READY;

    return 0;
}

int index_manager_drop_index(index_manager_t *mgr, int index_id)
{
    index_entry_t *entry;

    if (!mgr) {
        return -1;
    }

    entry = (index_entry_t *)index_catalog_get(mgr->catalog, index_id);
    if (!entry) {
        return -1;
    }

    entry->state = INDEX_STATE_DELETING;

    return index_catalog_remove(mgr->catalog, index_id);
}

int index_manager_rebuild_index(index_manager_t *mgr, int index_id)
{
    index_entry_t *entry;

    if (!mgr) {
        return -1;
    }

    entry = (index_entry_t *)index_catalog_get(mgr->catalog, index_id);
    if (!entry) {
        return -1;
    }

    if (entry->state == INDEX_STATE_DELETING) {
        return -1;
    }

    entry->state = INDEX_STATE_BUILDING;

    entry->state = INDEX_STATE_READY;

    return 0;
}

const index_entry_t *index_manager_get_index(index_manager_t *mgr, int index_id)
{
    if (!mgr) {
        return NULL;
    }

    return index_catalog_get(mgr->catalog, index_id);
}

int index_manager_get_table_indexes(index_manager_t *mgr,
                                    int table_id,
                                    index_entry_t **results,
                                    int max_results)
{
    if (!mgr) {
        return -1;
    }

    return index_catalog_get_by_table(mgr->catalog, table_id,
                                      results, max_results);
}

index_catalog_t *index_manager_get_catalog(index_manager_t *mgr)
{
    if (!mgr) {
        return NULL;
    }

    return mgr->catalog;
}
