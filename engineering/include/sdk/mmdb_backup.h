/**
 * @file mmdb_backup.h
 * @brief 数据库备份/恢复 API
 *
 * 支持两种备份模式：
 *   - 在线快照（ONLINE）：基于 sqlite3_backup API 逐页复制，不阻塞读写
 *   - 全量备份（FULL）：先 WAL checkpoint 再复制，保证一致性但短暂阻塞写
 *
 * 每次备份生成一个目录，内含数据库文件副本和 metadata.json 元数据。
 */
#ifndef SDK_MMDB_BACKUP_H
#define SDK_MMDB_BACKUP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

/* 备份模式 */
typedef enum {
    MMDB_BACKUP_ONLINE,          /* 在线快照，不阻塞读写 */
    MMDB_BACKUP_FULL,            /* 全量备份，先 checkpoint 再复制 */
} mmdb_backup_mode_t;

/* 备份元数据（对应 metadata.json） */
typedef struct {
    char        created_at[64];  /* 备份创建时间（ISO 8601） */
    uint32_t    db_size_bytes;   /* 原始数据库文件大小（字节） */
    uint32_t    backup_size_bytes; /* 备份文件大小（字节） */
    mmdb_backup_mode_t mode;     /* 备份模式 */
    char        db_path[512];    /* 原始数据库路径 */
    char        description[256]; /* 用户描述（可选） */
} mmdb_backup_metadata_t;

/* 备份信息（用于 mmdb_backup_list） */
typedef struct {
    char        path[512];       /* 备份目录路径 */
    mmdb_backup_metadata_t metadata; /* 元数据 */
} mmdb_backup_info_t;

/**
 * @brief 创建备份
 * @param db            数据库句柄
 * @param backup_path   备份目标路径（目录，会自动创建）
 * @param mode          备份模式
 * @param backup        输出备份句柄
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_create(mmdb_t* db, const char* backup_path,
                       mmdb_backup_mode_t mode, mmdb_backup_t** backup);

/**
 * @brief 设置备份描述（可选，在 mmdb_backup_run 之前调用）
 * @param backup        备份句柄
 * @param description   描述字符串
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_set_description(mmdb_backup_t* backup, const char* description);

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
 * @param db            数据库句柄（将被关闭并重新打开）
 * @param backup_path   备份目录路径（含 metadata.json 和数据库文件）
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_restore(mmdb_t* db, const char* backup_path);

/**
 * @brief 列出指定目录下的所有可用备份
 * @param dir_path      备份目录路径
 * @param infos         输出数组（调用方分配）
 * @param count         输入：数组容量；输出：实际备份数
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_list(const char* dir_path, mmdb_backup_info_t* infos, size_t* count);

/**
 * @brief 删除备份
 * @param backup_path   备份目录路径
 * @return 0 成功，非 0 错误码
 */
int mmdb_backup_delete(const char* backup_path);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_BACKUP_H */
