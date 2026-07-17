/*
 * sharding.h - 分片支持接口
 */

#ifndef DB_SHARDING_H
#define DB_SHARDING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 分片策略
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    SHARD_HASH,          /* Hash 分片 */
    SHARD_RANGE,         /* Range 分片 */
    SHARD_LIST           /* List 分片 */
} shard_strategy_t;

/* ─────────────────────────────────────────────────────────────────
 * 分片键类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    SHARD_KEY_INT,       /* 整数键 */
    SHARD_KEY_STRING,    /* 字符串键 */
    SHARD_KEY_COMPOSITE  /* 复合键 */
} shard_key_type_t;

/* ─────────────────────────────────────────────────────────────────
 * 分片信息
 * ───────────────────────────────────────────────────────────────── */

typedef struct shard_info {
    int shard_id;                /* 分片 ID */
    char *shard_name;           /* 分片名称 */
    char *host;                 /* 分片主机 */
    int port;                    /* 分片端口 */
    int64_t min_value;           /* 最小值（Range/List） */
    int64_t max_value;           /* 最大值（Range） */
    uint64_t row_count;         /* 行数 */
    bool is_primary;            /* 是否为主分片 */
} shard_info_t;

/* ─────────────────────────────────────────────────────────────────
 * 分片配置
 * ───────────────────────────────────────────────────────────────── */

typedef struct shard_config {
    shard_strategy_t strategy;    /* 分片策略 */
    shard_key_type_t key_type;   /* 键类型 */
    int num_shards;              /* 分片数量 */
    int shard_key_column;        /* 分片键列 ID */
    char *shard_key_name;       /* 分片键列名 */
    int replication_factor;     /* 副本因子 */
    bool consistent_hashing;     /* 一致性哈希 */
} shard_config_t;

/* ─────────────────────────────────────────────────────────────────
 * 分片路由
 * ───────────────────────────────────────────────────────────────── */

typedef struct shard_router shard_router_t;

/**
 * @brief 创建分片路由器
 * @param config 分片配置
 * @return 路由器句柄
 */
shard_router_t *shard_router_create(const shard_config_t *config);

/**
 * @brief 销毁分片路由器
 * @param router 路由器
 */
void shard_router_destroy(shard_router_t *router);

/**
 * @brief 添加分片
 * @param router 路由器
 * @param shard 分片信息
 * @return 0 成功
 */
int shard_router_add(shard_router_t *router, const shard_info_t *shard);

/**
 * @brief 移除分片
 * @param router 路由器
 * @param shard_id 分片 ID
 * @return 0 成功
 */
int shard_router_remove(shard_router_t *router, int shard_id);

/**
 * @brief 计算分片键的哈希值
 * @param router 路由器
 * @param key 分片键
 * @param key_len 键长度
 * @return 哈希值
 */
uint64_t shard_calculate_hash(const shard_router_t *router,
                              const void *key, size_t key_len);

/**
 * @brief 路由查询到分片
 * @param router 路由器
 * @param key 分片键
 * @param key_len 键长度
 * @return 分片 ID
 */
int shard_route(const shard_router_t *router, const void *key, size_t key_len);

/**
 * @brief 路由范围查询到分片
 * @param router 路由器
 * @param min_key 最小键
 * @param max_key 最大键
 * @param shard_ids 输出：分片 ID 数组
 * @param max_count 数组最大长度
 * @return 涉及的分片数量
 */
int shard_route_range(const shard_router_t *router,
                      const void *min_key, const void *max_key,
                      int *shard_ids, int max_count);

/**
 * @brief 获取分片信息
 * @param router 路由器
 * @param shard_id 分片 ID
 * @return 分片信息
 */
const shard_info_t *shard_get_info(const shard_router_t *router, int shard_id);

/**
 * @brief 获取所有分片
 * @param router 路由器
 * @param shards 输出：分片数组
 * @param max_count 数组最大长度
 * @return 分片数量
 */
int shard_get_all(const shard_router_t *router,
                  shard_info_t *shards, int max_count);

/**
 * @brief 获取分片数量
 * @param router 路由器
 * @return 分片数量
 */
int shard_count(const shard_router_t *router);

/* ─────────────────────────────────────────────────────────────────
 * 跨分片查询
 * ───────────────────────────────────────────────────────────────── */

typedef struct cross_shard_query {
    int shard_count;            /* 涉及的分片数 */
    int *shard_ids;             /* 分片 ID 数组 */
    char *query_template;       /* 查询模板 */
    void **params;              /* 查询参数 */
    int param_count;            /* 参数数量 */
} cross_shard_query_t;

/**
 * @brief 创建跨分片查询
 * @param router 路由器
 * @param key 分片键
 * @param key_len 键长度
 * @return 查询对象
 */
cross_shard_query_t *cross_shard_query_create(shard_router_t *router,
                                              const void *key, size_t key_len);

/**
 * @brief 添加分片到查询
 * @param query 查询
 * @param shard_id 分片 ID
 * @return 0 成功
 */
int cross_shard_query_add_shard(cross_shard_query_t *query, int shard_id);

/**
 * @brief 销毁跨分片查询
 * @param query 查询
 */
void cross_shard_query_destroy(cross_shard_query_t *query);

/* ─────────────────────────────────────────────────────────────────
 * 分片再平衡
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 检查是否需要再平衡
 * @param router 路由器
 * @param skew_threshold 数据倾斜阈值（百分比）
 * @return true 需要再平衡
 */
bool shard_needs_rebalance(const shard_router_t *router, double skew_threshold);

/**
 * @brief 执行分片再平衡
 * @param router 路由器
 * @return 0 成功
 */
int shard_do_rebalance(shard_router_t *router);

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建分片配置
 * @param strategy 分片策略
 * @param num_shards 分片数量
 * @return 配置
 */
shard_config_t *shard_config_create(shard_strategy_t strategy, int num_shards);

/**
 * @brief 销毁分片配置
 * @param config 配置
 */
void shard_config_destroy(shard_config_t *config);

/**
 * @brief 创建分片信息
 * @param shard_id 分片 ID
 * @param name 分片名
 * @return 分片信息
 */
shard_info_t *shard_info_create(int shard_id, const char *name);

/**
 * @brief 销毁分片信息
 * @param info 分片信息
 */
void shard_info_destroy(shard_info_t *info);

/**
 * @brief 获取策略名称
 * @param strategy 策略
 * @return 名称
 */
const char *shard_strategy_name(shard_strategy_t strategy);

/* ─────────────────────────────────────────────────────────────────
 * 向量分片支持
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 向量分片键（基于向量 ID 或哈希）
 */
typedef struct vector_shard_key {
    int64_t vector_id;           /* 向量 ID */
    uint64_t hash;               /* 一致性哈希值 */
} vector_shard_key_t;

/**
 * @brief 计算向量的分片键
 * @param router 路由器
 * @param vector_id 向量 ID
 * @param vector 向量数据（可选，用于 LSH）
 * @param dim 向量维度
 * @return 分片键
 */
vector_shard_key_t *vector_shard_key_create(const shard_router_t *router,
                                             int64_t vector_id,
                                             const float *vector,
                                             int dim);

/**
 * @brief 路由向量搜索到分片
 * @param router 路由器
 * @param query_vector 查询向量
 * @param dim 向量维度
 * @param top_k 返回数量
 * @return 涉及的分片 ID 数组
 */
int *vector_shard_route_search(const shard_router_t *router,
                                const float *query_vector,
                                int dim, int top_k);

/**
 * @brief 计算向量一致性哈希
 * @param vector 向量数据
 * @param dim 向量维度
 * @return 哈希值
 */
uint64_t vector_consistent_hash(const float *vector, int dim);

/**
 * @brief 合并多个分片的搜索结果
 * @param num_shards 分片数量
 * @param shard_results 每个分片的结果数组
 * @param top_k 最终返回数量
 * @return 合并后的结果（调用者负责释放）
 */
typedef struct {
    int64_t id;
    float distance;
    int shard_id;
} vector_shard_result_t;

vector_shard_result_t *vector_shard_merge_results(int num_shards,
                                                   vector_shard_result_t **shard_results,
                                                   int *result_counts,
                                                   int top_k);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_H */