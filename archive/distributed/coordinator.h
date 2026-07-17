/**
 * @file coordinator.h
 * @brief 多节点协调核心实现
 *
 * 实现节点注册发现、全局锁服务、领导者选举、配置管理、集群扩缩容
 */
#ifndef DB_COORDINATOR_H
#define DB_COORDINATOR_H

#include "raft.h"
#include "shard.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 节点状态
 * ======================================================================== */

/** 节点角色 */
typedef enum CoordinatorRole_e {
    COORD_ROLE_MEMBER = 0,      /**< 普通成员 */
    COORD_ROLE_COORDINATOR,      /**< 协调者 */
    COORD_ROLE_LEARNER,          /**< 学习者 */
} CoordinatorRole;

/** 节点健康状态 */
typedef enum NodeHealth_e {
    NODE_HEALTH_UNKNOWN = 0,    /**< 未知 */
    NODE_HEALTH_HEALTHY,        /**< 健康 */
    NODE_HEALTH_DEGRADED,       /**< 降级 */
    NODE_HEALTH_UNHEALTHY,     /**< 不健康 */
} NodeHealth;

/* ========================================================================
 * 节点注册与发现
 * ======================================================================== */

/** 节点元数据 */
typedef struct NodeMetadata_s {
    char node_id[64];           /**< 节点 ID */
    char node_name[128];       /**< 节点名称 */
    char host[256];            /**< 主机地址 */
    int port;                   /**< 端口 */
    int rpc_port;               /**< RPC 端口 */

    /* 角色和能力 */
    CoordinatorRole role;        /**< 节点角色 */
    char capabilities[256];    /**< 节点能力 */

    /* 资源信息 */
    size_t cpu_cores;           /**< CPU 核心数 */
    size_t memory_total;        /**< 总内存 */
    size_t memory_available;    /**< 可用内存 */
    size_t disk_total;          /**< 总磁盘空间 */
    size_t disk_available;      /**< 可用磁盘空间 */

    /* 负载信息 */
    double cpu_usage;           /**< CPU 使用率 */
    double memory_usage;        /**< 内存使用率 */
    double disk_usage;          /**< 磁盘使用率 */
    double load_avg;            /**< 负载均值 */

    /* 集群信息 */
    char cluster_id[64];       /**< 集群 ID */
    size_t num_shards;          /**< 分片数量 */
    size_t num_connections;     /**< 连接数 */
    uint64_t uptime;           /**< 运行时间 */
} NodeMetadata;

/** 节点注册信息 */
typedef struct NodeRegistration_s {
    NodeMetadata metadata;       /**< 节点元数据 */
    uint64_t registered_at;    /**< 注册时间 */
    uint64_t last_seen;        /**< 最后活跃时间 */
    NodeHealth health;          /**< 健康状态 */
    int lease_ttl;              /**< 租约 TTL（秒） */
    bool is_active;            /**< 是否活跃 */
} NodeRegistration;

/** 节点注册表 */
typedef struct NodeRegistry_s {
    NodeRegistration *nodes;   /**< 节点数组 */
    size_t num_nodes;           /**< 节点数量 */
    size_t capacity;             /**< 容量 */

    /* 索引 */
    void *node_index;           /**< 节点 ID 索引（哈希表） */

    pthread_mutex_t lock;       /**< 互斥锁 */

    /* 集群信息 */
    char cluster_id[64];       /**< 集群 ID */
    uint64_t cluster_version;   /**< 集群版本 */
} NodeRegistry;

/**
 * @brief 创建节点注册表
 */
NodeRegistry *node_registry_create(const char *cluster_id);

/**
 * @brief 销毁节点注册表
 */
void node_registry_destroy(NodeRegistry *registry);

/**
 * @brief 注册节点
 */
int node_registry_register(NodeRegistry *registry, const NodeMetadata *metadata);

/**
 * @brief 注销节点
 */
int node_registry_unregister(NodeRegistry *registry, const char *node_id);

/**
 * @brief 更新节点心跳
 */
int node_registry_heartbeat(NodeRegistry *registry, const char *node_id);

/**
 * @brief 获取节点信息
 */
const NodeRegistration *node_registry_get(NodeRegistry *registry, const char *node_id);

/**
 * @brief 获取所有活跃节点
 */
const NodeRegistration **node_registry_get_active(NodeRegistry *registry, size_t *out_count);

/**
 * @brief 更新节点负载信息
 */
int node_registry_update_load(NodeRegistry *registry,
                             const char *node_id,
                             const NodeMetadata *load_info);

/**
 * @brief 清理过期节点
 */
int node_registry_cleanup(NodeRegistry *registry);

/* ========================================================================
 * 全局锁服务
 * ======================================================================== */

/** 锁模式 */
typedef enum LockMode_e {
    LOCK_MODE_SHARED = 0,      /**< 共享锁（读锁） */
    LOCK_MODE_EXCLUSIVE,        /**< 排他锁（写锁） */
} LockMode;

/** 锁持有者 */
typedef struct LockHolder_s {
    char holder_id[64];        /**< 持有者 ID */
    uint64_t txn_id;           /**< 事务 ID */
    uint64_t acquired_at;       /**< 获取时间 */
    uint64_t expires_at;       /**< 过期时间 */
    LockMode mode;              /**< 锁模式 */
} LockHolder;

/** 锁信息 */
typedef struct LockInfo_s {
    char lock_key[256];        /**< 锁键 */
    LockMode mode;              /**< 锁模式 */

    /* 持有者信息 */
    LockHolder *holders;       /**< 持有者数组 */
    size_t num_holders;         /**< 持有者数量 */

    /* 等待队列 */
    struct LockWaiter_s *wait_queue; /**< 等待队列 */
    size_t wait_count;          /**< 等待者数量 */

    /* 统计 */
    uint64_t created_at;       /**< 创建时间 */
    uint64_t num_acquires;     /**< 获取次数 */
    uint64_t num_contention;    /**< 冲突次数 */
} LockInfo;

/** 全局锁服务 */
typedef struct GlobalLockService_s {
    char service_id[64];       /**< 服务 ID */

    /* 锁存储 */
    LockInfo **locks;          /**< 锁数组 */
    size_t num_locks;           /**< 锁数量 */
    size_t locks_capacity;       /**< 容量 */

    /* 索引 */
    void *lock_index;           /**< 锁索引（哈希表） */

    /* Raft 集成 */
    RaftServer *raft;          /**< Raft 服务器 */

    /* 线程安全 */
    pthread_mutex_t lock;       /**< 互斥锁 */

    /* 配置 */
    uint64_t default_ttl_ms;    /**< 默认 TTL */
    uint64_t max_lock_wait_ms;  /**< 最大等待时间 */

    /* 统计 */
    uint64_t total_acquires;    /**< 总获取次数 */
    uint64_t total_releases;    /**< 总释放次数 */
} GlobalLockService;

/**
 * @brief 创建全局锁服务
 */
GlobalLockService *global_lock_service_create(const char *service_id, RaftServer *raft);

/**
 * @brief 销毁全局锁服务
 */
void global_lock_service_destroy(GlobalLockService *service);

/**
 * @brief 获取锁（阻塞）
 */
int global_lock_acquire(GlobalLockService *service,
                        const char *lock_key,
                        const char *holder_id,
                        uint64_t txn_id,
                        LockMode mode,
                        uint64_t timeout_ms);

/**
 * @brief 尝试获取锁（非阻塞）
 */
bool global_lock_try_acquire(GlobalLockService *service,
                             const char *lock_key,
                             const char *holder_id,
                             uint64_t txn_id,
                             LockMode mode);

/**
 * @brief 释放锁
 */
int global_lock_release(GlobalLockService *service,
                        const char *lock_key,
                        const char *holder_id);

/**
 * @brief 续约锁
 */
int global_lock_renew(GlobalLockService *service,
                      const char *lock_key,
                      const char *holder_id,
                      uint64_t new_ttl_ms);

/**
 * @brief 获取锁信息
 */
const LockInfo *global_lock_get_info(GlobalLockService *service, const char *lock_key);

/**
 * @brief 检查锁是否被持有
 */
bool global_lock_is_held(GlobalLockService *service, const char *lock_key);

/**
 * @brief 获取锁模式
 */
LockMode global_lock_get_mode(GlobalLockService *service, const char *lock_key);

/**
 * @brief 列出所有锁
 */
const LockInfo **global_lock_list(GlobalLockService *service, size_t *out_count);

/**
 * @brief 清理过期锁
 */
int global_lock_cleanup_expired(GlobalLockService *service);

/* ========================================================================
 * 领导者选举
 * ======================================================================== */

/** 选举策略 */
typedef enum ElectionStrategy_e {
    ELECTION_FIRST_LEARNER = 0, /**< 优先学习者 */
    ELECTION_LOWEST_ID,         /**< 最低 ID */
    ELECTION_HIGHEST_PRIORITY,  /**< 最高优先级 */
    ELECTION_LOAD_BALANCED,     /**< 负载均衡 */
} ElectionStrategy;

/** 选举配置 */
typedef struct ElectionConfig_s {
    ElectionStrategy strategy;   /**< 选举策略 */
    uint64_t election_timeout;  /**< 选举超时 */
    uint64_t leader_lease_ms;   /**< Leader 租约 */
    int min_voters;             /**< 最少投票者 */
    bool allow_leaderless;      /**< 允许无 Leader */
} ElectionConfig;

/** 选举结果 */
typedef struct ElectionResult_s {
    bool elected;               /**< 是否当选 */
    char leader_id[64];        /**< Leader ID */
    uint64_t term;             /**< 任期 */
    int votes_received;         /**< 获得的票数 */
    uint64_t election_time_ms;  /**< 选举耗时 */
} ElectionResult;

/**
 * @brief 执行领导者选举
 */
ElectionResult *execute_election(NodeRegistry *registry,
                                 const ElectionConfig *config,
                                 RaftServer *raft);

/**
 * @brief 检查领导者是否有效
 */
bool is_leader_valid(NodeRegistry *registry, const char *leader_id);

/**
 * @brief 转移领导者
 */
int transfer_leadership(NodeRegistry *registry,
                       const char *from_id,
                       const char *to_id);

/**
 * @brief 获取当前领导者
 */
const NodeRegistration *get_current_leader(NodeRegistry *registry);

/**
 * @brief 设置选举优先级
 */
int set_election_priority(NodeRegistry *registry,
                         const char *node_id,
                         int priority);

/**
 * @brief 强制重新选举
 */
int force_reelection(NodeRegistry *registry, const ElectionConfig *config);

/* ========================================================================
 * 配置管理
 * ======================================================================== */

/** 配置项 */
typedef struct ConfigItem_s {
    char key[128];             /**< 配置键 */
    char value[512];           /**< 配置值 */
    char value_type[32];       /**< 值类型 */
    uint64_t version;           /**< 版本号 */
    uint64_t updated_at;        /**< 更新时间 */
    char updated_by[64];       /**< 更新者 */
} ConfigItem;

/** 配置变更监听器 */
typedef void (*ConfigChangeListener)(const char *key, const char *old_value, const char *new_value);

/** 配置管理服务 */
typedef struct ConfigManager_s {
    char cluster_id[64];       /**< 集群 ID */

    /* 配置存储 */
    ConfigItem *items;         /**< 配置项数组 */
    size_t num_items;           /**< 配置项数量 */
    size_t items_capacity;      /**< 容量 */

    /* 版本控制 */
    uint64_t config_version;    /**< 配置版本 */
    void *config_history;       /**< 配置历史 */

    /* Raft 集成 */
    RaftServer *raft;          /**< Raft 服务器 */

    /* 监听器 */
    ConfigChangeListener *listeners; /**< 监听器数组 */
    size_t num_listeners;        /**< 监听器数量 */

    /* 线程安全 */
    pthread_mutex_t lock;       /**< 互斥锁 */
} ConfigManager;

/**
 * @brief 创建配置管理器
 */
ConfigManager *config_manager_create(const char *cluster_id, RaftServer *raft);

/**
 * @brief 销毁配置管理器
 */
void config_manager_destroy(ConfigManager *mgr);

/**
 * @brief 获取配置
 */
const char *config_get(ConfigManager *mgr, const char *key, const char *default_value);

/**
 * @brief 设置配置
 */
int config_set(ConfigManager *mgr, const char *key, const char *value, const char *updater);

/**
 * @brief 删除配置
 */
int config_delete(ConfigManager *mgr, const char *key);

/**
 * @brief 列出所有配置
 */
const ConfigItem **config_list(ConfigManager *mgr, size_t *out_count);

/**
 * @brief 批量更新配置
 */
int config_batch_update(ConfigManager *mgr,
                        const char **keys,
                        const char **values,
                        size_t count,
                        const char *updater);

/**
 * @brief 订阅配置变更
 */
int config_subscribe(ConfigManager *mgr, ConfigChangeListener listener);

/**
 * @brief 获取配置版本
 */
uint64_t config_get_version(ConfigManager *mgr);

/**
 * @brief 验证配置
 */
bool config_validate(const ConfigItem *item);

/**
 * @brief 回滚配置
 */
int config_rollback(ConfigManager *mgr, uint64_t target_version);

/* ========================================================================
 * 集群扩缩容
 * ======================================================================== */

/** 扩缩容状态 */
typedef enum RescaleState_e {
    RESCALE_PLANNING = 0,      /**< 规划中 */
    RESCALE_PREPARING,         /**< 准备中 */
    RESCALE_MIGRATING_DATA,     /**< 数据迁移中 */
    RESCALE_UPDATING_CONFIG,   /**< 更新配置 */
    RESCALE_VERIFYING,         /**< 验证中 */
    RESCALE_COMPLETE,          /**< 完成 */
    RESCALE_FAILED,             /**< 失败 */
} RescaleState;

/** 扩缩容任务 */
typedef struct RescaleTask_s {
    char task_id[64];          /**< 任务 ID */
    RescaleState state;         /**< 当前状态 */

    /* 操作类型 */
    bool is_expansion;         /**< 是否扩容 */
    size_t target_node_count;   /**< 目标节点数 */
    size_t source_node_count;   /**< 源节点数 */

    /* 迁移信息 */
    uint32_t *shards_to_move;  /**< 待迁移分片 */
    size_t num_shards_to_move;  /**< 待迁移分片数 */

    /* 进度 */
    size_t shards_migrated;     /**< 已迁移分片 */
    size_t shards_total;        /**< 总分片数 */
    double progress;             /**< 进度百分比 */

    /* 时间 */
    uint64_t start_time;       /**< 开始时间 */
    uint64_t estimated_end;    /**< 预计结束 */
    uint64_t actual_end;       /**< 实际结束 */

    /* 错误信息 */
    char error[512];           /**< 错误信息 */
} RescaleTask;

/** 扩缩容策略 */
typedef enum RescaleStrategy_e {
    RESCALE_MANUAL = 0,        /**< 手动扩缩容 */
    RESCALE_AUTO_LOAD,          /**< 按负载自动 */
    RESCALE_AUTO_CAPACITY,     /**< 按容量自动 */
    RESCALE_AUTO_PERFORMANCE,  /**< 按性能自动 */
} RescaleStrategy;

/** 扩缩容策略配置 */
typedef struct RescalePolicy_s {
    RescaleStrategy strategy;   /**< 策略类型 */

    /* 阈值配置 */
    double max_cpu_usage;        /**< 最大 CPU 使用率 */
    double max_memory_usage;     /**< 最大内存使用率 */
    double min_node_utilization; /**< 最小节点利用率 */
    double max_node_utilization; /**< 最大节点利用率 */

    /* 行为配置 */
    size_t min_nodes;            /**< 最小节点数 */
    size_t max_nodes;           /**< 最大节点数 */
    size_t scale_step;          /**< 扩缩容步长 */
    uint64_t scale_cooldown_ms;  /**< 扩缩容冷却时间 */
} RescalePolicy;

/**
 * @brief 创建扩缩容任务
 */
RescaleTask *rescale_task_create(const char *task_id, bool is_expansion, size_t target_count);

/**
 * @brief 更新扩缩容进度
 */
void rescale_task_update_progress(RescaleTask *task, size_t migrated, size_t total);

/**
 * @brief 标记扩缩容完成
 */
void rescale_task_complete(RescaleTask *task);

/**
 * @brief 标记扩缩容失败
 */
void rescale_task_fail(RescaleTask *task, const char *error);

/**
 * @brief 销毁扩缩容任务
 */
void rescale_task_destroy(RescaleTask *task);

/**
 * @brief 执行扩容
 */
RescaleTask *cluster_expand(NodeRegistry *registry,
                           RaftRouter *router,
                           size_t target_nodes,
                           const RescalePolicy *policy);

/**
 * @brief 执行缩容
 */
RescaleTask *cluster_shrink(NodeRegistry *registry,
                           RaftRouter *router,
                           size_t target_nodes,
                           const RescalePolicy *policy);

/**
 * @brief 添加节点到集群
 */
int cluster_add_node(NodeRegistry *registry,
                     RaftServer *raft,
                     const NodeMetadata *node_info);

/**
 * @brief 从集群移除节点
 */
int cluster_remove_node(NodeRegistry *registry,
                        RaftServer *raft,
                        const char *node_id);

/**
 * @brief 重新均衡集群
 */
int cluster_rebalance(NodeRegistry *registry,
                     RaftRouter *router,
                     const RescalePolicy *policy);

/**
 * @brief 检查是否需要扩缩容
 */
bool cluster_needs_rescale(NodeRegistry *registry, const RescalePolicy *policy);

/**
 * @brief 获取扩缩容建议
 */
void cluster_get_rescale_suggestion(NodeRegistry *registry,
                                    const RescalePolicy *policy,
                                    bool *out_need_scale,
                                    size_t *out_suggested_nodes);

/**
 * @brief 取消扩缩容任务
 */
int rescale_task_cancel(RescaleTask *task);

/* ========================================================================
 * 服务发现
 * ======================================================================== */

/** 服务端点 */
typedef struct ServiceEndpoint_s {
    char service_name[64];     /**< 服务名称 */
    char node_id[64];         /**< 节点 ID */
    char host[256];           /**< 主机 */
    int port;                  /**< 端口 */
    bool is_healthy;           /**< 是否健康 */
    double weight;             /**< 权重 */
} ServiceEndpoint;

/** 服务发现 */
typedef struct ServiceDiscovery_s {
    ServiceEndpoint *endpoints; /**< 端点数组 */
    size_t num_endpoints;       /**< 端点数量 */
    pthread_mutex_t lock;        /**< 互斥锁 */
} ServiceDiscovery;

/**
 * @brief 创建服务发现
 */
ServiceDiscovery *service_discovery_create(void);

/**
 * @brief 销毁服务发现
 */
void service_discovery_destroy(ServiceDiscovery *sd);

/**
 * @brief 注册服务端点
 */
int service_discovery_register(ServiceDiscovery *sd, const ServiceEndpoint *endpoint);

/**
 * @brief 注销服务端点
 */
int service_discovery_unregister(ServiceDiscovery *sd, const char *service_name, const char *node_id);

/**
 * @brief 发现服务
 */
const ServiceEndpoint *service_discovery_find(ServiceDiscovery *sd,
                                             const char *service_name,
                                             size_t *out_count);

/**
 * @brief 查找最近的节点
 */
const ServiceEndpoint *service_discovery_find_nearest(ServiceDiscovery *sd,
                                                     const char *service_name,
                                                     const char *from_node_id);

/**
 * @brief 健康检查
 */
int service_discovery_health_check(ServiceDiscovery *sd);

#ifdef __cplusplus
}
#endif

#endif /* DB_COORDINATOR_H */
