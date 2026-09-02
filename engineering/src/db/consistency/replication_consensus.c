/**
 * @file replication_consensus.c
 * @brief ReplicationConsensus 统一入口实现
 *
 * 整合 Raft 共识算法与主从复制机制的统一入口实现。
 */

#include "db/consistency/replication_consensus.h"
#include "db/consistency/multisource.h"
#include "db/core/engine.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 内部结构
 * ============================================================ */

/* ============================================================
 * 常量定义
 * ============================================================ */

#define MAX_REPLICA_SOURCES 16  /**< 最大复制源数量 */

/* ============================================================
 * 内部结构
 * ============================================================ */

struct replication_consensus {
    RaftServer_t       *raft;          /**< Raft 服务器 */
    repl_manager_t     *repl;          /**< 复制管理器 */
    repl_config_t      *repl_cfg;      /**< 复制配置 */
    bool                started;       /**< 是否已启动 */
    void               *failover_ctx;  /**< 故障切换上下文 */
    /* 多源复制支持 */
    replica_node_t      sources[MAX_REPLICA_SOURCES];  /**< 复制源节点 */
    int                 source_count;                      /**< 复制源数量 */
};

/* ============================================================
 * 生命周期
 * ============================================================ */

replication_consensus_t *rc_create(const RaftServerConfig_t *raft_cfg)
{
    if (raft_cfg == NULL) {
        return NULL;
    }

    replication_consensus_t *rc = malloc(sizeof(replication_consensus_t));
    if (rc == NULL) {
        return NULL;
    }

    memset(rc, 0, sizeof(replication_consensus_t));

    /* 创建 Raft 服务器 */
    rc->raft = raft_server_create(raft_cfg);
    if (rc->raft == NULL) {
        free(rc);
        return NULL;
    }

    /* 创建复制配置（默认从节点配置） */
    rc->repl_cfg = repl_config_create(REPL_ROLE_REPLICA);
    if (rc->repl_cfg == NULL) {
        raft_server_destroy(rc->raft);
        free(rc);
        return NULL;
    }

    /* 创建复制管理器 */
    rc->repl = repl_manager_create(rc->repl_cfg);
    if (rc->repl == NULL) {
        repl_config_destroy(rc->repl_cfg);
        raft_server_destroy(rc->raft);
        free(rc);
        return NULL;
    }

    rc->started = false;
    return rc;
}

void rc_destroy(replication_consensus_t *rc)
{
    if (rc == NULL) {
        return;
    }

    if (rc->started) {
        rc_stop(rc);
    }

    if (rc->repl != NULL) {
        repl_manager_destroy(rc->repl);
        rc->repl = NULL;
    }

    if (rc->repl_cfg != NULL) {
        repl_config_destroy(rc->repl_cfg);
        rc->repl_cfg = NULL;
    }

    if (rc->raft != NULL) {
        raft_server_destroy(rc->raft);
        rc->raft = NULL;
    }

    free(rc);
}

int rc_start(replication_consensus_t *rc)
{
    if (rc == NULL || rc->started) {
        return -1;
    }

    /* 启动 Raft 服务器 */
    if (raft_server_start(rc->raft) != 0) {
        return -1;
    }

    /* 启动复制服务 */
    if (repl_start(rc->repl) != 0) {
        raft_server_stop(rc->raft);
        return -1;
    }

    rc->started = true;
    return 0;
}

void rc_stop(replication_consensus_t *rc)
{
    if (rc == NULL || !rc->started) {
        return;
    }

    repl_stop(rc->repl);
    raft_server_stop(rc->raft);
    rc->started = false;
}

/* ============================================================
 * 状态查询
 * ============================================================ */

bool rc_is_leader(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return false;
    }
    return raft_is_leader(rc->raft);
}

RaftRole_t rc_get_role(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return RAFT_ROLE_FOLLOWER;
    }
    return raft_get_role(rc->raft);
}

repl_state_t rc_get_repl_state(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->repl == NULL) {
        return REPL_STATE_DISCONNECTED;
    }
    return repl_get_state(rc->repl);
}

/* ============================================================
 * Raft 操作代理
 * ============================================================ */

size_t rc_submit(replication_consensus_t *rc, const void *data, size_t size)
{
    if (rc == NULL || rc->raft == NULL) {
        return SIZE_MAX;
    }
    return raft_submit(rc->raft, data, size);
}

uint64_t rc_get_current_term(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return 0;
    }
    return raft_get_current_term(rc->raft);
}

uint64_t rc_get_leader_id(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return RAFT_UNKNOWN_LEADER;
    }
    return raft_get_leader_id(rc->raft);
}

uint64_t rc_get_commit_index(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return 0;
    }
    return raft_get_commit_index(rc->raft);
}

/* ============================================================
 * 复制操作代理
 * ============================================================ */

ssize_t rc_send_wal(replication_consensus_t *rc, const void *wal_data,
                    size_t len, uint64_t lsn)
{
    if (rc == NULL || rc->repl == NULL) {
        return -1;
    }
    return repl_send_wal(rc->repl, wal_data, len, lsn);
}

int64_t rc_get_lag_ms(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->repl == NULL) {
        return -1;
    }
    return repl_get_lag_ms(rc->repl);
}

const repl_stats_t *rc_get_stats(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->repl == NULL) {
        return NULL;
    }
    return repl_get_stats(rc->repl);
}

/* ============================================================
 * 故障切换
 * ============================================================ */

bool rc_need_failover(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->repl == NULL) {
        return false;
    }
    return repl_need_failover(rc->repl);
}

int rc_do_failover(replication_consensus_t *rc)
{
    if (rc == NULL || rc->repl == NULL) {
        return -1;
    }
    return repl_do_failover(rc->repl);
}

/* ============================================================
 * 线性一致性
 * ============================================================ */

int rc_linear_wait(replication_consensus_t *rc, uint64_t lsn,
                   int quorum, int timeout_ms)
{
    if (rc == NULL || rc->raft == NULL) {
        return -1;
    }

    /* 只有 Leader 才能执行线性等待 */
    if (!raft_is_leader(rc->raft)) {
        return -1;
    }

    /* 参数校验 */
    if (quorum <= 0 || timeout_ms <= 0) {
        return -1;
    }

    /*
     * 等待 LSN 在多数派确认
     *
     * 实现策略：
     * 1. 获取当前 commit_index
     * 2. 如果 lsn <= commit_index，说明已确认，立即返回成功
     * 3. 否则轮询 commit_index，直到超时或确认
     *
     * 注意：这是简化实现，实际生产环境需要：
     * - 跟踪每个 follower 的 match_index 来统计 quorum
     * - 使用 lease 机制优化读取性能
     */
    uint64_t start_time = (uint64_t)time(NULL);

    while (1) {
        uint64_t commit_index = raft_get_commit_index(rc->raft);

        /* LSN 对应 commit_index，如果 lsn <= commit_index 说明已确认 */
        if (lsn <= commit_index) {
            return 0;
        }

        /* 检查是否超时 */
        uint64_t now = (uint64_t)time(NULL);
        if ((now - start_time) * 1000 >= (uint64_t)timeout_ms) {
            return -1;
        }

        /* 短暂让出 CPU，避免忙等待 */
        /* 实际实现中应使用条件变量或事件驱动 */
    }
}

bool rc_can_read(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return false;
    }

    RaftRole_t role = raft_get_role(rc->raft);

    if (role == RAFT_ROLE_LEADER) {
        /*
         * Leader 可以读取
         *
         * 严格线性一致性下：
         * - 需要检查 lease 是否有效
         * - 或执行同步读取等待确认
         *
         * 当前简化实现：Leader 始终可以读取
         */
        return true;
    } else if (role == RAFT_ROLE_FOLLOWER) {
        /*
         * Follower 读取：
         *
         * 严格线性一致性下：
         * - 如果启用 strict_linearizability，Follower 不能直接读取
         * - 需要转发给 Leader 或确保 lease 有效
         *
         * 当前简化实现：Follower 可以读取
         */
        return true;
    }

    /* Candidate 状态不能读取 */
    return false;
}

/* ============================================================
 * 多源复制
 * ============================================================ */

int rc_add_source(replication_consensus_t *rc, const replica_node_t *node)
{
    if (rc == NULL || node == NULL) {
        return -1;
    }

    if (node->node_id <= 0) {
        return -1;
    }

    if (rc->source_count >= MAX_REPLICA_SOURCES) {
        return -1;
    }

    /* 检查是否已存在 */
    for (int i = 0; i < rc->source_count; i++) {
        if (rc->sources[i].node_id == node->node_id) {
            /* 更新已存在的节点 */
            rc->sources[i] = *node;
            return 0;
        }
    }

    /* 添加新节点 */
    rc->sources[rc->source_count++] = *node;
    return 0;
}

int rc_remove_source(replication_consensus_t *rc, int node_id)
{
    if (rc == NULL || node_id <= 0) {
        return -1;
    }

    for (int i = 0; i < rc->source_count; i++) {
        if (rc->sources[i].node_id == node_id) {
            /* 移动后面的元素 */
            for (int j = i; j < rc->source_count - 1; j++) {
                rc->sources[j] = rc->sources[j + 1];
            }
            rc->source_count--;
            memset(&rc->sources[rc->source_count], 0, sizeof(replica_node_t));
            return 0;
        }
    }

    return -1;
}

int rc_get_sources(replication_consensus_t *rc, replica_node_t *nodes, int *count)
{
    if (rc == NULL || nodes == NULL || count == NULL) {
        return -1;
    }

    int max_count = *count;
    *count = rc->source_count;

    if (max_count < rc->source_count) {
        /* 缓冲区不够，只复制能容纳的数量 */
        for (int i = 0; i < max_count; i++) {
            nodes[i] = rc->sources[i];
        }
        return -1;
    }

    for (int i = 0; i < rc->source_count; i++) {
        nodes[i] = rc->sources[i];
    }

    return 0;
}

int rc_sync_all(replication_consensus_t *rc)
{
    if (rc == NULL) {
        return -1;
    }

    /* 同步所有源节点 */
    for (int i = 0; i < rc->source_count; i++) {
        replica_node_t *source = &rc->sources[i];

        /*
         * 简化实现：更新源节点状态
         * 实际实现中需要与每个源节点建立连接并同步 WAL
         */
        if (source->state == REPL_STATE_DISCONNECTED) {
            /* 尝试重新连接 */
            source->state = REPL_STATE_CONNECTING;
        } else if (source->state == REPL_STATE_STREAMING ||
                   source->state == REPL_STATE_NORMAL) {
            /* 正常的源节点，状态已经是同步的 */
            ;
        }
    }

    return 0;
}
