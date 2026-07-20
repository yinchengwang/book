/**
 * @file heapam.h
 * @brief Heap Access Method 公共接口
 *
 * Heap AM 负责管理表数据的存储和访问。
 * 参考 PostgreSQL 的 heapam.c 实现。
 */
#ifndef DB_HEAPAM_H
#define DB_HEAPAM_H

#include "db/rel.h"
#include "db/buf.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 页面常量
 * ============================================================ */

/** 页面大小（8KB，与 PostgreSQL 一致） */
#define HEAP_PAGE_SIZE 8192

/** 页面头部大小 */
#define SizeOfPageHeaderData 24

/** LinePointer 大小（offset + flags + xmax） */
#define SizeOfHeapLinePointer 6

/** 最小元组大小 */
#define MinHeapTupleSize 23

/* ============================================================
 * 页面结构
 * ============================================================ */

/**
 * @brief 页面头部
 */
typedef struct PageHeaderData {
    uint32_t    pd_lsn;           /**< 最后修改的 LSN */
    uint16_t    pd_checksum;      /**< 页面校验和 */
    uint16_t    pd_flags;         /**< 页面标志 */
    uint16_t    pd_lower;         /**< 空闲空间起始位置 */
    uint16_t    pd_upper;         /**< 空闲空间结束位置 */
    uint16_t    pd_special;       /**< 特殊空间起始位置 */
    uint16_t    pd_pagesize_version; /**< 页面大小和版本 */
    uint32_t    pd_prune_xid;     /**< 可清理的最早事务 ID */
    uint16_t    pd_xid_base;      /**< 事务 ID 基准 */
    uint16_t    pd_multi_base;    /**< 多事务 ID 基准 */
} PageHeaderData;

/** 页面头部指针 */
typedef PageHeaderData *PageHeader;

/**
 * @brief LinePointer（指向元组）
 */
typedef struct HeapLinePointerData {
    uint32_t    t_off;            /**< 元组在页面中的偏移 */
    uint8_t     t_flags;          /**< 标志（LP_USED/LP_DEAD/LP_REDIRECT 等） */
    uint16_t    t_len;            /**< 元组长度（用于快速计算） */
} HeapLinePointerData;

/** LinePointer 指针 */
typedef HeapLinePointerData *HeapLinePointer;

/* ============================================================
 * 元组信息
 * ============================================================ */

/** ItemPointer（CTID）结构 - 指向元组的指针 */
typedef struct ItemPointerData {
    uint32_t    ip_blkid;         /**< 块号 */
    uint16_t    ip_posid;         /**< 位置号（LinePointer 索引） */
} ItemPointerData;

/** ItemPointer 指针类型 */
typedef ItemPointerData *ItemPointer;

/** 元组信息标志（t_infomask） */
#define HEAP_HASNULL          0x0001  /**< 包含 NULL 值 */
#define HEAP_HASVARWIDTH      0x0002  /**< 包含变长列 */
#define HEAP_HASEXTERNAL      0x0004  /**< 包含外部存储 */
#define HEAP_COMBOCID         0x0008  /**< 组合 CID */
#define HEAP_XMAX_EXCL_LOCK   0x0010  /**< 排他锁 */
#define HEAP_XMAX_SHARED_LOCK 0x0020  /**< 共享锁 */
#define HEAP_XMAX_COMMITTED   0x0040  /**< xmax 已提交 */
#define HEAP_XMAX_INVALID     0x0080  /**< xmax 无效 */
#define HEAP_XMIN_COMMITTED   0x0100  /**< xmin 已提交 */
#define HEAP_XMIN_INVALID     0x0200  /**< xmin 无效 */
#define HEAP_UPDATED          0x0400  /**< 该元组是 UPDATE 新版本 */
#define HEAP_MOVED_OFF        0x0800  /**< 元组已被移动到其他页面 */
#define HEAP_MOVED_IN         0x1000  /**< 元组已被移动到本页面 */
#define HEAP_XMAX_KEYSHR_LOCK 0x2000  /**< 排他键共享锁 */
#define HEAP_XMAX_SHR_LOCK    (0x4000 | HEAP_XMAX_EXCL_LOCK)  /**< 共享锁 */

/** ItemIdData flags */
#define LP_USED               0x01    /**< LinePointer 有效 */
#define LP_DEAD               0x02    /**< 元组已死亡 */
#define LP_MOVED_OFF          0x04    /**< 元组已移出页面 */
#define LP_MOVED_IN           0x08    /**< 元组已移入页面 */
#define LP_REDIRECT           0x10    /**< HOT 重定向指针 */
#define LP_REDIRECT_DEAD       (0x10 | 0x02)  /**< 死亡元组的 HOT 重定向 */

/** 元组状态 */
typedef enum HeapTupleStatus_e {
    HEAP_TUPLE_LIVE = 0,          /**< 存活元组 */
    HEAP_TUPLE_DEAD = 1,          /**< 死亡元组 */
    HEAP_TUPLE_RECENTLY_DEAD = 2, /**< 最近死亡 */
    HEAP_TUPLE_PAGE_ONLY = 3      /**< 仅页面可见 */
} HeapTupleStatus;

/* ============================================================
 * 元组描述符
 * ============================================================ */

/**
 * @brief 元组描述符
 *
 * 用于元组版本链追踪的关键字段：
 * - t_ctid: 指向同一版本链中下一个元组的指针
 * - t_xmin: 创建该元组的事务 ID
 * - t_xmax: 删除/更新该元组的事务 ID
 * - t_infomask: 元组属性标志（是否包含 NULL、是否更新版本等）
 */
typedef struct HeapTupleTableData {
    ItemPointerData t_ctid;       /**< 指向同一版本链中的下一个元组（CTID） */
    uint32_t        t_xmin;      /**< 创建该元组的事务 ID */
    uint32_t        t_xmax;      /**< 删除/更新该元组的事务 ID */
    uint16_t        t_infomask;   /**< 元组属性标志（t_infomask） */
    uint16_t        t_infomask2;  /**< 元组属性标志 2（保留） */
    uint8_t         t_hoff;       /**< 头部大小（固定部分之后的偏移） */
    uint8_t         t_cid;        /**< 命令 ID */
    uint32_t        t_tableOid;   /**< 表 OID */
    int16_t         t_len;        /**< 元组总长度 */
} HeapTupleTableData;

/** 元组指针 */
typedef HeapTupleTableData *HeapTuple;

/**
 * @brief 设置 ItemPointer 的值
 * @param ip ItemPointer
 * @param blk 块号
 * @param pos 位置号
 */
static inline void ItemPointerSet(ItemPointer ip, uint32_t blk, uint16_t pos) {
    if (ip) {
        ip->ip_blkid = blk;
        ip->ip_posid = pos;
    }
}

/**
 * @brief 判断两个 ItemPointer 是否相等
 * @param a ItemPointer a
 * @param b ItemPointer b
 * @return true 相等
 */
static inline bool ItemPointerEquals(ItemPointer a, ItemPointer b) {
    return a && b && a->ip_blkid == b->ip_blkid && a->ip_posid == b->ip_posid;
}

/* ============================================================
 * Heap AM 函数
 * ============================================================ */

/**
 * @brief 初始化 Heap AM
 * @return 0 成功
 */
int heapam_init(void);

/**
 * @brief 关闭 Heap AM
 */
void heapam_shutdown(void);

/**
 * @brief 插入元组到表
 * @param rel Relation
 * @param tuple 元组数据
 * @param len 元组长度
 * @param cid 命令 ID
 * @param options 插入选项
 * @param bistate 批量插入状态
 * @return 0 成功，-1 失败
 */
int heap_insert(Relation rel, const void *tuple, size_t len,
                uint32_t cid, int options, void *bistate);

/**
 * @brief 删除元组
 * @param rel Relation
 * @param tid 元组 ID
 * @param cid 命令 ID
 * @param crosscheck 交叉检查
 * @param wait 是否等待锁
 * @return 0 成功，-1 失败
 */
int heap_delete(Relation rel, const void *tid, uint32_t cid,
                bool crosscheck, bool wait);

/**
 * @brief 更新元组
 * @param rel Relation
 * @param tid 旧元组 ID
 * @param newtuple 新元组数据
 * @param newlen 新元组长度
 * @param cid 命令 ID
 * @param options 更新选项
 * @param bistate 批量状态
 * @param lockmode 锁模式
 * @return 0 成功，-1 失败
 */
int heap_update(Relation rel, const void *tid,
                const void *newtuple, size_t newlen,
                uint32_t cid, int options, void *bistate,
                int lockmode);

/**
 * @brief 锁定元组
 * @param rel Relation
 * @param tid 元组 ID
 * @param cid 命令 ID
 * @param mode 锁模式
 * @param nowait 是否不等锁
 * @param tm_result 输出结果
 * @return 0 成功，-1 失败
 */
int heap_lock_tuple(Relation rel, void *tid, uint32_t cid,
                    int mode, bool nowait, void *tm_result);

/* ============================================================
 * 页面操作
 * ============================================================ */

/**
 * @brief 初始化页面
 * @param page 页面数据
 * @param size 页面大小
 */
void heap_page_init(void *page, size_t size);

/**
 * @brief 获取页面头部
 * @param page 页面数据
 * @return 页面头部指针
 */
PageHeader heap_page_get_header(void *page);

/**
 * @brief 获取页面空闲空间大小
 * @param page 页面数据
 * @return 空闲空间字节数
 */
int heap_page_get_free_space(void *page);

/**
 * @brief 在页面中插入元组
 * @param page 页面数据
 * @param tuple 元组数据
 * @param len 元组长度
 * @param lp 元组指针输出
 * @return 0 成功，-1 空间不足
 */
int heap_page_add_tuple(void *page, const void *tuple, size_t len,
                        HeapLinePointer *lp);

/**
 * @brief 清理页面
 * @param page 页面数据
 * @return 清理的元组数
 */
int heap_page_prune(Relation rel, void *page);

/**
 * @brief 获取页面的元组数
 * @param page 页面数据
 * @return 元组数
 */
int heap_page_get_tuple_count(void *page);

/* ============================================================
 * 扫描操作
 * ============================================================ */

/**
 * @brief 获取下一个有效元组
 * @param scan 扫描描述符
 * @param direction 扫描方向
 * @return 元组指针，到末尾返回 NULL
 */
void *heap_getnext(TableScanDesc scan, ScanDirection direction);

/**
 * @brief 获取当前元组
 * @param scan 扫描描述符
 * @return 元组指针
 */
void *heap_getcurr(TableScanDesc scan);

/**
 * @brief 标记元组为死亡
 * @param rel Relation
 * @param tid 元组 ID
 */
void heap_mark_tuples_dead(Relation rel, void *tid);

/**
 * @brief 条件等待锁
 * @param rel Relation
 * @param tid 元组 ID
 * @param mode 锁模式
 * @return true 成功获得锁
 */
bool heap_lock_tuple_wait(Relation rel, void *tid, int mode);

/* ============================================================
 * 统计信息
 * ============================================================ */

/** Heap AM 统计信息 */
typedef struct HeapAMStats_s {
    uint64_t    inserts;          /**< 插入次数 */
    uint64_t    deletes;          /**< 删除次数 */
    uint64_t    updates;          /**< 更新次数 */
    uint64_t    tuples_read;      /**< 读取的元组数 */
    uint64_t    tuples_hot_updated; /**< HOT 更新次数 */
    uint64_t    dead_tuples;      /**< 死亡元组数 */
} HeapAMStats;

/**
 * @brief 获取 Heap AM 统计信息
 * @param stats 输出统计信息
 */
void heapam_get_stats(HeapAMStats *stats);

/**
 * @brief 重置统计信息
 */
void heapam_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_HEAPAM_H */
