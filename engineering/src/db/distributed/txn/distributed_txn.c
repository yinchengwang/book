/**
 * @file distributed_txn.c
 * @brief 分布式事务实现（两阶段提交协议）
 *
 * Phase12 - 实现分布式两阶段提交（2PC）协议。
 */

#include "db/distributed/txn/distributed_txn.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define DTXN_MAGIC 0x4454584E  /* "DTXN" */

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 事务状态转换图 */
static const uint8_t g_state_transition[9][3] = {
    /* INIT,    OPEN,     PREPARING */
    [DTXN_STATE_INIT]        = { DTXN_STATE_OPEN, DTXN_STATE_INIT, DTXN_STATE_INIT },
    [DTXN_STATE_OPEN]       = { DTXN_STATE_OPEN, DTXN_STATE_PREPARING, DTXN_STATE_ABORTING },
    [DTXN_STATE_PREPARING]  = { DTXN_STATE_PREPARING, DTXN_STATE_PREPARING, DTXN_STATE_COMMITTING },
    [DTXN_STATE_PREPARED]   = { DTXN_STATE_PREPARED, DTXN_STATE_PREPARED, DTXN_STATE_ABORTING },
    [DTXN_STATE_COMMITTING] = { DTXN_STATE_COMMITTED, DTXN_STATE_COMMITTING, DTXN_STATE_UNKNOWN },
    [DTXN_STATE_COMMITTED]  = { DTXN_STATE_COMMITTED, DTXN_STATE_COMMITTED, DTXN_STATE_COMMITTED },
    [DTXN_STATE_ABORTING]   = { DTXN_STATE_ABORTED, DTXN_STATE_ABORTED, DTXN_STATE_ABORTED },
    [DTXN_STATE_ABORTED]    = { DTXN_STATE_ABORTED, DTXN_STATE_ABORTED, DTXN_STATE_ABORTED },
    [DTXN_STATE_UNKNOWN]     = { DTXN_STATE_UNKNOWN, DTXN_STATE_UNKNOWN, DTXN_STATE_UNKNOWN }
};

/** 内部事务结构 */
typedef struct distributed_txn_impl {
    uint32_t magic;                             /**< 魔数 */
    char transaction_id[128];                    /**< 事务 ID */
    distributed_txn_state_t state;                /**< 当前状态 */
    pthread_mutex_t lock;                         /**< 保护状态 */

    /* 参与者信息 */
    distributed_txn_participant_t *participants;  /**< 参与者数组 */
    uint32_t participant_count;                  /**< 参与者数量 */

    /* 操作接口 */
    distributed_txn_ops_t ops;                  /**< 操作接口 */

    /* 协调者专用 */
    bool all_prepared;                          /**< 所有参与者是否已准备 */
    uint32_t yes_votes;                         /**< 赞成票数 */
    uint32_t no_votes;                          /**< 反对票数 */

    /* 回调 */
    distributed_txn_callback_t callback;           /**< 完成回调 */
    void *callback_data;                        /**< 回调用户数据 */

    /* 超时 */
    uint64_t created_time;                     /**< 创建时间 */
    uint32_t timeout_ms;                       /**< 超时时间 */

    /* 节点 ID（参与者专用）*/
    uint64_t node_id;                          /**< 本节点 ID */
    uint64_t coordinator_id;                   /**< 协调者 ID */
} distributed_txn_impl_t;

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static bool is_valid_state_transition(distributed_txn_state_t from,
                                    distributed_txn_state_t to) {
    /* 允许的状态转换 */
    if (from == DTXN_STATE_OPEN && to == DTXN_STATE_PREPARING) return true;
    if (from == DTXN_STATE_PREPARING && to == DTXN_STATE_PREPARED) return true;
    if (from == DTXN_STATE_PREPARING && to == DTXN_STATE_ABORTING) return true;
    if (from == DTXN_STATE_PREPARED && to == DTXN_STATE_COMMITTING) return true;
    if (from == DTXN_STATE_PREPARED && to == DTXN_STATE_ABORTING) return true;
    if (from == DTXN_STATE_COMMITTING && to == DTXN_STATE_COMMITTED) return true;
    if (from == DTXN_STATE_ABORTING && to == DTXN_STATE_ABORTED) return true;
    if (from == DTXN_STATE_OPEN && to == DTXN_STATE_ABORTING) return true;
    return false;
}

/* ========================================================================
 * 协调者实现
 * ======================================================================== */

/**
 * @brief 创建分布式事务协调者
 */
distributed_txn_t *dtxn_coordinator_create(const char *transaction_id,
                                       const distributed_txn_ops_t *ops) {
    if (!transaction_id) return NULL;

    distributed_txn_impl_t *txn = (distributed_txn_impl_t *)calloc(1, sizeof(distributed_txn_impl_t));
    if (!txn) return NULL;

    txn->magic = DTXN_MAGIC;
    strncpy(txn->transaction_id, transaction_id, sizeof(txn->transaction_id) - 1);
    txn->state = DTXN_STATE_INIT;
    pthread_mutex_init(&txn->lock, NULL);

    if (ops) {
        txn->ops = *ops;
    }

    /* 初始化参与者数组 */
    txn->participants = (distributed_txn_participant_t *)calloc(
        DISTRIBUTED_TXN_MAX_PARTICIPANTS, sizeof(distributed_txn_participant_t));
    if (!txn->participants) {
        pthread_mutex_destroy(&txn->lock);
        free(txn);
        return NULL;
    }

    txn->participant_count = 0;
    txn->created_time = now_ms();

    return (distributed_txn_t *)txn;
}

/**
 * @brief 添加参与者
 */
int dtxn_add_participant(distributed_txn_t *txn,
                        uint64_t node_id,
                        const char *address,
                        uint16_t port) {
    if (!txn || !address) return -1;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return -1;

    pthread_mutex_lock(&impl->lock);

    if (impl->participant_count >= DISTRIBUTED_TXN_MAX_PARTICIPANTS) {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }

    distributed_txn_participant_t *p = &impl->participants[impl->participant_count];
    p->node_id = node_id;
    p->port = port;
    strncpy(p->address, address, sizeof(p->address) - 1);
    p->state = DTXN_PARTICIPANT_INIT;
    p->voted_yes = false;
    p->retry_count = 0;
    p->last_response_time = 0;

    impl->participant_count++;

    pthread_mutex_unlock(&impl->lock);

    return 0;
}

/**
 * @brief 两阶段提交主流程
 */
static int do_2pc(distributed_txn_impl_t *impl, uint32_t timeout_ms) {
    if (!impl) return DISTRIBUTED_TXN_INVALID_STATE;

    /* Phase 1: Prepare */
    impl->all_prepared = true;
    impl->yes_votes = 0;
    impl->no_votes = 0;

    /* 向所有参与者发送 Prepare */
    for (uint32_t i = 0; i < impl->participant_count; i++) {
        distributed_txn_participant_t *p = &impl->participants[i];

        /* TODO: 通过 RPC 发送 Prepare 到 p->address:p->port */
        /* 这里模拟投票结果 */
        bool vote = true; /* 假设所有参与者都投赞成票 */

        if (vote) {
            p->state = DTXN_PARTICIPANT_PREPARED;
            p->voted_yes = true;
            impl->yes_votes++;
        } else {
            p->state = DTXN_PARTICIPANT_ABORTED;
            p->voted_yes = false;
            impl->no_votes++;
        }
    }

    /* 检查投票结果 */
    if (impl->no_votes > 0) {
        /* 有反对票，需要回滚 */
        impl->state = DTXN_STATE_ABORTING;

        /* 向所有参与者发送 Abort */
        for (uint32_t i = 0; i < impl->participant_count; i++) {
            distributed_txn_participant_t *p = &impl->participants[i];
            /* TODO: 通过 RPC 发送 Abort */

            p->state = DTXN_PARTICIPANT_ABORTED;
        }

        impl->state = DTXN_STATE_ABORTED;
        return DISTRIBUTED_TXN_ABORT;
    }

    /* Phase 2: Commit */
    impl->state = DTXN_STATE_COMMITTING;

    /* 向所有参与者发送 Commit */
    for (uint32_t i = 0; i < impl->participant_count; i++) {
        distributed_txn_participant_t *p = &impl->participants[i];

        if (p->state == DTXN_PARTICIPANT_PREPARED) {
            /* TODO: 通过 RPC 发送 Commit */
            p->state = DTXN_PARTICIPANT_COMMITTED;
        }
    }

    impl->state = DTXN_STATE_COMMITTED;
    return DISTRIBUTED_TXN_OK;
}

/**
 * @brief 开始两阶段提交
 */
int dtxn_begin_2pc(distributed_txn_t *txn, uint32_t timeout_ms) {
    if (!txn) return DISTRIBUTED_TXN_INVALID_STATE;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return DISTRIBUTED_TXN_INVALID_STATE;

    pthread_mutex_lock(&impl->lock);

    /* 状态检查 */
    if (impl->state != DTXN_STATE_OPEN) {
        pthread_mutex_unlock(&impl->lock);
        return DISTRIBUTED_TXN_INVALID_STATE;
    }

    impl->state = DTXN_STATE_PREPARING;
    impl->timeout_ms = timeout_ms;

    pthread_mutex_unlock(&impl->lock);

    /* 执行 2PC */
    return do_2pc(impl, timeout_ms);
}

/**
 * @brief 中止事务
 */
int dtxn_abort(distributed_txn_t *txn) {
    if (!txn) return DISTRIBUTED_TXN_INVALID_STATE;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return DISTRIBUTED_TXN_INVALID_STATE;

    pthread_mutex_lock(&impl->lock);

    if (impl->state == DTXN_STATE_COMMITTED ||
        impl->state == DTXN_STATE_ABORTED) {
        pthread_mutex_unlock(&impl->lock);
        return 0; /* 已经结束，无需操作 */
    }

    impl->state = DTXN_STATE_ABORTING;

    /* 向所有参与者发送 Abort */
    for (uint32_t i = 0; i < impl->participant_count; i++) {
        distributed_txn_participant_t *p = &impl->participants[i];
        /* TODO: 通过 RPC 发送 Abort */
        p->state = DTXN_PARTICIPANT_ABORTED;
    }

    impl->state = DTXN_STATE_ABORTED;

    pthread_mutex_unlock(&impl->lock);

    return 0;
}

/* ========================================================================
 * 参与者实现
 * ======================================================================== */

/**
 * @brief 创建事务参与者
 */
distributed_txn_t *dtxn_participant_create(uint64_t node_id,
                                       const distributed_txn_ops_t *ops) {
    distributed_txn_impl_t *txn = (distributed_txn_impl_t *)calloc(1, sizeof(distributed_txn_impl_t));
    if (!txn) return NULL;

    txn->magic = DTXN_MAGIC;
    txn->node_id = node_id;
    txn->state = DTXN_STATE_INIT;
    pthread_mutex_init(&txn->lock, NULL);

    if (ops) {
        txn->ops = *ops;
    }

    txn->created_time = now_ms();

    return (distributed_txn_t *)txn;
}

/**
 * @brief 处理 Prepare 请求
 */
bool dtxn_handle_prepare(distributed_txn_t *txn,
                         uint64_t coordinator_id,
                         const char *transaction_id) {
    if (!txn) return false;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return false;

    pthread_mutex_lock(&impl->lock);

    impl->coordinator_id = coordinator_id;
    strncpy(impl->transaction_id, transaction_id, sizeof(impl->transaction_id) - 1);

    /* 调用应用层的 prepare 回调 */
    bool vote_yes = true;
    if (impl->ops.prepare) {
        int result = impl->ops.prepare(impl->ops.user_data, txn);
        vote_yes = (result == 0);
    }

    impl->state = vote_yes ? DTXN_STATE_PREPARED : DTXN_STATE_ABORTED;

    pthread_mutex_unlock(&impl->lock);

    return vote_yes;
}

/**
 * @brief 处理 Commit 请求
 */
int dtxn_handle_commit(distributed_txn_t *txn) {
    if (!txn) return -1;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return -1;

    pthread_mutex_lock(&impl->lock);

    if (impl->state != DTXN_STATE_PREPARED) {
        pthread_mutex_unlock(&impl->lock);
        return DISTRIBUTED_TXN_INVALID_STATE;
    }

    /* 调用应用层的 commit 回调 */
    if (impl->ops.commit) {
        impl->ops.commit(impl->ops.user_data, txn);
    }

    impl->state = DTXN_STATE_COMMITTED;

    pthread_mutex_unlock(&impl->lock);

    return 0;
}

/**
 * @brief 处理 Abort 请求
 */
int dtxn_handle_abort(distributed_txn_t *txn) {
    if (!txn) return -1;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return -1;

    pthread_mutex_lock(&impl->lock);

    /* 调用应用层的 abort 回调 */
    if (impl->ops.abort) {
        impl->ops.abort(impl->ops.user_data, txn);
    }

    impl->state = DTXN_STATE_ABORTED;

    pthread_mutex_unlock(&impl->lock);

    return 0;
}

/* ========================================================================
 * 查询接口
 * ======================================================================== */

/**
 * @brief 获取事务状态
 */
distributed_txn_state_t dtxn_get_state(const distributed_txn_t *txn) {
    if (!txn) return DTXN_STATE_UNKNOWN;

    const distributed_txn_impl_t *impl = (const distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return DTXN_STATE_UNKNOWN;

    pthread_mutex_lock((pthread_mutex_t *)&impl->lock);
    distributed_txn_state_t state = impl->state;
    pthread_mutex_unlock((pthread_mutex_t *)&impl->lock);

    return state;
}

/**
 * @brief 获取事务 ID
 */
const char *dtxn_get_id(const distributed_txn_t *txn) {
    if (!txn) return NULL;

    const distributed_txn_impl_t *impl = (const distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return NULL;

    return impl->transaction_id;
}

/**
 * @brief 获取事务结果描述
 */
const char *dtxn_error_string(distributed_txn_error_t error) {
    switch (error) {
        case DISTRIBUTED_TXN_OK: return "Success";
        case DISTRIBUTED_TXN_TIMEOUT: return "Transaction timeout";
        case DISTRIBUTED_TXN_ABORT: return "Transaction aborted";
        case DISTRIBUTED_TXN_COMMIT: return "Transaction committed";
        case DISTRIBUTED_TXN_PENDING: return "Transaction pending";
        case DISTRIBUTED_TXN_INVALID_STATE: return "Invalid transaction state";
        case DISTRIBUTED_TXN_NETWORK_ERROR: return "Network error";
        case DISTRIBUTED_TXN_PROTOCOL_ERROR: return "Protocol error";
        case DISTRIBUTED_TXN_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

/* ========================================================================
 * 生命周期
 * ======================================================================== */

/**
 * @brief 销毁分布式事务
 */
void dtxn_coordinator_destroy(distributed_txn_t *txn) {
    if (!txn) return;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return;

    pthread_mutex_lock(&impl->lock);

    if (impl->participants) {
        free(impl->participants);
        impl->participants = NULL;
    }

    impl->magic = 0;

    pthread_mutex_unlock(&impl->lock);
    pthread_mutex_destroy(&impl->lock);

    free(impl);
}

/* ========================================================================
 * 持久化（用于故障恢复）
 * ======================================================================== */

/**
 * @brief 保存事务状态
 */
int dtxn_persist_state(const distributed_txn_t *txn, const char *path) {
    if (!txn || !path) return -1;

    const distributed_txn_impl_t *impl = (const distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return -1;

    /* TODO: 实现状态持久化 */
    return 0;
}

/**
 * @brief 恢复事务状态
 */
distributed_txn_t *dtxn_recover_state(const char *path,
                                   const distributed_txn_ops_t *ops) {
    if (!path) return NULL;

    /* TODO: 实现状态恢复 */
    return NULL;
}

/* ========================================================================
 * 超时检测
 * ======================================================================== */

/**
 * @brief 检查参与者超时
 */
bool dtxn_check_participant_timeout(const distributed_txn_t *txn,
                                  uint32_t timeout_ms) {
    if (!txn) return false;

    const distributed_txn_impl_t *impl = (const distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return false;

    uint64_t now = now_ms();

    pthread_mutex_lock((pthread_mutex_t *)&impl->lock);

    for (uint32_t i = 0; i < impl->participant_count; i++) {
        const distributed_txn_participant_t *p = &impl->participants[i];

        if (p->last_response_time > 0 &&
            now - p->last_response_time > timeout_ms) {
            pthread_mutex_unlock((pthread_mutex_t *)&impl->lock);
            return true;
        }
    }

    pthread_mutex_unlock((pthread_mutex_t *)&impl->lock);

    return false;
}

/**
 * @brief 清理超时参与者
 */
int dtxn_cleanup_timedout_participants(distributed_txn_t *txn,
                                    uint32_t timeout_ms) {
    if (!txn) return 0;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return 0;

    uint64_t now = now_ms();
    int cleaned = 0;

    pthread_mutex_lock(&impl->lock);

    for (uint32_t i = 0; i < impl->participant_count; i++) {
        distributed_txn_participant_t *p = &impl->participants[i];

        if (p->last_response_time > 0 &&
            now - p->last_response_time > timeout_ms) {
            p->state = DTXN_PARTICIPANT_UNKNOWN;
            cleaned++;
        }
    }

    pthread_mutex_unlock(&impl->lock);

    return cleaned;
}

/**
 * @brief 异步开始两阶段提交
 */
void dtxn_begin_2pc_async(distributed_txn_t *txn,
                          distributed_txn_callback_t callback) {
    if (!txn) return;

    distributed_txn_impl_t *impl = (distributed_txn_impl_t *)txn;
    if (impl->magic != DTXN_MAGIC) return;

    pthread_mutex_lock(&impl->lock);

    impl->callback = callback;

    pthread_mutex_unlock(&impl->lock);

    /* TODO: 在后台线程执行 2PC，完成后调用 callback */
}
