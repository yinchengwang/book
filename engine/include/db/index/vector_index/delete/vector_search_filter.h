/*
 * 向量查询结果过滤
 *
 * 提供已删除向量过滤、结果替换等功能。
 * 支持 HNSW/DiskANN/IVF-PQ 等多种索引类型。
 */

#ifndef VECTOR_SEARCH_FILTER_H
#define VECTOR_SEARCH_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 查询结果过滤上下文
 * 包含删除位图和必要的搜索参数
 */
typedef struct vector_search_filter_ctx {
    void   *delete_bitmap;       // vector_delete_bitmap_t*
    int32_t search_k;            // 搜索请求的 k 值
    int32_t max_extra_results;   // 需要额外搜索以补充被过滤的结果
} vector_search_filter_ctx_t;

/**
 * 创建过滤上下文
 *
 * @param delete_bitmap 删除位图
 * @param search_k      搜索的 k 值
 * @param extra_ratio   额外搜索比例（如 0.2 表示多搜 20%）
 * @return 过滤上下文
 */
vector_search_filter_ctx_t *vector_search_filter_create(void *delete_bitmap,
                                                        int32_t search_k,
                                                        float extra_ratio);

/**
 * 销毁过滤上下文
 */
void vector_search_filter_destroy(vector_search_filter_ctx_t *ctx);

/**
 * 过滤已删除的向量
 *
 * 将 vec_id 和 distance 数组中的已删除向量移除，
 * 保持结果按距离从小到大排序。
 *
 * @param ctx        过滤上下文
 * @param vec_id     输入/输出向量 ID 数组
 * @param distance   输入/输出距离数组
 * @param num_results 输入结果数量
 * @return 过滤后的结果数量
 */
int32_t vector_search_filter_deleted(vector_search_filter_ctx_t *ctx,
                                     int32_t *vec_id,
                                     float *distance,
                                     int32_t num_results);

/**
 * 检查向量是否已删除（通过 delete_bitmap）
 *
 * @param delete_bitmap vector_delete_bitmap_t*
 * @param vec_id        向量 ID
 * @return true 已删除，false 未删除或无效 ID
 */
bool vector_check_deleted(const void *delete_bitmap, int32_t vec_id);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_SEARCH_FILTER_H */
