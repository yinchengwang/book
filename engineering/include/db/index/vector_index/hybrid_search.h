/**
 * @file hybrid_search.h
 * @brief 混合检索接口头文件
 *
 * 支持向量搜索与结构化过滤条件的混合检索：
 * 1. Pre-filter：先过滤后搜索（精确过滤）
 * 2. Post-filter：先搜索后过滤（高召回）
 * 3. 自适应策略：根据过滤率自动选择最优策略
 */
#ifndef DB_HYBRID_SEARCH_H
#define DB_HYBRID_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>
#include <db/core/query_filter.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 过滤策略
 * ======================================================================== */

/** 混合检索过滤策略 */
typedef enum HybridFilterStrategy_e {
    HYBRID_FILTER_AUTO = 0,       /**< 自适应策略选择 */
    HYBRID_FILTER_PRE = 1,        /**< Pre-filter：先过滤后搜索 */
    HYBRID_FILTER_POST = 2,       /**< Post-filter：先搜索后过滤 */
    HYBRID_FILTER_HYBRID = 3      /**< 混合：分批搜索 + 结果合并 */
} HybridFilterStrategy;

/** 自适应策略选择依据 */
typedef enum HybridAdaptiveMode_e {
    HYBRID_ADAPTIVE_FILTER_RATE = 0,  /**< 根据过滤率选择 */
    HYBRID_ADAPTIVE_DATA_SIZE = 1,    /**< 根据数据规模选择 */
    HYBRID_ADAPTIVE_QUERY_COMPLEXITY = 2 /**< 根据查询复杂度选择 */
} HybridAdaptiveMode;

/* ========================================================================
 * 过滤条件（复用 query_filter.h 的 VDBFilterOp/FilterValue，扩展 FilterCondition）
 * ======================================================================== */

/** FilterValue 类型别名（指向 query_filter.h 的 VDBFilterValue） */
#ifndef FILTER_VALUE_ALIAS_DEFINED
#define FILTER_VALUE_ALIAS_DEFINED
typedef VDBFilterValue FilterValue;
typedef FilterOperator2 FilterOperator;
#endif

/** 单个过滤条件（hybrid_search 专用，query_filter.h 不提供此类型） */
typedef struct FilterCondition_s {
    char field[64];           /**< 字段名 */
    VDBFilterOp op;           /**< 操作符 */
    VDBFilterValueType value_type; /**< 值类型 */
    VDBFilterValue value;        /**< 值 */
    VDBFilterValue value2;       /**< 第二个值（用于 BETWEEN） */
} FilterCondition;

/** 过滤条件表达式（支持 AND/OR/NOT） */
typedef struct FilterExpr_s {
    FilterCondition cond;     /**< 简单条件 */
    struct FilterExpr_s *left;   /**< 左子表达式 */
    struct FilterExpr_s *right;  /**< 右子表达式 */
    VDBFilterOp op;            /**< 连接操作符（仅 AND/OR/NOT 时有效） */
    bool is_leaf;             /**< 是否叶子节点 */
} FilterExpr;

/**
 * @brief 创建过滤表达式
 */
FilterExpr *filter_expr_create_leaf(const char *field, VDBFilterOp op,
                                    VDBFilterValueType value_type, FilterValue value);

/**
 * @brief 创建复合过滤表达式
 */
FilterExpr *filter_expr_create_compound(FilterExpr *left, FilterExpr *right,
                                        VDBFilterOp op);

/**
 * @brief 释放过滤表达式
 */
void filter_expr_free(FilterExpr *expr);

/**
 * @brief 复制过滤表达式
 */
FilterExpr *filter_expr_copy(const FilterExpr *expr);

/**
 * @brief 从 JSON 创建过滤表达式
 *
 * @param json JSON 格式的过滤条件
 * @return 过滤表达式，失败返回 NULL
 *
 * JSON 格式示例：
 * {
 *   "op": "AND",
 *   "conditions": [
 *     {"field": "category", "op": "=", "value": "electronics"},
 *     {"field": "price", "op": ">=", "value": 100},
 *     {"field": "tags", "op": "IN", "value": ["a", "b", "c"]}
 *   ]
 * }
 *
 * 简单格式：
 * {"field": "category", "op": "=", "value": "electronics"}
 */
FilterExpr *filter_expr_from_json(const char *json);

/**
 * @brief 将过滤表达式转换为 JSON
 */
char *filter_expr_to_json(const FilterExpr *expr);

/* ========================================================================
 * 搜索结果
 * ======================================================================== */

/** 混合检索搜索结果 */
typedef struct HybridSearchResult_s {
    int64_t id;               /**< 向量 ID */
    float distance;           /**< 距离/相似度 */
    float score;              /**< 综合评分（可包含过滤条件加权） */
    bool filtered;            /**< 是否通过过滤 */
    void *metadata;           /**< 元数据指针 */
} HybridSearchResult;

/** 搜索结果集 */
typedef struct HybridSearchResults_s {
    HybridSearchResult *results; /**< 结果数组 */
    int32_t count;               /**< 结果数量 */
    int32_t capacity;            /**< 容量 */
    int32_t total_candidates;    /**< 候选总数 */
    int32_t filtered_count;      /**< 通过过滤的数量 */
    int64_t search_time_us;      /**< 搜索耗时（微秒） */
    int64_t filter_time_us;      /**< 过滤耗时（微秒） */
} HybridSearchResults;

/* ========================================================================
 * 混合检索配置
 * ======================================================================== */

/** 混合检索配置 */
typedef struct HybridSearchConfig_s {
    /** 过滤策略 */
    HybridFilterStrategy strategy;

    /** 自适应模式 */
    HybridAdaptiveMode adaptive_mode;

    /** Pre-filter 配置 */
    struct {
        int32_t max_filter_batch;    /**< 最大过滤批次 */
        bool use_index_for_filter;   /**< 使用索引加速过滤 */
    } pre_filter;

    /** Post-filter 配置 */
    struct {
        int32_t oversample_factor;   /**< 过采样因子（搜索更多候选） */
        int32_t min_candidates;      /**< 最小候选数 */
    } post_filter;

    /** 混合策略配置 */
    struct {
        int32_t batch_size;          /**< 分批大小 */
        float batch_filter_rate;     /**< 每批过滤率阈值 */
    } hybrid;

    /** 自适应策略配置 */
    struct {
        float pre_filter_threshold;  /**< 切换到 Pre-filter 的过滤率阈值 */
        float post_filter_threshold; /**< 切换到 Post-filter 的过滤率阈值 */
        int32_t min_data_size;       /**< 使用 Pre-filter 的最小数据量 */
    } adaptive;

    /** 性能优化 */
    struct {
        bool parallel_filter;        /**< 并行过滤 */
        int32_t num_threads;         /**< 线程数 */
        bool cache_filter_results;   /**< 缓存过滤结果 */
    } optimization;
} HybridSearchConfig;

/**
 * @brief 创建默认混合检索配置
 */
HybridSearchConfig *hybrid_search_config_default(void);

/**
 * @brief 创建混合检索配置
 */
HybridSearchConfig *hybrid_search_config_create(HybridFilterStrategy strategy);

/**
 * @brief 释放混合检索配置
 */
void hybrid_search_config_free(HybridSearchConfig *config);

/**
 * @brief 克隆混合检索配置
 */
HybridSearchConfig *hybrid_search_config_clone(const HybridSearchConfig *config);

/* ========================================================================
 * 混合检索核心接口
 * ======================================================================== */

/** 混合检索索引接口（不透明类型） */
typedef struct HybridIndex_s HybridIndex;

/**
 * @brief 创建混合检索索引
 *
 * @param vector_dim 向量维度
 * @param metric 距离度量
 * @param config 混合检索配置
 * @return 索引句柄，失败返回 NULL
 */
HybridIndex *hybrid_index_create(int32_t vector_dim,
                                 distance_metric_t metric,
                                 const HybridSearchConfig *config);

/**
 * @brief 添加向量
 *
 * @param index 索引句柄
 * @param id 向量 ID
 * @param vector 向量数据
 * @param metadata 元数据（用于过滤）
 * @param metadata_size 元数据大小
 * @return 0 成功，-1 失败
 */
int32_t hybrid_index_add(HybridIndex *index,
                         int64_t id,
                         const float *vector,
                         const void *metadata,
                         int32_t metadata_size);

/**
 * @brief 批量添加向量
 *
 * @param index 索引句柄
 * @param n 向量数量
 * @param ids 向量 ID 数组
 * @param vectors 向量数据数组
 * @param metadata 元数据数组
 * @param metadata_sizes 元数据大小数组
 * @return 成功添加的数量，-1 失败
 */
int32_t hybrid_index_add_batch(HybridIndex *index,
                               int32_t n,
                               const int64_t *ids,
                               const float **vectors,
                               const void **metadata,
                               const int32_t *metadata_sizes);

/**
 * @brief 混合检索搜索
 *
 * @param index 索引句柄
 * @param query 查询向量
 * @param filter 过滤条件（可为 NULL 表示不过滤）
 * @param top_k 返回结果数
 * @return 搜索结果，失败返回 NULL
 */
HybridSearchResults *hybrid_index_search(HybridIndex *index,
                                         const float *query,
                                         const FilterExpr *filter,
                                         int32_t top_k);

/**
 * @brief Pre-filter 策略搜索
 *
 * 先过滤后搜索：
 * 1. 根据过滤条件筛选出符合条件的向量 ID
 * 2. 在过滤后的子集上进行向量搜索
 * 适合过滤率高（>50%）的场景
 */
HybridSearchResults *hybrid_index_search_pre(HybridIndex *index,
                                             const float *query,
                                             const FilterExpr *filter,
                                             int32_t top_k);

/**
 * @brief Post-filter 策略搜索
 *
 * 先搜索后过滤：
 * 1. 先进行向量搜索获取候选结果
 * 2. 对候选结果应用过滤条件
 * 适合过滤率低（<30%）的场景
 */
HybridSearchResults *hybrid_index_search_post(HybridIndex *index,
                                              const float *query,
                                              const FilterExpr *filter,
                                              int32_t top_k);

/**
 * @brief 混合策略搜索
 *
 * 分批搜索 + 结果合并：
 * 1. 将索引分成多个批次
 * 2. 对每个批次独立搜索
 * 3. 合并所有批次结果并应用过滤
 * 适合中等过滤率（30%-70%）的场景
 */
HybridSearchResults *hybrid_index_search_hybrid(HybridIndex *index,
                                                const float *query,
                                                const FilterExpr *filter,
                                                int32_t top_k);

/**
 * @brief 删除向量
 */
int32_t hybrid_index_delete(HybridIndex *index, int64_t id);

/**
 * @brief 更新向量元数据
 */
int32_t hybrid_index_update_metadata(HybridIndex *index,
                                     int64_t id,
                                     const void *metadata,
                                     int32_t metadata_size);

/**
 * @brief 获取索引大小
 */
int32_t hybrid_index_size(const HybridIndex *index);

/**
 * @brief 获取索引统计信息
 */
void hybrid_index_stats(const HybridIndex *index,
                        int32_t *out_size,
                        int32_t *out_dims,
                        HybridFilterStrategy *out_strategy);

/**
 * @brief 设置过滤策略
 */
void hybrid_index_set_strategy(HybridIndex *index, HybridFilterStrategy strategy);

/**
 * @brief 获取当前过滤策略
 */
HybridFilterStrategy hybrid_index_get_strategy(const HybridIndex *index);

/**
 * @brief 估算过滤率
 *
 * 根据历史数据和过滤条件估算过滤率
 */
float hybrid_index_estimate_filter_rate(const HybridIndex *index,
                                        const FilterExpr *filter);

/**
 * @brief 释放搜索结果
 */
void hybrid_search_results_free(HybridSearchResults *results);

/**
 * @brief 销毁混合检索索引
 */
void hybrid_index_destroy(HybridIndex *index);

/* ========================================================================
 * 索引后端支持（用于 HybridIndex 内部）
 * ======================================================================== */

/** 索引后端类型 */
typedef enum HybridBackendType_e {
    HYBRID_BACKEND_BRUTE_FORCE = 0,   /**< 暴力搜索 */
    HYBRID_BACKEND_HNSW = 1,          /**< HNSW */
    HYBRID_BACKEND_DISKANN = 2,       /**< DiskANN */
    HYBRID_BACKEND_IVF = 3,           /**< IVF */
    HYBRID_BACKEND_IVF_PQ = 4         /**< IVF-PQ */
} HybridBackendType;

/** 索引后端接口 */
typedef struct HybridBackendOps_s {
    /** 搜索接口 */
    int32_t (*search)(void *backend,
                      const float *query,
                      int32_t k,
                      int64_t *ids,
                      float *distances);

    /** 添加向量 */
    int32_t (*add)(void *backend,
                   int64_t id,
                   const float *vector);

    /** 删除向量 */
    int32_t (*remove)(void *backend, int64_t id);

    /** 获取向量 */
    const float *(*get_vector)(void *backend, int64_t id);

    /** 持久化 */
    int32_t (*save)(void *backend, const char *path);
    int32_t (*load)(void *backend, const char *path);
} HybridBackendOps;

/** 索引后端 */
typedef struct HybridBackend_s {
    HybridBackendType type;
    void *impl;                   /**< 后端实现 */
    HybridBackendOps ops;
    int32_t dims;
    int32_t size;
} HybridBackend;

/**
 * @brief 创建索引后端
 */
HybridBackend *hybrid_backend_create(HybridBackendType type, int32_t dims);

/**
 * @brief 销毁索引后端
 */
void hybrid_backend_destroy(HybridBackend *backend);

/* ========================================================================
 * SQL 函数集成
 * ======================================================================== */

/**
 * @brief VECTOR_SEARCH 函数参数
 *
 * VECTOR_SEARCH(collection, query_vector, top_k, filter_expr)
 */
typedef struct VectorSearchArgs_s {
    const char *collection;       /**< 集合名 */
    const float *query_vector;    /**< 查询向量 */
    int32_t vector_dim;           /**< 向量维度 */
    int32_t top_k;                /**< 返回数量 */
    FilterExpr *filter;           /**< 过滤条件（可选） */
    HybridFilterStrategy strategy;/**< 过滤策略（可选） */
} VectorSearchArgs;

/**
 * @brief 解析 VECTOR_SEARCH 函数参数
 *
 * @param sql_func SQL 函数调用字符串
 * @return 参数结构，失败返回 NULL
 */
VectorSearchArgs *vector_search_parse_args(const char *sql_func);

/**
 * @brief 释放 VECTOR_SEARCH 函数参数
 */
void vector_search_free_args(VectorSearchArgs *args);

/**
 * @brief 执行 VECTOR_SEARCH
 *
 * @param args 解析后的参数
 * @param results 输出结果
 * @return 0 成功，-1 失败
 */
int32_t vector_search_execute(const VectorSearchArgs *args,
                              HybridSearchResults *results);

#ifdef __cplusplus
}
#endif

#endif /* DB_HYBRID_SEARCH_H */
