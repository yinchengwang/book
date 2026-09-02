/**
 * @file index_cost.c
 * @brief Index cost estimation implementation
 */

#include "index_cost.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Internal Constants
 * ============================================================ */

#define INDEX_COST_CPU_TUPLE_COST    1.0   /* Base CPU cost per tuple */
#define INDEX_COST_IO_COST           1.0   /* Base I/O cost per page */
#define INDEX_COST_INDEX_STARTUP     0.5   /* Index scan startup cost */
#define INDEX_COST_BTREE_FANOUT      100   /* Assumed B-tree fanout */
#define INDEX_COST_HASH_BUCKETS      10    /* Assumed hash bucket chain length */

/* ============================================================
 * Internal State
 * ============================================================ */

static index_cost_t _cost_result = {0};

/* ============================================================
 * Condition Type String Conversion
 * ============================================================ */

static const char *_condition_strings[] = {
    "EQ",     /* COND_EQ */
    "LT",     /* COND_LT */
    "LE",     /* COND_LE */
    "GT",     /* COND_GT */
    "GE",     /* COND_GE */
    "RANGE",  /* COND_RANGE */
    "TEXT"    /* COND_TEXT */
};

const char *index_cost_condition_string(condition_type_t type)
{
    if (type < 0 || type > COND_TEXT) {
        return "UNKNOWN";
    }
    return _condition_strings[type];
}

/* ============================================================
 * Internal Helper Functions
 * ============================================================ */

/**
 * @brief Calculate the number of leaf pages in a B-tree index
 */
static size_t _btree_leaf_pages(size_t row_count, size_t distinct_values)
{
    if (distinct_values == 0) {
        distinct_values = 1;
    }
    size_t fanout = INDEX_COST_BTREE_FANOUT;
    size_t entries_per_page = fanout;
    size_t leaf_pages = (row_count + entries_per_page - 1) / entries_per_page;
    return leaf_pages > 1 ? leaf_pages : 1;
}

/**
 * @brief Calculate height of a B-tree index
 */
static int _btree_height(size_t row_count)
{
    if (row_count <= INDEX_COST_BTREE_FANOUT) {
        return 1;
    }
    int height = 1;
    size_t nodes = 1;
    size_t capacity = INDEX_COST_BTREE_FANOUT;
    while (nodes * capacity < row_count) {
        nodes *= INDEX_COST_BTREE_FANOUT;
        height++;
    }
    return height + 1; /* +1 for leaf level */
}

/**
 * @brief Check if a column is covered by the index
 */
static int _column_in_index(const index_entry_t *idx, int column_id)
{
    if (!idx || !idx->columns) {
        return 0;
    }
    for (int i = 0; i < idx->column_count; i++) {
        if (idx->columns[i] == column_id) {
            return 1;
        }
    }
    return 0;
}

/* ============================================================
 * Selectivity Calculation
 * ============================================================ */

double index_selectivity(const index_entry_t *idx,
                        const query_condition_t *cond,
                        const table_stats_t *stats)
{
    if (!idx || !cond || !stats) {
        return 1.0; /* Default to full scan if invalid inputs */
    }

    if (stats->row_count == 0) {
        return 0.0;
    }

    /* If column is not in index, selectivity is 1.0 (full scan) */
    if (!_column_in_index(idx, cond->column_id)) {
        return 1.0;
    }

    double ndv_ratio = 1.0;
    if (stats->distinct_values > 0) {
        ndv_ratio = 1.0 / (double)stats->distinct_values;
    }

    double selectivity = 1.0;

    switch (cond->type) {
        case COND_EQ:
            /* Equality: 1/NDV */
            selectivity = ndv_ratio;
            break;

        case COND_LT:
        case COND_LE:
        case COND_GT:
        case COND_GE:
            /* Range bound: approximately 25% of the range */
            selectivity = 0.25;
            break;

        case COND_RANGE:
            /* BETWEEN: approximately 10% of values */
            selectivity = 0.10;
            break;

        case COND_TEXT:
            /* Full-text: highly selective, estimate 1% */
            selectivity = 0.01;
            break;

        default:
            selectivity = 1.0;
            break;
    }

    /* Clamp selectivity to valid range */
    if (selectivity < 0.0) {
        selectivity = 0.0;
    }
    if (selectivity > 1.0) {
        selectivity = 1.0;
    }

    return selectivity;
}

/* ============================================================
 * Cost Estimation
 * ============================================================ */

const index_cost_t *index_cost_estimate(const index_entry_t *idx,
                                       const query_condition_t *cond,
                                       const table_stats_t *stats)
{
    /* Initialize result */
    memset(&_cost_result, 0, sizeof(_cost_result));

    if (!idx || !cond || !stats) {
        return &_cost_result;
    }

    _cost_result.index_id = idx->index_id;

    if (stats->row_count == 0) {
        return &_cost_result;
    }

    /* Calculate selectivity */
    double sel = index_selectivity(idx, cond, stats);
    size_t est_rows = (size_t)(sel * stats->row_count);
    if (est_rows < 1) {
        est_rows = 1;
    }
    _cost_result.rows_estimated = est_rows;

    /* Calculate costs based on index type */
    double startup_cost = INDEX_COST_INDEX_STARTUP;
    double total_cost = 0.0;

    switch (idx->type) {
        case INDEX_TYPE_BTREE:
        case INDEX_TYPE_BTREE: {
            /* B-tree index cost model */
            int height = _btree_height(stats->row_count);
            size_t leaf_pages = _btree_leaf_pages(stats->row_count,
                                                  stats->distinct_values);

            /* Startup: traverse from root to leaf */
            startup_cost = height * INDEX_COST_IO_COST;

            /* For equality: scan leaf pages and fetch rows */
            if (cond->type == COND_EQ) {
                size_t matching_pages = (leaf_pages + stats->distinct_values - 1)
                                       / stats->distinct_values;
                if (matching_pages < 1) {
                    matching_pages = 1;
                }
                total_cost = startup_cost +
                             matching_pages * INDEX_COST_IO_COST +
                             est_rows * INDEX_COST_CPU_TUPLE_COST;
            } else {
                /* Range scan: scan portion of leaf pages */
                double range_fraction = (cond->type == COND_RANGE) ? 0.10 : 0.25;
                size_t scanned_pages = (size_t)(leaf_pages * range_fraction);
                if (scanned_pages < 1) {
                    scanned_pages = 1;
                }
                total_cost = startup_cost +
                             scanned_pages * INDEX_COST_IO_COST +
                             est_rows * INDEX_COST_CPU_TUPLE_COST;
            }
            break;
        }

        case INDEX_TYPE_HASH: {
            /* Hash index cost model - O(1) lookup */
            startup_cost = INDEX_COST_INDEX_STARTUP;
            double bucket_cost = INDEX_COST_HASH_BUCKETS * INDEX_COST_CPU_TUPLE_COST;
            total_cost = startup_cost + bucket_cost + est_rows * INDEX_COST_CPU_TUPLE_COST;
            break;
        }

        case INDEX_TYPE_FULLTEXT:
        case INDEX_TYPE_GIN: {
            /* Full-text / GIN index: higher startup cost due to posting scan */
            startup_cost = INDEX_COST_INDEX_STARTUP * 2.0;
            double scan_cost = stats->page_count * 0.1 * INDEX_COST_IO_COST;
            total_cost = startup_cost + scan_cost + est_rows * INDEX_COST_CPU_TUPLE_COST * 2.0;
            break;
        }

        case INDEX_TYPE_HNSW:
        case INDEX_TYPE_IVF: {
            /* Vector indexes: cost based on ef_search parameter */
            int ef_search = 100;
            if (idx->config.ef_search > 0) {
                ef_search = idx->config.ef_search;
            }
            startup_cost = INDEX_COST_INDEX_STARTUP;
            double search_cost = ef_search * INDEX_COST_CPU_TUPLE_COST;
            total_cost = startup_cost + search_cost + est_rows * INDEX_COST_CPU_TUPLE_COST;
            break;
        }

        default: {
            /* Generic index cost model */
            double index_pages = (stats->row_count + INDEX_COST_BTREE_FANOUT - 1)
                                / INDEX_COST_BTREE_FANOUT;
            total_cost = startup_cost +
                         index_pages * INDEX_COST_IO_COST +
                         est_rows * INDEX_COST_CPU_TUPLE_COST;
            break;
        }
    }

    _cost_result.startup_cost = startup_cost;
    _cost_result.total_cost = total_cost;

    return &_cost_result;
}
