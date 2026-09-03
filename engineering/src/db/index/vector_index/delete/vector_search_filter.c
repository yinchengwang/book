/*
 * 向量查询结果过滤实现
 */

#include <stdlib.h>
#include <string.h>

#include <db/index/vector_index/delete/vector_delete_bitmap.h>
#include <db/index/vector_index/delete/vector_search_filter.h>

vector_search_filter_ctx_t *vector_search_filter_create(void *delete_bitmap,
                                                        int32_t search_k,
                                                        float extra_ratio)
{
    if (search_k <= 0) {
        return NULL;
    }

    vector_search_filter_ctx_t *ctx = (vector_search_filter_ctx_t *)malloc(
        sizeof(vector_search_filter_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->delete_bitmap     = delete_bitmap;
    ctx->search_k          = search_k;
    ctx->max_extra_results = (int32_t)(search_k * extra_ratio);

    return ctx;
}

void vector_search_filter_destroy(vector_search_filter_ctx_t *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

bool vector_check_deleted(const void *delete_bitmap, int32_t vec_id)
{
    if (!delete_bitmap) {
        return false;
    }
    return vector_delete_is_deleted(
        (const vector_delete_bitmap_t *)delete_bitmap, vec_id);
}

int32_t vector_search_filter_deleted(vector_search_filter_ctx_t *ctx,
                                     int32_t *vec_id,
                                     float *distance,
                                     int32_t num_results)
{
    if (!ctx || !vec_id || !distance || num_results <= 0) {
        return 0;
    }

    /*
     * 过滤策略：
     * 1. 遍历所有结果
     * 2. 跳过已删除的向量
     * 3. 将未删除的向量紧凑排列
     */
    int32_t write_idx = 0;

    for (int32_t read_idx = 0; read_idx < num_results; read_idx++) {
        if (vec_id[read_idx] < 0) {
            /* 无效 ID，跳过 */
            continue;
        }

        if (!vector_check_deleted(ctx->delete_bitmap, vec_id[read_idx])) {
            /* 未删除，保留 */
            if (write_idx != read_idx) {
                vec_id[write_idx]   = vec_id[read_idx];
                distance[write_idx]  = distance[read_idx];
            }
            write_idx++;
        }
        /* 已删除的向量被跳过 */
    }

    return write_idx;
}
