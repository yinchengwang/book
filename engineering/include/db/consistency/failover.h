/**
 * @file failover.h
 * @brief 自动故障切换接口
 *
 * 提供基于复制一致性的自动故障切换功能。
 * 当 Leader 节点发生故障时，自动进行选举和新 Leader 提升。
 */

#ifndef DB_CONSISTENCY_FAILOVER_H
#define DB_CONSISTENCY_FAILOVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct replication_consensus replication_consensus_t;

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 故障切换状态
 */
typedef enum {
    FAILOVER_IDLE,       /**< 空闲状态，未进行故障切换 */
    FAILOVER_DETECTING,  /**< 检测中，正在检测 Leader 是否故障 */
    FAILOVER_ELECTION,   /**< 选举中，正在进行 Leader 选举 */
    FAILOVER_PROMOTING,  /**< 提升中，正在将本节点提升为 Leader */
    FAILOVER_COMPLETE    /**< 完成，故障切换已完成 */
} failover_state_t;

/**
 * @brief 故障切换回调函数类型
 * @param old_leader 原 Leader 节点 ID
 * @param new_leader 新 Leader 节点 ID
 * @param arg 用户参数
 */
typedef void (*failover_callback_t)(int old_leader, int new_leader, void *arg);

/* ============================================================
 * 故障切换管理
 * ============================================================ */

/**
 * @brief 启动自动故障切换
 *
 * 注册故障切换回调函数并启动故障切换监控。
 * 当检测到 Leader 故障时，自动触发故障切换流程。
 *
 * @param rc 复制一致性实例
 * @param cb 故障切换完成回调函数
 * @param arg 传递给回调的用户参数
 * @return 0 成功；-1 失败
 */
int rc_failover_start(replication_consensus_t *rc, failover_callback_t cb, void *arg);

/**
 * @brief 停止自动故障切换
 *
 * 停止故障切换监控并注销回调函数。
 *
 * @param rc 复制一致性实例
 */
void rc_failover_stop(replication_consensus_t *rc);

/**
 * @brief 获取当前故障切换状态
 * @param rc 复制一致性实例
 * @return 故障切换状态
 */
int rc_get_failover_state(const replication_consensus_t *rc);

/**
 * @brief 获取当前 Leader ID
 *
 * 从 Raft 模块获取当前 Leader 节点 ID。
 *
 * @param rc 复制一致性实例
 * @return Leader ID，未知时返回负值
 */
int rc_get_leader_id(const replication_consensus_t *rc);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSISTENCY_FAILOVER_H */
