/**
 * @file backup.c
 * @brief 数据库备份/恢复实现
 *
 * 基于 SQLite WAL checkpoint + 文件复制实现在线备份。
 */
#include "sdk/mmdb_backup.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 备份句柄内部结构 */
struct mmdb_backup_s {
    mmdb_t*             db;             /* 数据库句柄 */
    char*               backup_path;    /* 备份文件路径 */
    mmdb_backup_state_t state;          /* 备份状态 */
    uint32_t            progress;       /* 进度（0-100） */
};

/**
 * @brief 创建备份
 */
int mmdb_backup_create(mmdb_t* db, const char* backup_path, mmdb_backup_t** backup) {
    if (!db || !backup_path || !backup) {
        return MMDB_ERR_INVALID;
    }

    /* 分配备份句柄 */
    mmdb_backup_t* b = (mmdb_backup_t*)calloc(1, sizeof(mmdb_backup_t));
    if (!b) {
        return MMDB_ERR_NOMEM;
    }

    b->db = db;
    b->backup_path = strdup(backup_path);
    if (!b->backup_path) {
        free(b);
        return MMDB_ERR_NOMEM;
    }

    b->state = MMDB_BACKUP_IDLE;
    b->progress = 0;

    *backup = b;
    return MMDB_OK;
}

/**
 * @brief 执行备份（同步）
 */
int mmdb_backup_run(mmdb_backup_t* backup) {
    if (!backup) {
        return MMDB_ERR_INVALID;
    }

    if (backup->state == MMDB_BACKUP_IN_PROGRESS) {
        return MMDB_ERR_INVALID;
    }

    if (backup->state == MMDB_BACKUP_COMPLETED) {
        return MMDB_ERR_INVALID;  /* 已完成，不能重复 */
    }

    backup->state = MMDB_BACKUP_IN_PROGRESS;
    backup->progress = 0;

    /* 1. 执行 WAL checkpoint（确保所有数据写入主数据库文件） */
    char* err_msg = NULL;
    int rc = sqlite3_exec(backup->db->db, "PRAGMA wal_checkpoint(TRUNCATE)",
                          NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            mmdb_set_error(backup->db, MMDB_ERR_INTERNAL, err_msg);
            sqlite3_free(err_msg);
        }
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_INTERNAL;
    }

    backup->progress = 30;

    /* 2. 使用 SQLite 在线备份 API */
    sqlite3* dest_db = NULL;
    rc = sqlite3_open(backup->backup_path, &dest_db);
    if (rc != SQLITE_OK) {
        mmdb_set_error(backup->db, MMDB_ERR_INTERNAL, "无法打开备份目标数据库");
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_INTERNAL;
    }

    sqlite3_backup* backup_handle = sqlite3_backup_init(
        dest_db, "main", backup->db->db, "main");
    if (!backup_handle) {
        sqlite3_close(dest_db);
        mmdb_set_error(backup->db, MMDB_ERR_INTERNAL, "无法初始化备份");
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_INTERNAL;
    }

    /* 3. 执行备份（每次 100 页） */
    do {
        rc = sqlite3_backup_step(backup_handle, 100);
        backup->progress = 30 + (sqlite3_backup_remaining(backup_handle) * 70 /
                                 sqlite3_backup_pagecount(backup_handle));

        /* 可以在这里添加取消检查 */
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY);

    sqlite3_backup_finish(backup_handle);
    sqlite3_close(dest_db);

    if (rc != SQLITE_DONE) {
        mmdb_set_error(backup->db, MMDB_ERR_INTERNAL, "备份执行失败");
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_INTERNAL;
    }

    backup->progress = 100;
    backup->state = MMDB_BACKUP_COMPLETED;
    return MMDB_OK;
}

/**
 * @brief 获取备份状态
 */
mmdb_backup_state_t mmdb_backup_get_state(const mmdb_backup_t* backup) {
    if (!backup) {
        return MMDB_BACKUP_FAILED;
    }
    return backup->state;
}

/**
 * @brief 获取备份进度
 */
uint32_t mmdb_backup_get_progress(const mmdb_backup_t* backup) {
    if (!backup) {
        return 0;
    }
    return backup->progress;
}

/**
 * @brief 释放备份句柄
 */
void mmdb_backup_free(mmdb_backup_t* backup) {
    if (!backup) return;
    free(backup->backup_path);
    free(backup);
}

/**
 * @brief 恢复数据库
 */
int mmdb_backup_restore(mmdb_t* db, const char* backup_path) {
    if (!db || !backup_path) {
        return MMDB_ERR_INVALID;
    }

    /* 1. 关闭当前数据库 */
    sqlite3* old_db = db->db;
    db->db = NULL;

    /* 2. 执行 WAL checkpoint */
    char* err_msg = NULL;
    int rc = sqlite3_exec(old_db, "PRAGMA wal_checkpoint(TRUNCATE)",
                          NULL, NULL, &err_msg);
    if (err_msg) sqlite3_free(err_msg);

    /* 3. 关闭旧数据库 */
    sqlite3_close(old_db);

    /* 4. 删除当前数据库文件 */
    remove(db->path);
    char wal_path[1024];
    char shm_path[1024];
    snprintf(wal_path, sizeof(wal_path), "%s-wal", db->path);
    snprintf(shm_path, sizeof(shm_path), "%s-shm", db->path);
    remove(wal_path);
    remove(shm_path);

    /* 5. 复制备份文件到当前数据库路径 */
    FILE* src = fopen(backup_path, "rb");
    if (!src) {
        mmdb_set_error(db, MMDB_ERR_INTERNAL, "无法打开备份文件");
        return MMDB_ERR_INTERNAL;
    }

    FILE* dst = fopen(db->path, "wb");
    if (!dst) {
        fclose(src);
        mmdb_set_error(db, MMDB_ERR_INTERNAL, "无法创建目标文件");
        return MMDB_ERR_INTERNAL;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dst);
    }
    fclose(src);
    fclose(dst);

    /* 6. 重新打开数据库 */
    rc = sqlite3_open(db->path, &db->db);
    if (rc != SQLITE_OK) {
        mmdb_set_error(db, MMDB_ERR_INTERNAL, "无法重新打开数据库");
        return MMDB_ERR_INTERNAL;
    }

    return MMDB_OK;
}
