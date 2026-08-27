/**
 * @file wal_recover.h
 * @brief 统一 WAL 恢复入口（C0-2 T9）
 *
 * 启动时按数据目录重放各模态 WAL，提供统一入口与回调注册机制。
 * 各模态实现 apply 回调，db_startup_recover() 按 WAL 记录类型分发。
 */
#ifndef DB_WAL_RECOVER_H
#define DB_WAL_RECOVER_H

#include <stdint.h>
#include <stddef.h>
#include "db/storage/wal/wal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*wal_recover_apply_fn)(void *ctx,
                                   wal_log_type_t type,
                                   const void *key, size_t key_len,
                                   const void *value, size_t value_len);

/**
 * @brief 注册模态 apply 回调
 * @param type WAL 记录类型（如 WAL_LOG_HEAP_INSERT）
 * @param apply 回调函数
 * @param ctx 回调上下文（通常为模态私有状态）
 * @return 0 成功，-1 失败（重复注册或超过最大槽位）
 */
int wal_recover_register(wal_log_type_t type, wal_recover_apply_fn apply, void *ctx);

/**
 * @brief 启动时重放 WAL
 * @param wal_path WAL 文件路径
 * @param page_size 数据库页面大小
 * @return 重放记录数；负数表示错误
 *
 * 流程：
 *   1. 打开 WAL 文件
 *   2. 顺序读记录（依赖 WAL 自带的 replay 函数）
 *   3. 按 type 分发到对应 apply 回调
 *   4. 失败记录日志但继续重放（不阻塞启动）
 */
int db_startup_recover(const char *wal_path, uint32_t page_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_WAL_RECOVER_H */
