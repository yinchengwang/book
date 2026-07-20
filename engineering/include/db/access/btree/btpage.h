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

    /* 并发 Latch — 简单读写锁（自旋实现） */
    volatile int    latch_readers;         /**< 当前读锁持有者数 */
    volatile int    latch_writers_waiting; /**< 等待写锁的线程数 */
    volatile int    latch_writer_active;   /**< 是否有写锁持有者 */
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

/* ============================================================
 * 页面 Latch 操作（自旋读写锁）
 *
 * 用于 BTree 并发访问控制，每页面一个 latch。
 * 读锁共享，写锁互斥，自旋等待。
 * ============================================================ */

/**
 * @brief 初始化页面 latch
 */
static inline void bt_latch_init(BTPageHeader header) {
    header->latch_readers = 0;
    header->latch_writers_waiting = 0;
    header->latch_writer_active = 0;
}

/**
 * @brief 获取读锁（共享）
 * 多个线程可同时持有读锁，写锁等待所有读锁释放。
 */
static inline void bt_latch_read_lock(BTPageHeader header) {
    for (;;) {
        while (header->latch_writer_active) { /* 等待写锁释放 */ }
        __sync_fetch_and_add(&header->latch_readers, 1);
        if (!header->latch_writer_active) return;
        __sync_fetch_and_sub(&header->latch_readers, 1);
    }
}

/**
 * @brief 释放读锁
 */
static inline void bt_latch_read_unlock(BTPageHeader header) {
    __sync_fetch_and_sub(&header->latch_readers, 1);
}

/**
 * @brief 获取写锁（排他）
 * 等待所有读锁释放后原子获取写锁。
 */
static inline void bt_latch_write_lock(BTPageHeader header) {
    __sync_fetch_and_add(&header->latch_writers_waiting, 1);
    for (;;) {
        while (header->latch_readers > 0) { /* 等待读锁释放 */ }
        if (__sync_bool_compare_and_swap(&header->latch_writer_active, 0, 1)) {
            __sync_fetch_and_sub(&header->latch_writers_waiting, 1);
            return;
        }
    }
}

/**
 * @brief 释放写锁
 */
static inline void bt_latch_write_unlock(BTPageHeader header) {
    __sync_fetch_and_and(&header->latch_writer_active, 0);
}

/**
 * @brief 尝试获取读锁（非阻塞）
 * @return 0 成功，-1 失败（有写锁持有者）
 */
static inline int bt_latch_read_trylock(BTPageHeader header) {
    if (header->latch_writer_active) return -1;
    __sync_fetch_and_add(&header->latch_readers, 1);
    if (!header->latch_writer_active) return 0;
    __sync_fetch_and_sub(&header->latch_readers, 1);
    return -1;
}

/**
 * @brief 尝试获取写锁（非阻塞）
 * @return 0 成功，-1 失败
 */
static inline int bt_latch_write_trylock(BTPageHeader header) {
    if (header->latch_readers > 0) return -1;
    if (__sync_bool_compare_and_swap(&header->latch_writer_active, 0, 1)) {
        return 0;
    }
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* DB_ACCESS_BTREE_BTPAGE_H */
