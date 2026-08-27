/**
 * @file gpu_vector_index.h
 * @brief GPU 向量索引公共 API
 *
 * 提供 GPU 加速的向量索引接口，支持 CUDA/OpenCL 双后端。
 * 包含 GPU-HNSW 和 GPU-IVF 索引实现。
 */
#ifndef DB_GPU_VECTOR_INDEX_H
#define DB_GPU_VECTOR_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * GPU 后端类型
 * ======================================================================== */

/**
 * @brief GPU 后端类型
 */
typedef enum {
    GPU_BACKEND_NONE = 0,      /**< 无 GPU（仅 CPU） */
    GPU_BACKEND_CUDA = 1,      /**< NVIDIA CUDA */
    GPU_BACKEND_OPENCL = 2,    /**< OpenCL（AMD/Intel/NVIDIA） */
    GPU_BACKEND_SYCL = 3,      /**< Intel SYCL */
} gpu_backend_t;

/* ========================================================================
 * GPU 设备信息
 * ======================================================================== */

/**
 * @brief GPU 设备信息
 */
typedef struct gpu_device_info_s {
    int device_id;             /**< 设备 ID */
    gpu_backend_t backend;     /**< 后端类型 */
    char name[256];            /**< 设备名称 */
    size_t total_memory;       /**< 总内存（字节） */
    size_t free_memory;        /**< 可用内存（字节） */
    int compute_units;         /**< 计算单元数 */
    int max_threads_per_block; /**< 每块最大线程数 */
    int max_grid_dim[3];       /**< 最大网格维度 */
    int max_block_dim[3];      /**< 最大块维度 */
} gpu_device_info_t;

/**
 * @brief GPU 设备列表
 */
typedef struct gpu_device_list_s {
    gpu_device_info_t *devices;    /**< 设备数组 */
    int32_t count;                 /**< 设备数量 */
    int32_t selected_device;       /**< 当前选中设备 ID */
} gpu_device_list_t;

/* ========================================================================
 * GPU 内存管理
 * ======================================================================== */

/**
 * @brief GPU 内存分配类型
 */
typedef enum {
    GPU_MEM_READ_WRITE = 0,    /**< 读写内存 */
    GPU_MEM_READ_ONLY = 1,     /**< 只读内存 */
    GPU_MEM_WRITE_ONLY = 2,    /**< 只写内存 */
    GPU_MEM_PINNED = 3,        /**< 页锁定内存（Host 加速传输） */
} gpu_memory_type_t;

/**
 * @brief GPU 内存句柄（不透明）
 */
typedef struct gpu_memory_s {
    void *host_ptr;            /**< Host 端指针 */
    void *device_ptr;          /**< Device 端指针 */
    size_t size;               /**< 内存大小（字节） */
    gpu_memory_type_t type;    /**< 内存类型 */
    int device_id;             /**< 所属设备 ID */
} gpu_memory_t;

/* ========================================================================
 * 索引类型与配置
 * ======================================================================== */

/**
 * @brief GPU 索引类型
 */
typedef enum {
    GPU_INDEX_HNSW = 0,        /**< GPU 加速 HNSW */
    GPU_INDEX_IVF = 1,         /**< GPU 加速 IVF */
    GPU_INDEX_IVF_PQ = 2,      /**< GPU 加速 IVF-PQ */
} gpu_index_type_t;

/**
 * @brief GPU-HNSW 配置
 */
typedef struct gpu_hnsw_config_s {
    int32_t dim;               /**< 向量维度 */
    int32_t M;                 /**< 每层最大连接数 */
    int32_t ef_construction;   /**< 构建时搜索范围 */
    int32_t ef_search;         /**< 搜索时搜索范围 */
    int32_t max_elements;      /**< 最大元素数量 */
    int32_t metric;            /**< 度量类型：0=L2, 1=IP, 2=Cosine */
} gpu_hnsw_config_t;

/**
 * @brief GPU-IVF 配置
 */
typedef struct gpu_ivf_config_s {
    int32_t dim;               /**< 向量维度 */
    int32_t nlist;             /**< 聚类中心数量 */
    int32_t nprobe;            /**< 搜索探针数量 */
    int32_t max_elements;      /**< 最大元素数量 */
    int32_t metric;            /**< 度量类型：0=L2, 1=IP, 2=Cosine */
} gpu_ivf_config_t;

/**
 * @brief GPU-IVF-PQ 配置
 */
typedef struct gpu_ivf_pq_config_s {
    int32_t dim;               /**< 向量维度 */
    int32_t nlist;             /**< 聚类中心数量 */
    int32_t nprobe;            /**< 搜索探针数量 */
    int32_t pq_m;              /**< PQ 子空间数 */
    int32_t pq_nbits;          /**< PQ 每子空间位数 */
    int32_t max_elements;      /**< 最大元素数量 */
    int32_t metric;            /**< 度量类型：0=L2, 1=IP, 2=Cosine */
} gpu_ivf_pq_config_t;

/* ========================================================================
 * 搜索结果
 * ======================================================================== */

/**
 * @brief GPU 搜索结果项
 */
typedef struct gpu_search_result_item_s {
    int32_t id;                /**< 向量 ID */
    float distance;            /**< 距离/相似度 */
} gpu_search_result_item_t;

/**
 * @brief GPU 搜索结果集
 */
typedef struct gpu_search_results_s {
    gpu_search_result_item_t *items;  /**< 结果数组 */
    int32_t count;                     /**< 结果数量 */
    int32_t capacity;                  /**< 容量 */
    float total_time_ms;               /**< 总耗时（毫秒） */
} gpu_search_results_t;

/* ========================================================================
 * GPU 索引句柄（不透明）
 * ======================================================================== */

typedef struct gpu_hnsw_index_s gpu_hnsw_index_t;  /**< GPU-HNSW 索引句柄 */
typedef struct gpu_ivf_index_s gpu_ivf_index_t;    /**< GPU-IVF 索引句柄 */
typedef struct gpu_ivf_pq_index_s gpu_ivf_pq_index_t; /**< GPU-IVF-PQ 索引句柄 */

/* ========================================================================
 * GPU 设备管理 API
 * ======================================================================== */

/**
 * @brief 初始化 GPU 子系统
 * @return 0 成功，负数失败
 */
int gpu_init(void);

/**
 * @brief 关闭 GPU 子系统
 * @return 0 成功
 */
int gpu_shutdown(void);

/**
 * @brief 获取可用 GPU 设备列表
 * @param backend 指定后端（GPU_BACKEND_NONE 表示自动检测）
 * @return 设备列表，失败返回 NULL
 */
gpu_device_list_t *gpu_get_device_list(gpu_backend_t backend);

/**
 * @brief 释放设备列表
 * @param list 设备列表
 */
void gpu_free_device_list(gpu_device_list_t *list);

/**
 * @brief 选择 GPU 设备
 * @param device_id 设备 ID
 * @return 0 成功，负数失败
 */
int gpu_select_device(int device_id);

/**
 * @brief 获取当前选中设备信息
 * @return 设备信息，失败返回 NULL
 */
const gpu_device_info_t *gpu_get_current_device(void);

/**
 * @brief 同步设备
 */
void gpu_sync(void);

/* ========================================================================
 * GPU 内存管理 API
 * ======================================================================== */

/**
 * @brief 分配 GPU 内存
 * @param size 内存大小（字节）
 * @param type 内存类型
 * @return 内存句柄，失败返回 NULL
 */
gpu_memory_t *gpu_malloc(size_t size, gpu_memory_type_t type);

/**
 * @brief 释放 GPU 内存
 * @param mem 内存句柄
 */
void gpu_free(gpu_memory_t *mem);

/**
 * @brief Host 到 Device 数据传输
 * @param dst 目标设备内存
 * @param src 源主机内存
 * @param size 传输大小
 * @return 0 成功，负数失败
 */
int gpu_memcpy_h2d(gpu_memory_t *dst, const void *src, size_t size);

/**
 * @brief Device 到 Host 数据传输
 * @param dst 目标主机内存
 * @param src 源设备内存
 * @param size 传输大小
 * @return 0 成功，负数失败
 */
int gpu_memcpy_d2h(void *dst, const gpu_memory_t *src, size_t size);

/**
 * @brief Device 到 Device 数据传输
 * @param dst 目标设备内存
 * @param src 源设备内存
 * @param size 传输大小
 * @return 0 成功，负数失败
 */
int gpu_memcpy_d2d(gpu_memory_t *dst, const gpu_memory_t *src, size_t size);

/* ========================================================================
 * GPU-HNSW 索引 API
 * ======================================================================== */

/**
 * @brief 创建 GPU-HNSW 索引
 * @param config 索引配置
 * @return 索引句柄，失败返回 NULL
 */
gpu_hnsw_index_t *gpu_hnsw_create(const gpu_hnsw_config_t *config);

/**
 * @brief 销毁 GPU-HNSW 索引
 * @param index 索引句柄
 */
void gpu_hnsw_destroy(gpu_hnsw_index_t *index);

/**
 * @brief 插入向量
 * @param index 索引句柄
 * @param vectors 向量数据 [n][dim]
 * @param n 向量数量
 * @param ids 向量 ID（可为 NULL，使用顺序 ID）
 * @return 成功数量，失败返回负数
 */
int32_t gpu_hnsw_insert(gpu_hnsw_index_t *index, const float *vectors,
                        int32_t n, const int32_t *ids);

/**
 * @brief 批量插入向量
 * @param index 索引句柄
 * @param vectors 向量数据
 * @param n 向量数量
 * @param ids 向量 ID
 * @return 成功数量，失败返回负数
 */
int32_t gpu_hnsw_insert_batch(gpu_hnsw_index_t *index, const float *vectors,
                              int32_t n, const int32_t *ids);

/**
 * @brief 搜索最近邻
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数量
 * @return 搜索结果集，失败返回 NULL
 */
gpu_search_results_t *gpu_hnsw_search(gpu_hnsw_index_t *index,
                                      const float *query, int32_t k);

/**
 * @brief 批量搜索
 * @param index 索引句柄
 * @param queries 查询向量数组 [n][dim]
 * @param n_queries 查询数量
 * @param k 返回结果数量
 * @param ids 输出结果 ID 数组 [n_queries][k]
 * @param distances 输出距离数组 [n_queries][k]
 * @return 成功数量，失败返回负数
 */
int32_t gpu_hnsw_search_batch(gpu_hnsw_index_t *index, const float *queries,
                              int32_t n_queries, int32_t k,
                              int32_t *ids, float *distances);

/**
 * @brief 设置搜索参数
 * @param index 索引句柄
 * @param ef_search 新的搜索范围
 */
void gpu_hnsw_set_ef_search(gpu_hnsw_index_t *index, int32_t ef_search);

/**
 * @brief 获取索引统计信息
 * @param index 索引句柄
 * @param num_vectors 输出向量数量
 * @param memory_usage 输出内存占用
 */
void gpu_hnsw_get_stats(gpu_hnsw_index_t *index, int32_t *num_vectors,
                        size_t *memory_usage);

/* ========================================================================
 * GPU-IVF 索引 API
 * ======================================================================== */

/**
 * @brief 创建 GPU-IVF 索引
 * @param config 索引配置
 * @return 索引句柄，失败返回 NULL
 */
gpu_ivf_index_t *gpu_ivf_create(const gpu_ivf_config_t *config);

/**
 * @brief 销毁 GPU-IVF 索引
 * @param index 索引句柄
 */
void gpu_ivf_destroy(gpu_ivf_index_t *index);

/**
 * @brief 训练索引
 * @param index 索引句柄
 * @param vectors 训练向量数据
 * @param n 训练向量数量
 * @return 0 成功，负数失败
 */
int32_t gpu_ivf_train(gpu_ivf_index_t *index, const float *vectors, int32_t n);

/**
 * @brief 插入向量
 * @param index 索引句柄
 * @param vectors 向量数据
 * @param n 向量数量
 * @param ids 向量 ID
 * @return 成功数量，失败返回负数
 */
int32_t gpu_ivf_insert(gpu_ivf_index_t *index, const float *vectors,
                       int32_t n, const int32_t *ids);

/**
 * @brief 搜索最近邻
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数量
 * @return 搜索结果集，失败返回 NULL
 */
gpu_search_results_t *gpu_ivf_search(gpu_ivf_index_t *index,
                                     const float *query, int32_t k);

/**
 * @brief 批量搜索
 * @param index 索引句柄
 * @param queries 查询向量数组
 * @param n_queries 查询数量
 * @param k 返回结果数量
 * @param ids 输出结果 ID 数组
 * @param distances 输出距离数组
 * @return 成功数量，失败返回负数
 */
int32_t gpu_ivf_search_batch(gpu_ivf_index_t *index, const float *queries,
                             int32_t n_queries, int32_t k,
                             int32_t *ids, float *distances);

/**
 * @brief 设置探针数量
 * @param index 索引句柄
 * @param nprobe 探针数量
 */
void gpu_ivf_set_nprobe(gpu_ivf_index_t *index, int32_t nprobe);

/* ========================================================================
 * GPU-IVF-PQ 索引 API
 * ======================================================================== */

/**
 * @brief 创建 GPU-IVF-PQ 索引
 * @param config 索引配置
 * @return 索引句柄，失败返回 NULL
 */
gpu_ivf_pq_index_t *gpu_ivf_pq_create(const gpu_ivf_pq_config_t *config);

/**
 * @brief 销毁 GPU-IVF-PQ 索引
 * @param index 索引句柄
 */
void gpu_ivf_pq_destroy(gpu_ivf_pq_index_t *index);

/**
 * @brief 训练索引
 * @param index 索引句柄
 * @param vectors 训练向量数据
 * @param n 训练向量数量
 * @return 0 成功，负数失败
 */
int32_t gpu_ivf_pq_train(gpu_ivf_pq_index_t *index, const float *vectors, int32_t n);

/**
 * @brief 插入向量
 * @param index 索引句柄
 * @param vectors 向量数据
 * @param n 向量数量
 * @param ids 向量 ID
 * @return 成功数量，失败返回负数
 */
int32_t gpu_ivf_pq_insert(gpu_ivf_pq_index_t *index, const float *vectors,
                          int32_t n, const int32_t *ids);

/**
 * @brief 搜索最近邻
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数量
 * @return 搜索结果集，失败返回 NULL
 */
gpu_search_results_t *gpu_ivf_pq_search(gpu_ivf_pq_index_t *index,
                                        const float *query, int32_t k);

/**
 * @brief 批量搜索
 * @param index 索引句柄
 * @param queries 查询向量数组
 * @param n_queries 查询数量
 * @param k 返回结果数量
 * @param ids 输出结果 ID 数组
 * @param distances 输出距离数组
 * @return 成功数量，失败返回负数
 */
int32_t gpu_ivf_pq_search_batch(gpu_ivf_pq_index_t *index, const float *queries,
                                int32_t n_queries, int32_t k,
                                int32_t *ids, float *distances);

/**
 * @brief 设置探针数量
 * @param index 索引句柄
 * @param nprobe 探针数量
 */
void gpu_ivf_pq_set_nprobe(gpu_ivf_pq_index_t *index, int32_t nprobe);

/* ========================================================================
 * 结果集管理
 * ======================================================================== */

/**
 * @brief 释放搜索结果集
 * @param results 搜索结果集
 */
void gpu_free_results(gpu_search_results_t *results);

#ifdef __cplusplus
}
#endif

#endif /* DB_GPU_VECTOR_INDEX_H */
