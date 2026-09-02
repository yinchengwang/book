/**
 * @file index_selector.h
 * @brief Index selector for optimal index selection
 *
 * The index selector provides optimal index selection for query planning
 * by evaluating all available indexes and selecting the best one based
 * on cost estimation.
 */

#ifndef DB_INDEX_SELECTOR_H
#define DB_INDEX_SELECTOR_H

#include "index_manager.h"
#include "index_cost.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Type Definitions
 * ============================================================ */

/** Index selector structure */
typedef struct index_selector {
    index_manager_t *mgr;
} index_selector_t;

/* ============================================================
 * Index Selector API
 * ============================================================ */

/**
 * @brief Create a new index selector
 * @param mgr Index manager (must not be NULL)
 * @return New index selector or NULL on failure
 */
index_selector_t *index_selector_create(index_manager_t *mgr);

/**
 * @brief Destroy an index selector and free all resources
 * @param sel Index selector to destroy
 */
void index_selector_destroy(index_selector_t *sel);

/**
 * @brief Find the best index for a given query condition
 *
 * Evaluates all available indexes for the specified table and condition,
 * then returns the one with the lowest cost.
 *
 * @param sel Index selector
 * @param table_id Table ID to find indexes for
 * @param cond Query condition to evaluate
 * @param stats Table statistics for cost estimation
 * @param best_cost Output parameter for the best cost result
 * @return 0 on success (best_cost is set), -1 on failure or no suitable index
 */
int index_selector_find_best(index_selector_t *sel,
                          int table_id,
                          const query_condition_t *cond,
                          const table_stats_t *stats,
                          index_cost_t *best_cost);

/**
 * @brief Evaluate all indexes for a given query condition
 *
 * Evaluates all available indexes for the specified table and condition,
 * storing the cost results in the provided array.
 *
 * @param sel Index selector
 * @param table_id Table ID to find indexes for
 * @param cond Query condition to evaluate
 * @param stats Table statistics for cost estimation
 * @param costs Array to store cost results (must be pre-allocated)
 * @param max_costs Maximum number of cost results (array size)
 * @return Number of indexes evaluated, or -1 on failure
 */
int index_selector_evaluate_all(index_selector_t *sel,
                              int table_id,
                              const query_condition_t *cond,
                              const table_stats_t *stats,
                              index_cost_t *costs,
                              int max_costs);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_SELECTOR_H */
