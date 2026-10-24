/**
 * @file wal_flush.h
 * @brief WAL 刷盘策略与跨平台 fsync 封装（C0-2）
 *
 * 统一的 WAL 刷盘策略控制，跨平台 fsync 封装，配套 GUC `wal_sync_mode`。
 * 设计动机：消除"仅 fflush 无 fsync"、"失败静默吞"、"策略分散"等缺陷。
 */
#ifndef DB_WAL_FLUSH_H
#define DB_WAL_FLUSH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAL 刷盘策略（四级，参考 PostgreSQL synchronous_commit）
 *
 * - NONE：仅应用层缓存，不强制落盘
 * - OS：依赖 OS 刷盘
 * - FSYNC：强制 fsync / FlushFileBuffers（默认）
 * - BATCH：累积 batch_size 条或 batch_timeout_ms 毫秒后 fsync
 */
typedef enum wal_flush_policy_e {
    WAL_FLUSH_NONE = 0,
    WAL_FLUSH_OS = 1,
    WAL_FLUSH_FSYNC = 2,
    WAL_FLUSH_BATCH = 3
} wal_flush_policy_t;

/** 当前全局刷盘策略 */
extern wal_flush_policy_t g_wal_flush_policy;

/** BATCH 模式阈值（默认值，可由 GUC 调整） */
#define WAL_FLUSH_BATCH_DEFAULT_SIZE    128
#define WAL_FLUSH_BATCH_DEFAULT_TIMEOUT 100  /* 毫秒 */

/**
 * @brief 跨平台 fsync 封装
 * @param fd 文件描述符（POSIX）或 HANDLE（Windows）
 * @return 0 成功，-1 失败
 *
 * - POSIX：调用 fsync(fd)
 * - Windows：调用 FlushFileBuffers((HANDLE)_get_osfhandle(fd))
 */
int db_fsync(int fd);

/**
 * @brief 设置刷盘策略（同时设置 GUC 变量）
 */
void wal_flush_set_policy(wal_flush_policy_t policy);

/**
 * @brief 获取当前刷盘策略
 */
wal_flush_policy_t wal_flush_get_policy(void);

/**
 * @brief 将策略名（字符串）解析为枚举
 * @param name "none" | "os" | "fsync" | "batch"
 * @return 枚举值；解析失败返回 WAL_FLUSH_FSYNC（默认值）
 */
wal_flush_policy_t wal_flush_parse(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* DB_WAL_FLUSH_H */
