/**
 * @file buf.h
 * @brief Buffer Pool 公共接口
 *
 * Buffer Pool 是数据库的内存缓存层，用于缓存磁盘页面。
 * 参考 PostgreSQL 的 bufmgr.c 实现。
 */
#ifndef DB_BUF_H
#define DB_BUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认 Buffer 数量 */
#define BUF_DEFAULT_NBUFFERS 1024

/** 页面大小（与 OS 页面对齐） */
#define BUF_PAGE_SIZE 8192

/** Hash 表大小因子 */
#define BUF_HASH_SIZE_FACTOR 2

/** Buffer 状态标志 */
typedef uint32_t BufferState;

/** Buffer 状态位 */
#define BM_VALID           0x00000001  /**< 页面数据有效 */
#define BM_DIRTY           0x00000002  /**< 页面被修改 */
#define BM_PINNED          0x00000004  /**< 页面被 pin 住 */
#define BM_IO_IN_PROGRESS  0x00000008  /**< 正在读取 */
#define BM_IO_COMPLETED    0x00000010  /**< 读取完成 */
#define BM_WRITING         0x00000020  /**< 正在写入 */
#define BM_JUST_DIRTIED    0x00000040  /**< 刚刚标记为脏 */
#define BM_CORRUPTED       0x00000080  /**< 页面数据损坏（校验和不通过） */

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct RelationData *Relation;
typedef struct BufferDesc_s BufferDesc;
typedef struct BufferPool_s BufferPool;
typedef struct lock_manager_s lock_manager_t;

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 块号类型 */
typedef uint32_t BlockNumber;

/** 缓冲区 ID 类型 */
typedef int32_t Buffer;

/** 无效缓冲区 */
#define InvalidBuffer (-1)

/** LSN (Log Sequence Number) */
typedef uint64_t LSN;

/* ============================================================
 * Buffer 描述符
 * ============================================================ */

/**
 * @brief Buffer 描述符结构
 *
 * 每个 Buffer 有一个描述符，包含元数据和锁
 */
struct BufferDesc_s {
    uint32_t    buf_id;           /**< Buffer ID (0 到 N-1) */
    BufferState state;            /**< 状态标志（atomic） */
    uint32_t    relfilenode;      /**< 物理文件节点 */
    BlockNumber blocknum;         /**< 块号 */
    int         usage_count;      /**< Clock-Sweep 计数 */

    /* 时间戳和 LSN */
    LSN         last_written;     /**< 最后写入的 LSN */

    /* 文件描述符（刷盘用） */
    void       *file;             /**< 对应的 db_file_t* */

    /* Hash 链表指针（用于快速查找） */
    uint32_t    hash_prev;        /**< Hash 链表前驱 */
    uint32_t    hash_next;        /**< Hash 链表后继 */

    /* 引用计数（pin 计数） */
    int         refcount;         /**< 引用计数（atomic） */
};

/* ============================================================
 * Buffer 公共 API
 * ============================================================ */

/**
 * @brief 初始化 Buffer Pool
 * @param nbuffers buffer 数量（0 表示使用默认值）
 * @return 0 成功，-1 失败
 */
int buf_init(int nbuffers);

/**
 * @brief 关闭 Buffer Pool
 * @note 会刷写所有脏页
 */
void buf_shutdown(void);

/**
 * @brief 获取 Buffer 数量
 * @return Buffer 数量
 */
int buf_get_nbuffers(void);

/* ============================================================
 * 页面读取与分配
 * ============================================================ */

/**
 * @brief 读取页面到 Buffer
 * @param relfilenode 物理文件节点
 * @param blocknum 块号
 * @param access_mode 访问模式（0=只读，1=可写）
 * @return Buffer 描述符指针，失败返回 NULL
 */
BufferDesc *buf_read(uint32_t relfilenode, BlockNumber blocknum, int access_mode);

/**
 * @brief 获取新页面（用于插入）
 * @param relfilenode 物理文件节点
 * @return Buffer 描述符指针
 *
 * @note 如果表已满，分配新页面
 */
BufferDesc *buf_new(uint32_t relfilenode);

/**
 * @brief 获取扩展表的新页面
 * @param relfilenode 物理文件节点
 * @return Buffer 描述符指针
 */
BufferDesc *buf_new_page(uint32_t relfilenode);

/**
 * @brief 分配专用临时页面（不写回磁盘）
 * @return Buffer 描述符指针
 */
BufferDesc *buf_new_temp(void);

/* ============================================================
 * Buffer Pin/Unpin
 * ============================================================ */

/**
 * @brief Pin 住 Buffer（增加引用计数）
 * @param buf Buffer 描述符
 */
void buf_pin(BufferDesc *buf);

/**
 * @brief Unpin Buffer（减少引用计数）
 * @param buf Buffer 描述符
 */
void buf_unpin(BufferDesc *buf);

/**
 * @brief 检查 Buffer 是否被 pin 住
 * @param buf Buffer 描述符
 * @return true 被 pin 住
 */
bool buf_ispinned(BufferDesc *buf);

/* ============================================================
 * Buffer Hash 表操作（供 Heap AM 等模块使用）
 * ============================================================ */

/**
 * @brief 将 (relfilenode, blocknum) → buf_id 映射插入 Hash 表
 * @param relfilenode 物理文件节点
 * @param blocknum 块号
 * @param buf_id Buffer ID
 *
 * @note buf_new() 分配 buffer 后需要调用此函数注册映射，
 *       否则后续 buf_read() 无法找到该页面
 */
void buf_hash_insert(uint32_t relfilenode, BlockNumber blocknum, uint32_t buf_id);

/**
 * @brief 从 Hash 表中删除 (relfilenode, blocknum) 映射
 * @param relfilenode 物理文件节点
 * @param blocknum 块号
 */
void buf_hash_delete(uint32_t relfilenode, BlockNumber blocknum);

/* ============================================================
 * 脏页标记
 * ============================================================ */

/**
 * @brief 标记 Buffer 为脏
 * @param buf Buffer 描述符
 */
void buf_dirty(BufferDesc *buf);

/**
 * @brief 取消脏标记
 * @param buf Buffer 描述符
 */
void buf_clean(BufferDesc *buf);

/**
 * @brief 检查 Buffer 是否脏
 * @param buf Buffer 描述符
 * @return true 脏
 */
bool buf_isdirty(BufferDesc *buf);

/* ============================================================
 * 页面刷盘
 * ============================================================ */

/**
 * @brief 刷写所有脏页到磁盘
 * @return 刷写的 Buffer 数量
 */
int buf_flush_all(void);

/**
 * @brief 刷写指定关系的所有脏页
 * @param relfilenode 物理文件节点
 * @return 刷写的 Buffer 数量
 */
int buf_flush_relation(uint32_t relfilenode);

/**
 * @brief 刷写单个 Buffer
 * @param buf Buffer 描述符
 * @return 0 成功，-1 失败
 */
int buf_write(BufferDesc *buf);

/* ============================================================
 * Buffer 数据访问
 * ============================================================ */

/**
 * @brief 获取 Buffer 内容指针
 * @param buf Buffer 描述符
 * @return 页面数据指针
 */
void *buf_get_data(BufferDesc *buf);

/**
 * @brief 获取 Buffer 对应的块号
 * @param buf Buffer 描述符
 * @return 块号
 */
BlockNumber buf_get_blocknum(BufferDesc *buf);

/**
 * @brief 获取 Buffer 对应的 relfilenode
 * @param buf Buffer 描述符
 * @return relfilenode
 */
uint32_t buf_get_relfilenode(BufferDesc *buf);

/**
 * @brief 获取 Buffer ID
 * @param buf Buffer 描述符
 * @return Buffer ID
 */
int buf_get_id(BufferDesc *buf);

/* ============================================================
 * Buffer 状态查询
 * ============================================================ */

/**
 * @brief 获取 Buffer 状态
 * @param buf Buffer 描述符
 * @return 状态标志
 */
BufferState buf_get_state(BufferDesc *buf);

/**
 * @brief 检查 Buffer 是否有效
 * @param buf Buffer 描述符
 * @return true 有效
 */
bool buf_isvalid(BufferDesc *buf);

/* ============================================================
 * 统计信息
 * ============================================================ */

/** Buffer 统计信息 */
typedef struct buf_stats_s {
    uint64_t    hits;             /**< 命中次数 */
    uint64_t    misses;           /**< 未命中次数 */
    uint64_t    writes;           /**< 写入次数 */
    uint64_t    reads;            /**< 读取次数 */
    uint64_t    bg_writes;        /**< 后台写入次数 */
    int         num_buf_alloc;    /**< 当前分配的 Buffer 数 */
    int         num_buf_used;     /**< 正在使用的 Buffer 数 */
} buf_stats_t;

/**
 * @brief 获取 Buffer 统计信息
 * @param stats 输出统计信息
 */
void buf_get_stats(buf_stats_t *stats);

/**
 * @brief 重置统计信息
 */
void buf_reset_stats(void);

/* ============================================================
 * 调试
 * ============================================================ */

/**
 * @brief 转储 Buffer Pool 状态
 */
void buf_dump(void);

/**
 * @brief 验证 Buffer Pool 一致性
 * @return true 一致
 */
bool buf_check_consistency(void);

/* ============================================================
 * 页面锁操作（并发安全）
 * ============================================================ */

/**
 * @brief 获取页面锁
 * @param relfilenode 物理文件节点
 * @param blocknum 块号
 * @param exclusive 是否排他锁（true=写锁，false=读锁）
 * @param timeout_ms 超时时间（毫秒）
 * @return LOCK_OK 成功，LOCK_TIMEOUT 超时，LOCK_DEADLOCK 死锁
 */
int buf_lock_page(uint32_t relfilenode, BlockNumber blocknum,
                  bool exclusive, uint32_t timeout_ms);

/**
 * @brief 释放页面锁
 * @param relfilenode 物理文件节点
 * @param blocknum 块号
 * @param exclusive 是否排他锁
 */
void buf_unlock_page(uint32_t relfilenode, BlockNumber blocknum, bool exclusive);

/**
 * @brief 初始化 Buffer Pool 锁子系统
 * @return 0 成功
 */
int buf_lock_init(void);

/**
 * @brief 关闭 Buffer Pool 锁子系统
 */
void buf_lock_shutdown(void);

/**
 * @brief 获取 Buffer Pool 的锁管理器
 * @return 锁管理器指针
 */
lock_manager_t *buf_get_lockmgr(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_BUF_H */
