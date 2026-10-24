/**
 * @file tid_resolver.c
 * @brief TID 解析器实现
 *
 * 从 heap tuple 提取 TID，并基于 TID 提供更新/删除操作。
 */
#include "db/tid_resolver.h"
#include "db/rel.h"
#include "db/buf.h"
#include <string.h>

/* ============================================================
 * 静态变量
 * ============================================================ */

/** 静态分配的解析器（避免频繁内存分配） */
static TIDResolver g_resolver;

/* ============================================================
 * 函数实现
 * ============================================================ */

/**
 * @brief 从 heap tuple 获取真实 TID
 * @param tuple HeapTuple 元组数据
 * @return TIDResolver 指针
 */
TIDResolver* tid_resolver_from_tuple(HeapTuple tuple)
{
    if (tuple == NULL) {
        return NULL;
    }

    HeapTupleTableData *tdata = (HeapTupleTableData *)tuple;

    /* 提取 TID 信息：t_tid 低 16 位为 block_num，高 16 位为 offset */
    g_resolver.block_num = (BlockNumber)(tdata->t_tid & 0xFFFF);
    g_resolver.offset = (OffsetNumber)((tdata->t_tid >> 16) & 0xFFFF);

    /* 构造完整的 TID */
    g_resolver.tid.ip_blkno = g_resolver.block_num;
    g_resolver.tid.ip_posid = g_resolver.offset;

    /* Relation 需要由调用者通过 tid_resolver_set_relation 设置 */
    g_resolver.rel = NULL;

    return &g_resolver;
}

/**
 * @brief 设置解析器关联的 Relation
 * @param resolver TID 解析器
 * @param rel Relation
 */
void tid_resolver_set_relation(TIDResolver *resolver, Relation rel)
{
    if (resolver != NULL && rel != NULL) {
        resolver->rel = rel;
    }
}

/**
 * @brief 使用 TID 更新记录
 * @param resolver TID 解析器
 * @param new_data 新数据
 * @return 0 成功，-1 失败
 */
int tid_resolver_update(TIDResolver* resolver, const void* new_data)
{
    if (resolver == NULL || new_data == NULL) {
        return -1;
    }

    /* 如果没有预设 Relation，无法执行更新 */
    if (resolver->rel == NULL) {
        return -1;
    }

    /* 构造 TID（6 字节） */
    char tid[6];
    memcpy(tid, &resolver->tid.ip_blkno, 4);
    memcpy(tid + 4, &resolver->tid.ip_posid, 2);

    /* 调用 heap_update 进行更新 */
    int ret = heap_update(resolver->rel, tid, new_data, 0,
                          0,  /* cid */
                          0,  /* options */
                          NULL,  /* bistate */
                          0); /* lockmode */

    return ret;
}

/**
 * @brief 使用 TID 删除记录
 * @param resolver TID 解析器
 * @return 0 成功，-1 失败
 */
int tid_resolver_delete(TIDResolver* resolver)
{
    if (resolver == NULL) {
        return -1;
    }

    /* 如果没有预设 Relation，无法执行删除 */
    if (resolver->rel == NULL) {
        return -1;
    }

    /* 构造 TID（6 字节） */
    char tid[6];
    memcpy(tid, &resolver->tid.ip_blkno, 4);
    memcpy(tid + 4, &resolver->tid.ip_posid, 2);

    /* 调用 heap_delete 进行删除 */
    int ret = heap_delete(resolver->rel, tid, 0,  /* cid */
                          false, /* crosscheck */
                          true); /* wait for lock */

    return ret;
}
