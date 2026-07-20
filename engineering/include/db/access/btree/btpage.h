/**
 * @file btpage.h
 * @brief BTree 页面格式定义
 *
 * BTree 页面头部结构，包含分裂和并发操作所需字段。
 * 参考 PostgreSQL 的 nbtree 页面格式。
 */
#ifndef DB_ACCESS_BTREE_BTPAGE_H
#define DB_ACCESS_BTREE_BTPAGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 页面标志位定义
 * ============================================================ */

/** 叶节点标志 */
#define BTP_LEAF       0x01

/** 根节点标志 */
#define BTP_ROOT       0x02

/** 内部节点标志 */
#define BTP_INTERNAL   0x04

/** 半删除状态（分裂过程中） */
#define BTP_HALF_DEAD  0x08

/** 元数据页面标志 */
#define BTP_META       0x10

/** 已删除页面标志 */
#define BTP_DELETED    0x20

/* ============================================================
 * 页面头部结构
 * ============================================================ */

/**
 * @brief BTree 页面头部结构
 *
 * 包含页面元数据和分裂/并发操作所需字段。
 */
typedef struct BTPageHeaderData {
    uint16_t    btpo_flags;       /**< 页面标志位组合 */
    uint16_t    btpo_level;       /**< 树层级（0 = 叶子） */
    uint32_t    btpo_prev;        /**< 左兄弟页面块号 */
    uint32_t    btpo_next;        /**< 右兄弟页面块号（原 btpo_next） */
    uint32_t    btpo_rightlink;   /**< 右兄弟页指针（并发分裂后的扫描用） */
    uint32_t    btpo_cycleid;     /**< 并发分裂标识 */
    uint32_t    btpo_xact;        /**< 事务 ID */
    uint16_t    btpo_offset;      /**< 空闲空间起始偏移 */
    uint16_t    btpo_count;       /**< 页面中的条目数 */
} BTPageHeaderData;

/** 页面头部指针类型 */
typedef BTPageHeaderData *BTPageHeader;

/* ============================================================
 * 页面大小常量
 * ============================================================ */

/** BTree 页面大小（8KB） */
#define BTREE_PAGE_SIZE          8192

/** 页面头部大小 */
#define BTREE_PAGE_HEADER_SIZE   sizeof(BTPageHeaderData)

/** 内部节点最小条目数 */
#define BTREE_MIN_ITEMS          4

/* ============================================================
 * 辅助宏定义
 * ============================================================ */

/** 检查是否是叶子页面 */
#define BT_PAGE_IS_LEAF(header)  (((header)->btpo_flags & BTP_LEAF) != 0)

/** 检查是否是根页面 */
#define BT_PAGE_IS_ROOT(header)  (((header)->btpo_flags & BTP_ROOT) != 0)

/** 检查是否是内部页面 */
#define BT_PAGE_IS_INTERNAL(header)  (((header)->btpo_flags & BTP_INTERNAL) != 0)

/** 检查是否是半删除状态 */
#define BT_PAGE_IS_HALF_DEAD(header)  (((header)->btpo_flags & BTP_HALF_DEAD) != 0)

/** 获取页面空闲空间 */
#define BT_PAGE_FREE_SPACE(header) \
    ((int)((header)->btpo_offset - BTREE_PAGE_HEADER_SIZE - \
           (header)->btpo_count * sizeof(uint32_t)))

#ifdef __cplusplus
}
#endif

#endif /* DB_ACCESS_BTREE_BTPAGE_H */
