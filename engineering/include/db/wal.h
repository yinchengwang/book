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

/** 默认 WAL 段大小 (16MB) */
#define WAL_SEGMENT_SIZE (16 * 1024 * 1024)

/** WAL 段目录名 */
#define WAL_SEGMENT_DIR "pg_wal"

/** WAL 段文件名格式：<timeline>_<segno> */
#define WAL_SEGMENT_NAME_FORMAT "%08X_%08X"

/** 默认时间线 ID */
#define WAL_DEFAULT_TIMELINE 1

/** 默认缓冲区大小 (1MB) */
#define WAL_BUFFER_SIZE (1024 * 1024)

/** 归档目录名 */
#define WAL_ARCHIVE_DIR "archive"

/** 归档命令最大长度 */
#define WAL_ARCHIVE_CMD_MAX 1024

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
 * WAL 归档 API
 * ============================================================ */

/**
 * @brief 设置归档命令
 * @param wal WAL 句柄
 * @param archive_command 归档命令（%p 替换段路径，%f 替换段文件名）
 * @return 0 成功，-1 失败
 */
int wal_set_archive_command(wal_t *wal, const char *archive_command);

/**
 * @brief 获取当前归档命令
 * @param wal WAL 句柄
 * @return 归档命令字符串（内部存储，不需要释放）
 */
const char *wal_get_archive_command(wal_t *wal);

/**
 * @brief 设置归档目录
 * @param wal WAL 句柄
 * @param archive_dir 归档目录路径
 * @return 0 成功，-1 失败
 */
int wal_set_archive_dir(wal_t *wal, const char *archive_dir);

/**
 * @brief 获取当前归档目录
 * @param wal WAL 句柄
 * @return 归档目录字符串（内部存储，不需要释放）
 */
const char *wal_get_archive_dir(wal_t *wal);

/**
 * @brief 检查归档是否启用
 * @param wal WAL 句柄
 * @return true 启用，false 未启用
 */
bool wal_archive_enabled(wal_t *wal);

/**
 * @brief 归档已完成的段文件
 * @param wal WAL 句柄
 * @param seg_path 段文件路径
 * @return 0 成功，-1 失败
 */
int wal_archive_segment(wal_t *wal, const char *seg_path);

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
 * WAL 段文件管理 API
 * ============================================================ */

/**
 * @brief 设置 WAL 段目录
 * @param wal WAL 句柄
 * @param dir 段目录路径（如 "pg_wal"）
 * @return 0 成功，-1 失败
 */
int wal_set_segment_dir(wal_t *wal, const char *dir);

/**
 * @brief 获取当前段文件路径
 * @param wal WAL 句柄
 * @return 段文件路径，NULL 表示无活动段
 */
const char *wal_current_segment_path(wal_t *wal);

/**
 * @brief 获取当前段序号
 * @param wal WAL 句柄
 * @return 段序号
 */
uint32_t wal_current_segment_no(wal_t *wal);

/**
 * @brief 强制切换 WAL 段文件
 *
 * 关闭当前段，创建新段，写入头部。
 *
 * @param wal WAL 句柄
 * @return 0 成功，-1 失败
 */
int wal_switch_segment(wal_t *wal);

/**
 * @brief 获取指定 LSN 对应的段文件路径
 * @param wal WAL 句柄
 * @param lsn LSN
 * @return 段文件路径，需要调用 free 释放
 */
char *wal_segment_path_for_lsn(wal_t *wal, uint64_t lsn);

/**
 * @brief 列出所有 WAL 段文件
 * @param wal WAL 句柄
 * @param segments 输出：段文件路径数组，需要调用 wal_free_segment_list 释放
 * @param count 输出：段文件数量
 * @return 0 成功，-1 失败
 */
int wal_list_segments(wal_t *wal, char ***segments, int *count);

/**
 * @brief 释放段文件列表
 * @param segments 段文件路径数组
 * @param count 数组长度
 */
void wal_free_segment_list(char **segments, int count);

/* ============================================================
 * PITR 恢复 API
 * ============================================================ */

/**
 * @brief PITR 恢复目标
 */
typedef enum wal_recovery_target_e {
    WAL_RECOVERY_END = 0,      /**< 恢复到最新状态 */
    WAL_RECOVERY_LSN = 1,      /**< 恢复到指定 LSN */
    WAL_RECOVERY_TIMESTAMP = 2, /**< 恢复到指定时间戳 */
    WAL_RECOVERY_XID = 3       /**< 恢复到指定事务 ID */
} wal_recovery_target_t;

/**
 * @brief PITR 恢复选项
 */
typedef struct wal_recovery_options_s {
    wal_recovery_target_t target_type;  /**< 恢复目标类型 */
    uint64_t target_lsn;                 /**< 目标 LSN */
    uint64_t target_timestamp;           /**< 目标时间戳 */
    uint32_t target_xid;                 /**< 目标事务 ID */
    const char *archive_dir;             /**< 归档目录 */
    bool pause_at_recovery_target;       /**< 是否暂停在恢复点 */
} wal_recovery_options_t;

/**
 * @brief 执行 PITR 恢复
 * @param wal_dir WAL 目录
 * @param data_dir 数据目录
 * @param options 恢复选项
 * @param progress_cb 进度回调（可选）
 * @return 0 成功，-1 失败
 */
int wal_recover(const char *wal_dir, const char *data_dir,
                const wal_recovery_options_t *options,
                void (*progress_cb)(uint64_t current_lsn, uint64_t target_lsn));

/**
 * @brief 获取指定 LSN 之前的最近检查点
 * @param wal_dir WAL 目录
 * @param before_lsn 在此 LSN 之前的检查点
 * @param checkpoint_lsn 输出：检查点 LSN
 * @param checkpoint_rec 输出：检查点记录数据
 * @param rec_size 输出：记录大小
 * @return 0 成功，-1 失败
 */
int wal_get_checkpoint_before(const char *wal_dir, uint64_t before_lsn,
                              uint64_t *checkpoint_lsn, void *checkpoint_rec, size_t *rec_size);

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
