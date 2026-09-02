/**
 * @file replication_consensus.h
 * @brief ReplicationConsensus 统一入口
 *
 * 整合 Raft 共识算法与主从复制机制的统一入口。
 * 提供复制一致性的高层抽象，封装 raft 和 replication 模块。
 */

#ifndef DB_CONSISTENCY_REPLICATION_CONSENSUS_H
#define DB_CONSISTENCY_REPLICATION_CONSENSUS_H

#include "raft.h"
#include "replication.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief ReplicationConsensus 不透明类型
 */
typedef struct replication_consensus replication_consensus_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 创建 ReplicationConsensus 实例
 * @param raft_cfg Raft 服务器配置（不可为 NULL）
 * @return 成功：实例指针；失败：NULL
 */
replication_consensus_t *rc_create(const RaftServerConfig_t *raft_cfg);

/**
 * @brief 销毁 ReplicationConsensus 实例
 * @param rc 实例
 */
void rc_destroy(replication_consensus_t *rc);

/**
 * @brief 启动复制一致性服务
 * @param rc 实例
 * @return 0 成功；-1 失败
 */
int rc_start(replication_consensus_t *rc);

/**
 * @brief 停止复制一致性服务
 * @param rc 实例
 */
void rc_stop(replication_consensus_t *rc);

/* ============================================================
 * 状态查询
 * ============================================================ */

/**
 * @brief 检查是否为 Leader
 * @param rc 实例
 * @return true 是 Leader
 */
bool rc_is_leader(const replication_consensus_t *rc);

/**
 * @brief 获取当前角色
 * @param rc 实例
 * @return Raft 角色
 */
RaftRole_t rc_get_role(const replication_consensus_t *rc);

/**
 * @brief 获取复制状态
 * @param rc 实例
 * @return 复制状态
 */
repl_state_t rc_get_repl_state(const replication_consensus_t *rc);

/* ============================================================
 * Raft 操作代理
 * ============================================================ */

/**
 * @brief 提交日志（仅 Leader 接受）
 * @param rc 实例
 * @param data 数据
 * @param size 数据大小
 * @return 成功：新 entry 索引；失败：SIZE_MAX
 */
size_t rc_submit(replication_consensus_t *rc, const void *data, size_t size);

/**
 * @brief 获取当前 Term
 * @param rc 实例
 * @return Term
 */
uint64_t rc_get_current_term(const replication_consensus_t *rc);

/**
 * @brief 获取 Leader ID
 * @param rc 实例
 * @return Leader 节点 ID
 */
uint64_t rc_get_leader_id(const replication_consensus_t *rc);

/**
 * @brief 获取 Commit Index
 * @param rc 实例
 * @return Commit Index
 */
uint64_t rc_get_commit_index(const replication_consensus_t *rc);

/* ============================================================
 * 复制操作代理
 * ============================================================ */

/**
 * @brief 发送 WAL 数据（Leader 调用）
 * @param rc 实例
 * @param wal_data WAL 数据
 * @param len 数据长度
 * @param lsn WAL LSN
 * @return 发送的字节数，-1 失败
 */
ssize_t rc_send_wal(replication_consensus_t *rc, const void *wal_data,
                    size_t len, uint64_t lsn);

/**
 * @brief 获取复制延迟
 * @param rc 实例
 * @return 延迟（毫秒）
 */
int64_t rc_get_lag_ms(const replication_consensus_t *rc);

/**
 * @brief 获取复制统计
 * @param rc 实例
 * @return 统计信息
 */
const repl_stats_t *rc_get_stats(const replication_consensus_t *rc);

/* ============================================================
 * 线性一致性
 * ============================================================ */

/**
 * @brief 线性一致性配置
 */
typedef struct {
    bool strict_linearizability;  /**< 是否启用严格线性一致性 */
    int  quorum_size;             /**< Quorum 大小（多数派） */
    int  ack_timeout_ms;          /**< 等待 ack 超时（毫秒） */
} linearizability_config_t;

/**
 * @brief 等待 LSN 在多数派确认（线性一致性写入）
 *
 * Leader 调用此函数等待日志在指定 quorum 确认。
 *
 * @param rc 实例
 * @param lsn 待确认的 LSN
 * @param quorum 确认节点数（多数派通常为 cluster_size/2+1）
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功（LSN 已确认）；-1 失败或超时
 */
int rc_linear_wait(replication_consensus_t *rc, uint64_t lsn,
                   int quorum, int timeout_ms);

/**
 * @brief 检查当前是否可以执行线性读
 *
 * 判断是否满足线性一致性读取的条件：
 * - 如果是 Leader：检查 lease 是否有效或是否需要同步
 * - 如果是 Follower：检查是否可读或需要转发 Leader
 *
 * @param rc 实例
 * @return true 可以读取；false 不可以读取
 */
bool rc_can_read(const replication_consensus_t *rc);

/* ============================================================
 * 故障切换
 * ============================================================ */

/**
 * @brief 检查是否需要故障切换
 * @param rc 实例
 * @return true 需要切换
 */
bool rc_need_failover(const replication_consensus_t *rc);

/**
 * @brief 执行故障切换
 * @param rc 实例
 * @return 0 成功
 */
int rc_do_failover(replication_consensus_t *rc);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSISTENCY_REPLICATION_CONSENSUS_H */
