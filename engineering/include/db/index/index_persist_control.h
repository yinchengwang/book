/**
 * @file index_persist_control.h
 * @brief 索引持久化控制模块 - 公共头文件
 *
 * 声明 persist_control_t（不透明类型）及其查询与日志辅助 API。
 *
 * 设计要点：
 *   - persist_enabled=true  : 完整 MVCC + WAL + Redo + Checkpoint
 *   - persist_enabled=false : 纯内存 + Undo（无崩溃恢复）
 *   - 本头文件仅暴露不透明指针，避免被调用方窥探内部结构
 */
#ifndef DB_INDEX_PERSIST_CONTROL_H
#define DB_INDEX_PERSIST_CONTROL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明类型：定义见 index_persist_control.c */
typedef struct persist_control persist_control_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 创建持久化控制
 *
 * @param enabled 是否启用持久化
 * @return 新创建的 control 指针；失败返回 NULL
 *
 * @note 调用方负责通过 persist_control_destroy 释放返回的指针。
 */
persist_control_t *persist_control_create(bool enabled);

/**
 * @brief 销毁持久化控制
 *
 * @param ctrl 待销毁的 control 指针（允许 NULL，空操作）
 */
void persist_control_destroy(persist_control_t *ctrl);

/* ============================================================
 * 查询 API
 * ============================================================ */

/**
 * @brief 检查是否启用持久化
 *
 * @param ctrl 持久化控制指针（允许 NULL，视为未启用）
 * @return true 表示启用持久化；false 表示纯内存模式或 NULL
 */
bool persist_is_enabled(const persist_control_t *ctrl);

/* ============================================================
 * 日志辅助 API
 * ============================================================ */

/**
 * @brief 写入 WAL 日志（仅 persist_enabled=true 时实际落盘）
 *
 * @param ctrl 持久化控制指针
 * @param data 待写入数据
 * @param len  数据长度
 * @return 0 成功；非 0 失败
 *
 * @note 纯内存模式下为 no-op，直接返回 0。
 */
int persist_write_wal(persist_control_t *ctrl, const void *data, size_t len);

/**
 * @brief 记录 Undo 信息（两种模式都需要）
 *
 * @param ctrl      持久化控制指针
 * @param undo_data Undo 数据
 * @param len       Undo 数据长度
 * @return 0 成功；非 0 失败
 *
 * @note 两种模式都记录 Undo，用于事务级回滚。
 */
int persist_write_undo(persist_control_t *ctrl, const void *undo_data, size_t len);

/**
 * @brief 提交事务（清理 Undo）
 *
 * @param ctrl 持久化控制指针
 * @return 0 成功；非 0 失败
 *
 * @note 纯内存模式下为 no-op，直接返回 0。
 */
int persist_commit(persist_control_t *ctrl);

/**
 * @brief 回滚事务（应用 Undo）
 *
 * @param ctrl 持久化控制指针
 * @return 0 成功；非 0 失败
 *
 * @note 两种模式都支持回滚。
 */
int persist_rollback(persist_control_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_PERSIST_CONTROL_H */