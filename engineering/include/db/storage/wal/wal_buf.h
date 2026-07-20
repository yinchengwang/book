/**
 * @file wal_buf.h
 * @brief WAL 与 Buffer Pool 协调层
 *
 * 负责 WAL 日志与 Buffer Pool 的协调：
 * 1.脏页标记与 WAL LSN 关联
 * 2.事务提交时确保日志先刷盘
 * 3.检查点时协调刷脏页
 * 4.崩溃恢复时的 redo/undo
 */
#ifndef DB_WAL_BUF_H
#define DB_WAL_BUF_H

#include "wal.h"
#include "db/buf.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct wal_buf_s wal_buf_t;
typedef struct txn_s txn_t;

/* ============================================================
 * WAL-Buf 协调器
 * ============================================================ */

/**
 * @brief WAL-Buf 协调器
 *
 * 维护 Buffer 与 WAL 的关联关系
 */
struct wal_buf_s {
    wal_t      *wal;               /**< WAL 句柄 */
    void       *buffer_pool;       /**< Buffer Pool 指针 */

    /* 脏页追踪 */
    uint32_t    dirty_count;       /**< 脏页数量 */
    uint32_t   *dirty_pages;       /**< 脏页列表 */
    uint32_t    dirty_capacity;    /**< 脏页列表容量 */

    /* LSN 追踪 */
    uint64_t    last_flush_lsn;    /**< 上次刷盘的 LSN */
    uint64_t    oldest_dirty_lsn;  /**< 最旧的脏 LSN */

    /* 统计 */
    uint64_t    log_waits;         /**< 等待日志刷盘的次数 */
    uint64_t    sync_waits;        /**< 等待同步的次数 */
};

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief 创建 WAL-Buf 协调器
 * @param wal WAL 句柄
 * @param buffer_pool Buffer Pool 指针
 * @return 协调器句柄
 */
wal_buf_t *wal_buf_create(wal_t *wal, void *buffer_pool);

/**
 * @brief 销毁 WAL-Buf 协调器
 * @param wb 协调器
 */
void wal_buf_destroy(wal_buf_t *wb);

/**
 * @brief 脏页写入 WAL（记录修改前的值）
 * @param wb 协调器
 * @param buf Buffer 描述符
 * @param txn_id 事务 ID
 * @return LSN
 */
uint64_t wal_buf_log_dirty(wal_buf_t *wb, BufferDesc *buf, uint32_t txn_id);

/**
 * @brief 标记 Buffer 为脏并记录到 WAL
 * @param wb 协调器
 * @param buf Buffer 描述符
 * @param txn_id 事务 ID
 */
void wal_buf_mark_dirty(wal_buf_t *wb, BufferDesc *buf, uint32_t txn_id);

/**
 * @brief 事务提交前刷日志
 * @param wb 协调器
 * @param txn_id 事务 ID
 * @return 0 成功
 *
 * @note 确保事务的所有日志都已刷到磁盘
 */
int wal_buf_commit(wal_buf_t *wb, uint32_t txn_id);

/**
 * @brief 执行检查点
 * @param wb 协调器
 * @return 0 成功
 *
 * @note 刷所有脏页并写入检查点日志
 */
int wal_buf_checkpoint(wal_buf_t *wb);

/**
 * @brief 获取需要恢复的 LSN
 * @param wb 协调器
 * @return 检查点 LSN（从该位置开始恢复）
 */
uint64_t wal_buf_get_recovery_lsn(wal_buf_t *wb);

/**
 * @brief 等待指定 LSN 之前的日志都已刷盘
 * @param wb 协调器
 * @param lsn 目标 LSN
 * @return 0 成功
 */
int wal_buf_wait_lsn(wal_buf_t *wb, uint64_t lsn);

/**
 * @brief 获取脏页数量
 * @param wb 协调器
 * @return 脏页数
 */
uint32_t wal_buf_dirty_count(wal_buf_t *wb);

/**
 * @brief 获取脏页列表
 * @param wb 协调器
 * @param pages 输出数组（调用者分配）
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t wal_buf_get_dirty_pages(wal_buf_t *wb, uint32_t *pages, uint32_t max_count);

/**
 * @brief 脏页是否需要刷盘
 * @param wb 协调器
 * @param buf Buffer 描述符
 * @return true 需要刷盘
 */
bool wal_buf_needs_flush(wal_buf_t *wb, BufferDesc *buf);

/* ============================================================
 * 恢复相关
 * ============================================================ */

/**
 * @brief 初始化恢复状态
 * @param wb 协调器
 * @return 0 成功
 */
int wal_buf_recovery_init(wal_buf_t *wb);

/**
 * @brief 执行 redo
 * @param wb 协调器
 * @param start_lsn 起始 LSN
 * @return 0 成功
 */
int wal_buf_do_redo(wal_buf_t *wb, uint64_t start_lsn);

/**
 * @brief 执行 undo
 * @param wb 协调器
 * @param txn_id 事务 ID
 * @return 0 成功
 */
int wal_buf_do_undo(wal_buf_t *wb, uint32_t txn_id);

/* ============================================================
 * 统计
 * ============================================================ */

/**
 * @brief WAL-Buf 统计信息
 */
typedef struct wal_buf_stats_s {
    uint64_t    log_waits;         /**< 日志等待次数 */
    uint64_t    sync_waits;        /**< 同步等待次数 */
    uint32_t    dirty_count;       /**< 当前脏页数 */
    uint64_t    last_flush_lsn;    /**< 上次刷盘 LSN */
    uint64_t    oldest_dirty_lsn;  /**< 最旧脏 LSN */
} wal_buf_stats_t;

/**
 * @brief 获取统计信息
 * @param wb 协调器
 * @param stats 输出统计
 */
void wal_buf_get_stats(wal_buf_t *wb, wal_buf_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_WAL_BUF_H */
