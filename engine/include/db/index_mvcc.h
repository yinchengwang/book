/**
 * @file index_mvcc.h
 * @brief 索引项 MVCC 可见性判断
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

#ifndef DB_INDEX_MVCC_H
#define DB_INDEX_MVCC_H

#include <stdint.h>
#include <stdbool.h>

#include <db/txn.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 索引项 MVCC 信息结构
 * ============================================================ */

/**
 * 索引项 MVCC 元数据
 *
 * 对于需要 MVCC 可见性判断的索引（如唯一索引），
 * 在索引项中嵌入 MVCC 信息。
 */
typedef struct index_mvcc_meta {
    txn_id_t xmin;           /**< 创建事务ID */
    txn_id_t xmax;           /**< 删除事务ID（0表示未删除） */
    txn_cid_t xmin_cid;      /**< 创建命令ID（同一事务内命令区分） */
    bool is_deleted;         /**< 是否已标记删除 */
} index_mvcc_meta_t;

/**
 * 索引扫描结果（带 MVCC 可见性）
 */
typedef struct index_mvcc_scan_result {
    void *heap_ptr;          /**< 指向 Heap 行的指针 */
    size_t heap_ptr_size;    /**< 指针大小 */
    bool visible;            /**< 对当前事务是否可见 */
    index_mvcc_meta_t meta;  /**< MVCC 元数据（如果有） */
} index_mvcc_scan_result_t;

/* ============================================================
 * 索引项可见性判断
 * ============================================================ */

/**
 * @brief 判断索引项对事务是否可见
 * @param txn 事务上下文
 * @param meta 索引项 MVCC 元数据
 * @return true 如果索引项可见
 *
 * ## 可见性规则
 *
 * 对于索引项，可见性规则与 Heap 项略有不同：
 * 1. 如果 xmax != 0 且 xmax 事务已提交 -> 索引项不可见
 * 2. 如果 xmin 在活跃事务列表中 -> 索引项不可见
 * 3. 其他情况 -> 索引项可见
 *
 * 注意：索引项可见不代表 Heap 行可见，需要进一步检查。
 */
bool index_mvcc_item_visible(const txn_context_t *txn,
                             const index_mvcc_meta_t *meta);

/**
 * @brief 检查唯一索引键的唯一性
 * @param txn 事务上下文
 * @param index_name 索引名
 * @param key 键值
 * @param key_size 键值大小
 * @param ignore_xid 忽略指定事务创建的版本（用于更新时）
 * @return true 如果唯一
 *
 * ## 唯一性检查的 MVCC 问题
 *
 * 在 MVCC 下，唯一性检查需要考虑：
 * 1. 已提交且不在活跃列表的事务创建的键 -> 冲突
 * 2. 未提交或活跃事务创建的键 -> 不冲突
 * 3. 当前事务自己创建的键 -> 需要特殊处理（忽略）
 */
bool index_mvcc_check_unique(const txn_context_t *txn,
                             const char *index_name,
                             const void *key,
                             size_t key_size,
                             txn_id_t ignore_xid);

/* ============================================================
 * 索引扫描可见性过滤
 * ============================================================ */

/**
 * @brief 过滤索引扫描结果，只保留可见项
 * @param txn 事务上下文
 * @param results 扫描结果数组
 * @param result_count 结果数量
 * @return 过滤后的可见结果数量
 *
 * 示例：
 * @code
 * index_mvcc_scan_result_t results[100];
 * int count = index_scan(index, query, results, 100);
 * int visible_count = index_mvcc_filter_visible(txn, results, count);
 * // results[0..visible_count-1] 都是可见的
 * @endcode
 */
int index_mvcc_filter_visible(const txn_context_t *txn,
                              index_mvcc_scan_result_t *results,
                              int result_count);

/**
 * @brief 检查 Heap 行是否对索引扫描可见
 * @param txn 事务上下文
 * @param heap_ptr Heap 指针
 * @param heap_xmin Heap 行的 xmin
 * @param heap_xmax Heap 行的 xmax
 * @return true 如果 Heap 行可见
 *
 * ## 索引覆盖扫描优化
 *
 * 如果查询所需列都包含在索引中（覆盖扫描），
 * 就不需要回表检查 Heap 行。
 */
bool index_mvcc_check_heap_visible(const txn_context_t *txn,
                                   const void *heap_ptr,
                                   txn_id_t heap_xmin,
                                   txn_id_t heap_xmax);

/* ============================================================
 * 唯一索引 MVCC 处理
 * ============================================================ */

/**
 * @brief 唯一索引插入前的唯一性预校验
 * @param txn 事务上下文
 * @param index_name 索引名
 * @param key 键值
 * @param key_size 键值大小
 * @return 0 唯一（可以插入），非0 冲突
 *
 * ## 预校验机制
 *
 * 为避免幻读问题，唯一索引在插入前需要：
 * 1. 检查是否有已提交的相同键
 * 2. 如果有，检查该版本是否对当前事务可见
 * 3. 如果可见，则冲突
 *
 * 注意：预校验不能完全避免幻读，但可以减少大多数冲突。
 */
int index_mvcc_unique_precheck(const txn_context_t *txn,
                               const char *index_name,
                               const void *key,
                               size_t key_size);

/**
 * @brief 唯一索引插入时的唯一性锁定
 * @param txn 事务上下文
 * @param index_name 索引名
 * @param key 键值
 * @param key_size 键值大小
 * @return 0 成功，非0 失败
 *
 * ## 插入锁定
 *
 * 唯一索引插入时，需要：
 * 1. 获取键的排他锁
 * 2. 再次检查唯一性
 * 3. 插入索引项
 * 4. 释放锁（或在事务结束时释放）
 */
int index_mvcc_unique_lock(const txn_context_t *txn,
                           const char *index_name,
                           const void *key,
                           size_t key_size);

/* ============================================================
 * 索引覆盖扫描 MVCC
 * ============================================================ */

/**
 * @brief 检查查询是否可以使用覆盖扫描
 * @param index_name 索引名
 * @param needed_cols 需要的列
 * @param needed_col_count 列数量
 * @return true 如果索引覆盖所有列
 *
 * ## 覆盖扫描的优势
 *
 * 如果索引包含查询所需的所有列：
 * 1. 不需要回表读取 Heap 行
 * 2. 索引项的可见性判断更简单
 * 3. 性能更好
 */
bool index_mvcc_can_cover(const char *index_name,
                          const char **needed_cols,
                          int needed_col_count);

/**
 * @brief 索引覆盖扫描的可见性判断
 * @param txn 事务上下文
 * @param index_name 索引名
 * @param key 索引键
 * @param key_size 键大小
 * @param included_values 索引中包含的值
 * @param value_count 值数量
 * @return true 如果覆盖扫描结果可见
 *
 * ## 覆盖扫描的 MVCC 可见性
 *
 * 覆盖扫描时，索引项直接包含数据，需要：
 * 1. 检查索引项的 xmin 是否可见
 * 2. 检查索引项的 xmax 是否表示删除
 * 3. 如果都满足，索引项的数据可直接返回
 */
bool index_mvcc_cover_scan_visible(const txn_context_t *txn,
                                   const char *index_name,
                                   const void *key,
                                   size_t key_size,
                                   const void **included_values,
                                   int value_count);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_MVCC_H */
