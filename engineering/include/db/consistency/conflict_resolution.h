/**
 * @file conflict_resolution.h
 * @brief Conflict Resolution - Vector Clock and CRDT
 *
 * Provides conflict resolution mechanisms using Vector Clocks and
 * Conflict-free Replicated Data Types (CRDTs).
 */

#ifndef DB_CONSISTENCY_CONFLICT_RESOLUTION_H
#define DB_CONSISTENCY_CONFLICT_RESOLUTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Vector Clock
 * ============================================================ */

#define MAX_NODES 64

/**
 * @brief Vector Clock structure
 */
typedef struct {
    uint64_t clocks[MAX_NODES];  /**< Clock values for each node */
    int node_count;              /**< Number of nodes in the cluster */
    int local_node_id;           /**< Local node identifier (0-based) */
} vector_clock_t;

/**
 * @brief Initialize a vector clock
 * @param vc Vector clock to initialize
 * @param local_node_id Local node identifier
 * @param node_count Total number of nodes in cluster
 */
void vc_init(vector_clock_t *vc, int local_node_id, int node_count);

/**
 * @brief Increment local clock counter
 * @param vc Vector clock
 */
void vc_inc(vector_clock_t *vc);

/**
 * @brief Merge another vector clock into this one (take max of each component)
 * @param vc Target vector clock (modified in place)
 * @param other Source vector clock to merge from
 */
void vc_merge(vector_clock_t *vc, const vector_clock_t *other);

/**
 * @brief Compare two vector clocks
 * @param a First vector clock
 * @param b Second vector clock
 * @return  1 if a > b (a happened after b)
 *         -1 if a < b (a happened before b)
 *          0 if concurrent (a || b, neither happens-before)
 */
int vc_compare(const vector_clock_t *a, const vector_clock_t *b);

/**
 * @brief Check if vector clock is concurrent with another
 * @param a First vector clock
 * @param b Second vector clock
 * @return true if concurrent (neither happens-before the other)
 */
bool vc_is_concurrent(const vector_clock_t *a, const vector_clock_t *b);

/**
 * @brief Copy a vector clock
 * @param dest Destination vector clock
 * @param src Source vector clock
 */
void vc_copy(vector_clock_t *dest, const vector_clock_t *src);

/* ============================================================
 * CRDT Types
 * ============================================================ */

/**
 * @brief CRDT type enumeration
 */
typedef enum {
    CRDT_GCOUNTER,   /**< Grow-only Counter */
    CRDT_PNCOUNTER,  /**< Positive-Negative Counter (increment/decrement) */
    CRDT_LWWREG      /**< Last-Writer-Wins Register */
} crdt_type_t;

/**
 * @brief CRDT opaque type
 */
typedef struct crdt crdt_t;

/**
 * @brief Create a CRDT instance
 * @param type CRDT type to create
 * @return CRDT instance, or NULL on failure
 */
crdt_t *crdt_create(crdt_type_t type);

/**
 * @brief Destroy a CRDT instance
 * @param crdt CRDT to destroy
 */
void crdt_destroy(crdt_t *crdt);

/**
 * @brief Merge two CRDTs (union operation)
 * @param a Target CRDT (modified in place)
 * @param b Source CRDT to merge from
 * @return 0 on success; -1 on failure
 */
int crdt_merge(crdt_t *a, const crdt_t *b);

/**
 * @brief Get CRDT value as integer (for counters)
 * @param crdt CRDT instance
 * @return Current value
 */
int64_t crdt_get_value(const crdt_t *crdt);

/**
 * @brief Increment a G-Counter or PN-Counter
 * @param crdt CRDT instance
 * @param delta Amount to increment
 * @return 0 on success; -1 on failure
 */
int crdt_inc(crdt_t *crdt, uint64_t delta);

/**
 * @brief Decrement a PN-Counter
 * @param crdt CRDT instance
 * @param delta Amount to decrement
 * @return 0 on success; -1 on failure
 */
int crdt_dec(crdt_t *crdt, uint64_t delta);

/**
 * @brief Set value of LWW-Register
 * @param crdt CRDT instance
 * @param value Value to set
 * @param timestamp Logical timestamp (higher wins)
 * @return 0 on success; -1 on failure
 */
int crdt_set(crdt_t *crdt, uint64_t value, uint64_t timestamp);

/**
 * @brief Get CRDT type
 * @param crdt CRDT instance
 * @return CRDT type
 */
crdt_type_t crdt_get_type(const crdt_t *crdt);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSISTENCY_CONFLICT_RESOLUTION_H */
