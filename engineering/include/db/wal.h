/**
 * @file wal.h
 * @brief Write-Ahead Logging (WAL) 日志系统
 *
 * WAL 格式设计：
 * ┌─────────────────────────────────────────────────────────┐
 * │                    WAL 文件结构                         │
 * ├─────────────────────────────────────────────────────────┤
 * │ Header (64 bytes)                                       │
 * │  - magic: 0x57414C31 ("WAL1")    [4 bytes]              │
 * │  - version: 1                    [4 bytes]              │
 * │  - page_size: 数据库页面大小      [4 bytes]              │
 * │  - checksum: 头部校验和           [4 bytes]              │
 * │  - reserved                       [48 bytes]            │
 * ├─────────────────────────────────────────────────────────┤
 * │ Log Records (循环追加)                                  │
 * │  ┌─────────────────────────────────────────────────┐    │
 * │  │ Record Header (24 bytes)                        │    │
 * │  │  - type: 日志类型         [1 byte]              │    │
 * │  │  - size: 记录大小         [3 bytes]             │    │
 * │  │  - lsn: 日志序列号        [8 bytes]             │    │
 * │  │  - txn_id: 事务ID        [4 bytes]             │    │
 * │  │  - prev_lsn: 上条日志    [4 bytes]             │    │
 * │  │  - checksum: 记录校验和  [4 bytes]             │    │
 * │  ├─────────────────────────────────────────────────┤    │
 * │  │ Record Data                                      │    │
 * │  │  - 修改的数据内容                                │    │
 * │  └─────────────────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────┘
 *
 * 日志类型：
 * - UPDATE: 更新记录（包含旧值用于 undo，新值用于 redo）
 * - INSERT: 插入记录
 * - DELETE: 删除记录
 * - COMMIT: 事务提交
 * - ABORT:  事务回滚
 * - CHECKPOINT: 检查点标记
 */
#ifndef DB_WAL_H
#define DB_WAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** WAL 文件魔数 "WAL1" */
#define WAL_MAGIC 0x57414C31

/** WAL 版本号 */
#define WAL_VERSION 1

/** WAL 文件头大小 */
#define WAL_HEADER_SIZE 64

/** 日志记录头大小 */
#define WAL_RECORD_HEADER_SIZE 24

/** 最大日志文件大小 (64MB) */
#define WAL_MAX_FILE_SIZE (64 * 1024 * 1024)

/** 默认 WAL 缓冲区大小 (1MB) */
#define WAL_BUFFER_SIZE (1024 * 1024)

/* ============================================================
 * 日志类型
 * ============================================================ */

/** 日志记录类型 */
typedef enum wal_log_type_e {
    WAL_LOG_UPDATE = 1,   /**< 更新记录 */
    WAL_LOG_INSERT = 2,   /**< 插入记录 */
    WAL_LOG_DELETE = 3,   /**< 删除记录 */
    WAL_LOG_COMMIT = 4,   /**< 事务提交 */
    WAL_LOG_ABORT = 5,    /**< 事务回滚 */
    WAL_LOG_CHECKPOINT = 6, /**< 检查点 */
    WAL_LOG_BEGIN = 7,    /**< 事务开始 */
    WAL_LOG_PREPARE = 8   /**< 两阶段提交 PREPARE */
} wal_log_type_t;

/* ============================================================
 * 日志记录头
 * ============================================================ */

#pragma pack(push, 1)

/** 日志记录头（24字节） */
typedef struct wal_record_header_s {
    uint8_t  type;           /**< 日志类型 */
    uint8_t  size[3];        /**< 记录大小（小端序，3字节） */
    uint64_t lsn;            /**< 日志序列号 */
    uint32_t txn_id;         /**< 事务ID */
    uint32_t prev_lsn;       /**< 上一条日志的 LSN */
    uint32_t checksum;       /**< 记录校验和 */
} wal_record_header_t;

#pragma pack(pop)

/* ============================================================
 * WAL 上下文
 * ============================================================ */

/** WAL 状态 */
typedef enum wal_state_e {
    WAL_STATE_IDLE = 0,      /**< 空闲 */
    WAL_STATE_ACTIVE = 1,    /**< 活动中 */
    WAL_STATE_CHECKPOINT = 2 /**< 正在做检查点 */
} wal_state_t;

/** WAL 句柄 */
typedef struct wal_s wal_t;

/** WAL 统计信息 */
typedef struct wal_stats_s {
    uint64_t total_records;      /**< 总日志记录数 */
    uint64_t total_bytes;        /**< 总日志大小 */
    uint64_t current_lsn;        /**< 当前 LSN */
    uint64_t checkpoint_lsn;     /**< 上次检查点 LSN */
    uint32_t active_txns;        /**< 活动事务数 */
} wal_stats_t;

/* ============================================================
 * API 声明
 * ============================================================ */

/**
 * @brief 创建 WAL
 * @param path WAL 文件路径
 * @param page_size 数据库页面大小
 * @return WAL 句柄，失败返回 NULL
 */
wal_t *wal_create(const char *path, uint32_t page_size);

/**
 * @brief 打开已有 WAL
 * @param path WAL 文件路径
 * @return WAL 句柄，失败返回 NULL
 */
wal_t *wal_open(const char *path);

/**
 * @brief 关闭 WAL
 * @param wal WAL 句柄
 * @return 0 成功
 */
int wal_close(wal_t *wal);

/**
 * @brief 刷日志到磁盘
 * @param wal WAL 句柄
 * @return 0 成功
 */
int wal_flush(wal_t *wal);

/**
 * @brief 获取当前 LSN
 * @param wal WAL 句柄
 * @return 当前 LSN
 */
uint64_t wal_get_lsn(wal_t *wal);

/* ============================================================
 * 日志写入 API
 * ============================================================ */

/**
 * @brief 写入事务开始日志
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @return LSN，失败返回 0
 */
uint64_t wal_write_begin(wal_t *wal, uint32_t txn_id);

/**
 * @brief 写入插入日志
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @param key 键
 * @param key_len 键长度
 * @param value 值
 * @param value_len 值长度
 * @return LSN，失败返回 0
 */
uint64_t wal_write_insert(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *value, size_t value_len);

/**
 * @brief 写入更新日志（包含旧值，用于 undo）
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @param key 键
 * @param key_len 键长度
 * @param old_value 旧值（用于回滚）
 * @param old_len 旧值长度
 * @param new_value 新值（用于恢复）
 * @param new_len 新值长度
 * @return LSN，失败返回 0
 */
uint64_t wal_write_update(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *old_value, size_t old_len,
                          const void *new_value, size_t new_len);

/**
 * @brief 写入删除日志
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @param key 键
 * @param key_len 键长度
 * @param old_value 删除前的值（用于回滚）
 * @param old_len 旧值长度
 * @return LSN，失败返回 0
 */
uint64_t wal_write_delete(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *old_value, size_t old_len);

/**
 * @brief 写入事务预提交日志（两阶段提交 PREPARE）
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @return LSN，失败返回 0
 */
uint64_t wal_write_prepare(wal_t *wal, uint32_t txn_id);

/**
 * @brief 写入事务提交日志
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @return LSN，失败返回 0
 */
uint64_t wal_write_commit(wal_t *wal, uint32_t txn_id);

/**
 * @brief 写入事务回滚日志
 * @param wal WAL 句柄
 * @param txn_id 事务ID
 * @return LSN，失败返回 0
 */
uint64_t wal_write_abort(wal_t *wal, uint32_t txn_id);

/**
 * @brief 写入检查点日志
 * @param wal WAL 句柄
 * @param dirty_pages 脏页列表（page_id 数组）
 * @param num_pages 脏页数量
 * @return LSN，失败返回 0
 */
uint64_t wal_write_checkpoint(wal_t *wal, const uint32_t *dirty_pages, size_t num_pages);

/* ============================================================
 * 恢复 API
 * ============================================================ */

/**
 * @brief 恢复状态（用于崩溃恢复）
 */
typedef struct wal_recovery_info_s {
    uint64_t last_checkpoint_lsn;   /**< 上次检查点 LSN */
    uint32_t *active_txns;          /**< 活动事务列表 */
    size_t    active_txn_count;     /**< 活动事务数量 */
} wal_recovery_info_t;

/**
 * @brief 分析 WAL 文件，收集恢复所需信息
 * @param path WAL 文件路径
 * @param info 输出恢复信息
 * @return 0 成功，-1 失败
 */
int wal_analyze(const char *path, wal_recovery_info_t *info);

/**
 * @brief 执行 redo（重做已提交事务的修改）
 * @param path WAL 文件路径
 * @param start_lsn 从哪个 LSN 开始 redo
 * @param apply_fn 应用日志记录的回调函数
 * @param ctx 回调上下文
 * @return 0 成功，-1 失败
 *
 * apply_fn 签名：
 *   int (*apply_fn)(void *ctx, wal_log_type_t type,
 *                   const void *key, size_t key_len,
 *                   const void *value, size_t value_len);
 */
typedef int (*wal_apply_fn)(void *ctx, wal_log_type_t type,
                            const void *key, size_t key_len,
                            const void *value, size_t value_len);

int wal_redo(const char *path, uint64_t start_lsn,
             wal_apply_fn apply_fn, void *ctx);

/**
 * @brief 执行 undo（撤销未提交事务的修改）
 * @param path WAL 文件路径
 * @param txn_id 事务ID
 * @param start_lsn 从哪个 LSN 开始 undo
 * @param apply_fn 应用日志记录的回调函数
 * @param ctx 回调上下文
 * @return 0 成功，-1 失败
 */
int wal_undo(const char *path, uint32_t txn_id, uint64_t start_lsn,
             wal_apply_fn apply_fn, void *ctx);

/**
 * @brief 释放恢复信息
 * @param info 恢复信息
 */
void wal_recovery_info_free(wal_recovery_info_t *info);

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 获取 WAL 统计信息
 * @param wal WAL 句柄
 * @param stats 输出统计信息
 * @return 0 成功
 */
int wal_get_stats(wal_t *wal, wal_stats_t *stats);

/**
 * @brief 检查 WAL 是否需要做检查点
 * @param wal WAL 句柄
 * @return true 需要检查点
 */
bool wal_need_checkpoint(wal_t *wal);

/**
 * @brief 获取错误信息
 * @param wal WAL 句柄
 * @return 错误信息字符串
 */
const char *wal_errmsg(wal_t *wal);

#ifdef __cplusplus
}
#endif

#endif /* DB_WAL_H */
