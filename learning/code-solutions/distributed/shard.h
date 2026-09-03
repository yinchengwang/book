/**
 * @file shard.h
 * @brief 分片与路由核心数据结构
 *
 * 实现 Hash 分片、Range 分片、分片路由、拓扑管理、动态扩缩容
 */
#ifndef DB_SHARD_H
#define DB_SHARD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 分片策略类型
 * ======================================================================== */

/** 分片策略 */
typedef enum ShardStrategy_e {
    SHARD_HASH = 0,          /**< Hash 分片 */
    SHARD_RANGE,              /**< Range 分片 */
    SHARD_LIST,               /**< List 分片 */
    SHARD_MOD,                /**< 取模分片 */
} ShardStrategy;

/** 分片状态 */
typedef enum ShardState_e {
    SHARD_STATE_ACTIVE = 0,   /**< 活跃 */
    SHARD_STATE_MIGRATING,    /**< 迁移中 */
    SHARD_STATE_READONLY,     /**< 只读 */
    SHARD_STATE_OFFLINE,      /**< 离线 */
} ShardState;

/* ========================================================================
 * 分片元数据
 * ======================================================================== */

/** 分片信息 */
typedef struct ShardInfo_s {
    uint32_t shard_id;         /**< 分片 ID */
    ShardStrategy strategy;    /**< 分片策略 */
    ShardState state;         /**< 分片状态 */

    /* 节点信息 */
    char primary_node[64];    /**< 主节点 */
    char *replica_nodes;       /**< 副本节点列表 */
    size_t replica_count;     /**< 副本数量 */

    /* 分片范围（用于 Range/List 分片） */
    int64_t range_start;      /**< 范围起始 */
    int64_t range_end;        /**< 范围结束 */
    char range_key[64];       /**< 分片键 */

    /* 统计信息 */
    size_t num_tuples;        /**< 元组数量 */
    size_t data_size;         /**< 数据大小 */
    double load_factor;       /**< 负载因子 */

    /* 元数据 */
    uint64_t version;         /**< 版本号 */
    uint64_t created_at;      /**< 创建时间 */
    uint64_t updated_at;      /**< 更新时间 */
} ShardInfo;

/** 分片路由信息 */
typedef struct ShardRouting_s {
    uint32_t shard_id;        /**< 目标分片 ID */
    char node_addr[64];       /**< 目标节点地址 */
    int node_port;           /**< 目标节点端口 */
    bool is_local;            /**< 是否本地分片 */
} ShardRouting;

/** 虚拟节点（用于一致性 Hash） */
typedef struct VirtualNode_s {
    uint32_t vnode_id;        /**< 虚拟节点 ID */
    uint32_t shard_id;        /**< 所属物理分片 */
    uint64_t hash;            /**< Hash 值 */
} VirtualNode;

/* ========================================================================
 * 分片拓扑
 * ======================================================================== */

/** 节点信息 */
typedef struct ShardNode_s {
    char node_id[64];         /**< 节点 ID */
    char host[256];           /**< 主机地址 */
    int port;                 /**< 端口 */
    bool is_primary;          /**< 是否主节点 */
    bool is_healthy;          /**< 是否健康 */
    double weight;            /**< 权重 */

    /* 负载信息 */
    size_t shard_count;        /**< 分片数量 */
    size_t total_data_size;   /**< 总数据大小 */
    double cpu_usage;          /**< CPU 使用率 */
    double memory_usage;       /**< 内存使用率 */

    uint64_t last_heartbeat;  /**< 最后心跳时间 */
} ShardNode;

/** 分片拓扑 */
typedef struct ShardTopology_s {
    ShardNode *nodes;         /**< 节点数组 */
    size_t num_nodes;         /**< 节点数量 */
    size_t capacity;          /**< 容量 */

    ShardInfo *shards;        /**< 分片数组 */
    size_t num_shards;        /**< 分片数量 */

    VirtualNode *vnodes;      /**< 虚拟节点数组（一致性 Hash） */
    size_t num_vnodes;         /**< 虚拟节点数量 */
    size_t vnode_capacity;     /**< 虚拟节点容量 */

    uint64_t version;         /**< 拓扑版本 */
} ShardTopology;

/* ========================================================================
 * 分片键计算
 * ======================================================================== */

/** Hash 函数类型 */
typedef uint64_t (*HashFunc)(const void *key, size_t len);

/**
 * @brief 计算分片键 Hash 值
 * @param key 分片键
 * @param len 键长度
 * @param num_shards 分片数量
 * @return 分片 ID
 */
uint32_t shard_calc_hash(const void *key, size_t len, uint32_t num_shards);

/**
 * @brief 使用默认 Hash 函数计算
 */
uint32_t shard_default_hash(const char *key);

/**
 * @brief MurmurHash3 实现
 */
uint64_t murmurhash3(const void *key, size_t len, uint32_t seed);

/* ========================================================================
 * 分片管理
 * ======================================================================== */

/**
 * @brief 创建分片拓扑
 */
ShardTopology *shard_topology_create(void);

/**
 * @brief 销毁分片拓扑
 */
void shard_topology_destroy(ShardTopology *topo);

/**
 * @brief 添加节点到拓扑
 */
int shard_topology_add_node(ShardTopology *topo, const ShardNode *node);

/**
 * @brief 从拓扑移除节点
 */
int shard_topology_remove_node(ShardTopology *topo, const char *node_id);

/**
 * @brief 添加分片到拓扑
 */
int shard_topology_add_shard(ShardTopology *topo, const ShardInfo *shard);

/**
 * @brief 移除分片
 */
int shard_topology_remove_shard(ShardTopology *topo, uint32_t shard_id);

/**
 * @brief 获取节点
 */
const ShardNode *shard_topology_get_node(const ShardTopology *topo, const char *node_id);

/**
 * @brief 获取分片
 */
const ShardInfo *shard_topology_get_shard(const ShardTopology *topo, uint32_t shard_id);

/**
 * @brief 获取活跃节点列表
 */
const ShardNode **shard_topology_get_active_nodes(const ShardTopology *topo, size_t *out_count);

/* ========================================================================
 * 路由计算
 * ======================================================================== */

/**
 * @brief 创建路由计算器
 */
typedef struct ShardRouter_s {
    ShardTopology *topo;
    ShardStrategy default_strategy;
    uint32_t num_shards;
    uint64_t mur_hash_seed;
} ShardRouter;

/**
 * @brief 创建路由实例
 */
ShardRouter *shard_router_create(ShardTopology *topo, ShardStrategy strategy, uint32_t num_shards);

/**
 * @brief 销毁路由实例
 */
void shard_router_destroy(ShardRouter *router);

/**
 * @brief 设置默认分片策略
 */
void shard_router_set_strategy(ShardRouter *router, ShardStrategy strategy);

/**
 * @brief 路由分片键到分片
 * @param router 路由实例
 * @param key 分片键
 * @param key_len 键长度
 * @return 路由结果
 */
ShardRouting *shard_route(ShardRouter *router, const void *key, size_t key_len);

/**
 * @brief 路由到多个分片（用于扫描）
 */
ShardRouting **shard_route_range(ShardRouter *router,
                                  const void *start_key, size_t start_len,
                                  const void *end_key, size_t end_len,
                                  size_t *out_count);

/**
 * @brief 获取负责该分片的所有副本
 */
const char **shard_get_replicas(ShardRouter *router, uint32_t shard_id, size_t *out_count);

/* ========================================================================
 * Hash 分片
 * ======================================================================== */

/**
 * @brief Hash 分片路由
 */
ShardRouting *shard_route_hash(ShardRouter *router, const void *key, size_t key_len);

/**
 * @brief 添加一致性 Hash 虚拟节点
 */
int shard_add_virtual_node(ShardRouter *router, uint32_t shard_id, size_t num_vnodes);

/**
 * @brief 一致性 Hash 路由
 */
ShardRouting *shard_route_consistent_hash(ShardRouter *router, const void *key, size_t key_len);

/* ========================================================================
 * Range 分片
 * ======================================================================== */

/**
 * @brief 创建 Range 分片
 */
int shard_create_range_shard(ShardTopology *topo,
                              uint32_t shard_id,
                              const char *range_key,
                              int64_t start, int64_t end);

/**
 * @brief Range 分片路由
 */
ShardRouting *shard_route_range_scan(ShardRouter *router,
                                      const void *start_key, size_t start_len,
                                      const void *end_key, size_t end_len);

/* ========================================================================
 * 分片迁移
 * ======================================================================== */

/** 迁移状态 */
typedef enum MigrationState_e {
    MIGRATION_PENDING = 0,     /**< 待迁移 */
    MIGRATION_IN_PROGRESS,     /**< 迁移中 */
    MIGRATION_CATCHUP,         /**< 数据追赶 */
    MIGRATION_COMPLETE,        /**< 迁移完成 */
    MIGRATION_FAILED,         /**< 迁移失败 */
} MigrationState;

/** 迁移任务 */
typedef struct MigrationTask_s {
    uint32_t task_id;         /**< 任务 ID */
    uint32_t shard_id;         /**< 待迁移分片 */
    char src_node[64];         /**< 源节点 */
    char dst_node[64];         /**< 目标节点 */

    MigrationState state;      /**< 迁移状态 */
    double progress;            /**< 进度 0.0-1.0 */
    size_t bytes_migrated;     /**< 已迁移字节数 */
    size_t total_bytes;        /**< 总字节数 */

    uint64_t start_time;       /**< 开始时间 */
    uint64_t estimated_end;     /**< 预计结束时间 */
} MigrationTask;

/**
 * @brief 创建迁移任务
 */
MigrationTask *migration_task_create(uint32_t shard_id,
                                      const char *src_node,
                                      const char *dst_node);

/**
 * @brief 更新迁移进度
 */
void migration_task_update(MigrationTask *task, double progress);

/**
 * @brief 获取迁移状态
 */
MigrationState migration_task_get_state(const MigrationTask *task);

/**
 * @brief 标记迁移完成
 */
void migration_task_complete(MigrationTask *task);

/**
 * @brief 销毁迁移任务
 */
void migration_task_destroy(MigrationTask *task);

/* ========================================================================
 * 动态扩缩容
 * ======================================================================== */

/** 扩缩容策略 */
typedef enum ReshardPolicy_e {
    RESHARD_MANUAL = 0,        /**< 手动扩缩容 */
    RESHARD_AUTO_LOAD,         /**< 按负载自动 */
    RESHARD_AUTO_SIZE,         /**< 按大小自动 */
} ReshardPolicy;

/**
 * @brief 触发分片分裂
 */
int shard_split(ShardRouter *router, uint32_t shard_id, const void *split_key);

/**
 * @brief 触发分片合并
 */
int shard_merge(ShardRouter *router, uint32_t shard_id1, uint32_t shard_id2);

/**
 * @brief 重新平衡分片
 */
int shard_rebalance(ShardRouter *router, ReshardPolicy policy);

/**
 * @brief 添加新节点
 */
int shard_add_node(ShardRouter *router, const ShardNode *node);

/**
 * @brief 移除节点
 */
int shard_remove_node(ShardRouter *router, const char *node_id);

/**
 * @brief 获取迁移任务列表
 */
MigrationTask **shard_get_migration_tasks(ShardRouter *router, size_t *out_count);

/**
 * @brief 检查是否需要扩缩容
 */
bool shard_needs_reshard(const ShardRouter *router,
                         double load_threshold,
                         size_t size_threshold);

/* ========================================================================
 * 拓扑同步
 * ======================================================================== */

/**
 * @brief 序列化拓扑
 */
void *shard_topology_serialize(const ShardTopology *topo, size_t *out_size);

/**
 * @brief 反序列化拓扑
 */
ShardTopology *shard_topology_deserialize(const void *data, size_t size);

/**
 * @brief 拓扑版本比较
 */
bool shard_topology_is_stale(const ShardTopology *local, const ShardTopology *remote);

/**
 * @brief 生成拓扑变更日志
 */
int shard_topology_gen_diff(const ShardTopology *old_topo,
                            const ShardTopology *new_topo,
                            void **out_diff, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARD_H */
