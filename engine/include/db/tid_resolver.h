/**
 * @file tid_resolver.h
 * @brief TID 解析器 - 从 heap tuple 提取并使用 TID 进行更新/删除
 *
 * 解决 nodeModifyTable.c 硬编码 TID 的问题，提供统一的 TID 解析接口。
 */
#ifndef DB_TID_RESOLVER_H
#define DB_TID_RESOLVER_H

#include "heapam.h"
#include "db/buf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 行指针编号类型（页面内偏移号） */
typedef uint16_t OffsetNumber;

/**
 * @brief ItemPointerData 结构
 *
 * TID（Tuple Identifier）用于唯一标识表中的一行。
 * 格式：block_num(4B) + offset(2B) = 6 字节
 */
typedef struct ItemPointerData {
    BlockNumber ip_blkno;     /**< 块号 */
    OffsetNumber ip_posid;    /**< 页面内位置号（LinePointer 编号） */
} ItemPointerData;

/**
 * @brief TID 解析器
 *
 * 从 HeapTuple 中解析出 TID，并提供基于 TID 的更新/删除操作。
 */
typedef struct TIDResolver {
    BlockNumber     block_num;    /**< 块号 */
    OffsetNumber    offset;       /**< 偏移号 */
    ItemPointerData tid;          /**< 完整的 TID */
    Relation        rel;          /**< 关联的 Relation（用于更新/删除） */
} TIDResolver;

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief 从 heap tuple 获取真实 TID
 * @param tuple HeapTuple 元组数据
 * @return TIDResolver 指针，失败返回 NULL
 *
 * @note 返回的 TIDResolver 指向静态分配的内存，不可跨调用保留
 */
TIDResolver* tid_resolver_from_tuple(HeapTuple tuple);

/**
 * @brief 使用 TID 更新记录
 * @param resolver TID 解析器
 * @param new_data 新数据
 * @return 0 成功，-1 失败
 */
int tid_resolver_update(TIDResolver* resolver, const void* new_data);

/**
 * @brief 使用 TID 删除记录
 * @param resolver TID 解析器
 * @return 0 成功，-1 失败
 */
int tid_resolver_delete(TIDResolver* resolver);

#ifdef __cplusplus
}
#endif

#endif /* DB_TID_RESOLVER_H */
