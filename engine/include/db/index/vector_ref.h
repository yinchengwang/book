/**
 * @file vector_ref.h
 * @brief 向量引用结构定义
 *
 * 用于解决向量膨胀问题：
 * - Heap 存储完整向量（Single Source of Truth）
 * - 索引只存储向量引用（page_id + offset + length）
 * - 查询时通过引用从 Heap 获取向量，避免重复存储
 *
 * 设计要点：
 * - 引用采用定长结构（12 字节），适合作为索引 Payload 直接嵌入 B+Tree 等结构
 * - page_id_t 复用 storage_backend.h 的定义，保持与索引子系统一致
 * - 提供有效性与等价比较的内联辅助函数，方便索引模块直接调用
 */
#ifndef DB_VECTOR_REF_H
#define DB_VECTOR_REF_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "db/index/storage_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 无效向量引用（哨兵值） */
#define VECTOR_REF_INVALID { INVALID_PAGE_ID, 0u, 0u }

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 向量引用结构
 *
 * 仅描述向量在 Heap 中的位置，不持有向量数据本身。
 * 索引中存储该结构，可显著降低向量膨胀率（从 ~2.2x 降至 ~1.2x）。
 */
typedef struct vector_ref {
    page_id_t heap_page_id;   /**< Heap 页面 ID（指向存储完整向量的页面） */
    uint32_t  offset;         /**< 页内偏移（字节） */
    uint32_t  length;         /**< 向量长度（字节） */
} vector_ref_t;

/* ============================================================
 * 内联辅助函数
 * ============================================================ */

/**
 * @brief 检查引用是否有效
 *
 * 判定条件：
 * - 指针非空
 * - heap_page_id >= 0（不等于 INVALID_PAGE_ID）
 *
 * @param ref  待校验的引用指针
 * @return     true 表示引用指向一个合法的 Heap 页面
 */
static inline bool vector_ref_is_valid(const vector_ref_t *ref)
{
    return ref != NULL && ref->heap_page_id != INVALID_PAGE_ID;
}

/**
 * @brief 比较两个引用是否等价
 *
 * 比较三个字段是否完全一致：page_id、offset、length。
 * 任一指针为 NULL 时返回 false。
 *
 * @param a  引用 A
 * @param b  引用 B
 * @return   true 表示两个引用指向同一位置且长度相同
 */
static inline bool vector_ref_equal(const vector_ref_t *a, const vector_ref_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->heap_page_id == b->heap_page_id
        && a->offset == b->offset
        && a->length == b->length;
}

/**
 * @brief 创建无效引用
 *
 * @return VECTOR_REF_INVALID 哨兵值
 */
static inline vector_ref_t vector_ref_make_invalid(void)
{
    vector_ref_t ref = VECTOR_REF_INVALID;
    return ref;
}

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_REF_H */