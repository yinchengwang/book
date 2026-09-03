/**
 * @file dist_txn.h
 * @brief 分布式事务核心实现
 *
 * 实现 2PC/SAGA/TSO/分布式 MVCC/故障恢复
 */
#ifndef DB_DIST_TXN_H
#define DB_DIST_TXN_H

#include "shard.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 分布式事务状态
 * ======================================================================== */

/** 事务状态 */
typedef enum DistTxnState_e {
    DIST_TXN_ACTIVE = 0,      /**< 活跃 */
    DIST_TXN_PREPARING,        /**< 准备阶段 */
    DIST_TXN_PREPARED,         /**< 已准备 */
    DIST_TXN_COMMITTING,      /**< 提交中 */
    DIST_TXN_COMMITTED,       /**< 已提交 */
    DIST_TXN_ABORTING,         /**< 回滚中 */
    DIST_TXN_ABORTED,          /**< 已回滚 */
    DIST_TXN_UNKNOWN,          /**< 未知（协调者丢失） */
} DistTxnState;

/** 参与者状态 */
typedef enum ParticipantState_e {
    PARTICIPANT_INIT = 0,      /**< 初始 */
    PARTICIPANT_PREPARED,      /**< 已准备 */
    PARTICIPANT_COMMITTED,     /**< 已提交 */
    PARTICIPANT_ABORTED,       /**< 已回滚 */
    PARTICIPANT_UNKNOWN,       /**< 未知 */
} ParticipantState;

/* ========================================================================
 * 全局事务 ID
 * ======================================================================== */

/** 全局事务标识 */
typedef struct GlobalTxnId_s {
    uint64_t txn_id;           /**< 事务 ID */
    uint32_t coordinator_id;   /**< 协调者 ID */
    uint64_t start_time;       /**< 开始时间 */
    uint64_t commit_time;      /**< 提交时间 */
} GlobalTxnId;

/** 全局事务 ID 比较 */
static inline int global_txn_id_cmp(const GlobalTxnId *a, const GlobalTxnId *b) {
    if (a->txn_id < b->txn_id) return -1;
    if (a->txn_id > b->txn_id) return 1;
    if (a->coordinator_id < b->coordinator_id) return -1;
    if (a->coordinator_id > b->coordinator_id) return 1;
    return 0;
}

/* ========================================================================
 * 参与者信息
 * ======================================================================== */

/** 事务参与者 */
typedef struct TxnParticipant_s {
    char node_id[64];         /**< 节点 ID */
    char address[256];        /**< 网络地址 */
    int port;                  /**< 端口 */
    ParticipantState state;     /**< 参与者状态 */
    bool is_readonly;          /**< 是否只读 */

    /* 重试信息 */
    int retry_count;           /**< 重试次数 */
    uint64_t last_attempt;     /**< 上次尝试时间 */
} TxnParticipant;

/* ========================================================================
 * 分布式事务
 * ======================================================================== */

/** 分布式事务 */
typedef struct DistTxn_s {
    GlobalTxnId txn_id;        /**< 全局事务 ID */

    DistTxnState state;         /**< 事务状态 */

    /* 参与者列表 */
    TxnParticipant *participants;  /**< 参与者数组 */
    size_t num_participants;    /**< 参与者数量 */
    size_t participants_capacity;/**< 参与者容量 */

    /* 协调者信息 */
    char coordinator_id[64];   /**< 协调者 ID */
    bool is_coordinator;        /**< 是否为协调者 */

    /* 锁信息 */
    char *held_locks;           /**< 持有的锁列表 */
    size_t num_locks;           /**< 锁数量 */

    /* 快照信息 */
    uint64_t snapshot_time;    /**< 快照时间戳 */

    /* 统计 */
    uint64_t start_time;       /**< 开始时间 */
    uint64_t prepare_time;     /**< 准备完成时间 */
    uint64_t commit_time;      /**< 提交时间 */
    int num_retries;           /**< 重试次数 */
} DistTxn;

/* ========================================================================
 * 2PC 协调者
 * ======================================================================== */

/** 2PC 协调者 */
typedef struct TwoPCoordinator_s {
    char coordinator_id[64];    /**< 协调者 ID */
    ShardRouter *router;       /**< 分片路由 */

    /* 活跃事务 */
    DistTxn **active_txns;     /**< 活跃事务数组 */
    size_t num_active_txns;    /**< 活跃事务数量 */
    size_t active_capacity;     /**< 容量 */

    /* 持久化日志 */
    void *log_handle;          /**< 日志文件句柄 */

    /* 线程安全 */
    pthread_mutex_t lock;       /**< 互斥锁 */

    /* 超时配置 */
    uint64_t prepare_timeout_ms;   /**< Prepare 超时 */
    uint64_t commit_timeout_ms;     /**< Commit 超时 */
    uint64_t heartbeat_interval_ms; /**< 心跳间隔 */
} TwoPCoordinator;

/**
 * @brief 创建 2PC 协调者
 */
TwoPCoordinator *tpc_coordinator_create(const char *coordinator_id, ShardRouter *router);

/**
 * @brief 销毁 2PC 协调者
 */
void tpc_coordinator_destroy(TwoPCoordinator *coord);

/**
 * @brief 开始新事务
 */
DistTxn *tpc_begin(TwoPCoordinator *coord, uint64_t txn_id);

/**
 * @brief 添加参与者
 */
int tpc_add_participant(DistTxn *txn, const char *node_id, const char *address, int port);

/**
 * @brief Prepare 阶段
 */
int tpc_prepare(TwoPCoordinator *coord, DistTxn *txn);

/**
 * @brief Commit 阶段
 */
int tpc_commit(TwoPCoordinator *coord, DistTxn *txn);

/**
 * @brief Abort 阶段
 */
int tpc_abort(TwoPCoordinator *coord, DistTxn *txn);

/**
 * @brief 单阶段提交（简化路径）
 */
int tpc_single_phase_commit(TwoPCoordinator *coord, DistTxn *txn);

/**
 * @brief 获取事务状态
 */
DistTxnState tpc_get_txn_state(TwoPCoordinator *coord, const GlobalTxnId *txn_id);

/**
 * @brief 超时检测
 */
void tpc_check_timeouts(TwoPCoordinator *coord);

/**
 * @brief 恢复未决事务
 */
int tpc_recover_pending(TwoPCoordinator *coord);

/* ========================================================================
 * 2PC 参与者
 * ======================================================================== */

/** 2PC 参与者 */
typedef struct TwoPCParticipant_s {
    char node_id[64];          /**< 节点 ID */
    char coordinator_addr[256]; /**< 协调者地址 */
    int coordinator_port;       /**< 协调者端口 */

    /* 本地资源管理器 */
    void *resource_manager;    /**< 资源管理器 */

    /* Prepared 事务日志 */
    void *prepared_log;         /**< Prepared 日志 */

    /* 线程安全 */
    pthread_mutex_t lock;
} TwoPCParticipant;

/**
 * @brief 创建 2PC 参与者
 */
TwoPCParticipant *tpc_participant_create(const char *node_id);

/**
 * @brief 销毁 2PC 参与者
 */
void tpc_participant_destroy(TwoPCParticipant *part);

/**
 * @brief 投票 Prepare
 */
int tpc_vote_prepare(TwoPCParticipant *part, const GlobalTxnId *txn_id, bool vote_yes);

/**
 * @brief 执行本地提交
 */
int tpc_local_commit(TwoPCParticipant *part, const GlobalTxnId *txn_id);

/**
 * @brief 执行本地回滚
 */
int tpc_local_abort(TwoPCParticipant *part, const GlobalTxnId *txn_id);

/**
 * @brief 重新连接协调者
 */
int tpc_reconnect_coordinator(TwoPCParticipant *part);

/* ========================================================================
 * SAGA 模式
 * ======================================================================== */

/** SAGA 执行方向 */
typedef enum SagaDirection_e {
    SAGA_FORWARD = 0,          /**< 正向执行 */
    SAGA_BACKWARD,             /**< 回滚执行 */
} SagaDirection;

/** SAGA 步骤 */
typedef struct SagaStep_s {
    char step_name[64];        /**< 步骤名称 */
    char service_name[64];     /**< 服务名称 */
    int (*forward_fn)(void *ctx);  /**< 正向函数 */
    int (*compensate_fn)(void *ctx); /**< 补偿函数 */
    void *context;              /**< 上下文 */
    bool completed;             /**< 是否完成 */
    bool compensated;           /**< 是否已补偿 */
} SagaStep;

/** SAGA 编排器 */
typedef struct SagaOrchestrator_s {
    char saga_id[64];          /**< SAGA ID */
    SagaStep *steps;           /**< 步骤数组 */
    size_t num_steps;           /**< 步骤数量 */
    size_t current_step;        /**< 当前步骤索引 */
    SagaDirection direction;    /**< 执行方向 */

    /* 状态 */
    bool in_progress;          /**< 是否进行中 */
    size_t completed_steps;     /**< 已完成步骤数 */
    size_t compensated_steps;   /**< 已补偿步骤数 */
} SagaOrchestrator;

/**
 * @brief 创建 SAGA 编排器
 */
SagaOrchestrator *saga_create(const char *saga_id, size_t max_steps);

/**
 * @brief 添加 SAGA 步骤
 */
int saga_add_step(SagaOrchestrator *saga,
                  const char *step_name,
                  const char *service_name,
                  int (*forward_fn)(void *),
                  int (*compensate_fn)(void *));

/**
 * @brief 执行 SAGA
 */
int saga_execute(SagaOrchestrator *saga, void *initial_ctx);

/**
 * @brief 回滚 SAGA
 */
int saga_rollback(SagaOrchestrator *saga);

/**
 * @brief 获取 SAGA 状态
 */
const char *saga_get_status(const SagaOrchestrator *saga);

/**
 * @brief 销毁 SAGA 编排器
 */
void saga_destroy(SagaOrchestrator *saga);

/* ========================================================================
 * TSO (Timestamp Oracle)
 * ======================================================================== */

/** TSO 配置 */
typedef struct TSOConfig_s {
    uint32_t node_id;          /**< 节点 ID */
    uint64_t epoch;            /**< Epoch */
    uint64_t physical_time;    /**< 物理时间 */
    uint32_t logical_counter;  /**< 逻辑计数器 */
    uint64_t max_logical;      /**< 最大逻辑时间 */
} TSOConfig;

/** TSO 客户端 */
typedef struct TSOClient_s {
    char server_addr[256];     /**< TSO 服务器地址 */
    int server_port;           /**< TSO 服务器端口 */
    uint64_t last_timestamp;  /**< 上次获取的时间戳 */
    uint64_t last_epoch;        /**< 上次获取的 epoch */
    int connection_fd;         /**< 连接描述符 */
} TSOClient;

/**
 * @brief 创建 TSO 客户端
 */
TSOClient *tso_client_create(const char *server_addr, int server_port);

/**
 * @brief 获取全局时间戳
 */
uint64_t tso_get_timestamp(TSOClient *client);

/**
 * @brief 获取批量时间戳
 */
int tso_get_batch(TSOClient *client, uint64_t *timestamps, size_t count);

/**
 * @brief 关闭 TSO 客户端
 */
void tso_client_destroy(TSOClient *client);

/**
 * @brief 本地 TSO 生成（单节点）
 */
uint64_t tso_generate_local(TSOConfig *config);

/**
 * @brief 同步 TSO 配置
 */
int tso_sync(TSOConfig *config, const char *server_addr, int server_port);

/* ========================================================================
 * 分布式 MVCC
 * ======================================================================== */

/** 分布式 ReadView */
typedef struct DistReadView_s {
    uint64_t snapshot_time;   /**< 快照时间 */
    uint64_t min_active_txn;  /**< 最小活跃事务 */
    uint64_t max_active_txn;  /**< 最大活跃事务 */

    /* 活跃事务列表 */
    GlobalTxnId *active_txns;  /**< 活跃事务数组 */
    size_t num_active;         /**< 活跃事务数量 */

    /* 元数据 */
    uint32_t origin_node;     /**< 来源节点 */
    uint64_t created_at;      /**< 创建时间 */
} DistReadView;

/** 分布式版本链 */
typedef struct DistVersionChain_s {
    uint64_t xmin;             /**< 创建事务 */
    uint64_t xmax;             /**< 删除事务 */
    uint64_t ctid;             /**< 行指针 */
    uint32_t origin_node;      /**< 来源节点 */
    bool is_locked;            /**< 是否加锁 */
} DistVersionChain;

/**
 * @brief 创建分布式 ReadView
 */
DistReadView *dist_readview_create(uint64_t snapshot_time,
                                   const GlobalTxnId *active_txns,
                                   size_t num_active,
                                   uint32_t origin_node);

/**
 * @brief 检查版本可见性
 */
bool dist_version_visible(const DistReadView *view, const DistVersionChain *version);

/**
 * @brief 获取全局可见版本
 */
const DistVersionChain *dist_get_visible_version(DistReadView *view,
                                                   uint64_t row_id,
                                                   uint32_t shard_id);

/**
 * @brief 销毁 ReadView
 */
void dist_readview_destroy(DistReadView *view);

/* ========================================================================
 * 分布式快照
 * ======================================================================== */

/** 分布式快照 */
typedef struct DistSnapshot_s {
    uint64_t snapshot_id;     /**< 快照 ID */
    uint64_t snapshot_time;   /**< 快照时间 */
    uint64_t local_time;      /**< 本地时间 */

    /* 全局活跃事务 */
    GlobalTxnId *global_active; /**< 全局活跃事务 */
    size_t num_global_active;   /**< 全局活跃事务数 */

    /* 来源节点 */
    uint32_t origin_node;     /**< 快照来源节点 */

    /* 有效期 */
    uint64_t valid_until;     /**< 有效期 */
} DistSnapshot;

/**
 * @brief 创建分布式快照
 */
DistSnapshot *dist_snapshot_create(TSOClient *tso,
                                   const GlobalTxnId *active_txns,
                                   size_t num_active,
                                   uint32_t origin_node);

/**
 * @brief 验证快照有效性
 */
bool dist_snapshot_valid(const DistSnapshot *snapshot);

/**
 * @brief 获取快照时间
 */
uint64_t dist_snapshot_get_time(const DistSnapshot *snapshot);

/**
 * @brief 销毁快照
 */
void dist_snapshot_destroy(DistSnapshot *snapshot);

/* ========================================================================
 * 故障恢复
 * ======================================================================== */

/** 恢复状态 */
typedef enum RecoveryState_e {
    RECOVERY_INIT = 0,         /**< 初始化 */
    RECOVERY_SCAN_LOG,         /**< 扫描日志 */
    RECOVERY_REBUILD_STATE,    /**< 重建状态 */
    RECOVERY_REPLAY_PREPARED,  /**< 重放 Prepared */
    RECOVERY_COMPLETE,         /**< 完成 */
} RecoveryState;

/** 恢复报告 */
typedef struct RecoveryReport_s {
    RecoveryState state;        /**< 恢复状态 */
    size_t total_prepared;     /**< 总 Prepared 数 */
    size_t committed_count;     /**< 已提交数 */
    size_t aborted_count;      /**< 已回滚数 */
    size_t unknown_count;       /**< 未知数 */
    uint64_t start_time;       /**< 开始时间 */
    uint64_t end_time;         /**< 结束时间 */
} RecoveryReport;

/**
 * @brief 执行协调者恢复
 */
RecoveryReport *dist_recover_coordinator(TwoPCoordinator *coord);

/**
 * @brief 执行参与者恢复
 */
RecoveryReport *dist_recover_participant(TwoPCParticipant *part);

/**
 * @brief 处理未知状态事务
 */
int dist_resolve_unknown_txn(TwoPCCoordinator *coord, const GlobalTxnId *txn_id);

/**
 * @brief 扫描 Prepared 日志
 */
GlobalTxnId *dist_scan_prepared_log(void *log, size_t *out_count);

/**
 * @brief 清理过期数据
 */
int dist_cleanup_old_data(DistTxn *txn, uint64_t before_time);

/* ========================================================================
 * 事务管理器
 * ======================================================================== */

/** 分布式事务管理器 */
typedef struct DistTxnManager_s {
    TwoPCoordinator *coordinator;  /**< 协调者 */
    TwoPCParticipant *participant; /**< 参与者 */
    TSOClient *tso_client;         /**< TSO 客户端 */

    /* 配置 */
    bool is_single_node;          /**< 单节点模式 */
    uint64_t txn_timeout_ms;      /**< 事务超时 */
    uint64_t lock_timeout_ms;     /**< 锁超时 */

    /* 统计 */
    size_t total_txns;            /**< 总事务数 */
    size_t committed_txns;        /**< 已提交数 */
    size_t aborted_txns;          /**< 已回滚数 */
    size_t active_txns;           /**< 活跃事务数 */
} DistTxnManager;

/**
 * @brief 创建事务管理器
 */
DistTxnManager *dist_txn_manager_create(const char *node_id, bool is_coordinator);

/**
 * @brief 初始化事务管理器
 */
int dist_txn_manager_init(DistTxnManager *mgr,
                         const char *tso_addr,
                         int tso_port,
                         ShardRouter *router);

/**
 * @brief 开始分布式事务
 */
DistTxn *dist_txn_begin(DistTxnManager *mgr);

/**
 * @brief 提交事务
 */
int dist_txn_commit(DistTxn *txn);

/**
 * @brief 回滚事务
 */
int dist_txn_rollback(DistTxn *txn);

/**
 * @brief 销毁事务管理器
 */
void dist_txn_manager_destroy(DistTxnManager *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_DIST_TXN_H */
