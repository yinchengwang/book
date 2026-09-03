/**
 * @file raft.h
 * @brief Raft 共识算法实现
 *
 * 实现 Raft 核心算法、成员变更、故障检测、线性一致性读、快照恢复
 */
#ifndef DB_RAFT_H
#define DB_RAFT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Raft 角色
 * ======================================================================== */

/** Raft 节点角色 */
typedef enum RaftRole_e {
    RAFT_ROLE_FOLLOWER = 0,    /**< 跟随者 */
    RAFT_ROLE_CANDIDATE,        /**< 候选者 */
    RAFT_ROLE_LEADER,           /**< 领导者 */
} RaftRole;

/* ========================================================================
 * Raft 节点配置
 * ======================================================================== */

/** Raft 节点信息 */
typedef struct RaftNode_s {
    uint64_t node_id;          /**< 节点 ID */
    char address[256];         /**< 网络地址 */
    int port;                   /**< 端口 */
    bool is_learner;           /**< 是否为只读学习者 */
} RaftNode;

/** Raft 集群配置 */
typedef struct RaftClusterConfig_s {
    RaftNode *nodes;           /**< 节点数组 */
    size_t num_nodes;          /**< 节点数量 */
    size_t quorum;             /**< 多数派数量 */
    uint64_t config_version;    /**< 配置版本 */
} RaftClusterConfig;

/* ========================================================================
 * 日志条目
 * ======================================================================== */

/** 日志条目类型 */
typedef enum RaftEntryType_e {
    RAFT_ENTRY_DATA = 0,        /**< 数据条目 */
    RAFT_ENTRY_CONFIGURATION,   /**< 配置变更条目 */
    RAFT_ENTRY_SNAPSHOT_META,  /**< 快照元数据 */
} RaftEntryType;

/** 日志条目 */
typedef struct RaftEntry_s {
    uint64_t term;             /**< 任期号 */
    uint64_t index;            /**< 日志索引 */
    RaftEntryType type;        /**< 条目类型 */

    /* 数据 */
    void *data;                /**< 条目数据 */
    size_t data_size;          /**< 数据大小 */

    /* 元数据 */
    uint64_t timestamp;        /**< 创建时间 */
    uint64_t client_id;        /**< 客户端 ID */
} RaftEntry;

/** 日志存储接口 */
typedef struct RaftLogStore_s {
    /* 日志操作 */
    int (*append)(void *ctx, const RaftEntry *entry);
    int (*get)(void *ctx, uint64_t index, RaftEntry *entry);
    int (*truncate)(void *ctx, uint64_t from_index);
    uint64_t (*last_index)(void *ctx);
    uint64_t (*term)(void *ctx);

    /* 快照操作 */
    int (*snapshot)(void *ctx, void **data, size_t *size);
    int (*restore)(void *ctx, const void *data, size_t size);
} RaftLogStore;

/* ========================================================================
 * Raft 状态
 * ======================================================================== */

/** Raft 节点状态 */
typedef struct RaftServer_s {
    /* 身份 */
    RaftRole role;              /**< 当前角色 */
    uint64_t node_id;           /**< 节点 ID */
    char *cluster_address;      /**< 集群地址 */

    /* 持久化状态 (需要定期保存) */
    uint64_t current_term;     /**< 当前任期 */
    uint64_t voted_for;        /**< 投票给的候选者 */
    RaftEntry *log;            /**< 日志条目数组 */
    size_t log_size;           /**< 日志大小 */
    size_t log_capacity;       /**< 日志容量 */

    /* Volatile 状态 */
    uint64_t commit_index;     /**< 已提交日志索引 */
    uint64_t last_applied;     /**< 已应用日志索引 */
    uint64_t last_snapshot_index;  /**< 快照最后索引 */
    uint64_t last_snapshot_term;   /**< 快照最后任期 */

    /* Leader Volatile 状态 */
    uint64_t *next_index;      /**< 每个跟随者的下一个日志索引 */
    uint64_t *match_index;     /**< 每个跟随者已匹配的日志索引 */

    /* Leader 配置（用于 Joint Consensus）*/
    RaftClusterConfig *new_config;  /**< 新配置（Joint Consensus）*/

    /* 集群配置 */
    RaftClusterConfig *config;  /**< 当前配置 */
    bool joint_consensus;        /**< 是否处于 Joint Consensus 阶段 */

    /* 网络和状态机 */
    void *state_machine;        /**< 状态机 */
    void *transport;            /**< 网络传输层 */

    /* 线程安全 */
    pthread_mutex_t lock;       /**< 主锁 */

    /* 时间管理 */
    uint64_t election_timeout;  /**< 选举超时（毫秒） */
    uint64_t heartbeat_interval;/**< 心跳间隔（毫秒） */
    uint64_t last_heartbeat;    /**< 最后心跳时间 */

    /* 统计 */
    uint64_t voted_count;       /**< 获得的票数 */
} RaftServer;

/**
 * @brief 创建 Raft 服务器
 */
RaftServer *raft_server_create(uint64_t node_id, const char *address);

/**
 * @brief 销毁 Raft 服务器
 */
void raft_server_destroy(RaftServer *server);

/**
 * @brief 初始化 Raft 服务器
 */
int raft_server_init(RaftServer *server,
                     RaftClusterConfig *config,
                     RaftLogStore *log_store);

/**
 * @brief 启动 Raft 服务器
 */
int raft_server_start(RaftServer *server);

/**
 * @brief 停止 Raft 服务器
 */
int raft_server_stop(RaftServer *server);

/**
 * @brief 提交日志条目
 */
int raft_submit(RaftServer *server, const void *data, size_t size, uint64_t *out_index);

/**
 * @brief 获取已提交日志条目
 */
int raft_get_committed(RaftServer *server, uint64_t index, void *data, size_t *size);

/**
 * @brief 获取当前领导
 */
uint64_t raft_get_leader(RaftServer *server);

/**
 * @brief 检查是否为领导者
 */
bool raft_is_leader(RaftServer *server);

/**
 * @brief 获取集群配置
 */
const RaftClusterConfig *raft_get_config(RaftServer *server);

/* ========================================================================
 * Raft 消息
 * ======================================================================== */

/** 消息类型 */
typedef enum RaftMsgType_e {
    /* 请求 */
    RAFT_MSG_REQUEST_VOTE = 0,  /**< 请求投票 */
    RAFT_MSG_REQUEST_VOTE_RESPONSE, /**< 请求投票响应 */
    RAFT_MSG_APPEND_ENTRIES,    /**< 追加条目 */
    RAFT_MSG_APPEND_ENTRIES_RESPONSE, /**< 追加条目响应 */
    RAFT_MSG_INSTALL_SNAPSHOT,  /**< 安装快照 */
    RAFT_MSG_HEARTBEAT,         /**< 心跳 */
    RAFT_MSG_HEARTBEAT_RESPONSE,/**< 心跳响应 */

    /* 配置变更 */
    RAFT_MSG_CONFIGURATION,      /**< 配置变更 */
    RAFT_MSG_CONFIGURATION_ACK, /**< 配置变更确认 */
} RaftMsgType;

/** Raft 消息 */
typedef struct RaftMessage_s {
    RaftMsgType type;           /**< 消息类型 */
    uint64_t from_node;         /**< 来源节点 */
    uint64_t to_node;           /**< 目标节点 */
    uint64_t term;              /**< 发送者任期 */
    uint64_t timestamp;         /**< 时间戳 */

    union {
        /* RequestVote */
        struct {
            uint64_t last_log_index;
            uint64_t last_log_term;
        } request_vote;

        /* RequestVoteResponse */
        struct {
            bool vote_granted;
        } request_vote_response;

        /* AppendEntries */
        struct {
            uint64_t prev_log_index;
            uint64_t prev_log_term;
            uint64_t entries_length;
            RaftEntry *entries;
            uint64_t leader_commit;
        } append_entries;

        /* AppendEntriesResponse */
        struct {
            bool success;
            uint64_t match_index;
            uint64_t last_log_index;
        } append_entries_response;

        /* InstallSnapshot */
        struct {
            uint64_t last_included_index;
            uint64_t last_included_term;
            size_t snapshot_size;
            void *snapshot_data;
        } install_snapshot;

        /* Configuration */
        struct {
            RaftClusterConfig *config;
            bool is_joint;
        } configuration;
    } data;
} RaftMessage;

/**
 * @brief 发送消息
 */
int raft_send_message(RaftServer *server, const RaftMessage *msg);

/**
 * @brief 处理消息
 */
int raft_handle_message(RaftServer *server, const RaftMessage *msg);

/* ========================================================================
 * 选举
 * ======================================================================== */

/**
 * @brief 开始选举
 */
int raft_start_election(RaftServer *server);

/**
 * @brief 检查选举超时
 */
bool raft_check_election_timeout(RaftServer *server);

/**
 * @brief 重置选举定时器
 */
void raft_reset_election_timer(RaftServer *server);

/**
 * @brief 处理投票请求
 */
int raft_handle_request_vote(RaftServer *server,
                              uint64_t candidate_id,
                              uint64_t candidate_term,
                              uint64_t last_log_index,
                              uint64_t last_log_term,
                              bool *out_vote_granted);

/* ========================================================================
 * 日志复制
 * ======================================================================== */

/**
 * @brief 处理追加日志请求
 */
int raft_handle_append_entries(RaftServer *server,
                               uint64_t leader_id,
                               uint64_t prev_log_index,
                               uint64_t prev_log_term,
                               const RaftEntry *entries,
                               size_t num_entries,
                               uint64_t leader_commit,
                               bool *out_success);

/**
 * @brief 广播日志条目
 */
int raft_broadcast_append_entries(RaftServer *server);

/**
 * @brief 发送心跳
 */
int raft_send_heartbeat(RaftServer *server, uint64_t to_node);

/**
 * @brief 广播心跳
 */
int raft_broadcast_heartbeat(RaftServer *server);

/**
 * @brief 检查是否需要快照
 */
bool raft_needs_snapshot(RaftServer *server, size_t max_log_size);

/* ========================================================================
 * 成员变更 (Joint Consensus)
 * ======================================================================== */

/** 成员变更类型 */
typedef enum RaftConfigChangeType_e {
    RAFT_CONFIG_ADD_NODE = 0,   /**< 添加节点 */
    RAFT_CONFIG_REMOVE_NODE,    /**< 移除节点 */
    RAFT_CONFIG_ADD_LEARNER,    /**< 添加学习者 */
    RAFT_CONFIG_PROMOTE_LEARNER,/**< 提升学习者 */
} RaftConfigChangeType;

/** 成员变更请求 */
typedef struct RaftConfigChange_s {
    RaftConfigChangeType type;   /**< 变更类型 */
    RaftNode node;               /**< 目标节点 */
    uint64_t changes_since;      /**< 变更后版本 */
} RaftConfigChange;

/**
 * @brief 添加节点
 */
int raft_add_node(RaftServer *server, const RaftNode *node);

/**
 * @brief 移除节点
 */
int raft_remove_node(RaftServer *server, uint64_t node_id);

/**
 * @brief 开始 Joint Consensus
 */
int raft_start_joint_consensus(RaftServer *server, RaftClusterConfig *new_config);

/**
 * @brief 提交 Joint 配置
 */
int raft_commit_joint_config(RaftServer *server);

/**
 * @brief 处理配置变更消息
 */
int raft_handle_configuration(RaftServer *server,
                              const RaftClusterConfig *config,
                              bool is_joint);

/* ========================================================================
 * 故障检测和自动切换
 * ======================================================================== */

/** 故障检测器 */
typedef struct RaftFailureDetector_s {
    uint64_t *last_heartbeat;   /**< 每个节点的最后心跳 */
    size_t num_nodes;           /**< 节点数量 */
    uint64_t heartbeat_timeout; /**< 心跳超时 */
    bool *is_alive;            /**< 节点存活状态 */
} RaftFailureDetector;

/**
 * @brief 创建故障检测器
 */
RaftFailureDetector *raft_failure_detector_create(size_t num_nodes,
                                                   uint64_t heartbeat_timeout);

/**
 * @brief 记录心跳
 */
void raft_failure_detector_record_heartbeat(RaftFailureDetector *detector, uint64_t node_id);

/**
 * @brief 检查节点是否存活
 */
bool raft_failure_detector_is_alive(RaftFailureDetector *detector, uint64_t node_id);

/**
 * @brief 获取可疑节点
 */
uint64_t *raft_failure_detector_get_suspected_nodes(RaftFailureDetector *detector,
                                                    size_t *out_count);

/**
 * @brief 销毁故障检测器
 */
void raft_failure_detector_destroy(RaftFailureDetector *detector);

/**
 * @brief 触发领导者选举
 */
int raft_trigger_leader_election(RaftServer *server);

/**
 * @brief 执行领导者转移
 */
int raft_transfer_leadership(RaftServer *server, uint64_t target_node_id);

/* ========================================================================
 * 线性一致性读
 * ======================================================================== */

/** ReadIndex 上下文 */
typedef struct RaftReadIndex_s {
    uint64_t read_index;       /**< Read 索引 */
    uint64_t heartbeat_rd_term; /**< 获取 ReadIndex 时的任期 */
    uint64_t client_id;        /**< 客户端 ID */
    bool confirmed;            /**< 是否已确认 */
} RaftReadIndex;

/** LeaseRead 上下文 */
typedef struct RaftLeaseRead_s {
    uint64_t lease_start;     /**< Lease 开始时间 */
    uint64_t lease_duration;  /**< Lease 持续时间 */
    uint64_t leader_epoch;    /**< Leader 任期 */
    bool valid;                /**< Lease 是否有效 */
} RaftLeaseRead;

/**
 * @brief 获取 ReadIndex
 */
uint64_t raft_get_read_index(RaftServer *server);

/**
 * @brief 确认 ReadIndex
 */
int raft_confirm_read_index(RaftServer *server, uint64_t read_index);

/**
 * @brief 检查是否可以使用 Lease Read
 */
bool raft_can_lease_read(RaftServer *server);

/**
 * @brief 执行 Lease Read
 */
int raft_lease_read(RaftServer *server, int (*callback)(void *, size_t), void *ctx);

/**
 * @brief 更新 Lease
 */
void raft_update_lease(RaftServer *server);

/**
 * @brief 验证领导者身份
 */
bool raft_verify_leader(RaftServer *server);

/* ========================================================================
 * 快照和恢复
 * ======================================================================== */

/** 快照元数据 */
typedef struct RaftSnapshot_s {
    uint64_t last_included_index;  /**< 快照包含的最后索引 */
    uint64_t last_included_term;  /**< 快照包含的最后任期 */
    RaftClusterConfig *config;   /**< 快照时的配置 */
    size_t snapshot_size;        /**< 快照大小 */
} RaftSnapshot;

/** 快照状态 */
typedef enum RaftSnapshotStatus_e {
    RAFT_SNAPSHOT_IDLE = 0,      /**< 空闲 */
    RAFT_SNAPSHOT_SENDING,       /**< 发送中 */
    RAFT_SNAPSHOT_RECEIVING,     /**< 接收中 */
    RAFT_SNAPSHOT_DONE,          /**< 完成 */
} RaftSnapshotStatus;

/**
 * @brief 创建快照
 */
int raft_create_snapshot(RaftServer *server,
                        uint64_t last_included_index,
                        uint64_t last_included_term,
                        void *snapshot_data,
                        size_t snapshot_size);

/**
 * @brief 恢复快照
 */
int raft_restore_snapshot(RaftServer *server,
                         uint64_t last_included_index,
                         uint64_t last_included_term,
                         const void *snapshot_data,
                         size_t snapshot_size);

/**
 * @brief 安装快照
 */
int raft_install_snapshot(RaftServer *server,
                          uint64_t last_included_index,
                          uint64_t last_included_term,
                          const void *snapshot_data,
                          size_t snapshot_size);

/**
 * @brief 发送快照给跟随者
 */
int raft_send_snapshot(RaftServer *server, uint64_t to_node);

/**
 * @brief 处理快照消息
 */
int raft_handle_install_snapshot(RaftServer *server,
                                 uint64_t leader_id,
                                 uint64_t last_included_index,
                                 uint64_t last_included_term,
                                 const void *snapshot_data,
                                 size_t snapshot_size);

/**
 * @brief 获取快照元数据
 */
const RaftSnapshot *raft_get_snapshot_metadata(RaftServer *server);

/**
 * @brief 检查是否需要发送快照
 */
bool raft_need_snapshot(RaftServer *server, uint64_t next_index);

/* ========================================================================
 * 定时器管理
 * ======================================================================== */

/**
 * @brief 运行选举超时检查
 */
void raft_run_election_timer(RaftServer *server);

/**
 * @brief 运行心跳
 */
void raft_run_heartbeat(RaftServer *server);

/**
 * @brief 运行快照检查
 */
void raft_run_snapshot_check(RaftServer *server);

/**
 * @brief 运行 Lease 更新
 */
void raft_run_lease_update(RaftServer *server);

/* ========================================================================
 * 统计和监控
 * ======================================================================== */

/** Raft 统计信息 */
typedef struct RaftStats_s {
    uint64_t current_term;     /**< 当前任期 */
    RaftRole role;              /**< 当前角色 */
    uint64_t commit_index;     /**< 提交索引 */
    uint64_t last_applied;     /**< 已应用索引 */
    uint64_t log_size;          /**< 日志大小 */
    uint64_t voted_count;       /**< 投票数 */
    uint64_t election_timeouts; /**< 选举超时次数 */
    uint64_t heartbeats_sent;    /**< 发送心跳数 */
    uint64_t snapshots_created; /**< 创建快照数 */
} RaftStats;

/**
 * @brief 获取统计信息
 */
void raft_get_stats(RaftServer *server, RaftStats *out_stats);

/**
 * @brief 打印调试信息
 */
void raft_print_status(RaftServer *server);

#ifdef __cplusplus
}
#endif

#endif /* DB_RAFT_H */
