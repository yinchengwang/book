/**
 * @file index_cost.h
 * @brief Index cost estimation for query planning
 *
 * This module provides cost estimation facilities for the index subsystem,
 * enabling the query planner to estimate the cost of using different indexes
 * for various query conditions.
 */

#ifndef DB_INDEX_COST_H
#define DB_INDEX_COST_H

#include "index_catalog.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Query Condition Types
 * ============================================================ */

/** Query condition type enumeration */
typedef enum {
    COND_EQ = 0,       /* Equality (=) */
    COND_LT,           /* Less than (<) */
    COND_LE,           /* Less than or equal (<=) */
    COND_GT,           /* Greater than (>) */
    COND_GE,           /* Greater than or equal (>=) */
    COND_RANGE,        /* Range (BETWEEN) */
    COND_TEXT          /* Full-text search */
} condition_type_t;

/* ============================================================
 * Query Condition Structure
 * ============================================================ */

/**
 * @brief Query condition structure
 *
 * Represents a single condition in a query predicate that can
 * potentially use an index for evaluation.
 */
typedef struct {
    condition_type_t type;  /**< Condition type */
    int column_id;          /**< Column this condition applies to */
    void *value;            /**< Comparison value (for single-value conditions) */
    void *value2;           /**< Second value for range queries */
} query_condition_t;

/* ============================================================
 * Table Statistics
 * ============================================================ */

/**
 * @brief Table statistics structure
 *
 * Contains statistical information about a table used for
 * cost estimation calculations.
 */
typedef struct {
    size_t row_count;        /**< Total number of rows in the table */
    size_t page_count;       /**< Number of pages occupied by the table */
    double avg_row_width;    /**< Average row width in bytes */
    size_t distinct_values;  /**< Number of distinct values in the column */
} table_stats_t;

/* ============================================================
 * Cost Estimation Result
 * ============================================================ */

/**
 * @brief Cost estimation result structure
 *
 * Contains the result of index cost estimation including
 * startup cost, total cost, and row estimate.
 */
typedef struct {
    int index_id;           /**< Index ID this cost estimate is for */
    double startup_cost;    /**< Startup cost (e.g., index scan initiation) */
    double total_cost;      /**< Total cost of using this index */
    size_t rows_estimated;  /**< Estimated number of rows returned */
} index_cost_t;

/* ============================================================
 * Cost Estimation API
 * ============================================================ */

/**
 * @brief Estimate the cost of using an index for a query condition
 *
 * Calculates the startup cost, total cost, and estimated row count
 * for using the given index to evaluate the specified query condition.
 *
 * @param idx Index entry to estimate cost for
 * @param cond Query condition to evaluate
 * @param stats Table statistics for the base table
 * @return Cost estimation result (caller should not free)
 */
const index_cost_t *index_cost_estimate(const index_entry_t *idx,
                                        const query_condition_t *cond,
                                        const table_stats_t *stats);

/**
 * @brief Calculate the selectivity of a query condition for an index
 *
 * Selectivity is the fraction of rows that satisfy the query condition,
 * used for estimating the number of rows an index scan will return.
 *
 * @param idx Index entry to calculate selectivity for
 * @param cond Query condition to evaluate
 * @param stats Table statistics for the base table
 * @return Selectivity factor (0.0 to 1.0)
 */
double index_selectivity(const index_entry_t *idx,
                        const query_condition_t *cond,
                        const table_stats_t *stats);

/**
 * @brief Get a string representation of a condition type
 *
 * @param type Condition type
 * @return String representation or "UNKNOWN" if invalid
 */
const char *index_cost_condition_string(condition_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_COST_H */
