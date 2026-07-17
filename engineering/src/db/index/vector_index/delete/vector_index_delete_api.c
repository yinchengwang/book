/*
 * 向量索引删除和 GC 统一 API 实现
 */

#include <stdlib.h>
#include <string.h>

#include <db/index/vector_index/hnsw/faiss_hnsw.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>
#include <db/index/vector_index/delete/vector_index_delete_api.h>

/*
 * 删除上下文类型
 */
typedef enum {
    DELETE_CTX_HNSW,
    DELETE_CTX_DISKANN,
    DELETE_CTX_IVF_PQ
} delete_ctx_type_t;

/*
 * 统一删除上下文
 */
struct vector_index_delete_ctx {
    delete_ctx_type_t type;
    void *index;

    /* 统一的删除位图（用于不支持原生删除的索引） */
    void *delete_bitmap;
};

/*
 * 获取 HNSW 索引的删除位图
 */
static void *hnsw_get_delete_bitmap(void *index)
{
    if (!index) return NULL;
    return ((faiss_hnsw_t *)index)->delete_bitmap;
}

/*
 * 设置 HNSW 索引的删除位图
 */
static void hnsw_set_delete_bitmap(void *index, void *bitmap)
{
    if (!index) return;
    ((faiss_hnsw_t *)index)->delete_bitmap = bitmap;
}

/*
 * 获取 HNSW 索引的向量总数
 */
static int32_t hnsw_get_total(void *index)
{
    if (!index) return 0;
    return ((faiss_hnsw_t *)index)->n_total;
}

vector_index_delete_ctx_t *vector_index_delete_ctx_create_hnsw(void *hnsw_index)
{
    if (!hnsw_index) return NULL;

    vector_index_delete_ctx_t *ctx = (vector_index_delete_ctx_t *)malloc(
        sizeof(vector_index_delete_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(vector_index_delete_ctx_t));

    ctx->type          = DELETE_CTX_HNSW;
    ctx->index         = hnsw_index;
    ctx->delete_bitmap = hnsw_get_delete_bitmap(hnsw_index);

    return ctx;
}

vector_index_delete_ctx_t *vector_index_delete_ctx_create_diskann(void *diskann_index)
{
    /* DiskANN 暂未实现原生删除，使用统一位图 */
    if (!diskann_index) return NULL;

    vector_index_delete_ctx_t *ctx = (vector_index_delete_ctx_t *)malloc(
        sizeof(vector_index_delete_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(vector_index_delete_ctx_t));

    ctx->type          = DELETE_CTX_DISKANN;
    ctx->index         = diskann_index;
    /* TODO: DiskANN 原生删除位图 */
    ctx->delete_bitmap = NULL;

    return ctx;
}

vector_index_delete_ctx_t *vector_index_delete_ctx_create_ivf_pq(void *ivf_pq_index)
{
    /* IVF-PQ 暂未实现原生删除，使用统一位图 */
    if (!ivf_pq_index) return NULL;

    vector_index_delete_ctx_t *ctx = (vector_index_delete_ctx_t *)malloc(
        sizeof(vector_index_delete_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(vector_index_delete_ctx_t));

    ctx->type          = DELETE_CTX_IVF_PQ;
    ctx->index         = ivf_pq_index;
    /* TODO: IVF-PQ 原生删除位图 */
    ctx->delete_bitmap = NULL;

    return ctx;
}

void vector_index_delete_ctx_destroy(vector_index_delete_ctx_t *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

void *vector_index_delete_get_bitmap(vector_index_delete_ctx_t *ctx)
{
    if (!ctx) return NULL;

    if (ctx->type == DELETE_CTX_HNSW) {
        return hnsw_get_delete_bitmap(ctx->index);
    }

    return ctx->delete_bitmap;
}

int32_t vector_index_delete(vector_index_delete_ctx_t *ctx, int32_t id)
{
    if (!ctx || id < 0) return -1;

    switch (ctx->type) {
    case DELETE_CTX_HNSW:
        return faiss_hnsw_index_delete(ctx->index, id);

    case DELETE_CTX_DISKANN:
        /* TODO: DiskANN 原生删除 */
        if (ctx->delete_bitmap) {
            return vector_delete_mark(ctx->delete_bitmap, id);
        }
        return -1;

    case DELETE_CTX_IVF_PQ:
        /* TODO: IVF-PQ 原生删除 */
        if (ctx->delete_bitmap) {
            return vector_delete_mark(ctx->delete_bitmap, id);
        }
        return -1;

    default:
        return -1;
    }
}

int32_t vector_index_delete_batch(vector_index_delete_ctx_t *ctx,
                                  const int32_t *ids,
                                  int32_t n)
{
    if (!ctx || !ids || n <= 0) return 0;

    switch (ctx->type) {
    case DELETE_CTX_HNSW:
        return faiss_hnsw_index_delete_batch(ctx->index, ids, n);

    case DELETE_CTX_DISKANN:
    case DELETE_CTX_IVF_PQ:
        if (ctx->delete_bitmap) {
            return vector_delete_mark_batch(ctx->delete_bitmap, ids, n);
        }
        return -1;

    default:
        return -1;
    }
}

int32_t vector_index_undelete(vector_index_delete_ctx_t *ctx, int32_t id)
{
    if (!ctx || id < 0) return -1;

    switch (ctx->type) {
    case DELETE_CTX_HNSW:
        return faiss_hnsw_index_undelete(ctx->index, id);

    case DELETE_CTX_DISKANN:
    case DELETE_CTX_IVF_PQ:
        if (ctx->delete_bitmap) {
            return vector_delete_unmark(ctx->delete_bitmap, id);
        }
        return -1;

    default:
        return -1;
    }
}

bool vector_index_is_deleted(vector_index_delete_ctx_t *ctx, int32_t id)
{
    if (!ctx || id < 0) return false;

    switch (ctx->type) {
    case DELETE_CTX_HNSW:
        return faiss_hnsw_index_is_deleted(ctx->index, id);

    case DELETE_CTX_DISKANN:
    case DELETE_CTX_IVF_PQ:
        if (ctx->delete_bitmap) {
            return vector_delete_is_deleted(ctx->delete_bitmap, id);
        }
        return false;

    default:
        return false;
    }
}

int32_t vector_index_delete_get_stats(vector_index_delete_ctx_t *ctx,
                                      int32_t *total,
                                      int32_t *deleted,
                                      float *ratio)
{
    if (!ctx) return -1;

    int32_t total_count   = 0;
    int32_t deleted_count = 0;

    switch (ctx->type) {
    case DELETE_CTX_HNSW:
        total_count = hnsw_get_total(ctx->index);
        if (ctx->delete_bitmap || hnsw_get_delete_bitmap(ctx->index)) {
            void *bitmap = hnsw_get_delete_bitmap(ctx->index);
            vector_delete_stats_t stats;
            if (bitmap && vector_delete_get_stats(bitmap, &stats) == 0) {
                deleted_count = stats.deleted_count;
            }
        }
        break;

    case DELETE_CTX_DISKANN:
    case DELETE_CTX_IVF_PQ:
        if (ctx->delete_bitmap) {
            vector_delete_stats_t stats;
            if (vector_delete_get_stats(ctx->delete_bitmap, &stats) == 0) {
                total_count   = stats.total_vectors;
                deleted_count = stats.deleted_count;
            }
        }
        break;

    default:
        return -1;
    }

    if (total) *total = total_count;
    if (deleted) *deleted = deleted_count;
    if (ratio) {
        *ratio = (total_count > 0) ? ((float)deleted_count / (float)total_count) : 0.0f;
    }

    return 0;
}
