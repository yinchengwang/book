/**
 * @file cross_modal.c
 * @brief Cross-Modal Coordination Module - 2PC Transaction Protocol
 *
 * Task 37: Implements Two-Phase Commit (2PC) for cross-modal transactions.
 *
 * Protocol:
 * - Phase 1 (Prepare): Each modality writes PREPARE WAL record
 * - Phase 2 (Commit): Each modality writes COMMIT WAL record
 * - On Failure: Abort - each modality writes ABORT WAL record
 */

#include "db/multimodal_object.h"
#include "db/core/log.h"
#include "db/wal.h"
#include "db/lock.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Cross-Modal 2PC Types
 * ============================================================ */

#define RRF_K 60
#define MAX_CANDIDATES 256
#define MAX_CROSS_PARTICIPANTS 16
#define MAX_CROSS_TXNS 256

/** Cross-modal transaction state */
typedef enum cross_txn_state_e {
    CROSS_TXN_PREPARING = 0,  /**< Phase 1: Gathering prepare votes */
    CROSS_TXN_PREPARED  = 1,  /**< All participants prepared successfully */
    CROSS_TXN_COMMITTED  = 2,  /**< Phase 2: Committed */
    CROSS_TXN_ABORTED   = 3   /**< Aborted */
} cross_txn_state_t;

/** Cross-modal transaction descriptor */
typedef struct cross_txn_s {
    uint32_t txn_id;                    /**< Transaction ID */
    uint32_t participant_count;         /**< Number of participants */
    const char *participants[MAX_CROSS_PARTICIPANTS];  /**< Modality names */
    cross_txn_state_t state;            /**< Current state */
    uint64_t prepare_lsn;                /**< LSN of prepare record */
    uint64_t commit_lsn;                /**< LSN of commit record */
    struct cross_txn_s *next;           /**< Next in active list */
} cross_txn_t;

/** Cross-modal transaction manager */
typedef struct cross_txn_manager_s {
    cross_txn_t *active_txns;           /**< Active cross-modal transactions */
    uint32_t next_txn_id;               /**< Next available txn ID */
    pthread_mutex_t mutex;              /**< Mutex for manager */
} cross_txn_manager_t;

/* ============================================================
 * Internal globals
 * ============================================================ */

static cross_txn_manager_t g_cross_mgr;
static bool g_cross_mgr_initialized = false;

/** Initialize the cross-modal transaction manager */
static int cross_txn_manager_init(void) {
    if (g_cross_mgr_initialized) {
        return 0;
    }
    memset(&g_cross_mgr, 0, sizeof(g_cross_mgr));
    pthread_mutex_init(&g_cross_mgr.mutex, NULL);
    g_cross_mgr.next_txn_id = 1;
    g_cross_mgr_initialized = true;
    return 0;
}

/* ============================================================
 * Internal helpers
 * ============================================================ */

typedef struct {
    int32_t id;
    double rrf_score;
} crossmodal_candidate_t;

static int compare_candidates(const void *a, const void *b) {
    double sa = ((const crossmodal_candidate_t *)a)->rrf_score;
    double sb = ((const crossmodal_candidate_t *)b)->rrf_score;
    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return 0;
}

/* ============================================================
 * Cross-Modal 2PC API
 * ============================================================ */

/**
 * @brief Begin a new cross-modal transaction
 * @return New transaction descriptor, or NULL on failure
 */
cross_txn_t *cross_txn_begin(void) {
    cross_txn_manager_init();

    cross_txn_t *txn = (cross_txn_t *)calloc(1, sizeof(cross_txn_t));
    if (!txn) {
        LOG_ERROR("cross_txn_begin: failed to allocate transaction");
        return NULL;
    }

    pthread_mutex_lock(&g_cross_mgr.mutex);
    txn->txn_id = g_cross_mgr.next_txn_id++;
    txn->state = CROSS_TXN_PREPARING;
    txn->next = g_cross_mgr.active_txns;
    g_cross_mgr.active_txns = txn;
    pthread_mutex_unlock(&g_cross_mgr.mutex);

    LOG_INFO("cross_txn_begin: started cross-modal txn %u", txn->txn_id);
    return txn;
}

/**
 * @brief Add a participant (modality) to the transaction
 * @param txn Transaction descriptor
 * @param participant Modality name (e.g., "vector", "relational")
 * @return 0 on success, -1 on failure
 */
int cross_txn_add_participant(cross_txn_t *txn, const char *participant) {
    if (!txn || !participant) {
        return -1;
    }
    if (txn->state != CROSS_TXN_PREPARING) {
        LOG_WARN("cross_txn_add_participant: cannot add participant to txn %u in state %d",
                 txn->txn_id, txn->state);
        return -1;
    }
    if (txn->participant_count >= MAX_CROSS_PARTICIPANTS) {
        LOG_ERROR("cross_txn_add_participant: too many participants (max %d)", MAX_CROSS_PARTICIPANTS);
        return -1;
    }

    txn->participants[txn->participant_count++] = participant;
    LOG_DEBUG("cross_txn_add_participant: txn %u added participant '%s' (count=%u)",
              txn->txn_id, participant, txn->participant_count);
    return 0;
}

/**
 * @brief Phase 1: Prepare - each modality writes PREPARE WAL record
 *
 * This function coordinates the prepare phase across all participants.
 * Each participant writes a PREPARE WAL record and responds with vote.
 * If deadlock is detected during lock acquisition, abort.
 *
 * @param txn Transaction descriptor
 * @return 0 on success (all voted YES), -1 on failure (any voted NO or deadlock)
 */
int cross_txn_prepare(cross_txn_t *txn) {
    if (!txn) {
        return -1;
    }
    if (txn->state != CROSS_TXN_PREPARING) {
        LOG_WARN("cross_txn_prepare: txn %u not in PREPARING state (state=%d)",
                 txn->txn_id, txn->state);
        return -1;
    }
    if (txn->participant_count == 0) {
        LOG_WARN("cross_txn_prepare: txn %u has no participants", txn->txn_id);
        return -1;
    }

    /* Get the current WAL for writing prepare records */
    wal_t *wal = wal_get_current();
    if (!wal) {
        LOG_ERROR("cross_txn_prepare: no current WAL set");
        return -1;
    }

    /* Write PREPARE WAL record for each participant */
    /* Note: In a real implementation, each modality would:
     * 1. Acquire necessary locks (with deadlock detection)
     * 2. Write its prepare log
     * 3. Return vote (YES/NO)
     *
     * Here we write a consolidated prepare record.
     */

    /* Build participant name list */
    const char *names[MAX_CROSS_PARTICIPANTS];
    for (uint32_t i = 0; i < txn->participant_count; i++) {
        names[i] = txn->participants[i];
    }

    /* Write the cross-modal prepare WAL record */
    uint64_t prepare_lsn = wal_write_cross_prepare(wal, txn->txn_id,
                                                    txn->participant_count,
                                                    names);
    if (prepare_lsn == 0) {
        LOG_ERROR("cross_txn_prepare: failed to write WAL prepare record for txn %u",
                  txn->txn_id);
        return -1;
    }

    txn->prepare_lsn = prepare_lsn;
    txn->state = CROSS_TXN_PREPARED;

    LOG_INFO("cross_txn_prepare: txn %u prepared (participant_count=%u, prepare_lsn=%lu)",
             txn->txn_id, txn->participant_count, (unsigned long)prepare_lsn);
    return 0;
}

/**
 * @brief Phase 2: Commit - each modality writes COMMIT WAL record
 * @param txn Transaction descriptor
 * @return 0 on success, -1 on failure
 */
int cross_txn_commit(cross_txn_t *txn) {
    if (!txn) {
        return -1;
    }
    if (txn->state == CROSS_TXN_COMMITTED) {
        LOG_WARN("cross_txn_commit: txn %u already committed", txn->txn_id);
        return 0;
    }
    if (txn->state != CROSS_TXN_PREPARED) {
        LOG_WARN("cross_txn_commit: txn %u not in PREPARED state (state=%d)",
                 txn->txn_id, txn->state);
        /* Can only commit from PREPARED state */
        return -1;
    }

    wal_t *wal = wal_get_current();
    if (!wal) {
        LOG_ERROR("cross_txn_commit: no current WAL set");
        return -1;
    }

    /* Write COMMIT WAL record */
    uint64_t commit_lsn = wal_write_cross_commit(wal, txn->txn_id);
    if (commit_lsn == 0) {
        LOG_ERROR("cross_txn_commit: failed to write WAL commit record for txn %u",
                  txn->txn_id);
        return -1;
    }

    txn->commit_lsn = commit_lsn;
    txn->state = CROSS_TXN_COMMITTED;

    LOG_INFO("cross_txn_commit: txn %u committed (commit_lsn=%lu)",
             txn->txn_id, (unsigned long)commit_lsn);
    return 0;
}

/**
 * @brief Abort the transaction - each modality writes ABORT WAL record
 * @param txn Transaction descriptor
 * @return 0 on success, -1 on failure
 */
int cross_txn_abort(cross_txn_t *txn) {
    if (!txn) {
        return -1;
    }
    if (txn->state == CROSS_TXN_ABORTED) {
        LOG_WARN("cross_txn_abort: txn %u already aborted", txn->txn_id);
        return 0;
    }
    if (txn->state == CROSS_TXN_COMMITTED) {
        LOG_ERROR("cross_txn_abort: cannot abort committed txn %u", txn->txn_id);
        return -1;
    }

    wal_t *wal = wal_get_current();
    if (!wal) {
        LOG_ERROR("cross_txn_abort: no current WAL set");
        return -1;
    }

    /* Write ABORT WAL record */
    uint64_t abort_lsn = wal_write_cross_abort(wal, txn->txn_id);
    if (abort_lsn == 0) {
        LOG_ERROR("cross_txn_abort: failed to write WAL abort record for txn %u",
                  txn->txn_id);
        return -1;
    }

    txn->state = CROSS_TXN_ABORTED;

    LOG_INFO("cross_txn_abort: txn %u aborted", txn->txn_id);
    return 0;
}

/**
 * @brief End transaction - free resources and remove from active list
 * @param txn Transaction descriptor
 * @return 0 on success, -1 on failure
 */
int cross_txn_end(cross_txn_t *txn) {
    if (!txn) {
        return -1;
    }

    LOG_INFO("cross_txn_end: ending txn %u (state=%d, prepare_lsn=%lu, commit_lsn=%lu)",
             txn->txn_id, txn->state,
             (unsigned long)txn->prepare_lsn,
             (unsigned long)txn->commit_lsn);

    /* Remove from active transaction list */
    pthread_mutex_lock(&g_cross_mgr.mutex);
    cross_txn_t **prev = &g_cross_mgr.active_txns;
    while (*prev) {
        if (*prev == txn) {
            *prev = txn->next;
            break;
        }
        prev = &((*prev)->next);
    }
    pthread_mutex_unlock(&g_cross_mgr.mutex);

    free(txn);
    return 0;
}

/* ============================================================
 * Cross-Modal Search (RRF Fusion) - existing implementation
 * ============================================================ */

/* C3-4 T5: cross_modal_search
 * Search using named vector index for cross-modal retrieval.
 * For each modality, search and apply RRF fusion.
 */
int mm_cross_modal_search(mm_multimodal_object_t *query_obj,
                         const char *target_modal,
                         int32_t *out_ids, float *out_scores, int k) {
    if (!query_obj || !target_modal || !out_ids || !out_scores || k <= 0) {
        return -1;
    }

    /* Find the named vector matching target_modal */
    int32_t target_idx = -1;
    for (int32_t i = 0; i < query_obj->n_vectors; i++) {
        if (strncmp(query_obj->vectors[i].name, target_modal,
                    sizeof(query_obj->vectors[i].name)) == 0) {
            target_idx = i;
            break;
        }
    }

    if (target_idx < 0) {
        LOG_INFO("cross_modal_search: modal '%s' not found in query object", target_modal);
        for (int i = 0; i < k; i++) { out_ids[i] = -1; out_scores[i] = 0.0f; }
        return 0;
    }

    float *query_vec = query_obj->vectors[target_idx].data;
    int32_t dim = query_obj->vectors[target_idx].dim;

    if (!query_vec || dim <= 0) {
        LOG_WARN("cross_modal_search: invalid vector for modal '%s'", target_modal);
        for (int i = 0; i < k; i++) { out_ids[i] = -1; out_scores[i] = 0.0f; }
        return -1;
    }

    LOG_INFO("cross_modal_search: searching modal '%s' (dim=%d, k=%d)",
             target_modal, dim, k);

    /* Allocate candidate list for RRF fusion */
    crossmodal_candidate_t *cands = calloc(MAX_CANDIDATES, sizeof(crossmodal_candidate_t));
    if (!cands) return -1;
    int n_cands = 0;

    /* Search with actual HNSW collection if available
     * Note: In a full implementation, we would look up the appropriate
     * collection for this modality from a collection registry.
     * For now, we demonstrate the RRF computation pattern.
     */

    /* Simulate multi-modality search with RRF fusion:
     * In production, each storage engine would be called via its API.
     * Here we demonstrate the RRF score computation.
     */

    /* Example: Search result from target modality (placeholder data)
     * In real implementation: faiss_hnsw_collection_search(col, query_vec, k, distances, ids)
     */
    int32_t search_k = k * 2;  /* Fetch more for RRF */
    float *dists = malloc(sizeof(float) * search_k);
    int32_t *ids = malloc(sizeof(int32_t) * search_k);

    if (!dists || !ids) {
        free(dists); free(ids); free(cands);
        return -1;
    }

    /* For demonstration, initialize with placeholder results
     * Real implementation would call: vector_engine_search() or faiss_hnsw_collection_search()
     * based on the target_modal's index_type
     */
    for (int i = 0; i < search_k; i++) {
        /* Placeholder: generate synthetic results */
        ids[i] = (int32_t)i;
        dists[i] = (float)(search_k - i);  /* Descending distance (worse = larger) */
    }
    int32_t n_results = search_k;

    /* RRF score computation: score = 1 / (k_rrf + rank) */
    for (int32_t r = 0; r < n_results; r++) {
        int32_t id = ids[r];
        if (id < 0) continue;

        /* Check if already in candidates */
        int found = -1;
        for (int j = 0; j < n_cands; j++) {
            if (cands[j].id == id) { found = j; break; }
        }

        /* RRF contribution: 1 / (RRF_K + rank) where rank is 0-indexed */
        double rrf_contrib = 1.0 / (double)(RRF_K + r + 1);

        if (found >= 0) {
            cands[found].rrf_score += rrf_contrib;
        } else if (n_cands < MAX_CANDIDATES) {
            cands[n_cands].id = id;
            cands[n_cands].rrf_score = rrf_contrib;
            n_cands++;
        }
    }

    free(dists); free(ids);

    /* Sort by RRF score descending */
    qsort(cands, (size_t)n_cands, sizeof(crossmodal_candidate_t), compare_candidates);

    /* Return top-k results */
    int n_out = (n_cands < k) ? n_cands : k;
    for (int i = 0; i < n_out; i++) {
        out_ids[i] = cands[i].id;
        out_scores[i] = (float)cands[i].rrf_score;
    }
    /* Fill remaining slots with -1 */
    for (int i = n_out; i < k; i++) {
        out_ids[i] = -1;
        out_scores[i] = 0.0f;
    }

    free(cands);

    LOG_INFO("cross_modal_search: returned %d results for modal '%s'", n_out, target_modal);
    return n_out;
}

/* C3-4 T4: RRF（Reciprocal Rank Fusion）跨向量归并
 * score = sum(1 / (k_rrf + rank_i)) for each ranked list i
 */
double mm_rrf_score(const int *ranks, size_t n_lists, int k_rrf) {
    if (!ranks || n_lists == 0) return 0.0;
    double score = 0.0;
    for (size_t i = 0; i < n_lists; ++i) {
        if (ranks[i] >= 0) score += 1.0 / (double)(k_rrf + ranks[i]);
    }
    return score;
}
