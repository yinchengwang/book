/**
 * @file btree_split.h
 * @brief BTree 页面分裂接口
 *
 * 当页面满时，将页面分裂成两个页面。
 * 参考 PostgreSQL 的 _bt_split 实现。
 */
#ifndef DB_ACCESS_BTREE_BTREE_SPLIT_H
#define DB_ACCESS_BTREE_BTREE_SPLIT_H

#include <stdint.h>
#include "db/rel.h"
#include "db/buf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/* 前向声明，避免与 btreeam.h 中的定义冲突 */
struct BTPageHeaderData;
typedef struct BTPageHeaderData *BTPageHeader;

/* ============================================================
 * 页面分裂函数
 * ============================================================ */

/**
 * @brief 找到叶节点的中间键索引
 * @param page 页面数据指针
 * @param pivot_idx 输出：中间键索引
 * @return 0 成功，-1 失败
 */
int btree_split_find_pivot(void *page, int *pivot_idx);

/**
 * @brief 叶节点分裂
 * @param rel 索引 Relation
 * @param old_blkno 待分裂页面的块号
 * @param new_blkno 输出：新页面的块号
 * @return 0 成功，-1 失败
 *
 * 分裂步骤：
 * 1. 分配新页面
 * 2. 将旧页右半部分条目搬到新页
 * 3. 更新兄弟链接（btpo_next, btpo_prev, btpo_rightlink）
 * 4. 更新旧页条目计数
 * 5. 标记脏页
 */
int btree_split_leaf(Relation rel, uint32_t old_blkno, uint32_t *new_blkno);

/**
 * @brief 根节点分裂（创建新根）
 * @param rel 索引 Relation
 * @param old_blkno 原根页面的块号
 * @param new_blkno 分裂产生的新页面的块号
 * @return 0 成功，-1 失败
 *
 * 当根页面分裂时，需要创建新的根页面。
 */
int btree_split_root(Relation rel, uint32_t old_blkno, uint32_t new_blkno);

#ifdef __cplusplus
}
#endif

#endif /* DB_ACCESS_BTREE_BTREE_SPLIT_H */
