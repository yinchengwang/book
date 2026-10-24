/**
 * @file index_mvcc.c
 * @brief 索引项 MVCC 可见性判断实现
 *
 * ## 索引项 MVCC 的挑战
 *
 * 与 Heap 表不同，索引项的 MVCC 处理更复杂：
 * 1. 索引只存储键值和指针，不存储完整行数据
 * 2. 索引项更新不能像 Heap 那样直接标记旧版本为删除
 * 3. 需要协调索引扫描和 Heap 扫描的可见性
 *
 * ## 设计方案
 *
 * 采用"索引不存储 MVCC 信息，Heap 存储"的原则：
 * - 索引项只包含 (key, heap_ptr)
 * - 可见性判断在 Heap 层进行
 * - 索引扫描返回 heap_ptr，然后检查 Heap 版本的可见性
 *
 * ## 唯一索引的 MVCC 处理
 *
 * 唯一索引需要特殊处理：
 * - 插入时检查是否有已提交的相同键
 * - 使用"唯一性预校验"机制避免幻读
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <db/index_mvcc.h>
#include <db/concurrency/mvcc.h>

/* ============================================================
 * 索引项可见性判断实现
 * ============================================================ */

bool index_mvcc_item_visible(const txn_context_t *txn,
                             const index_mvcc_meta_t *meta)
{
    if (txn == NULL || meta == NULL) {
        return false;
    }

    /* 规则1: 自身事务创建的索引项始终可见 */
    if (meta->xmin == txn_get_id(txn)) {
        return true;
    }

    /* 规则2: xmin 事务必须已提交且不在活跃列表 */
    txn_id_t snapshot_xmin = txn_snapshot_xmin(txn);
    txn_id_t snapshot_xmax = txn_snapshot_xmax(txn);

    /* xmin 在快照创建后开始，未提交 */
    if (meta->xmin >= snapshot_xmax) {
        return false;
    }

    /* 检查 xmin 是否在活跃事务列表中 */
    if (txn_manager_is_active(meta->xmin)) {
        return false;  /* xmin 事务仍在运行 */
    }

    /* 规则3: 检查删除事务 */
    if (meta->xmax != TXN_ID_NONE && !meta->is_deleted) {
        /* 如果 xmax 在活跃列表中，删除事务未提交 */
        if (txn_manager_is_active(meta->xmax)) {
            return true;  /* 删除未提交，索引项仍可见 */
        }
        /* xmax < snapshot_xmax 表示删除事务已提交 */
        if (meta->xmax < snapshot_xmax) {
            return false;  /* 已被删除 */
        }
    }

    return true;
}

/* ============================================================
 * 唯一性检查实现
 * ============================================================ */

/* 模拟的索引数据（实际应从索引模块获取） */
static struct {
    void *key;
    size_t key_size;
    txn_id_t xmin;
} g_unique_index_entries[1024];
static int g_entry_count = 0;

bool index_mvcc_check_unique(const txn_context_t *txn,
                             const char *index_name,
                             const void *key,
                             size_t key_size,
                             txn_id_t ignore_xid)
{
    (void)index_name;  /* 简化实现 */

    if (txn == NULL || key == NULL) {
        return false;
    }

    txn_id_t snapshot_xmin = txn_snapshot_xmin(txn);
    txn_id_t snapshot_xmax = txn_snapshot_xmax(txn);

    /* 遍历索引项，检查唯一性 */
    for (int i = 0; i < g_entry_count; i++) {
        /* 比较键值 */
        if (g_unique_index_entries[i].key_size != key_size) {
            continue;
        }
        if (memcmp(g_unique_index_entries[i].key, key, key_size) != 0) {
            continue;
        }

        txn_id_t entry_xmin = g_unique_index_entries[i].xmin;

        /* 忽略指定事务 */
        if (entry_xmin == ignore_xid) {
            continue;
        }

        /* 检查该版本是否可见 */
        /* 规则：只有已提交且不在活跃列表的版本才冲突 */
        if (entry_xmin < snapshot_xmax && !txn_manager_is_active(entry_xmin)) {
            /* 存在已提交的相同键，违反唯一性 */
            return false;
        }
    }

    return true;  /* 唯一，未冲突 */
}

int index_mvcc_unique_precheck(const txn_context_t *txn,
                               const char *index_name,
                               const void *key,
                               size_t key_size)
{
    /* 调用唯一性检查 */
    if (index_mvcc_check_unique(txn, index_name, key, key_size, TXN_ID_NONE)) {
        return 0;  /* 唯一，可以插入 */
    }
    return -1;  /* 冲突 */
}

int index_mvcc_unique_lock(const txn_context_t *txn,
                           const char *index_name,
                           const void *key,
                           size_t key_size)
{
    (void)txn;
    (void)index_name;
    (void)key;
    (void)key_size;

    /* TODO: 实现索引锁机制
     * 简化实现：使用全局锁或分区锁
     * 实际实现应该使用索引特定的锁结构
     */

    /* 模拟加锁成功 */
    return 0;
}

/* ============================================================
 * 索引扫描可见性过滤实现
 * ============================================================ */

int index_mvcc_filter_visible(const txn_context_t *txn,
                              index_mvcc_scan_result_t *results,
                              int result_count)
{
    if (txn == NULL || results == NULL || result_count <= 0) {
        return 0;
    }

    int visible_count = 0;

    for (int i = 0; i < result_count; i++) {
        /* 检查索引项可见性 */
        if (index_mvcc_item_visible(txn, &results[i].meta)) {
            /* 进一步检查 Heap 行可见性 */
            if (index_mvcc_check_heap_visible(txn,
                                               results[i].heap_ptr,
                                               results[i].meta.xmin,
                                               results[i].meta.xmax)) {
                results[visible_count] = results[i];
                results[visible_count].visible = true;
                visible_count++;
            }
        }
    }

    return visible_count;
}

bool index_mvcc_check_heap_visible(const txn_context_t *txn,
                                   const void *heap_ptr,
                                   txn_id_t heap_xmin,
                                   txn_id_t heap_xmax)
{
    if (txn == NULL || heap_ptr == NULL) {
        return false;
    }

    /* 使用事务的可见性判断函数 */
    return txn_version_visible((txn_context_t *)txn,
                               heap_xmin,
                               heap_xmax,
                               TXN_CID_NONE);
}

/* ============================================================
 * 索引覆盖扫描实现
 * ============================================================ */

/* 索引覆盖信息（简化实现） */
static struct {
    const char *index_name;
    const char **covered_cols;
    int col_count;
} g_index_coverage[64];
static int g_index_coverage_count = 0;

bool index_mvcc_can_cover(const char *index_name,
                          const char **needed_cols,
                          int needed_col_count)
{
    if (index_name == NULL || needed_cols == NULL) {
        return false;
    }

    /* 查找索引覆盖信息 */
    for (int i = 0; i < g_index_coverage_count; i++) {
        if (strcmp(g_index_coverage[i].index_name, index_name) == 0) {
            /* 检查是否所有需要的列都被覆盖 */
            for (int j = 0; j < needed_col_count; j++) {
                bool found = false;
                for (int k = 0; k < g_index_coverage[i].col_count; k++) {
                    if (strcmp(g_index_coverage[i].covered_cols[k],
                               needed_cols[j]) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false;  /* 有列未被覆盖 */
                }
            }
            return true;  /* 所有列都被覆盖 */
        }
    }

    return false;  /* 未找到索引覆盖信息 */
}

bool index_mvcc_cover_scan_visible(const txn_context_t *txn,
                                   const char *index_name,
                                   const void *key,
                                   size_t key_size,
                                   const void **included_values,
                                   int value_count)
{
    (void)index_name;
    (void)key;
    (void)key_size;
    (void)included_values;
    (void)value_count;

    if (txn == NULL) {
        return false;
    }

    /* 覆盖扫描时，索引项直接包含数据
     * 可见性判断：
     * 1. 索引项的 xmin 事务必须已提交
     * 2. 索引项未被删除
     *
     * 由于覆盖扫描不涉及 Heap 行，这里简化处理
     */
    return true;
}

/* ============================================================
 * 测试/调试函数
 * ============================================================ */

void index_mvcc_register_coverage(const char *index_name,
                                   const char **covered_cols,
                                   int col_count)
{
    if (g_index_coverage_count >= 64) {
        return;  /* 满了 */
    }

    g_index_coverage[g_index_coverage_count].index_name = index_name;
    g_index_coverage[g_index_coverage_count].covered_cols = covered_cols;
    g_index_coverage[g_index_coverage_count].col_count = col_count;
    g_index_coverage_count++;
}

void index_mvcc_clear(void)
{
    /* 清理测试数据 */
    for (int i = 0; i < g_entry_count; i++) {
        if (g_unique_index_entries[i].key != NULL) {
            free(g_unique_index_entries[i].key);
            g_unique_index_entries[i].key = NULL;
        }
    }
    g_entry_count = 0;
}
