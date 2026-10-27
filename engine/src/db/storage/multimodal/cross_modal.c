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
static pthread_once_t g_cross_mgr_once = PTHREAD_ONCE_INIT;
static int g_cross_mgr_init_rc = 0;

/** 实际初始化体，由 pthread_once 保证只执行一次。
 *
 * 注意：这里**不能** memset 整个 manager —— 那样会连已初始化的 mutex 一起清零，
 * 并发首次调用时会把别的线程刚持有的锁抹掉。只初始化数据成员，mutex 单独初始化。
 */
static void cross_txn_manager_do_init(void) {
    g_cross_mgr.active_txns = NULL;
    g_cross_mgr.next_txn_id = 1;
    if (pthread_mutex_init(&g_cross_mgr.mutex, NULL) != 0) {
        g_cross_mgr_init_rc = -1;
    }
}

/** Initialize the cross-modal transaction manager */
static int cross_txn_manager_init(void) {
    if (pthread_once(&g_cross_mgr_once, cross_txn_manager_do_init) != 0) {
        return -1;
    }
    return g_cross_mgr_init_rc;
}

/* ============================================================
 * Cross-Modal 2PC API
 * ============================================================ */

/**
 * @brief Begin a new cross-modal transaction
 * @return New transaction descriptor, or NULL on failure
 */
cross_txn_t *cross_txn_begin(void) {
    if (cross_txn_manager_init() != 0) {
        LOG_ERROR("cross_txn_begin: manager 初始化失败");
        return NULL;
    }

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
