/**
 * @file failover.c
 * @brief 自动故障切换实现
 *
 * 实现基于复制一致性的自动故障切换功能。
 */

#include "db/consistency/failover.h"
#include "db/consistency/replication_consensus.h"
#include "db/consensus/raft.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 内部结构
 * ============================================================ */

/**
 * @brief 故障切换上下文
 */
typedef struct failover_context {
    failover_state_t    state;          /**< 当前状态 */
    failover_callback_t callback;       /**< 回调函数 */
    void               *callback_arg;   /**< 回调参数 */
    int                 old_leader;    /**< 原 Leader ID */
    int                 active;        /**< 是否启用 */
} failover_context_t;

/* ============================================================
 * 内部函数声明
 * ============================================================ */

static void failover_detect_and_elect(replication_consensus_t *rc,
                                      failover_context_t *ctx);
static void failover_promote(replication_consensus_t *rc,
                            failover_context_t *ctx);

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

int rc_failover_start(replication_consensus_t *rc, failover_callback_t cb,
                     void *arg)
{
    if (rc == NULL) {
        return -1;
    }

    /* 获取或分配故障切换上下文 */
    failover_context_t *ctx = (failover_context_t *)rc->failover_ctx;
    if (ctx == NULL) {
        ctx = calloc(1, sizeof(failover_context_t));
        if (ctx == NULL) {
            return -1;
        }
        rc->failover_ctx = ctx;
    }

    ctx->callback = cb;
    ctx->callback_arg = arg;
    ctx->state = FAILOVER_IDLE;
    ctx->active = 1;

    return 0;
}

void rc_failover_stop(replication_consensus_t *rc)
{
    if (rc == NULL) {
        return;
    }

    failover_context_t *ctx = (failover_context_t *)rc->failover_ctx;
    if (ctx != NULL) {
        ctx->active = 0;
        ctx->state = FAILOVER_IDLE;
        ctx->callback = NULL;
        ctx->callback_arg = NULL;
    }
}

int rc_get_failover_state(const replication_consensus_t *rc)
{
    if (rc == NULL) {
        return FAILOVER_IDLE;
    }

    failover_context_t *ctx = (failover_context_t *)rc->failover_ctx;
    if (ctx == NULL) {
        return FAILOVER_IDLE;
    }

    return (int)ctx->state;
}

int rc_get_leader_id(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return -1;
    }
    return (int)raft_get_leader_id(rc->raft);
}

/* ============================================================
 * 内部实现
 * ============================================================ */

/**
 * @brief 检测故障并触发选举
 */
static void failover_detect_and_elect(replication_consensus_t *rc,
                                      failover_context_t *ctx)
{
    if (rc == NULL || ctx == NULL || !ctx->active) {
        return;
    }

    /* 检查当前是否为 Leader */
    if (raft_is_leader(rc->raft)) {
        /* 本节点是 Leader，无需故障切换 */
        ctx->state = FAILOVER_IDLE;
        return;
    }

    /* 获取当前 Leader ID */
    uint64_t current_leader = raft_get_leader_id(rc->raft);

    /* 检查是否需要发起选举 */
    if (current_leader == RAFT_UNKNOWN_LEADER) {
        /* 没有活跃的 Leader，尝试发起选举 */
        ctx->state = FAILOVER_ELECTION;

        /*
         * Raft 模块会处理选举超时检测，
         * 这里只需设置状态并等待 Raft tick 触发选举
         */
    }
}

/**
 * @brief 提升本节点为 Leader
 */
static void failover_promote(replication_consensus_t *rc,
                            failover_context_t *ctx)
{
    if (rc == NULL || ctx == NULL || !ctx->active) {
        return;
    }

    /*
     * 在 Raft 中，当选票足够时 Raft 会自动提升为 Leader。
     * 这里检查是否已经完成选举并成为 Leader。
     */
    if (raft_is_leader(rc->raft)) {
        int new_leader = (int)raft_get_leader_id(rc->raft);
        ctx->state = FAILOVER_COMPLETE;

        /* 调用回调通知故障切换完成 */
        if (ctx->callback != NULL) {
            ctx->callback(ctx->old_leader, new_leader, ctx->callback_arg);
        }

        ctx->state = FAILOVER_IDLE;
    }
}
