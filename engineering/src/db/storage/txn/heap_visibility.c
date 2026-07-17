/**
 * @file heap_visibility.c
 * @brief 元组可见性判断实现
 *
 * 实现 HeapTupleSatisfiesMVCC 等可见性判断函数
 * 参考 PostgreSQL: src/backend/access/heap/heapam_visibility.c
 */

#include "db/storage/txn/mvcc.h"
#include "db/storage/access/heap/heapam.h"

#include <stdbool.h>

/* ========================================================================
 * 可见性判断核心
 * ======================================================================== */

/**
 * @brief 检查元组对当前 ReadView 是否可见
 *
 * 这是可见性判断的核心函数，整合 MVCC 逻辑
 *
 * @param xmin 元组的插入事务 ID
 * @param xmax 元组的删除/更新事务 ID
 * @param xmin_status xmin 状态
 * @param xmax_status xmax 状态
 * @param ctid 元组指针
 * @return true 可见
 */
bool heap_tuple_visible(TransactionId xmin,
                       TransactionId xmax,
                       heap_xmin_status_t xmin_status,
                       heap_xmax_status_t xmax_status,
                       TransactionId ctid)
{
    ReadView *rv = mvcc_get_current_readview();
    if (!rv) {
        /* 没有 ReadView，默认可见 */
        return true;
    }

    HeapTupleFields fields;
    fields.xmin = xmin;
    fields.xmax = xmax;
    fields.xmin_status = xmin_status;
    fields.xmax_status = xmax_status;
    fields.t_ctid_is_self = (ctid == xmin);  /* 简化判断 */
    fields.t_ctid = ctid;

    return mvcc_tuple_visible(rv, &fields);
}

/**
 * @brief 检查元组是否被标记为死亡
 */
bool heap_tuple_dead(TransactionId xmax, heap_xmax_status_t status)
{
    if (!mvcc_is_xid_valid(xmax)) {
        return false;
    }

    if (status == HEAP_XMAX_INVALID) {
        return false;
    }

    /* 如果 xmax 已提交且不是 LOCK_ONLY，则是死亡元组 */
    transaction_status_t txn_status = mvcc_get_status(xmax);
    if (txn_status == TRANSACTION_STATUS_COMMITTED &&
        status != HEAP_XMAX_LOCK_ONLY) {
        return true;
    }

    return false;
}

/**
 * @brief 检查元组是否需要清理
 *
 * @param xmin 插入事务 ID
 * @param xmax 删除事务 ID
 * @param xmin_status xmin 状态
 * @param xmax_status xmax 状态
 * @return true 需要 VACUUM 清理
 */
bool heap_tuple_needs_prune(TransactionId xmin,
                            TransactionId xmax,
                            heap_xmin_status_t xmin_status,
                            heap_xmax_status_t xmax_status)
{
    /* 元组是死亡元组 */
    if (heap_tuple_dead(xmax, xmax_status)) {
        /* 且没有更新的版本指向它 */
        return true;
    }

    /* TODO: 检查是否可以被 freeze */

    return false;
}

/* ========================================================================
 * 可见性辅助函数
 * ======================================================================== */

/**
 * @brief 检查 xmin 是否有效
 */
bool heap_xmin_valid(TransactionId xmin)
{
    if (!mvcc_is_xid_valid(xmin)) {
        return false;
    }

    /* Frozen 元组始终有效 */
    if (mvcc_is_xid_frozen(xmin)) {
        return true;
    }

    return true;
}

/**
 * @brief 检查 xmax 是否表示已删除
 */
bool heap_xmax_invalid(TransactionId xmax, heap_xmax_status_t status)
{
    return !mvcc_is_xid_valid(xmax) ||
           status == HEAP_XMAX_INVALID;
}

/**
 * @brief 检查是否是 LOCK_ONLY 删除
 */
bool heap_xmax_lock_only(TransactionId xmax, heap_xmax_status_t status)
{
    if (!mvcc_is_xid_valid(xmax)) {
        return false;
    }
    return status == HEAP_XMAX_LOCK_ONLY;
}

/**
 * @brief 获取元组的最新版本指针
 *
 * @param ctid 当前元组指针
 * @return 最新版本的指针
 */
TransactionId heap_get_latest_ctid(TransactionId ctid)
{
    /* TODO: 实现版本链遍历 */
    return ctid;
}

/* ========================================================================
 * 页面级别可见性
 * ======================================================================== */

/**
 * @brief 检查页面是否包含可见元组
 *
 * 用于优化扫描：跳过没有可见元组的页面
 */
bool heap_page_has_visible_tuples(void *page)
{
    /* TODO: 实现页面级别可见性检查 */
    (void)page;
    return true;
}

/**
 * @brief 获取页面的可见元组数
 */
int heap_page_count_visible(void *page)
{
    /* TODO: 实现可见元组计数 */
    (void)page;
    return 0;
}

/* ========================================================================
 * 扫描相关可见性
 * ======================================================================== */

/**
 * @brief 在扫描中检查元组可见性
 *
 * 用于 TableScan 过程中的可见性判断
 */
bool heap_scan_tuple_visible(void *tid,
                            TransactionId xmin,
                            TransactionId xmax,
                            heap_xmax_status_t xmax_status)
{
    /* 获取 xmin 状态 */
    heap_xmin_status_t xmin_status = HEAP_XMIN_COMMITTED;
    if (!heap_xmin_valid(xmin)) {
        xmin_status = HEAP_XMIN_INVALID;
    }

    return heap_tuple_visible(xmin, xmax, xmin_status, xmax_status, (TransactionId)(uintptr_t)tid);
}

/**
 * @brief 检查元组是否对当前命令可见
 *
 * 用于 INSERT/UPDATE/DELETE 中的自可见性检查
 */
bool heap_tuple_is_self_visible(TransactionId xmin, TransactionId xmax)
{
    TransactionId current_xid = txn_current_xid();

    /* xmin 是当前事务 */
    if (xmin == current_xid) {
        /* 如果 xmax 也是当前事务（自更新），检查状态 */
        if (xmax == current_xid) {
            /* 如果不是 LOCK_ONLY，则已被删除 */
            return false;
        }
        /* xmax 为空，表示未被删除 */
        return true;
    }

    return false;
}

/* ========================================================================
 * 调试辅助
 * ======================================================================== */

/**
 * @brief 打印元组的 MVCC 信息
 */
void heap_dump_tuple_mvcc(uint32_t relfilenode,
                          uint32_t blocknum,
                          uint32_t offset,
                          TransactionId xmin,
                          TransactionId xmax,
                          heap_xmin_status_t xmin_status,
                          heap_xmax_status_t xmax_status)
{
    printf("Tuple MVCC Info:\n");
    printf("  Relation: %u, Block: %u, Offset: %u\n",
           relfilenode, blocknum, offset);
    printf("  xmin: %u (status: %d)\n", xmin, xmin_status);
    printf("  xmax: %u (status: %d)\n", xmax, xmax_status);
    printf("  Visible: %s\n",
           heap_tuple_visible(xmin, xmax, xmin_status, xmax_status, 0) ? "yes" : "no");
}

/**
 * @brief 打印 ReadView 信息
 */
void heap_dump_readview(const ReadView *rv)
{
    if (!rv) {
        printf("ReadView: NULL\n");
        return;
    }

    printf("ReadView:\n");
    printf("  xmin: %u\n", rv->xmin);
    printf("  xmax: %u\n", rv->xmax);
    printf("  snapshot_xmax: %u\n", rv->snapshot_xmax);
    printf("  isolation: %d\n", rv->isolation);
    printf("  active_txs: %d\n", rv->num_active);
}
