/**
 * @file mmdb_backup.h
 * @brief 数据库备份/恢复 API
 *
 * 基于 SQLite WAL checkpoint + 文件复制实现在线备份。
 */
#ifndef SDK_MMDB_BACKUP_H
#define SDK_MMDB_BACKUP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct mmdb_s mmdb_t;

/* 备份句柄（不透明类型） */
typedef struct mmdb_backup_s mmdb_backup_t;

/* 备份状态 */
typedef enum {
    MMDB_BACKUP_IDLE,            /* 空闲状态 */
    MMDB_BACKUP_IN_PROGRESS,     /* 备份进行中 */
    MMDB_BACKUP_COMPLETED,       /* 备份完成 */
    MMDB_BACKUP_FAILED,          /* 备份失败 */
} mmdb_backup_state_t;

/**
 * @brief 创建备份
 * @param db            数据库句柄
 * @param backup_path   备份文件路径
 * @param backup        输出备份句柄
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_create(mmdb_t* db, const char* backup_path, mmdb_backup_t** backup);

/**
 * @brief 执行备份（同步）
 * @param backup        备份句柄
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_run(mmdb_backup_t* backup);

/**
 * @brief 获取备份状态
 * @param backup        备份句柄
 * @return 备份状态
 */
mmdb_backup_state_t mmdb_backup_get_state(const mmdb_backup_t* backup);

/**
 * @brief 获取备份进度（0-100）
 * @param backup        备份句柄
 * @return 进度百分比
 */
uint32_t mmdb_backup_get_progress(const mmdb_backup_t* backup);

/**
 * @brief 释放备份句柄
 * @param backup        备份句柄
 */
void mmdb_backup_free(mmdb_backup_t* backup);

/**
 * @brief 恢复数据库
 * @param db            数据库句柄
 * @param backup_path   备份文件路径
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_restore(mmdb_t* db, const char* backup_path);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_BACKUP_H */
