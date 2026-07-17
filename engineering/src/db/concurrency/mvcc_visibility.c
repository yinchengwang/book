/*
 * mvcc_visibility.c - MVCC 可见性判断实现
 */

#include <stdlib.h>
#include <string.h>

#include <db/concurrency/mvcc.h>

/* ─────────────────────────────────────────────────────────────────
 * 可见性判断
 *
 * PostgreSQL 可见性规则:
 *
 * 版本的创建事务 T 创建者:
 * - 如果 T 在当前快照的活跃事务列表中 -> 不可见
 * - 如果 T.xmax 已提交 -> 不可见
 * - 否则 -> 可见
 *
 * 版本删除事务 T 删除者:
 * - 如果 T 在当前快照的活跃事务列表中 -> 不可见（尚未删除）
 * - 如果 T 已提交且 T.xmax < snapshot.xmax -> 不可见（已删除）
 * - 否则 -> 可见（未删除或删除事务未提交）
 * ───────────────────────────────────────────────────────────────── */

bool mvcc_version_visible(const mvcc_snapshot_t *snapshot,
                          mvcc_txn_id_t xmin,
                          mvcc_txn_id_t xmax,
                          mvcc_txn_id_t cur_txn_id)
{
    /* 参数检查 */
    if (snapshot == NULL) {
        return false;
    }

    /* 特殊情况：自身事务创建的版本始终可见 */
    if (xmin == cur_txn_id) {
        return true;
    }

    /* 规则 1: xmin 事务必须已提交
     * - xmin 不在活跃列表中
     * - xmin < snapshot->xmax (事务在快照创建前已提交)
     */
    if (xmin >= snapshot->xmax) {
        /* 事务 xmin 在快照创建后开始，未提交 */
        return false;
    }

    if (mvcc_snapshot_contains_txn(snapshot, xmin)) {
        /* xmin 事务仍在活跃列表中，未提交 */
        return false;
    }

    /* 规则 2: 检查删除事务
     * - 如果 xmax = 0，版本未被删除
     * - 如果 xmax 在活跃列表中，删除事务未提交
     * - 如果 xmax >= snapshot->xmax，删除事务在快照创建后开始
     */
    if (xmax != MVCC_INVALID_TXN_ID) {
        if (mvcc_snapshot_contains_txn(snapshot, xmax)) {
            /* 删除事务仍在活跃列表中，未提交 */
            return false;
        }
        if (xmax < snapshot->xmax) {
            /* 删除事务已提交，且在快照创建前提交 -> 已删除 */
            return false;
        }
    }

    return true;
}

mvcc_version_t *mvcc_version_find_visible(const mvcc_version_t *head,
                                           const mvcc_snapshot_t *snapshot,
                                           mvcc_txn_id_t cur_txn_id)
{
    if (head == NULL || snapshot == NULL) {
        return NULL;
    }

    /* 从链表头部向下遍历，找到第一个可见版本 */
    const mvcc_version_t *cur = head;
    while (cur != NULL) {
        if (mvcc_version_visible(snapshot, cur->xmin, cur->xmax,
                                  cur_txn_id)) {
            return (mvcc_version_t *)cur;
        }
        cur = cur->next;
    }

    return NULL;
}

bool mvcc_check_write_conflict(const mvcc_snapshot_t *snapshot,
                               mvcc_txn_id_t row_xmax,
                               mvcc_txn_id_t cur_txn_id)
{
    if (snapshot == NULL) {
        return false;
    }

    /* 无删除标记，无冲突 */
    if (row_xmax == MVCC_INVALID_TXN_ID) {
        return false;
    }

    /* 自身事务的删除，无冲突 */
    if (row_xmax == cur_txn_id) {
        return false;
    }

    /* 删除事务未提交，无冲突 */
    if (mvcc_snapshot_contains_txn(snapshot, row_xmax)) {
        return false;
    }

    /* 删除事务已提交 -> 存在冲突
     * 当前事务尝试修改已被其他事务删除的行
     */
    if (row_xmax < snapshot->xmax) {
        return true;
    }

    return false;
}