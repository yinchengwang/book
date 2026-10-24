/**
 * @file index_selector.c
 * @brief Index selector implementation
 */

#include "index_selector.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Index Selector API
 * ============================================================ */

index_selector_t *index_selector_create(index_manager_t *mgr)
{
    index_selector_t *sel;

    if (!mgr) {
        return NULL;
    }

    sel = malloc(sizeof(index_selector_t));
    if (!sel) {
        return NULL;
    }

    memset(sel, 0, sizeof(index_selector_t));
    sel->mgr = mgr;

    return sel;
}

void index_selector_destroy(index_selector_t *sel)
{
    if (!sel) {
        return;
    }

    free(sel);
}

int index_selector_find_best(index_selector_t *sel,
                          int table_id,
                          const query_condition_t *cond,
                          const table_stats_t *stats,
                          index_cost_t *best_cost)
{
    index_entry_t *indexes[16];
    int count;
    int best_idx;
    double lowest_cost;

    if (!sel || !cond || !stats || !best_cost) {
        return -1;
    }

    /* Get all indexes for the table */
    count = index_manager_get_table_indexes(sel->mgr, table_id,
                                           indexes, 16);
    if (count <= 0) {
        return -1;
    }

    /* Initialize with first suitable index */
    best_idx = -1;
    lowest_cost = 0.0;

    for (int i = 0; i < count; i++) {
        index_entry_t *idx = indexes[i];

        /* Skip indexes that are not ready */
        if (idx->state != INDEX_STATE_READY) {
            continue;
        }

        /* Estimate cost for this index */
        const index_cost_t *cost = index_cost_estimate(idx, cond, stats);

        /* Skip if this column is not covered by the index */
        if (cost->total_cost == 0 && cost->rows_estimated == 0) {
            continue;
        }

        /* Select the first suitable index as initial best */
        if (best_idx == -1) {
            best_idx = i;
            lowest_cost = cost->total_cost;
            *best_cost = *cost;
        } else if (cost->total_cost < lowest_cost) {
            best_idx = i;
            lowest_cost = cost->total_cost;
            *best_cost = *cost;
        }
    }

    if (best_idx == -1) {
        return -1;
    }

    return 0;
}

int index_selector_evaluate_all(index_selector_t *sel,
                              int table_id,
                              const query_condition_t *cond,
                              const table_stats_t *stats,
                              index_cost_t *costs,
                              int max_costs)
{
    index_entry_t *indexes[16];
    int count;
    int result_count;

    if (!sel || !cond || !stats || !costs || max_costs <= 0) {
        return -1;
    }

    /* Get all indexes for the table */
    count = index_manager_get_table_indexes(sel->mgr, table_id,
                                           indexes, 16);
    if (count <= 0) {
        return -1;
    }

    /* Limit to max_costs */
    if (count > max_costs) {
        count = max_costs;
    }

    result_count = 0;

    for (int i = 0; i < count; i++) {
        index_entry_t *idx = indexes[i];

        /* Skip indexes that are not ready */
        if (idx->state != INDEX_STATE_READY) {
            continue;
        }

        /* Estimate cost for this index */
        const index_cost_t *cost = index_cost_estimate(idx, cond, stats);

        /* Copy cost result */
        costs[result_count] = *cost;
        result_count++;
    }

    return result_count;
}
