/**
 * @file backup.h
 * @brief 备份恢复接口
 *
 * 提供数据库冷备/恢复功能，支持文件级拷贝 + FNV-1a 校验和 + meta.json 元数据。
 */
#ifndef DB_BACKUP_H
#define DB_BACKUP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 备份格式版本 */
#define BACKUP_VERSION 1

/** 备份元数据 */
typedef struct backup_meta_s {
    uint32_t version;            /**< 备份格式版本 */
    char     db_name[256];       /**< 数据库名称 */
    char     created_at[64];     /**< 备份时间 ISO8601 */
    uint64_t db_size;            /**< 数据文件大小 */
    uint64_t wal_size;           /**< WAL 文件大小 */
    uint32_t db_checksum;        /**< 数据文件 FNV-1a 校验和 */
    uint32_t wal_checksum;       /**< WAL 文件 FNV-1a 校验和 */
    uint32_t db_version;         /**< 数据库格式版本 */
    char     engine_version[32]; /**< 引擎版本 */
} backup_meta_t;

/**
 * @brief 备份数据库
 * @param db_path    数据库路径（*.db 文件）
 * @param backup_dir 备份目标目录
 * @return 0 成功，-1 失败
 */
int db_backup(const char *db_path, const char *backup_dir);

/**
 * @brief 恢复数据库
 * @param backup_dir 备份目录
 * @param db_dir     目标数据目录
 * @return 0 成功，-1 失败
 */
int db_restore(const char *backup_dir, const char *db_dir);

/**
 * @brief 获取备份元数据
 * @param backup_dir 备份目录
 * @param meta       输出元数据
 * @return 0 成功，-1 失败
 */
int db_backup_meta(const char *backup_dir, backup_meta_t *meta);

#ifdef __cplusplus
}
#endif

#endif /* DB_BACKUP_H */