/**
 * @file vector_query.h
 * @brief 向量查询执行器头文件
 *
 * ============================================================================
 * 设计目标
 * ============================================================================
 *
 * 实现统一的向量查询执行管道，整合：
 * 1. 粗排阶段 (HNSW/DiskANN/IVF)
 * 2. 精排阶段 (ReRanker)
 * 3. 混合检索 (BM25 + Vector)
 *
 * 架构设计：
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    VectorQueryPipeline                          │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │   Query Input                                                   │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              Query Planner (查询计划)                    │  │
 * │   │   - 索引选择                                             │  │
 * │   │   - 参数调优                                             │  │
 * │   │   - 缓存查询计划                                         │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              Stage 1: 粗排 (Coarse Ranking)              │  │
 * │   │   - HNSW / DiskANN / IVF                                │  │
 * │   │   - 返回 Top-N 候选 (N >> K)                            │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │ 候选集 (N 个)                                           │
 * │     ▼                                                         │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              Stage 2: 精排 (Reranking)                   │  │
 * │   │   - ReRanker 精确距离计算                               │  │
 * │   │   - MMR 去重                                            │  │
 * │   │   - 多度量融合                                          │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │ Top-K 结果                                              │
 * │     ▼                                                         │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              Stage 3: 混合检索 (可选)                    │  │
 * │   │   - BM25 全文检索                                       │  │
 * │   │   - RRF / 加权融合                                      │  │
 * │   │   - 分值归一化                                          │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │ 最终 Top-K 结果                                         │
 * │     ▼                                                         │
 * │   Query Result                                                 │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ============================================================================
 * 使用示例
 * ============================================================================
 *
 * @code
 * // 1. 初始化执行器
 * vector_query_init();
 *
 * // 2. 创建查询计划
 * VectorQueryPlan *plan = vector_query_plan_create();
 * vector_query_plan_set_coarse(plan, COARSE_HNSW, 100, 100);
 * vector_query_plan_set_rerank(plan, RERANKER_PRECISE, 50);
 * vector_query_plan_set_topk(plan, 10);
 *
 * // 3. 执行查询
 * float query[128] = {...};
 * VectorQueryResult *result = vector_query_execute(plan, query, 128);
 *
 * // 4. 处理结果
 * for (int i = 0; i < result->count; i++) {
 *     printf("id=%ld, distance=%f\n", result->ids[i], result->distances[i]);
 * }
 *
 * // 5. 清理
 * vector_query_result_free(result);
 * vector_query_plan_destroy(plan);
 * vector_query_shutdown();
 * @endcode
 */
#ifndef DB_VECTOR_QUERY_H
#define DB_VECTOR_QUERY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <db/index/vector_index/reranker/reranker.h>
#include <db/index/vector_index/hybrid_search.h>
#include <db/core/vector_types.h>  /* VectorBatch, VectorBlock, CompareOp, DistanceMetric, VectorQueryState, VectorQueryProfileEntry */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 默认候选集大小（粗排返回数量） */
#define VECTOR_QUERY_DEFAULT_CANDIDATES 100

/** 最大候选集大小 */
#define VECTOR_QUERY_MAX_CANDIDATES 10000

/** 默认 Top-K */
#define VECTOR_QUERY_DEFAULT_TOPK 10

/** 查询计划缓存大小 */
#define VECTOR_QUERY_PLAN_CACHE_SIZE 64

/* ========================================================================
 * 索引类型
 * ======================================================================== */

/** 粗排索引类型 */
typedef enum VectorIndexType_e {
    VECTOR_INDEX_HNSW = 0,         /**< HNSW 索引 */
    VECTOR_INDEX_DISKANN = 1,      /**< DiskANN 索引 */
    VECTOR_INDEX_IVF = 2,          /**< IVF 索引 */
    VECTOR_INDEX_BRUTE_FORCE = 3,  /**< 暴力线性扫描（适合 < 10K 调试用） */
    VECTOR_INDEX_AUTO = 99         /**< 自动选择 */
} VectorIndexType;

/** 融合类型 */
typedef enum VectorFusionType_e {
    FUSION_RRF = 0,            /**< Reciprocal Rank Fusion */
    FUSION_WEIGHTED = 1,       /**< 加权融合 */
    FUSION_RRFR = 2            /**< RRF with Reciprocal */
} VectorFusionType;

/* ========================================================================
 * 算子链结构 (新增)
 * ======================================================================== */

/** 算子类型 */
typedef enum PlanNodeType_e {
    PLAN_NODE_VECTOR_SCAN = 0,    /**< 向量扫描 */
    PLAN_NODE_VECTOR_FILTER = 1,  /**< 向量过滤 */
    PLAN_NODE_VECTOR_LIMIT = 2,   /**< 向量截断 */
    PLAN_NODE_VECTOR_JOIN = 3,    /**< 向量连接 (混合检索) */
    PLAN_NODE_VECTOR_SORT = 4,    /**< 向量排序 (预留) */
    PLAN_NODE_VECTOR_AGG = 5      /**< 向量聚合 (预留) */
} PlanNodeType;

/** 算子节点基类 */
typedef struct PlanNode_s {
    PlanNodeType type;            /**< 算子类型 */
    struct PlanNode_s *left;      /**< 左子节点 (上游算子) */
    struct PlanNode_s *right;     /**< 右子节点 (下游算子，Join 用) */
    int32_t cost_estimate;        /**< 代价估计 */
    int32_t rows_estimate;        /**< 行数估计 */
    void *private_data;           /**< 算子私有数据 */
} PlanNode;

/* ========================================================================
 * 查询计划
 * ======================================================================== */

/** 向量查询计划 */
typedef struct VectorQueryPlan_s {
    /* === 算子链 (新增) === */
    PlanNode *root_node;          /**< 算子链根节点 */
    int32_t num_nodes;            /**< 算子数量 */

    /* 粗排配置 */
    VectorIndexType coarse_index_type;  /**< 粗排索引类型 */
    int32_t         coarse_ef_search;   /**< 搜索参数 ef */
    int32_t         coarse_max_candidates; /**< 最大候选数 */

    /* 精排配置 */
    RerankerType   reranker_type;       /**< 精排类型 */
    RerankerConfig reranker_config;     /**< 精排配置 */

    /* 混合检索配置 */
    bool           hybrid_enabled;       /**< 是否启用混合检索 */
    float          vector_weight;        /**< 向量权重 */
    float          text_weight;          /**< 文本权重 */
    VectorFusionType fusion_type;        /**< 融合类型 */

    /* 输出配置 */
    int32_t        top_k;                /**< 返回 Top-K */
    int32_t        offset;               /**< 分页偏移 (新增) */
    bool           with_scores;          /**< 是否返回分数 */

    /* 统计信息 */
    int64_t        plan_id;              /**< 计划 ID */
    int64_t        created_at;           /**< 创建时间 */
    int64_t        hit_count;            /**< 命中次数 */
} VectorQueryPlan;

/**
 * @brief 创建查询计划
 */
VectorQueryPlan *vector_query_plan_create(void);

/**
 * @brief 销毁查询计划
 */
void vector_query_plan_destroy(VectorQueryPlan *plan);

/**
 * @brief 复制查询计划
 */
VectorQueryPlan *vector_query_plan_copy(const VectorQueryPlan *plan);

/**
 * @brief 设置粗排参数
 */
void vector_query_plan_set_coarse(VectorQueryPlan *plan,
                                  VectorIndexType index_type,
                                  int32_t ef_search,
                                  int32_t max_candidates);

/**
 * @brief 设置精排参数
 */
void vector_query_plan_set_rerank(VectorQueryPlan *plan,
                                  RerankerType reranker_type,
                                  int32_t k);

/**
 * @brief 设置混合检索参数
 */
void vector_query_plan_set_hybrid(VectorQueryPlan *plan,
                                  bool enabled,
                                  float vector_weight,
                                  float text_weight,
                                  VectorFusionType fusion_type);

/**
 * @brief 设置 Top-K
 */
void vector_query_plan_set_topk(VectorQueryPlan *plan, int32_t top_k);

/**
 * @brief 设置分页参数 (新增)
 */
void vector_query_plan_set_offset(VectorQueryPlan *plan, int32_t offset);

/* ========================================================================
 * 算子节点构建 (新增)
 * ======================================================================== */

/**
 * @brief 创建 VectorScan 算子节点
 */
PlanNode *create_vector_scan_node(VectorIndexType index_type,
                                   int32_t ef_search,
                                   int32_t max_candidates);

/**
 * @brief 创建 VectorFilter 算子节点
 */
PlanNode *create_vector_filter_node(int32_t column_idx,
                                     CompareOp op,
                                     void *value);

/**
 * @brief 创建 VectorLimit 算子节点
 */
PlanNode *create_vector_limit_node(int32_t limit, int32_t offset);

/**
 * @brief 创建 VectorJoin 算子节点 (混合检索)
 */
PlanNode *create_vector_join_node(VectorFusionType fusion_type,
                                   float vector_weight,
                                   float text_weight);

/**
 * @brief 构建简单查询计划 (Scan → Limit)
 */
VectorQueryPlan *build_simple_query_plan(VectorIndexType index_type, int32_t k);

/**
 * @brief 构建混合查询计划 (VectorScan + BM25 → Join → Limit)
 */
VectorQueryPlan *build_hybrid_query_plan(int32_t k,
                                          float vector_weight,
                                          float text_weight);

/**
 * @brief 销毁算子节点树
 */
void destroy_plan_node(PlanNode *node);

/* ========================================================================
 * 查询结果
 * ======================================================================== */

/** 向量查询结果 */
typedef struct VectorQueryResult_s {
    /* === 结果数据 (已有) === */
    int64_t  *ids;           /**< 向量 ID 数组 */
    float    *distances;     /**< 距离数组 */
    float    *scores;        /**< 综合分数数组 (混合检索时使用) */
    int32_t  count;          /**< 结果数量 */
    int32_t  capacity;       /**< 容量 */

    /* === 分页信息 (新增) === */
    int32_t  offset;         /**< 偏移量 */
    int32_t  total_candidates; /**< 总候选数 (未分页前) */

    /* === 元数据 (新增) === */
    void     **metadata;     /**< 元数据数组 (可选) */
    int32_t  metadata_count; /**< 元数据列数 */

    /* 统计信息 */
    int64_t  coarse_time_us; /**< 粗排耗时 (微秒) */
    int64_t  rerank_time_us; /**< 精排耗时 (微秒) */
    int64_t  fusion_time_us; /**< 融合耗时 (微秒) (新增) */
    int64_t  total_time_us;  /**< 总耗时 (微秒) */
    int32_t  candidates;     /**< 候选数量 */
} VectorQueryResult;

/**
 * @brief 释放查询结果
 */
void vector_query_result_free(VectorQueryResult *result);

/**
 * @brief 结果集 JSON 序列化 (新增)
 */
char *vector_query_result_to_json(const VectorQueryResult *result);

/* ========================================================================
 * 执行上下文 (新增)
 * ======================================================================== */

/** 执行上下文结构 */
typedef struct ExecContext_s {
    /* === 查询输入 === */
    const float *query_vector;    /**< 查询向量 */
    int32_t query_dims;           /**< 向量维度 */
    DistanceMetric metric;        /**< 距离度量 */

    /* === 执行配置 === */
    VectorExecMode exec_mode;     /**< 执行模式 */
    int32_t batch_size;           /**< 批次大小 */
    int32_t timeout_ms;           /**< 超时时间 (毫秒) */
    volatile bool *cancelled;     /**< 取消标志 */

    /* === 执行状态 === */
    VectorQueryState state;       /**< 执行状态 */
    int64_t start_time_us;        /**< 开始时间 */
    int64_t current_time_us;      /**< 当前时间 */

    /* === 剖析信息 === */
    bool profiling_enabled;       /**< 是否启用剖析 */
    VectorQueryProfileEntry profile_entries[5]; /**< 剖析条目 */
    int32_t profile_idx;          /**< 当前剖析索引 */

    /* === 错误处理 === */
    char error_msg[256];          /**< 错误信息 */
    int32_t error_code;           /**< 错误码 */
} ExecContext;

/**
 * @brief 创建执行上下文
 */
ExecContext *exec_context_create(const float *query, int32_t dims, DistanceMetric metric);

/**
 * @brief 销毁执行上下文
 */
void exec_context_destroy(ExecContext *ctx);

/**
 * @brief 执行算子链 (新增)
 */
VectorQueryResult *execute_plan(VectorQueryPlan *plan, ExecContext *ctx);

/* ========================================================================
 * 查询优化 (新增)
 * ======================================================================== */

/**
 * @brief 优化查询计划
 *
 * @param plan 查询计划
 * @param num_vectors 数据集向量数量
 * @param dims 向量维度
 * @param metric 距离度量
 * @return 优化后的查询计划
 */
VectorQueryPlan *optimize_query_plan(VectorQueryPlan *plan,
                                      int32_t num_vectors,
                                      int32_t dims,
                                      DistanceMetric metric);

/* ========================================================================
 * 查询参数配置 (新增)
 * ======================================================================== */

/**
 * @brief 设置距离度量类型
 */
void vector_query_plan_set_metric(VectorQueryPlan *plan, DistanceMetric metric);

/**
 * @brief 设置查询超时时间 (毫秒)
 */
void vector_query_plan_set_timeout(VectorQueryPlan *plan, int32_t timeout_ms);

/**
 * @brief 设置执行批次大小
 */
void vector_query_plan_set_batch_size(VectorQueryPlan *plan, int32_t batch_size);

/* ========================================================================
 * 执行器核心
 * ======================================================================== */

/**
 * @brief 初始化查询执行器
 * @return 0 成功，-1 失败
 */
int vector_query_init(void);

/**
 * @brief 关闭查询执行器
 */
void vector_query_shutdown(void);

/**
 * @brief 检查执行器是否就绪
 */
bool vector_query_is_ready(void);

/**
 * @brief 执行向量查询
 *
 * @param plan 查询计划
 * @param query 查询向量
 * @param dims 向量维度
 * @return 查询结果，失败返回 NULL
 */
VectorQueryResult *vector_query_execute(const VectorQueryPlan *plan,
                                        const float *query,
                                        int32_t dims);

/**
 * @brief 执行带过滤的向量查询
 *
 * @param plan 查询计划
 * @param query 查询向量
 * @param dims 向量维度
 * @param filter 过滤条件 (可为 NULL)
 * @return 查询结果，失败返回 NULL
 */
VectorQueryResult *vector_query_execute_filtered(const VectorQueryPlan *plan,
                                                  const float *query,
                                                  int32_t dims,
                                                  const FilterExpr *filter);

/**
 * @brief 取消正在执行的查询
 * @return 0 成功，-1 无查询正在执行
 */
int vector_query_cancel(void);

/* ========================================================================
 * 索引注册
 * ======================================================================== */

/** 索引搜索函数类型 */
typedef int32_t (*vector_index_search_fn)(
    void *index,
    const float *query,
    int32_t k,
    int64_t *ids,
    float *distances);

/** 索引获取向量函数类型 */
typedef const float *(*vector_index_get_vector_fn)(void *index, int64_t id);

/**
 * @brief 注册向量索引
 *
 * @param name 索引名称
 * @param index_type 索引类型
 * @param index_impl 索引实现指针
 * @param search_fn 搜索函数
 * @param get_vector_fn 获取向量函数
 * @return 0 成功，-1 失败
 */
int vector_query_register_index(const char *name,
                                VectorIndexType index_type,
                                void *index_impl,
                                vector_index_search_fn search_fn,
                                vector_index_get_vector_fn get_vector_fn);

/**
 * @brief 注销向量索引
 */
int vector_query_unregister_index(const char *name);

/**
 * @brief 获取已注册的索引
 */
void *vector_query_get_index(const char *name);

/* ========================================================================
 * 查询计划缓存
 * ======================================================================== */

/**
 * @brief 获取缓存的查询计划
 *
 * @param query_signature 查询签名 (如 "hnsw_128_l2_10")
 * @return 缓存的计划，NULL 表示未命中
 */
VectorQueryPlan *vector_query_get_cached_plan(const char *query_signature);

/**
 * @brief 缓存查询计划
 */
void vector_query_cache_plan(const char *query_signature,
                             const VectorQueryPlan *plan);

/**
 * @brief 清空计划缓存
 */
void vector_query_clear_plan_cache(void);

/**
 * @brief 获取缓存命中率
 */
float vector_query_get_cache_hit_rate(void);

/* ========================================================================
 * 执行统计
 * ======================================================================== */

/** 执行统计信息 */
typedef struct VectorQueryStats_s {
    uint64_t total_queries;       /**< 总查询数 */
    uint64_t successful_queries;  /**< 成功查询数 */
    uint64_t failed_queries;      /**< 失败查询数 */
    uint64_t cached_queries;      /**< 命中缓存的查询数 */
    uint64_t total_latency_us;    /**< 总延迟 (微秒) */
    uint64_t avg_latency_us;      /**< 平均延迟 (微秒) */
    uint64_t min_latency_us;      /**< 最小延迟 (微秒) */
    uint64_t max_latency_us;      /**< 最大延迟 (微秒) */
} VectorQueryStats;

/**
 * @brief 获取执行统计
 */
void vector_query_get_stats(VectorQueryStats *stats);

/**
 * @brief 重置统计信息
 */
void vector_query_reset_stats(void);

/* ========================================================================
 * 执行剖析 (Profiling)
 * ======================================================================== */

/**
 * @brief 获取最近查询的剖析信息
 *
 * @param entries 输出数组 (至少 5 个元素)
 * @param max_entries 最大条目数
 * @return 实际条目数
 */
int vector_query_get_profile(VectorQueryProfileEntry *entries,
                             int max_entries);

/**
 * @brief 启用/禁用剖析
 */
void vector_query_set_profiling(bool enabled);

/**
 * @brief 获取剖析是否启用
 */
bool vector_query_is_profiling(void);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 生成查询签名
 *
 * @param plan 查询计划
 * @param dims 向量维度
 * @param out_signature 输出缓冲区
 * @param sig_size 缓冲区大小
 */
void vector_query_generate_signature(const VectorQueryPlan *plan,
                                     int32_t dims,
                                     char *out_signature,
                                     size_t sig_size);

/**
 * @brief RRF 融合计算
 *
 * @param results 结果数组
 * @param num_results 结果数量
 * @param k RRF 参数 k (通常取 60)
 * @param out_scores 输出分数数组
 */
void vector_fusion_rrf(const float **distances,
                       const int32_t *counts,
                       int num_results,
                       int32_t k,
                       float *out_scores);

/**
 * @brief 加权融合计算
 *
 * @param distances 距离数组
 * @param weights 权重数组
 * @param num_items 项数
 * @param out_scores 输出分数数组
 */
void vector_fusion_weighted(const float *distances,
                            const float *weights,
                            int num_items,
                            float *out_scores);

/**
 * @brief 距离归一化
 *
 * @param distances 距离数组
 * @param num_items 项数
 * @param out_normalized 输出归一化数组
 */
void vector_normalize_distances(const float *distances,
                                int num_items,
                                float *out_normalized);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_QUERY_H */
