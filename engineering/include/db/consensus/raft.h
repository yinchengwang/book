/**
 * @file raft.h
 * @brief Raft 共识算法（工程层实现）
 *
 * Phase11 — 工程层 Raft 模块从零实现。
 * 仅提供**最小可用**版本：单节点角色管理、tick 推进选举、leader 接受日志。
 *
 * 不包含（明确范围外）：
 * - 集群网络通信（loopback 占位）
 * - 日志持久化
 * - Joint Consensus / Snapshot
 * - PreVote
 *
 * 用法（典型）：
 *   RaftServerConfig cfg = {.node_id = 1, .cluster_size = 3,
 *                            .heartbeat_interval_ms = 150,
 *                            .election_timeout_min_ms = 1000,
 *                            .election_timeout_max_ms = 2000};
 *   RaftServer *srv = raft_server_create(&cfg);
 *   raft_server_start(srv);
 *
 *   while (running) {
 *       raft_tick(srv);
 *       usleep(20 * 1000);  // 50Hz tick
 *       if (raft_is_leader(srv)) {
 *           raft_submit(srv, payload, payload_size);
 *       }
 *   }
 *
 *   raft_server_destroy(srv);
 */

#ifndef DB_CONSENSUS_RAFT_H
#define DB_CONSENSUS_RAFT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量
 * ============================================================ */

#define RAFT_UNKNOWN_LEADER UINT64_MAX
#define RAFT_NO_VOTE        UINT64_MAX

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief Raft 节点角色（简化版）
 */
typedef enum RaftRole {
    RAFT_ROLE_FOLLOWER = 0,    /**< 跟随者 */
    RAFT_ROLE_CANDIDATE,      /**< 候选者 */
    RAFT_ROLE_LEADER          /**< 领导者 */
} RaftRole_t;

/**
 * @brief Raft 服务器配置
 */
typedef struct RaftServerConfig {
    uint64_t node_id;                  /**< 唯一节点 id */
    uint32_t cluster_size;             /**< 集群节点数（用于 quorum 计算） */
    uint32_t heartbeat_interval_ms;    /**< leader 心跳间隔 */
    uint32_t election_timeout_min_ms;  /**< 选举超时下限 */
    uint32_t election_timeout_max_ms;  /**< 选举超时上限 */
} RaftServerConfig_t;

/**
 * @brief Raft 状态持久化配置（P5）
 */
typedef struct RaftStateConfig {
    const char *state_path;  /**< bin 文件路径；NULL = in-memory (P5 不支持) */
} RaftStateConfig_t;

/**
 * @brief Raft 服务器不透明类型
 */
typedef struct RaftServer RaftServer_t;
typedef struct RaftLogEntry {
    uint64_t term;       /**< 创建该 entry 时的 term */
    uint64_t index;      /**< 全局递增索引 */
    void    *data;       /**< 应用数据（不持有所有权） */
    size_t   data_size;
} RaftLogEntry_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 创建 Raft 服务器
 * @param cfg 配置（不可为 NULL）
 * @return 成功：服务器指针；失败：NULL
 */
RaftServer_t *raft_server_create(const RaftServerConfig_t *cfg);

/**
 * @brief 创建 Raft 服务器（P5 + 带持久化）
 *
 * @param cfg 普通配置
 * @param state 持久化配置（state_path=NULL 时 in-memory，等同 raft_server_create）
 * @return 成功：服务器指针；失败：NULL
 */
RaftServer_t *raft_server_create_ex(const RaftServerConfig_t *cfg,
                                    const RaftStateConfig_t *state);

/**
 * @brief 启动服务器（不阻塞）
 */
int raft_server_start(RaftServer_t *srv);

/**
 * @brief 停止服务器
 */
int raft_server_stop(RaftServer_t *srv);

/**
 * @brief 销毁服务器（自动 flush 持久化）
 */
void raft_server_destroy(RaftServer_t *srv);

/* ============================================================
 * 持久化（P5）
 * ============================================================ */

/**
 * @brief 保存当前状态（含 term、voted_for、log）到 state_path
 *
 * 同步写入。返回 0 成功，-1 失败（state_path 未配置时）。
 */
int raft_server_save_state(RaftServer_t *srv);

/**
 * @brief 从 state_path 装载状态（P5）
 *
 * 装载 term、voted_for、log（追加在内存里）
 * 返回 0 成功，-1 失败
 */
int raft_server_load_state(RaftServer_t *srv, const char *path);

/**
 * @brief 测试钩子：清空状态
 */
void raft_test_reset_state(RaftServer_t *srv);

/**
 * @brief 测试钩子：强制将 candidate 提升为 leader（P8 cluster 使用）
 *
 * 在 raft_cluster_force_election 投票完成后调用，使 candidate 跳过 quorum 检查。
 * 仅在测试中用于验证集群功能，不涉及安全风险。
 */
void raft_test_become_leader(RaftServer_t *srv);

/* ============================================================
 * Snapshot（P7）
 * ============================================================ */

/**
 * @brief 创建 Snapshot + 日志压缩
 *
 * 将 committed log 状态 + 应用层数据写入 snapshot 文件，
 * 然后丢弃 last_included_index 之前的日志。
 *
 * @param srv  Raft 服务器
 * @param snapshot_data  应用层快照数据（如 key-value dump），可 NULL
 * @param data_size      snapshot_data 字节数
 * @param last_included_index  快照覆盖的最后一条日志索引（0 = 不从日志创建）
 *                               若 > 0 且 >= commit_index，日志会被截断
 * @return 0 成功；-1 失败
 */
int raft_take_snapshot(RaftServer_t *srv,
                       const void *snapshot_data, size_t data_size,
                       uint64_t last_included_index);

/**
 * @brief 安装 Snapshot（从 leader 收到后调用）
 *
 * 清空当前日志 + 状态，加载 snapshot，重置 commit_index。
 *
 * @param srv  Raft 服务器
 * @param snapshot_data  快照数据
 * @param data_size      快照字节数
 * @param last_included_index  快照包含的最后日志索引
 * @param last_included_term   快照包含的最后日志 term
 * @return 0 成功；-1 失败
 */
int raft_install_snapshot(RaftServer_t *srv,
                          const void *snapshot_data, size_t data_size,
                          uint64_t last_included_index,
                          uint64_t last_included_term);

/**
 * @brief 读取当前 Snapshot（leader 用于发送给 lagging follower）
 *
 * @param srv      Raft 服务器
 * @param[out] buf         输出缓冲区（可 NULL 只查长度）
 * @param[in] cap          缓冲区容量
 * @param[out] out_size    实际 snapshot data 字节数
 * @param[out] out_last_included_index  快照的最后日志索引
 * @param[out] out_last_included_term   快照的最后日志 term
 * @return 0 成功；-1 无 snapshot
 */
int raft_get_snapshot(const RaftServer_t *srv,
                      void *buf, size_t cap, size_t *out_size,
                      uint64_t *out_last_included_index,
                      uint64_t *out_last_included_term);

/**
 * @brief 查询 snapshot 状态
 */
bool raft_has_snapshot(const RaftServer_t *srv);
uint64_t raft_get_snapshot_last_index(const RaftServer_t *srv);
uint64_t raft_get_snapshot_last_term(const RaftServer_t *srv);

/**
 * @brief 获取日志中第一条存活条目的逻辑索引（用于 snapshot 压缩后计算偏移）
 *
 * 尚未 snapshot 时返回 1（Raft 1-based 约定）。
 * snapshot + compaction 后返回 last_included_index + 1。
 */
uint64_t raft_get_first_log_index(const RaftServer_t *srv);

/* ============================================================
 * 时间推进（测试驱动）
 * ============================================================ */

/**
 * @brief 推进时间 + 处理超时事件（一次 tick）
 *
 * 在测试中，每次调用对应一次"虚拟时间前进"。
 * 真实运行时，调用方按 ~20ms 间隔调用以检测 leader 选举超时。
 */
void raft_tick(RaftServer_t *srv);

/**
 * @brief 推进虚拟时间（仅测试用）
 *
 * @param ms 推进毫秒数（默认实时 tick 用 sys clock）
 */
void raft_tick_advance_ms(RaftServer_t *srv, uint64_t ms);

/* ============================================================
 * 状态查询
 * ============================================================ */

RaftRole_t raft_get_role(const RaftServer_t *srv);
uint64_t raft_get_current_term(const RaftServer_t *srv);
uint64_t raft_get_leader_id(const RaftServer_t *srv);
uint64_t raft_get_commit_index(const RaftServer_t *srv);
uint64_t raft_get_last_applied(const RaftServer_t *srv);
bool raft_is_leader(const RaftServer_t *srv);

/* ============================================================
 * 日志
 * ============================================================ */

/**
 * @brief leader 提交日志（仅 leader 接受）
 *
 * 非 leader 返回 -1；leader 内部 append log，commit 由后续 tick 推进。
 *
 * @return 成功：新 entry 的索引；失败：SIZE_MAX
 */
size_t raft_submit(RaftServer_t *srv, const void *data, size_t size);

/**
 * @brief 读 committed 日志（按 index 从 0 开始）
 *
 * @param idx 0-based 索引
 * @param buf 输出 buffer
 * @param cap buffer 容量
 * @return 实际复制字节数
 */
size_t raft_get_committed(const RaftServer_t *srv, size_t idx,
                          void *buf, size_t cap);

/* ============================================================
 * 测试钩子（公共 ABI）
 * ============================================================ */

/**
 * @brief 强制 tick → 模拟选举超时（仅测试用）
 *
 * 直接推进到 election_timeout 触发点，跳过时间推进。
 */
void raft_test_force_election_timeout(RaftServer_t *srv);

/**
 * @brief 模拟 leader 心跳（重置 election_deadline）
 */
void raft_test_receive_heartbeat(RaftServer_t *srv, uint64_t from_node,
                                 uint64_t term);

/* ============================================================
 * P1 RPC API（多节点 + 日志复制）
 * ============================================================ */

/**
 * @brief RPC 类型
 */
typedef enum RaftRPCType {
    RAFT_RPC_REQUEST_VOTE = 1,
    RAFT_RPC_APPEND_ENTRIES = 2,
} RaftRPCType_t;

/**
 * @brief RequestVote 参数
 */
typedef struct RaftRequestVoteArgs {
    uint64_t term;
    uint64_t candidate_id;
    uint64_t last_log_index;
    uint64_t last_log_term;
} RaftRequestVoteArgs_t;

/**
 * @brief RequestVote 结果
 */
typedef struct RaftRequestVoteResult {
    uint64_t term;          /**< 当前 term（candidate 更新自己用） */
    bool vote_granted;
} RaftRequestVoteResult_t;

/**
 * @brief AppendEntries 参数
 */
typedef struct RaftAppendEntriesArgs {
    uint64_t term;
    uint64_t leader_id;
    uint64_t prev_log_index;
    uint64_t prev_log_term;
    uint64_t leader_commit;
    const RaftLogEntry_t *entries;  /**< 不持有所有权（cluster 拥有） */
    size_t entry_count;
} RaftAppendEntriesArgs_t;

/**
 * @brief AppendEntries 结果
 */
typedef struct RaftAppendEntriesResult {
    uint64_t term;
    bool success;
    uint64_t match_index;    /**< follower 已匹配的索引（leader 用于 next_index） */
} RaftAppendEntriesResult_t;

/**
 * @brief 处理 RequestVote RPC
 */
RaftRequestVoteResult_t raft_handle_request_vote(RaftServer_t *srv,
                                                const RaftRequestVoteArgs_t *args);

/**
 * @brief 处理 AppendEntries RPC
 */
RaftAppendEntriesResult_t raft_handle_append_entries(RaftServer_t *srv,
                                                    const RaftAppendEntriesArgs_t *args);

/**
 * @brief leader 推进 commit_index（in-memory 投票模拟）
 */
void raft_advance_commit_index(RaftServer_t *srv, uint64_t new_commit);

/**
 * @brief 拿到 server 的 commit_index（leader 用来统计 quorum ack）
 */
uint64_t raft_server_get_commit_index(const RaftServer_t *srv);

/**
 * @brief 拿到 server 日志条目（leader 用于 AppendEntries 复制）
 */
size_t raft_server_get_log(const RaftServer_t *srv,
                           uint64_t start_index,
                           uint64_t *out_term,
                           const RaftLogEntry_t **out_entries);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSENSUS_RAFT_H */
