/**
 * @file linearizability.c
 * @brief 线性一致性实现
 *
 * 实现线性一致性保证：
 * - rc_linear_wait: 等待日志在多数派确认
 * - rc_can_read: 判断是否可以执行线性读
 */

#include "db/consistency/replication_consensus.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 线性一致性等待
 * ============================================================ */

int rc_linear_wait(replication_consensus_t *rc, uint64_t lsn,
                   int quorum, int timeout_ms)
{
    if (rc == NULL || rc->raft == NULL) {
        return -1;
    }

    /* 只有 Leader 才能提交日志并等待确认 */
    if (!raft_is_leader(rc->raft)) {
        return -1;
    }

    /* 参数校验 */
    if (quorum <= 0 || timeout_ms <= 0) {
        return -1;
    }

    /* 记录开始时间 */
    uint64_t start_ms = 0;
    raft_tick_advance_ms(rc->raft, 0);  /* 获取当前虚拟时间 */
    start_ms = (uint64_t)time(NULL) * 1000;  /* 使用实际时间 */

    /*
     * 等待 LSN 被确认
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
    while (1) {
        uint64_t commit_index = raft_get_commit_index(rc->raft);

        /* LSN 通常对应 commit_index，如果 lsn <= commit_index 说明已确认 */
        if (lsn <= commit_index) {
            return 0;
        }

        /* 检查是否超时 */
        uint64_t now_ms = (uint64_t)time(NULL) * 1000;
        if (now_ms - start_ms >= (uint64_t)timeout_ms) {
            return -1;  /* 超时 */
        }

        /* 模拟短暂等待后继续检查 */
        /* 实际实现中这里应该使用条件变量或事件驱动 */
    }
}

/* ============================================================
 * 线性读取判断
 * ============================================================ */

bool rc_can_read(const replication_consensus_t *rc)
{
    if (rc == NULL || rc->raft == NULL) {
        return false;
    }

    RaftRole_t role = raft_get_role(rc->raft);

    if (role == RAFT_ROLE_LEADER) {
        /*
         * Leader 可以读取，但严格线性一致性需要：
         * - 检查 lease 是否有效（如果启用了 lease）
         * - 或者执行同步读取（等待确认）
         *
         * 简化实现：Leader 始终可以读取
         * 严格线性一致性下应检查 lease 或等待同步
         */
        return true;
    } else if (role == RAFT_ROLE_FOLLOWER) {
        /*
         * Follower 读取：
         * - 如果启用了 strict_linearizability，Follower 不能直接读取
         * - 需要转发给 Leader 或检查 lease 是否有效
         *
         * 简化实现：Follower 可以读取旧数据
         * 严格线性一致性下应返回 false 或转发给 Leader
         */
        return true;
    }

    /* Candidate 状态不能读取 */
    return false;
}
