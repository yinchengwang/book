/**
 * @file vector_engine.h
 * @brief 向量存储引擎头文件
 *
 * 定义向量存储引擎的接口和数据结构，支持向量插入、相似度搜索、
 * HNSW 索引等功能。
 */
#ifndef DB_VECTOR_ENGINE_H
#define DB_VECTOR_ENGINE_H

#include "storage_engine.h"
#include "db/mm_pool.h"
#include "db/index/vector_index/vector_index_selector.h"
#include "db/core/vector_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 引入 vector_persist_result_t 类型定义 */
#include "db/vector_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

struct faiss_hnsw;
typedef struct faiss_hnsw faiss_hnsw_t;
typedef struct ivf_pq_index ivf_pq_index_t;
typedef struct lock_manager_s lock_manager_t;
typedef struct lock_ctx_s lock_ctx_t;

/* ========================================================================
 * 向量存储引擎内部数据结构
 * ======================================================================== */

/* 使用 db/vector_engine.h 中的公共类型定义 */

/**
 * @brief 向量引擎数据库内部扩展字段
 *
 * vector_engine_db_t 定义在 db/vector_engine.h 中，
 * 这里仅声明内部实现所需的扩展字段。
 */

/**
 * @brief 向量引擎数据库内部结构（扩展字段）
 *
 * 注意：实际使用的是 db/vector_engine.h 中定义的 vector_engine_db_t。
 * 这个结构体仅用于文档说明内部字段的布局。
 */
typedef struct vector_engine_db_internal_s {
    char name[256];            /**< 集合名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    int32_t dimension;         /**< 向量维度 */
    vector_metric_t metric;    /**< 度量类型 */
    uint64_t num_vectors;      /**< 向量数量 */

    /* 内存池相关 */
    void *mem_pool;            /**< 内存池指针（使用 mm_pool） */
    bool use_mem_pool;         /**< 是否使用内存池 */

    /* HNSW 索引相关 */
    faiss_hnsw_t *hnsw_index;  /**< HNSW 索引指针 */
    bool index_built;          /**< 索引是否已构建 */
    bool index_loaded;         /**< 索引是否已从磁盘加载 */

    /* VecPage 分页存储相关 */
    void *page_pool;           /**< 向量页池指针 */
    bool use_page_pool;        /**< 是否使用页池存储 */

    /* PQ 量化压缩相关 */
    void *quantizer;           /**< 量化器指针 */
    bool use_quantization;     /**< 是否启用量化压缩 */
    uint8_t *compressed_codes; /**< 压缩码存储 */
    uint64_t compressed_count; /**< 压缩码数量 */

    /* IVF-PQ 索引相关 */
    ivf_pq_index_t *ivf_pq_index; /**< IVF-PQ 索引指针 */
    bool use_ivf_pq;                /**< 是否使用 IVF-PQ 索引 */
    int ivf_pq_nlist;              /**< IVF 聚类数量 */
    int ivf_pq_nprobe;             /**< 搜索探针数量 */

    /* 并发控制 */
    lock_manager_t *lockmgr;   /**< 锁管理器 */
    void *rwlock;              /**< 读写锁（pthread_rwlock 或模拟） */
    bool use_lock;             /**< 是否启用锁 */

    /* WAL 持久化 */
    void *wal;                 /**< WAL 句柄（vector_wal_t*） */
    bool use_wal;              /**< 是否启用 WAL */
    uint32_t segment_id;       /**< WAL 段 ID */
} vector_engine_db_internal_t;

/* ========================================================================
 * HNSW 索引 API
 * ======================================================================== */

/**
 * @brief 构建 HNSW 索引
 *
 * @param rel 向量引擎句柄
 * @param m 每层最大连接数（默认 16）
 * @param ef_construction 构造时搜索范围（默认 200）
 * @return 0 成功，-1 失败
 */
int vector_engine_build_index(void *rel, int m, int ef_construction);

/**
 * @brief 使用 HNSW 索引搜索
 *
 * 如果索引未构建，回退到暴力搜索
 *
 * @param rel 向量引擎句柄
 * @param query 查询向量
 * @param top_k 返回最近邻数量
 * @param results 搜索结果输出
 * @return 0 成功，-1 失败
 */
int vector_engine_search_hnsw(void *rel, const float *query,
                               int32_t top_k, vector_search_results_t *results);

/**
 * @brief 保存 HNSW 索引到文件
 *
 * @param rel 向量引擎句柄
 * @param path 索引文件路径（可为 NULL，使用默认路径）
 * @return 0 成功，-1 失败
 */
int vector_engine_save_index(void *rel, const char *path);

/**
 * @brief 从文件加载 HNSW 索引
 *
 * @param rel 向量引擎句柄
 * @param path 索引文件路径（可为 NULL，使用默认路径）
 * @return 0 成功，-1 失败
 */
int vector_engine_load_index(void *rel, const char *path);

/**
 * @brief 删除 HNSW 索引
 *
 * @param rel 向量引擎句柄
 * @return 0 成功，-1 失败
 */
int vector_engine_drop_index(void *rel);

/**
 * @brief 检查索引是否已构建
 *
 * @param rel 向量引擎句柄
 * @return true 已构建，false 未构建
 */
bool vector_engine_index_built(const void *rel);

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 获取向量引擎操作表
 */
const storage_ops_t *vector_engine_get_ops(void);

/**
 * @brief 初始化向量引擎
 */
int vector_engine_init(const char *data_dir);

/**
 * @brief 关闭向量引擎
 */
int vector_engine_shutdown(void);

/**
 * @brief 创建向量集合
 */
int vector_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开向量集合
 */
void *vector_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭向量集合
 */
int vector_engine_close(void *rel);

/**
 * @brief 删除向量集合
 */
int vector_engine_drop(const char *name);

/**
 * @brief 插入向量
 */
int vector_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 搜索最近邻
 */
int vector_engine_search(void *rel,
                         const float *query, int32_t query_dim,
                         int32_t top_k,
                         vector_search_results_t *results);

/**
 * @brief 释放搜索结果
 */
void vector_engine_free_results(vector_search_results_t *results);

/**
 * @brief 获取统计信息
 */
int vector_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * PQ 量化压缩 API
 * ======================================================================== */

/**
 * @brief 启用 PQ 量化压缩
 *
 * 启用后，向量将自动进行 PQ 编码压缩存储，
 * 训练所需样本数量建议为 256 * num_subquantizers。
 *
 * @param rel 向量引擎句柄
 * @param subquantizers 子空间数量（建议 dim/8）
 * @param bits 每个码字的位数（4/6/8）
 * @return 0 成功，-1 失败
 */
int vector_engine_enable_pq(void *rel, int32_t subquantizers, int32_t bits);

/**
 * @brief 训练 PQ 量化器
 *
 * 使用已有向量数据训练码书。
 *
 * @param rel 向量引擎句柄
 * @param sample_size 采样数量（0 表示使用所有向量）
 * @return 0 成功，-1 失败
 */
int vector_engine_train_pq(void *rel, int32_t sample_size);

/**
 * @brief 保存 PQ 量化器到文件
 *
 * @param rel 向量引擎句柄
 * @return 0 成功，-1 失败
 */
int vector_engine_save_pq(void *rel);

/**
 * @brief 从文件加载 PQ 量化器
 *
 * @param rel 向量引擎句柄
 * @return 0 成功，-1 失败
 */
int vector_engine_load_pq(void *rel);

/**
 * @brief 禁用 PQ 量化
 *
 * @param rel 向量引擎句柄
 */
void vector_engine_disable_pq(void *rel);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 计算两个向量的欧氏距离
 */
float vector_l2_distance(const float *a, const float *b, int32_t dim);

/**
 * @brief 计算两个向量的余弦相似度
 */
float vector_cosine_similarity(const float *a, const float *b, int32_t dim);

/**
 * @brief 归一化向量
 */
void vector_normalize(float *v, int32_t dim);

/* ========================================================================
 * 内存池管理 API
 * ======================================================================== */

/**
 * @brief 启用/禁用内存池
 *
 * @param rel 向量引擎句柄
 * @param use_pool true 启用内存池，false 禁用
 * @return 0 成功，-1 失败
 */
int vector_engine_enable_mem_pool(void *rel, bool use_pool);

/**
 * @brief 获取内存池统计信息
 *
 * @param rel 向量引擎句柄
 * @param stats 输出统计信息
 * @return 0 成功，-1 失败
 */
int vector_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats);

/* ========================================================================
 * 并发锁控制 API
 * ======================================================================== */

/**
 * @brief 启用/禁用并发锁
 *
 * @param rel 向量引擎句柄
 * @param use_lock true 启用锁，false 禁用
 * @return 0 成功，-1 失败
 */
int vector_engine_enable_lock(void *rel, bool use_lock);

/**
 * @brief 获取向量引擎的读锁
 *
 * 用于搜索操作，多个搜索可以并发执行
 *
 * @param rel 向量引擎句柄
 * @return 0 成功，-1 失败
 */
int vector_engine_read_lock(void *rel);

/**
 * @brief 释放向量引擎的读锁
 *
 * @param rel 向量引擎句柄
 */
void vector_engine_read_unlock(void *rel);

/**
 * @brief 获取向量引擎的写锁
 *
 * 用于插入、删除操作，写操作需要排他锁
 *
 * @param rel 向量引擎句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，LOCK_TIMEOUT 超时，-1 失败
 */
int vector_engine_write_lock(void *rel, uint32_t timeout_ms);

/**
 * @brief 释放向量引擎的写锁
 *
 * @param rel 向量引擎句柄
 */
void vector_engine_write_unlock(void *rel);

/**
 * @brief 获取锁管理器
 *
 * @param rel 向量引擎句柄
 * @return 锁管理器指针
 */
void *vector_engine_get_lockmgr(void *rel);

/* ========================================================================
 * IVF-PQ 索引 API
 * ======================================================================== */

/**
 * @brief 启用 IVF-PQ 索引
 *
 * @param rel 向量引擎句柄
 * @param nlist IVF 聚类数量（建议 100-1000）
 * @param nprobe 搜索探针数量（建议 nlist 的 1-10%）
 * @return 0 成功，-1 失败
 */
int vector_engine_enable_ivf_pq(void *rel, int nlist, int nprobe);

/**
 * @brief 训练 IVF-PQ 索引
 *
 * 使用已有向量数据训练码书和聚类中心。
 *
 * @param rel 向量引擎句柄
 * @param sample_size 采样数量（0 表示使用所有向量）
 * @return 0 成功，-1 失败
 */
int vector_engine_train_ivf_pq(void *rel, int32_t sample_size);

/**
 * @brief 使用 IVF-PQ 索引搜索
 *
 * @param rel 向量引擎句柄
 * @param query 查询向量
 * @param top_k 返回最近邻数量
 * @param results 搜索结果输出
 * @return 0 成功，-1 失败
 */
int vector_engine_search_ivf_pq(void *rel, const float *query,
                                int32_t top_k, vector_search_results_t *results);

/**
 * @brief 保存 IVF-PQ 索引
 *
 * @param rel 向量引擎句柄
 * @param path 索引文件路径（可为 NULL，使用默认路径）
 * @return 0 成功，-1 失败
 */
int vector_engine_save_ivf_pq(void *rel, const char *path);

/**
 * @brief 加载 IVF-PQ 索引
 *
 * @param rel 向量引擎句柄
 * @param path 索引文件路径（可为 NULL，使用默认路径）
 * @return 0 成功，-1 失败
 */
int vector_engine_load_ivf_pq(void *rel, const char *path);

/**
 * @brief 禁用 IVF-PQ 索引
 *
 * @param rel 向量引擎句柄
 */
void vector_engine_disable_ivf_pq(void *rel);

/**
 * @brief 获取 IVF-PQ 索引统计
 *
 * @param rel 向量引擎句柄
 * @param out_n_vectors 输出向量数量
 * @param out_code_size 输出压缩码大小
 */
void vector_engine_ivf_pq_stats(void *rel, int *out_n_vectors, int *out_code_size);

/**
 * @brief 设置 IVF-PQ 搜索探针数量
 *
 * @param rel 向量引擎句柄
 * @param nprobe 探针数量
 */
void vector_engine_set_ivf_pq_nprobe(void *rel, int nprobe);

/* ========================================================================
 * 自动索引选择 API
 * ======================================================================== */

/**
 * @brief 获取推荐的索引类型
 *
 * 根据当前数据规模获取推荐的索引类型。
 *
 * @param rel 向量引擎句柄
 * @param out_type 输出索引类型
 * @param out_param1 输出第一个参数
 * @param out_param2 输出第二个参数
 * @return 0 成功，-1 失败
 */
int vector_engine_get_recommended_index(void *rel, int *out_type, int *out_param1, int *out_param2);

/**
 * @brief 自动选择并启用最优索引
 *
 * 根据数据规模自动选择最优索引类型并启用。
 *
 * @param rel 向量引擎句柄
 * @param target_qps 目标 QPS（0 表示使用默认值）
 * @param target_recall 目标召回率（0 表示使用默认值）
 * @return 0 成功，-1 失败
 */
int vector_engine_auto_select_index(void *rel, float target_qps, float target_recall);

/**
 * @brief 获取当前索引类型名称
 *
 * @param rel 向量引擎句柄
 * @return 索引类型名称
 */
const char *vector_engine_get_index_name(void *rel);

/* ========================================================================
 * WAL 持久化 API
 * ======================================================================== */

/**
 * @brief 启用 WAL 持久化
 *
 * 启用后，所有写操作（插入/删除）都会记录到 WAL。
 *
 * @param rel 向量引擎句柄
 * @param mode WAL 模式（SYNC/ASYNC/NONE）
 * @return 0 成功，-1 失败
 */
int vector_engine_enable_wal(void *rel, int mode);

/**
 * @brief 执行 WAL 检查点
 *
 * 刷新所有缓冲数据并更新检查点。
 *
 * @param rel 向量引擎句柄
 * @return 0 成功，-1 失败
 */
int vector_engine_checkpoint(void *rel);

/**
 * @brief 恢复向量集合
 *
 * 从 WAL 重放未提交的操作，恢复数据一致性。
 *
 * @param name 集合名称
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int vector_engine_recover(const char *name, const char *data_dir);

/**
 * @brief 获取 WAL 统计信息
 *
 * @param rel 向量引擎句柄
 * @param out_records 输出记录数
 * @param out_bytes 输出字节数
 * @return 0 成功，-1 失败
 */
int vector_engine_get_wal_stats(void *rel, uint64_t *out_records, uint64_t *out_bytes);

/* ========================================================================
 * 扩展查询接口 API (Wave 1-③ 新增)
 * ======================================================================== */

/**
 * @brief 执行范围查询 (距离阈值过滤)
 *
 * 返回距离小于指定阈值的向量。
 *
 * @param rel 向量引擎句柄
 * @param query 查询向量
 * @param query_dim 向量维度
 * @param max_distance 最大距离阈值
 * @param max_results 最大结果数量
 * @param results 搜索结果输出
 * @return 0 成功，-1 失败
 */
int vector_query_range(void *rel,
                       const float *query, int32_t query_dim,
                       float max_distance,
                       int32_t max_results,
                       vector_search_results_t *results);

/**
 * @brief 执行带过滤条件的向量查询
 *
 * 先执行向量搜索，再应用标量过滤条件。
 *
 * @param rel 向量引擎句柄
 * @param query 查询向量
 * @param query_dim 向量维度
 * @param top_k Top-K 数量
 * @param filter_column 过滤列名
 * @param filter_op 比较操作符 (CMP_EQ/CMP_NE/CMP_LT/CMP_LE/CMP_GT/CMP_GE)
 * @param filter_value 过滤值
 * @param results 搜索结果输出
 * @return 0 成功，-1 失败
 */
int vector_query_filtered(void *rel,
                          const float *query, int32_t query_dim,
                          int32_t top_k,
                          const char *filter_column,
                          int filter_op,
                          const void *filter_value,
                          vector_search_results_t *results);

/**
 * @brief 执行多索引混合查询
 *
 * 同时使用向量索引 (HNSW) 和文本索引 (BM25) 进行混合检索，
 * 使用 RRF (Reciprocal Rank Fusion) 融合结果。
 *
 * @param rel 向量引擎句柄
 * @param query 查询向量
 * @param query_dim 向量维度
 * @param text_query 文本查询字符串
 * @param top_k Top-K 数量
 * @param vector_weight 向量权重 (建议 0.7)
 * @param text_weight 文本权重 (建议 0.3)
 * @param results 搜索结果输出
 * @return 0 成功，-1 失败
 */
int vector_query_hybrid(void *rel,
                        const float *query, int32_t query_dim,
                        const char *text_query,
                        int32_t top_k,
                        float vector_weight,
                        float text_weight,
                        vector_search_results_t *results);

/**
 * @brief 查询配置结构
 */
typedef struct VectorQueryConfig_s {
    int32_t timeout_ms;           /**< 超时时间 (毫秒，默认 30000) */
    int32_t batch_size;           /**< 批次大小 (默认 1024) */
    int32_t ef_search;            /**< HNSW ef_search 参数 (默认 100) */
    int32_t nprobe;               /**< IVF nprobe 参数 (默认 10) */
    bool enable_rerank;           /**< 是否启用精排 (默认 false) */
    bool enable_profiling;        /**< 是否启用剖析 (默认 false) */
    DistanceMetric metric;        /**< 距离度量类型 */
} VectorQueryConfig;

/**
 * @brief 执行带配置的向量查询
 *
 * @param rel 向量引擎句柄
 * @param query 查询向量
 * @param query_dim 向量维度
 * @param top_k Top-K 数量
 * @param config 查询配置
 * @param results 搜索结果输出
 * @return 0 成功，-1 失败
 */
int vector_query_with_config(void *rel,
                             const float *query, int32_t query_dim,
                             int32_t top_k,
                             const VectorQueryConfig *config,
                             vector_search_results_t *results);

/**
 * @brief 设置默认查询配置
 *
 * 后续所有查询将使用此配置，直到被覆盖。
 *
 * @param rel 向量引擎句柄
 * @param config 查询配置
 * @return 0 成功，-1 失败
 */
int vector_engine_set_query_config(void *rel, const VectorQueryConfig *config);

/**
 * @brief 获取当前默认查询配置
 *
 * @param rel 向量引擎句柄
 * @param config 输出配置
 * @return 0 成功，-1 失败
 */
int vector_engine_get_query_config(void *rel, VectorQueryConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_ENGINE_H */
