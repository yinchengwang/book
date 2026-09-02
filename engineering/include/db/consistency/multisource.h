/**
 * @file multisource.h
 * @brief Multi-source replication interface
 *
 * Manages multiple replica sources for a node, allowing a single node
 * to replicate from multiple upstream sources.
 */

#ifndef DB_CONSISTENCY_MULTISOURCE_H
#define DB_CONSISTENCY_MULTISISTEM_MULTISOURCE_H

#include "db/replication/replication.h"
#include "db/consistency/replication_consensus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Type Definitions
 * ============================================================ */

/**
 * @brief Replica node information
 */
typedef struct {
    int         node_id;       /**< Unique node identifier */
    char        host[64];      /**< Node host address */
    int         port;          /**< Node port */
    repl_state_t state;        /**< Current replication state */
    uint64_t    last_lsn;      /**< Last received LSN */
    int64_t     lag_ms;        /**< Replication lag in milliseconds */
} replica_node_t;

/* ============================================================
 * Multi-source Replication API
 * ============================================================ */

/**
 * @brief Add a replica source
 *
 * Registers a new source node for multi-source replication.
 *
 * @param rc   Replication consensus instance
 * @param node Node information
 * @return 0 success; -1 failure
 */
int rc_add_source(replication_consensus_t *rc, const replica_node_t *node);

/**
 * @brief Remove a replica source
 *
 * Unregisters a source node from multi-source replication.
 *
 * @param rc     Replication consensus instance
 * @param node_id Node ID to remove
 * @return 0 success; -1 failure
 */
int rc_remove_source(replication_consensus_t *rc, int node_id);

/**
 * @brief Get all replica sources
 *
 * Retrieves the list of registered replica sources.
 *
 * @param rc     Replication consensus instance
 * @param nodes  Output array for node information
 * @param count  In: array capacity; Out: actual count
 * @return 0 success; -1 failure
 */
int rc_get_sources(replication_consensus_t *rc, replica_node_t *nodes, int *count);

/**
 * @brief Synchronize all sources
 *
 * Forces synchronization with all registered sources.
 *
 * @param rc Replication consensus instance
 * @return 0 success; -1 failure
 */
int rc_sync_all(replication_consensus_t *rc);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSISTENCY_MULTISOURCE_H */
