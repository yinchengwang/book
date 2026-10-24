/*
 * 向量索引删除和 GC 统一 API
 *
 * 为 HNSW/DiskANN/IVF-PQ 提供统一的删除和 GC 接口。
 */

#ifndef VECTOR_INDEX_DELETE_API_H
#define VECTOR_INDEX_DELETE_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 统一删除上下文（实际指向具体索引的删除模块）
 */
typedef struct vector_index_delete_ctx vector_index_delete_ctx_t;

/**
 * 为 HNSW 索引创建删除上下文
 *
 * @param hnsw_index HNSW 索引
 * @return 删除上下文，失败返回 NULL
 */
vector_index_delete_ctx_t *vector_index_delete_ctx_create_hnsw(void *hnsw_index);

/**
 * 为 DiskANN 索引创建删除上下文
 */
vector_index_delete_ctx_t *vector_index_delete_ctx_create_diskann(void *diskann_index);

/**
 * 为 IVF-PQ 索引创建删除上下文
 */
vector_index_delete_ctx_t *vector_index_delete_ctx_create_ivf_pq(void *ivf_pq_index);

/**
 * 销毁删除上下文
 */
void vector_index_delete_ctx_destroy(vector_index_delete_ctx_t *ctx);

/**
 * 获取删除位图
 *
 * @param ctx 删除上下文
 * @return 删除位图指针
 */
void *vector_index_delete_get_bitmap(vector_index_delete_ctx_t *ctx);

/**
 * 删除单个向量
 */
int32_t vector_index_delete(vector_index_delete_ctx_t *ctx, int32_t id);

/**
 * 批量删除向量
 */
int32_t vector_index_delete_batch(vector_index_delete_ctx_t *ctx, const int32_t *ids, int32_t n);

/**
 * 撤销删除（恢复向量）
 */
int32_t vector_index_undelete(vector_index_delete_ctx_t *ctx, int32_t id);

/**
 * 查询向量是否已删除
 */
bool vector_index_is_deleted(vector_index_delete_ctx_t *ctx, int32_t id);

/**
 * 获取删除统计信息
 */
int32_t vector_index_delete_get_stats(vector_index_delete_ctx_t *ctx,
                                     int32_t *total,
                                     int32_t *deleted,
                                     float *ratio);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_INDEX_DELETE_API_H */
