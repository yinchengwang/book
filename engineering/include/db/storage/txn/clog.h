/**
 * @file clog.h
 * @brief CLOG（Commit Log）持久化层
 *
 * CLOG 存储每个事务的提交状态（已提交/已回滚/进行中/已预提交）。
 * 每个事务用 2bit 编码，存储在 pg_xact/ 目录下的段文件中（256KB/段）。
 * 采用 SLRU（Sublist Used Recently）缓存减少磁盘 I/O。
 *
 * 参考 PostgreSQL: src/backend/access/transam/slru.c + xact.c
 */
#ifndef DB_STORAGE_TXN_CLOG_H
#define DB_STORAGE_TXN_CLOG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * CLOG 常量定义
 * ============================================================ */

/** CLOG 页面大小（与数据库块大小相同） */
#ifndef BLCKSZ
#define BLCKSZ 8192
#endif

/** 每个事务用 2bit 编码 */
#define CLOG_BITS_PER_XACT   2

/** 每字节存储 4 个事务状态 */
#define CLOG_XACTS_PER_BYTE  4

/** 每个页面覆盖的事务数：8192字节 * 4事务/字节 = 32768 */
#define CLOG_XACTS_PER_PAGE  ((size_t)(BLCKSZ) * CLOG_XACTS_PER_BYTE)

/** 每段只包含 1 个页面：32KB 段文件 */
#define CLOG_XACTS_PER_SEG   CLOG_XACTS_PER_PAGE

/** 段文件大小（32KB，与每段事务数对齐） */
#define CLOG_SEG_SIZE        (BLCKSZ * CLOG_XACTS_PER_BYTE)

/** 事务状态值（2bit 编码） */
#define CLOG_STATUS_IN_PROGRESS  0x0  /**< 进行中 */
#define CLOG_STATUS_COMMITTED    0x1  /**< 已提交 */
#define CLOG_STATUS_ABORTED      0x2  /**< 已回滚 */
#define CLOG_STATUS_PREPARED     0x3  /**< 已预提交（两阶段提交） */

/** SLRU 缓存页面数 */
#define CLOG_CACHE_PAGES  64

/** CLOG 数据目录名 */
#define CLOG_DIR_NAME     "pg_xact"

/* ============================================================
 * SLRU 缓存条目
 * ============================================================ */

/** CLOG 缓存条目 */
typedef struct ClogCacheEntry {
    bool         valid;          /**< 缓存是否有效 */
    bool         dirty;          /**< 是否需要写盘 */
    uint32_t     base_xid;      /**< 该页面覆盖的起始 XID */
    int          segno;         /**< 段文件序号 */
    unsigned char data[BLCKSZ]; /**< 页面数据 */
    int          lru_prev;      /**< LRU 链表前驱索引 */
    int          lru_next;      /**< LRU 链表后继索引 */
} ClogCacheEntry;

/* ============================================================
 * CLOG 初始化与销毁
 * ============================================================ */

/**
 * @brief 初始化 CLOG 子系统
 * @param data_dir 数据目录（应传入 base 的绝对路径）
 * @return 0 成功，-1 失败
 */
int clog_init(const char *data_dir);

/**
 * @brief 关闭 CLOG 子系统
 */
void clog_shutdown(void);

/**
 * @brief 检查 CLOG 是否已初始化
 * @return true 已初始化
 */
bool clog_initialized(void);

/* ============================================================
 * 事务状态读写
 * ============================================================ */

/**
 * @brief 设置事务状态
 * @param xid 事务 ID
 * @param status 事务状态（CLOG_STATUS_*）
 * @return 0 成功，-1 失败
 */
int clog_set_status(uint32_t xid, uint8_t status);

/**
 * @brief 获取事务状态
 * @param xid 事务 ID
 * @return 事务状态（CLOG_STATUS_*）
 */
uint8_t clog_get_status(uint32_t xid);

/* ============================================================
 * 刷盘操作
 * ============================================================ */

/**
 * @brief 将所有脏页刷到磁盘
 * @return 0 成功，-1 失败
 */
int clog_flush(void);

/**
 * @brief 将指定段文件的脏页刷盘
 * @param segno 段序号
 * @return 0 成功，-1 失败
 */
int clog_flush_segment(int segno);

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 获取事务 ID 对应的段序号
 * @param xid 事务 ID
 * @return 段序号
 */
static inline int clog_get_segno(uint32_t xid) {
    return (int)(xid / CLOG_XACTS_PER_SEG);
}

/**
 * @brief 获取事务 ID 在段内的字节偏移
 * @param xid 事务 ID
 * @return 段内字节偏移
 */
static inline size_t clog_get_byte_offset(uint32_t xid) {
    return (size_t)(xid % CLOG_XACTS_PER_SEG) / CLOG_XACTS_PER_BYTE;
}

/**
 * @brief 获取事务 ID 在字节内的位偏移（2bit 一组）
 * @param xid 事务 ID
 * @return 位偏移（0, 2, 4, 6）
 */
static inline int clog_get_bit_offset(uint32_t xid) {
    return (int)((xid % CLOG_XACTS_PER_BYTE) * CLOG_BITS_PER_XACT);
}

/**
 * @brief 将事务 ID 映射到它在页面中的字节位置
 * @param xid 事务 ID
 * @param out_byte_offset 输出：页面内字节偏移
 * @param out_bit_offset 输出：字节内位偏移
 */
static inline void clog_get_position(uint32_t xid, size_t *out_byte_offset, int *out_bit_offset) {
    size_t xid_in_seg = (size_t)(xid % CLOG_XACTS_PER_SEG);
    *out_byte_offset = xid_in_seg / CLOG_XACTS_PER_BYTE;
    *out_bit_offset = (int)((xid_in_seg % CLOG_XACTS_PER_BYTE) * CLOG_BITS_PER_XACT);
}

/* ============================================================
 * 统计信息
 * ============================================================ */

/** CLOG 统计信息 */
typedef struct ClogStats {
    uint64_t cache_hits;       /**< 缓存命中 */
    uint64_t cache_misses;     /**< 缓存未命中 */
    uint64_t disk_reads;       /**< 磁盘读取次数 */
    uint64_t disk_writes;      /**< 磁盘写入次数 */
    uint64_t status_sets;      /**< 状态设置次数 */
    uint64_t status_gets;      /**< 状态读取次数 */
} ClogStats;

/**
 * @brief 获取 CLOG 统计信息
 * @param stats 输出统计信息
 */
void clog_get_stats(ClogStats *stats);

/**
 * @brief 重置统计信息
 */
void clog_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TXN_CLOG_H */
