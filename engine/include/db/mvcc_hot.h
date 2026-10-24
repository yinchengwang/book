/**
 * @file mvcc_hot.h
 * @brief HOT (Heap Only Tuple) 更新机制
 *
 * ## HOT 简介
 *
 * HOT 是 PostgreSQL 的一种优化机制，用于减少索引项更新。
 * 当更新一行时，如果新版本在同一页面内，可以复用索引项。
 *
 * ## HOT 条件
 *
 * 满足以下条件时使用 HOT：
 * 1. 新版本与旧版本在同一页面
 * 2. 索引键未改变
 * 3. 页面有足够空间
 *
 * ## HOT 优势
 *
 * - 减少索引项数量
 * - 减少 VACUUM 需要
 * - 提高缓存命中率
 */

#ifndef DB_MVCC_HOT_H
#define DB_MVCC_HOT_H

#include <stdint.h>
#include <stdbool.h>

#include <db/txn.h>
#include <db/concurrency/mvcc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * HOT 状态
 * ============================================================ */

/** HOT 状态标志 */
typedef enum hot_status_e {
    HOT_STATUS_NORMAL = 0,       /**< 普通元组 */
    HOT_STATUS_HOT_CHAIN_HEAD = 1,  /**< HOT 链头 */
    HOT_STATUS_HOT_CHAIN_MEMBER = 2, /**< HOT 链成员 */
    HOT_STATUS_DEAD = 3          /**< 死亡元组（可回收） */
} hot_status_t;

/**
 * HOT 链信息
 */
typedef struct hot_chain_info {
    uint32_t page_id;            /**< 页面ID */
    uint16_t head_offset;        /**< 链头偏移 */
    uint16_t tail_offset;        /**< 链尾偏移 */
    int chain_length;            /**< 链长度 */
    hot_status_t status;         /**< 链状态 */
} hot_chain_info_t;

/* ============================================================
 * HOT 判断
 * ============================================================ */

/**
 * @brief 检查是否可以使用 HOT 更新
 * @param txn 事务上下文
 * @param table_oid 表ID
 * @param old_version 旧版本
 * @param new_key 是否修改了索引键
 * @return true 如果可以使用 HOT
 *
 * ## HOT 条件检查
 *
 * 1. 旧版本在同一页面有空间
 * 2. 索引键未改变（新版本对索引可见）
 * 3. 页面没有严重的碎片
 */
bool hot_can_update(const txn_context_t *txn,
                    uint32_t table_oid,
                    const mvcc_version_t *old_version,
                    bool new_key);

/**
 * @brief 执行 HOT 更新
 * @param txn 事务上下文
 * @param table_oid 表ID
 * @param old_ctid 旧版本位置
 * @param new_data 新数据
 * @param new_data_size 新数据大小
 * @param out_new_ctid 输出：新版本位置
 * @return 0 成功，非0 失败
 *
 * ## HOT 更新流程
 *
 * 1. 在同一页面分配新空间
 * 2. 设置 HOT 链指针
 * 3. 更新旧版本的 ctid 指向新版本
 * 4. 不需要更新索引（复用索引项）
 */
int hot_execute_update(txn_context_t *txn,
                       uint32_t table_oid,
                       const mvcc_ctid_t *old_ctid,
                       const void *new_data,
                       size_t new_data_size,
                       mvcc_ctid_t *out_new_ctid);

/* ============================================================
 * HOT 链遍历
 * ============================================================ */

/**
 * @brief 遍历 HOT 链，查找可见版本
 * @param head 链头版本
 * @param snapshot 快照
 * @param cur_txn_id 当前事务ID
 * @return 可见版本，NULL 表示无可见版本
 */
mvcc_version_t *hot_chain_find_visible(const mvcc_version_t *head,
                                        const mvcc_snapshot_t *snapshot,
                                        txn_id_t cur_txn_id);

/**
 * @brief 获取 HOT 链信息
 * @param head 链头版本
 * @param info 输出：链信息
 * @return 0 成功，非0 失败
 */
int hot_chain_get_info(const mvcc_version_t *head,
                       hot_chain_info_t *info);

/* ============================================================
 * HOT 与 VACUUM
 * ============================================================ */

/**
 * @brief 检查 HOT 链是否可以裁剪
 * @param head 链头版本
 * @param oldest_active_xmin 最早的活跃事务 xmin
 * @return true 如果可以裁剪
 *
 * ## HOT 链裁剪条件
 *
 * 1. 链头版本已被删除且删除事务已提交
 * 2. 链中所有版本对 oldest_active_xmin 之前的事务不可见
 * 3. 没有索引项指向链中的中间版本
 */
bool hot_chain_can_prune(const mvcc_version_t *head,
                         txn_id_t oldest_active_xmin);

/**
 * @brief 裁剪 HOT 链
 * @param head 链头版本
 * @param prune_after 裁剪此版本之后的所有版本
 * @return 裁剪的版本数
 *
 * ## HOT 链裁剪流程
 *
 * 1. 标记要裁剪的版本为死亡
 * 2. 更新链头指向保留版本
 * 3. 回收死亡版本的页面空间
 * 4. 更新页面空闲空间信息
 */
int hot_chain_prune(mvcc_version_t *head,
                    const mvcc_version_t *prune_after);

/**
 * @brief 检查页面碎片程度
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @return 碎片百分比 (0-100)
 *
 * ## 碎片检测
 *
 * 碎片率 = 空闲空间 / 页面总空间 * 100%
 * 当碎片率超过阈值时，应该进行 VACUUM。
 */
int hot_page_fragmentation(const void *page_data, size_t page_size);

/**
 * @brief 清理页面碎片
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @return 清理后的空闲空间大小
 *
 * ## 碎片清理流程
 *
 * 1. 识别页面中的死亡元组
 * 2. 将存活元组紧凑排列
 * 3. 更新元组指针
 * 4. 更新页面空闲空间指针
 */
size_t hot_defragment_page(void *page_data, size_t page_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_MVCC_HOT_H */
