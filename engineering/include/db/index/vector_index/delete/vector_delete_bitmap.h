/*
 * 向量删除标记位图
 *
 * 提供墓碑式删除标记，支持 HNSW/DiskANN/IVF-PQ 索引的软删除需求。
 * 删除操作仅标记状态，不立即物理删除，物理删除由 GC 后台线程完成。
 */

#ifndef VECTOR_DELETE_BITMAP_H
#define VECTOR_DELETE_BITMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 删除标记统计信息
 */
typedef struct vector_delete_stats {
    int32_t max_id;          // 支持的最大向量 ID
    int32_t total_vectors;   // 位图覆盖的向量总数
    int32_t deleted_count;    // 已删除的向量数量
    float  deleted_ratio;    // 删除比例 (deleted_count / total_vectors)
} vector_delete_stats_t;

/*
 * 删除标记位图结构
 *
 * 使用位图存储删除状态，每 8 个向量占用 1 字节。
 * 支持动态扩容和并发访问。
 */
typedef struct vector_delete_bitmap {
    uint8_t  *bitmap;        // 删除标记位图
    int32_t   max_id;        // 当前支持的最大向量 ID
    int32_t   capacity;      // 位图容量 (字节数 * 8)
    int32_t   deleted_count; // 已删除的向量数量
    pthread_mutex_t lock;     // 并发访问锁
} vector_delete_bitmap_t;

/**
 * 创建删除标记位图
 *
 * @param initial_capacity 初始容量（向量数量）
 * @return 位图结构指针，失败返回 NULL
 */
vector_delete_bitmap_t *vector_delete_bitmap_create(int32_t initial_capacity);

/**
 * 销毁删除标记位图
 *
 * @param bitmap 位图指针
 */
void vector_delete_bitmap_destroy(vector_delete_bitmap_t *bitmap);

/**
 * 标记向量为已删除
 *
 * @param bitmap 位图指针
 * @param id     向量 ID
 * @return 0 成功，-1 失败
 */
int32_t vector_delete_mark(vector_delete_bitmap_t *bitmap, int32_t id);

/**
 * 撤销删除标记（恢复向量）
 *
 * @param bitmap 位图指针
 * @param id     向量 ID
 * @return 0 成功，-1 失败
 */
int32_t vector_delete_unmark(vector_delete_bitmap_t *bitmap, int32_t id);

/**
 * 查询向量是否已删除
 *
 * @param bitmap 位图指针
 * @param id     向量 ID
 * @return true 已删除，false 未删除
 */
bool vector_delete_is_deleted(const vector_delete_bitmap_t *bitmap, int32_t id);

/**
 * 批量标记删除
 *
 * @param bitmap 位图指针
 * @param ids    向量 ID 数组
 * @param n      数组长度
 * @return 成功标记的数量
 */
int32_t vector_delete_mark_batch(vector_delete_bitmap_t *bitmap, const int32_t *ids, int32_t n);

/**
 * 批量撤销删除
 *
 * @param bitmap 位图指针
 * @param ids    向量 ID 数组
 * @param n      数组长度
 * @return 成功撤销的数量
 */
int32_t vector_delete_unmark_batch(vector_delete_bitmap_t *bitmap, const int32_t *ids, int32_t n);

/**
 * 获取删除统计信息
 *
 * @param bitmap 位图指针
 * @param stats  统计信息输出
 * @return 0 成功，-1 失败
 */
int32_t vector_delete_get_stats(const vector_delete_bitmap_t *bitmap, vector_delete_stats_t *stats);

/**
 * 清空所有删除标记
 *
 * @param bitmap 位图指针
 */
void vector_delete_clear(vector_delete_bitmap_t *bitmap);

/**
 * 获取已删除向量 ID 列表（用于 GC）
 *
 * @param bitmap      位图指针
 * @param out_ids     输出数组（外部分配）
 * @param max_out     输出数组容量
 * @return 实际返回的数量
 */
int32_t vector_delete_get_deleted_ids(const vector_delete_bitmap_t *bitmap, int32_t *out_ids, int32_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_DELETE_BITMAP_H */
