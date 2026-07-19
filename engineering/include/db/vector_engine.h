/**
 * @file vector_engine.h
 * @brief 向量存储引擎公共接口
 *
 * 定义向量存储引擎的公共接口，供外部模块调用。
 */
#ifndef DB_VECTOR_ENGINE_PUBLIC_H
#define DB_VECTOR_ENGINE_PUBLIC_H

#include "storage_engine.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"  /* 引入 faiss_hnsw_t 类型 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 向量相关类型定义
 * ======================================================================== */

/**
 * @brief 度量类型
 */
typedef enum {
    METRIC_L2 = 0,          /**< 欧氏距离（L2） */
    METRIC_COSINE = 1,      /**< 余弦相似度 */
    METRIC_DOT = 2,         /**< 点积 */
} vector_metric_t;

/**
 * @brief 向量搜索结果
 */
typedef struct vector_search_result_s {
    uint64_t id;            /**< 向量 ID */
    float distance;         /**< 距离/相似度 */
} vector_search_result_t;

/**
 * @brief 向量搜索结果集
 */
typedef struct vector_search_results_s {
    vector_search_result_t *results;   /**< 结果数组 */
    int32_t count;                     /**< 结果数量 */
    int32_t capacity;                  /**< 容量 */
} vector_search_results_t;

/**
 * @brief 向量引擎数据库
 */
typedef struct vector_engine_db_s {
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
    struct faiss_hnsw *hnsw_index;  /**< HNSW 索引指针（faiss_hnsw_t*） */
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
    void *ivf_pq_index;        /**< IVF-PQ 索引指针 */
    bool use_ivf_pq;           /**< 是否使用 IVF-PQ 索引 */
    int ivf_pq_nlist;          /**< IVF 聚类数量 */
    int ivf_pq_nprobe;         /**< 搜索探针数量 */

    /* 并发控制 */
    void *lockmgr;             /**< 锁管理器 */
    void *rwlock;              /**< 读写锁（pthread_rwlock 或模拟） */
    bool use_lock;             /**< 是否启用锁 */

    /* WAL 持久化 */
    void *wal;                 /**< WAL 句柄（vector_wal_t*） */
    bool use_wal;              /**< 是否启用 WAL */
    uint32_t segment_id;       /**< WAL 段 ID */

    /* 当前激活的索引类型 */
    int active_index_type;     /**< 0=无, 1=HNSW, 2=IVF-PQ */
} vector_engine_db_t;

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
 * 持久化接口
 * ======================================================================== */

/**
 * @brief 向量索引保存结果
 */
typedef struct vector_persist_result_s {
    int32_t success;             /**< 是否成功 */
    int32_t error_code;          /**< 错误码 */
    char error_msg[256];         /**< 错误信息 */
    uint64_t bytes_written;      /**< 写入字节数 */
    uint64_t bytes_read;         /**< 读取字节数 */
} vector_persist_result_t;

/**
 * @brief 保存向量索引到文件
 *
 * @param rel 向量集合句柄
 * @param path 目标文件路径
 * @param result 保存结果（可为 NULL）
 * @return 0 成功，-1 失败
 */
int vector_index_save(void *rel, const char *path, vector_persist_result_t *result);

/**
 * @brief 从文件加载向量索引
 *
 * @param rel 向量集合句柄（已打开的集合）
 * @param path 源文件路径
 * @param result 加载结果（可为 NULL）
 * @return 0 成功，-1 失败
 */
int vector_index_load(void *rel, const char *path, vector_persist_result_t *result);

/**
 * @brief 检查索引文件是否有效
 *
 * @param path 文件路径
 * @return true 有效，false 无效或不存在
 */
bool vector_index_is_valid(const char *path);

/**
 * @brief 获取索引文件元数据
 *
 * @param path 文件路径
 * @param dims 维度输出
 * @param n_total 向量数量输出
 * @param metric 度量类型输出
 * @return 0 成功，-1 失败
 */
int vector_index_get_meta(const char *path, int32_t *dims, int32_t *n_total, int32_t *metric);

/* ========================================================================
 * 索引类型常量
 * ======================================================================== */

/** 无索引（暴力搜索） */
#define VEC_INDEX_NONE   0
/** HNSW 索引 */
#define VEC_INDEX_HNSW   1
/** IVF-PQ 索引 */
#define VEC_INDEX_IVF_PQ 2

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

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_ENGINE_PUBLIC_H */
