#ifndef FAISS_HNSW_INDEX_H
#define FAISS_HNSW_INDEX_H

#include <stdint.h>
#include <stdbool.h>

#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>
#include <db/index/vector_ref.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VERBOSE 0

typedef struct faiss_hnsw_quantization_params {
    int32_t pq_m;
    int32_t pq_bits;
    int32_t train_max_vectors;
    bool retrain_on_add;
} faiss_hnsw_quantization_params_t;

/*
 * 删除位图前向声明
 */
typedef struct vector_delete_bitmap vector_delete_bitmap_t;

/*
 * Heap 向量存储前向声明
 */
typedef struct heap_vector_store heap_vector_store_t;

/*
 * 索引配置和存储后端前向声明
 * 使用前向声明避免循环依赖（index_config.h 依赖 storage_backend.h）
 */
typedef struct index_config index_config_t;
typedef struct storage_backend storage_backend_t;
typedef struct persist_control persist_control_t;

typedef struct faiss_hnsw {
    float *vectors;
    uint8_t *codes;
    int32_t n_total;
    int32_t dims;
    int32_t M;
    int32_t ef_construction;
    int32_t ef_search;
    distance_metric_t metric;
    quantization_type_t quantization_type;
    int32_t code_size;
    int32_t *levels;
    uint32_t levels_size;
    int32_t *offsets;
    uint32_t offsets_size;
    int32_t *nbs;
    uint32_t nb_size;
    float *assign_probas;
    uint32_t assign_probas_size;
    int32_t *cum_nneighbor_per_level;
    uint32_t cum_nneighbor_per_level_size;
    int32_t entry_pointer;
    int32_t max_level;
    faiss_hnsw_quantization_params_t quantization_params;
    quantizer_config_t quantizer_config;
    quantizer_t *quantizer;

    /* 删除标记位图 */
    vector_delete_bitmap_t *delete_bitmap;

    /* 存储后端（持久化抽象，索引生命周期内由索引负责释放） */
    storage_backend_t *storage;
    persist_control_t *persist_ctrl;

    /*
     * Heap 向量存储（Single Source of Truth）。
     *
     * - heap_store != NULL 时：
     *     vector_refs[i] 保存第 i 个向量在 Heap 中的位置，
     *     索引本身不再持有完整向量（index->vectors 保持为 NULL 或保留为空）。
     *     搜索阶段 2 通过 heap_vector_get + 精确距离重排序。
     * - heap_store == NULL 时：
     *     走兼容路径，向量直接存放在 index->vectors 连续数组中（历史行为）。
     */
    heap_vector_store_t *heap_store;

    /* 与 n_total 对齐的向量引用数组（仅当 heap_store != NULL 时使用）。 */
    vector_ref_t *vector_refs;
    uint32_t vector_refs_size;
} faiss_hnsw_t;

faiss_hnsw_t *faiss_hnsw_index_create(int32_t M,
                                      int32_t dims,
                                      int32_t ef_construction,
                                      distance_metric_t metric,
                                      quantization_type_t quantization_type);

/**
 * @brief 使用 index_config 创建 HNSW 索引
 *
 * 根据配置中的存储后端类型与持久化开关，自动选择合适的存储后端：
 *   - persist_enabled=false 或 storage_type==STORAGE_BACKEND_MEMORY：纯内存后端
 *   - persist_enabled=true 且 storage_type==STORAGE_BACKEND_PAGE_FILE：页面文件后端
 *   - persist_enabled=true 且 storage_type==STORAGE_BACKEND_MMAP：内存映射后端
 *
 * 索引持有 storage 后端的所有权，由 faiss_hnsw_index_drop 统一释放。
 *
 * @param config 索引配置（已通过 index_config_validate 校验）
 * @return 新创建的 HNSW 索引；失败返回 NULL
 */
faiss_hnsw_t *faiss_hnsw_index_create_with_config(const index_config_t *config);

/**
 * @brief 为已有索引绑定/替换存储后端
 *
 * 将旧后端替换为新后端；若 index 已持有 storage，原后端会被销毁。
 * 调用方不得在外部释放 backend，backend 所有权由索引接管。
 *
 * @param index 目标索引
 * @param backend 新后端指针
 * @return 0 成功；非 0 失败
 */
int faiss_hnsw_index_set_storage(faiss_hnsw_t *index, storage_backend_t *backend);

/**
 * @brief 为索引绑定/替换 Heap 向量存储
 *
 * 绑定后索引将只保存向量引用（vector_ref_t），不再在 index->vectors 中
 * 持有完整向量；插入路径走 heap_vector_insert，搜索路径走两阶段 +
 * heap_vector_get 重排序。
 *
 * 调用方负责 heap_store 的生命周期管理（索引不接管所有权）。
 * 索引销毁时仅置空 heap_store 指针，不调用 heap_vector_store_destroy()。
 *
 * @param index     目标索引
 * @param heap_store Heap 存储（必须与 index->dims 一致）
 * @return 0 成功；非 0 失败
 */
int faiss_hnsw_index_set_heap_store(faiss_hnsw_t *index, heap_vector_store_t *heap_store);

/**
 * @brief 获取当前索引绑定的 Heap 向量存储
 *
 * @param index 目标索引
 * @return Heap 指针；未绑定或参数非法时返回 NULL
 */
heap_vector_store_t *faiss_hnsw_index_get_heap_store(const faiss_hnsw_t *index);
quantization_type_t faiss_hnsw_index_quantization_type(const faiss_hnsw_t *index);
int32_t faiss_hnsw_index_set_quantization_params(faiss_hnsw_t *index,
                                                 const faiss_hnsw_quantization_params_t *params);
int32_t faiss_hnsw_index_get_quantization_params(const faiss_hnsw_t *index,
                                                 faiss_hnsw_quantization_params_t *params);
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vector);
int32_t faiss_hnsw_vector_algo_insert(faiss_hnsw_t *index, int32_t n, const float *vector);
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query, int32_t k, int32_t ef_search, float *distance, int32_t *vec_id);
void faiss_hnsw_index_drop(faiss_hnsw_t *index);

/* 删除相关 API */
int32_t faiss_hnsw_index_delete(faiss_hnsw_t *index, int32_t id);
int32_t faiss_hnsw_index_delete_batch(faiss_hnsw_t *index, const int32_t *ids, int32_t n);
int32_t faiss_hnsw_index_undelete(faiss_hnsw_t *index, int32_t id);
bool faiss_hnsw_index_is_deleted(const faiss_hnsw_t *index, int32_t id);

#ifdef __cplusplus
}
#endif

#endif