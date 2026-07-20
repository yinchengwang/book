/**
 * @file buffer.h
 * @brief 缓存池 - 内存页面管理
 *
 * 缓存池管理内存中的页面，实现 LRU 淘汰策略
 */
#ifndef DB_BUFFER_H
#define DB_BUFFER_H

#include "disk.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认缓存池大小（页面数） */
#define DEFAULT_BUFFER_SIZE 1000

/** 最大缓存池大小 */
#define MAX_BUFFER_SIZE 100000

/* ============================================================
 * 缓存页状态
 * ============================================================ */

/** 页面状态标志 */
typedef enum buffer_flag_e {
    BF_NONE      = 0,      /**< 无标志 */
    BF_DIRTY     = 1 << 0, /**< 脏页（需要刷盘） */
    BF_PINNED    = 1 << 1, /**< 被持有（不可淘汰） */
    BF_READING   = 1 << 2, /**< 正在从磁盘读取 */
    BF_WRITING   = 1 << 3  /**< 正在写入磁盘 */
} buffer_flag_t;

/* ============================================================
 * 缓存页结构
 * ============================================================ */

/** 缓存页节点 */
typedef struct buffer_frame_s {
    page_id_t    page_id;      /**< 对应的磁盘页面 ID */
    page_t      *page;         /**< 页面数据 */
    uint32_t     flags;        /**< 状态标志 */
    uint32_t    pin_count;     /**< 引用计数 */

    /* LRU 链表指针 */
    struct buffer_frame_s *prev;
    struct buffer_frame_s *next;

    /* 哈希表指针（解决哈希冲突的链表） */
    struct buffer_frame_s *hash_next;
    struct buffer_frame_s *hash_prev;
} buffer_frame_t;

/* ============================================================
 * 缓存池结构
 * ============================================================ */

/** 缓存池 */
typedef struct buffer_pool_s {
    db_file_t   *file;          /**< 关联的数据库文件 */

    size_t       page_size;     /**< 页面大小 */
    size_t       pool_size;    /**< 缓存池大小（帧数） */
    size_t       page_count;   /**< 当前缓存的页面数 */

    buffer_frame_t *frames;    /**< 帧数组 */
    buffer_frame_t *free_list; /**< 空闲帧链表 */

    /* LRU 链表（双向循环链表） */
    buffer_frame_t *lru_head;  /**< LRU 链表头（最久未使用） */
    buffer_frame_t *lru_tail;  /**< LRU 链表尾（最近使用） */

    /* 哈希表（快速查找） */
    buffer_frame_t **hash_table;
    size_t          hash_size;

    /* 统计信息 */
    uint64_t hit_count;        /**< 缓存命中次数 */
    uint64_t miss_count;       /**< 缓存未命中次数 */
    uint64_t evict_count;      /**< 淘汰次数 */
    uint64_t write_count;      /**< 写盘次数 */
} buffer_pool_t;

/* ============================================================
 * 缓存池操作函数
 * ============================================================ */

/**
 * @brief 创建缓存池
 * @param file 数据库文件
 * @param pool_size 缓存池大小（页面数）
 * @return 缓存池句柄
 */
buffer_pool_t *buffer_create(db_file_t *file, size_t pool_size);

/**
 * @brief 销毁缓存池
 * @param pool 缓存池句柄
 */
void buffer_destroy(buffer_pool_t *pool);

/**
 * @brief 获取页面（从缓存或磁盘）
 * @param pool 缓存池
 * @param page_id 页面 ID
 * @return 页面指针，失败返回 NULL
 *
 * 调用者使用完后必须调用 buffer_unpin()
 */
page_t *buffer_get_page(buffer_pool_t *pool, page_id_t page_id);

/**
 * @brief 分配新页面
 * @param pool 缓存池
 * @param type 页面类型
 * @param out_page_id 输出：新页面 ID
 * @return 页面指针
 */
page_t *buffer_alloc_page(buffer_pool_t *pool, page_type_t type, page_id_t *out_page_id);

/**
 * @brief 释放页面（减少引用计数）
 * @param pool 缓存池
 * @param page_id 页面 ID
 */
void buffer_unpin_page(buffer_pool_t *pool, page_id_t page_id);

/**
 * @brief 标记页面为脏
 * @param pool 缓存池
 * @param page_id 页面 ID
 */
void buffer_mark_dirty(buffer_pool_t *pool, page_id_t page_id);

/**
 * @brief 刷新指定页面到磁盘
 * @param pool 缓存池
 * @param page_id 页面 ID
 * @return 成功返回 0
 */
int buffer_flush_page(buffer_pool_t *pool, page_id_t page_id);

/**
 * @brief 刷新所有脏页到磁盘
 * @param pool 缓存池
 * @return 成功返回 0
 */
int buffer_flush_all(buffer_pool_t *pool);

/**
 * @brief 获取统计信息
 * @param pool 缓存池
 * @param hit_rate 输出：缓存命中率
 * @param page_count 输出：当前缓存页数
 * @param dirty_count 输出：脏页数
 */
void buffer_get_stats(buffer_pool_t *pool,
                      double *hit_rate,
                      size_t *page_count,
                      size_t *dirty_count);

/**
 * @brief 检查页面是否在缓存中
 * @param pool 缓存池
 * @param page_id 页面 ID
 * @return 在缓存中返回 true
 */
bool buffer_contains_page(buffer_pool_t *pool, page_id_t page_id);

/**
 * @brief 获取页面（不增加引用计数）
 * @param pool 缓存池
 * @param page_id 页面 ID
 * @return 页面指针，不存在返回 NULL
 */
page_t *buffer_peek_page(buffer_pool_t *pool, page_id_t page_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_BUFFER_H */
