/**
 * @file backup.c
 * @brief 数据库备份/恢复实现
 *
 * 支持两种备份模式：
 *   - ONLINE：基于 sqlite3_backup API 逐页复制，不先做 WAL checkpoint，不阻塞读写
 *   - FULL：先 WAL checkpoint(TRUNCATE) 再复制，保证强一致性
 *
 * 每次备份生成一个目录结构：
 *   backup_path/
 *     backup.db          — 数据库文件副本
 *     backup.db-wal      — WAL 文件副本（如有）
 *     backup.db-shm      — SHM 文件副本（如有）
 *     metadata.json      — 备份元数据
 */
#include "sdk/mmdb_backup.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

/* 跨平台 mkdir */
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/* 备份句柄内部结构 */
struct mmdb_backup_s {
    mmdb_t*             db;             /* 数据库句柄 */
    char*               backup_path;    /* 备份目录路径 */
    mmdb_backup_mode_t  mode;           /* 备份模式 */
    mmdb_backup_state_t state;          /* 备份状态 */
    uint32_t            progress;       /* 进度（0-100） */
    char*               description;    /* 用户描述（可选） */
};

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 生成 ISO 8601 格式的时间字符串
 */
static void format_iso8601(char* buf, size_t len) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

/**
 * @brief 获取文件大小
 */
static uint32_t file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint32_t)st.st_size;
}

/**
 * @brief 复制单个文件
 */
static int copy_file(const char* src, const char* dst) {
    FILE* fsrc = fopen(src, "rb");
    if (!fsrc) return MMDB_ERR_IO;

    FILE* fdst = fopen(dst, "wb");
    if (!fdst) {
        fclose(fsrc);
        return MMDB_ERR_IO;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        if (fwrite(buf, 1, n, fdst) != n) {
            fclose(fsrc);
            fclose(fdst);
            return MMDB_ERR_IO;
        }
    }
    fclose(fsrc);
    fclose(fdst);
    return MMDB_OK;
}

/**
 * @brief 删除单个文件（忽略不存在的情况）
 */
static void remove_file(const char* path) {
    remove(path);
}

/**
 * @brief 递归删除目录（简易实现，仅删除已知结构）
 */
static void remove_dir_recursive(const char* path) {
    char buf[1024];
    /* 删除已知的备份文件 */
    snprintf(buf, sizeof(buf), "%s/backup.db", path);
    remove_file(buf);
    snprintf(buf, sizeof(buf), "%s/backup.db-wal", path);
    remove_file(buf);
    snprintf(buf, sizeof(buf), "%s/backup.db-shm", path);
    remove_file(buf);
    snprintf(buf, sizeof(buf), "%s/metadata.json", path);
    remove_file(buf);
    /* 删除目录本身 */
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}

/**
 * @brief 写入 metadata.json
 */
static int write_metadata(const mmdb_backup_t* backup, uint32_t db_size) {
    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s/metadata.json", backup->backup_path);

    FILE* f = fopen(meta_path, "w");
    if (!f) return MMDB_ERR_IO;

    char timestamp[64];
    format_iso8601(timestamp, sizeof(timestamp));

    uint32_t bak_size = file_size(backup->backup_path);
    /* bak_size 是目录级别，这里用 backup.db 的实际大小 */
    char db_file[1024];
    snprintf(db_file, sizeof(db_file), "%s/backup.db", backup->backup_path);
    bak_size = file_size(db_file);

    fprintf(f, "{\n");
    fprintf(f, "  \"created_at\": \"%s\",\n", timestamp);
    fprintf(f, "  \"db_size_bytes\": %u,\n", db_size);
    fprintf(f, "  \"backup_size_bytes\": %u,\n", bak_size);
    fprintf(f, "  \"mode\": \"%s\",\n",
            backup->mode == MMDB_BACKUP_ONLINE ? "online" : "full");
    fprintf(f, "  \"db_path\": \"%s\",\n", backup->db->path);
    if (backup->description) {
        fprintf(f, "  \"description\": \"%s\"\n", backup->description);
    } else {
        fprintf(f, "  \"description\": \"\"\n");
    }
    fprintf(f, "}\n");

    fclose(f);
    return MMDB_OK;
}

/**
 * @brief 读取 metadata.json（用于 list）
 */
static int read_metadata(const char* dir_path, mmdb_backup_metadata_t* meta) {
    char meta_file[1024];
    snprintf(meta_file, sizeof(meta_file), "%s/metadata.json", dir_path);

    FILE* f = fopen(meta_file, "r");
    if (!f) return MMDB_ERR_IO;

    /* 简易 JSON 解析（逐字段扫描） */
    char line[1024];
    memset(meta, 0, sizeof(*meta));

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "\"created_at\"")) {
            char* p = strchr(line, ':');
            if (p) {
                p++;
                while (*p == ' ') p++;
                if (*p == '"') {
                    p++;
                    char* end = strchr(p, '"');
                    if (end) {
                        size_t len = (size_t)(end - p);
                        if (len >= sizeof(meta->created_at)) len = sizeof(meta->created_at) - 1;
                        memcpy(meta->created_at, p, len);
                        meta->created_at[len] = '\0';
                    }
                }
            }
        } else if (strstr(line, "\"db_size_bytes\"")) {
            char* p = strchr(line, ':');
            if (p) { p++; while (*p == ' ') p++; meta->db_size_bytes = (uint32_t)atol(p); }
        } else if (strstr(line, "\"backup_size_bytes\"")) {
            char* p = strchr(line, ':');
            if (p) { p++; while (*p == ' ') p++; meta->backup_size_bytes = (uint32_t)atol(p); }
        } else if (strstr(line, "\"mode\"")) {
            if (strstr(line, "\"online\"")) meta->mode = MMDB_BACKUP_ONLINE;
            else meta->mode = MMDB_BACKUP_FULL;
        } else if (strstr(line, "\"db_path\"")) {
            char* p = strchr(line, ':');
            if (p) {
                p++;
                while (*p == ' ') p++;
                if (*p == '"') {
                    p++;
                    char* end = strchr(p, '"');
                    if (end) {
                        size_t len = (size_t)(end - p);
                        if (len >= sizeof(meta->db_path)) len = sizeof(meta->db_path) - 1;
                        memcpy(meta->db_path, p, len);
                        meta->db_path[len] = '\0';
                    }
                }
            }
        } else if (strstr(line, "\"description\"")) {
            char* p = strchr(line, ':');
            if (p) {
                p++;
                while (*p == ' ') p++;
                if (*p == '"') {
                    p++;
                    char* end = strchr(p, '"');
                    if (end) {
                        size_t len = (size_t)(end - p);
                        if (len >= sizeof(meta->description)) len = sizeof(meta->description) - 1;
                        memcpy(meta->description, p, len);
                        meta->description[len] = '\0';
                    }
                }
            }
        }
    }

    fclose(f);
    return MMDB_OK;
}

/* ========================================================================
 * 公开 API 实现
 * ======================================================================== */

/**
 * @brief 创建备份
 */
int mmdb_backup_create(mmdb_t* db, const char* backup_path,
                       mmdb_backup_mode_t mode, mmdb_backup_t** backup) {
    if (!db || !backup_path || !backup) {
        return MMDB_ERR_INVALID;
    }

    /* 分配备份句柄 */
    mmdb_backup_t* b = (mmdb_backup_t*)calloc(1, sizeof(mmdb_backup_t));
    if (!b) return MMDB_ERR_NOMEM;

    b->db = db;
    b->backup_path = strdup(backup_path);
    if (!b->backup_path) { free(b); return MMDB_ERR_NOMEM; }

    b->mode = mode;
    b->state = MMDB_BACKUP_IDLE;
    b->progress = 0;
    b->description = NULL;

    *backup = b;
    return MMDB_OK;
}

/**
 * @brief 设置备份描述
 */
int mmdb_backup_set_description(mmdb_backup_t* backup, const char* description) {
    if (!backup) return MMDB_ERR_INVALID;
    free(backup->description);
    backup->description = description ? strdup(description) : NULL;
    return MMDB_OK;
}

/**
 * @brief 执行备份（同步）
 */
int mmdb_backup_run(mmdb_backup_t* backup) {
    if (!backup) return MMDB_ERR_INVALID;
    if (backup->state == MMDB_BACKUP_IN_PROGRESS) return MMDB_ERR_BUSY;
    if (backup->state == MMDB_BACKUP_COMPLETED) return MMDB_ERR_ALREADY;

    backup->state = MMDB_BACKUP_IN_PROGRESS;
    backup->progress = 0;

    /* 创建备份目录 */
    if (MKDIR(backup->backup_path) != 0) {
        /* 目录可能已存在，忽略 */
    }

    /* 构建备份数据库文件路径 */
    char dst_db_path[1024];
    snprintf(dst_db_path, sizeof(dst_db_path), "%s/backup.db", backup->backup_path);

    /* ---- FULL 模式：先做 WAL checkpoint ---- */
    if (backup->mode == MMDB_BACKUP_FULL) {
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
    }

    backup->progress = 10;

    /* ---- 使用 sqlite3_backup API 逐页复制 ---- */
    sqlite3* dest_db = NULL;
    int rc = sqlite3_open(dst_db_path, &dest_db);
    if (rc != SQLITE_OK) {
        mmdb_set_error(backup->db, MMDB_ERR_IO, "无法创建备份数据库文件");
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_IO;
    }

    sqlite3_backup* bk = sqlite3_backup_init(dest_db, "main", backup->db->db, "main");
    if (!bk) {
        sqlite3_close(dest_db);
        mmdb_set_error(backup->db, MMDB_ERR_INTERNAL, "sqlite3_backup_init 失败");
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_INTERNAL;
    }

    /* 每次复制 100 页，进度从 10% 到 90% */
    do {
        rc = sqlite3_backup_step(bk, 100);
        int remaining = sqlite3_backup_remaining(bk);
        int pagecount = sqlite3_backup_pagecount(bk);
        if (pagecount > 0) {
            backup->progress = 10 + (uint32_t)((pagecount - remaining) * 80 / pagecount);
        }
        /* ONLINE 模式下让出 CPU 给其他连接（5ms） */
        if (backup->mode == MMDB_BACKUP_ONLINE && (rc == SQLITE_OK || rc == SQLITE_BUSY)) {
            sqlite3_sleep(5);
        }
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY);

    sqlite3_backup_finish(bk);
    sqlite3_close(dest_db);

    if (rc != SQLITE_DONE) {
        mmdb_set_error(backup->db, MMDB_ERR_INTERNAL, "sqlite3_backup_step 失败");
        backup->state = MMDB_BACKUP_FAILED;
        return MMDB_ERR_INTERNAL;
    }

    backup->progress = 90;

    /* 复制 WAL 和 SHM 文件（如果存在） */
    char src_wal[1024], dst_wal[1024];
    char src_shm[1024], dst_shm[1024];
    snprintf(src_wal, sizeof(src_wal), "%s-wal", backup->db->path);
    snprintf(dst_wal, sizeof(dst_wal), "%s/backup.db-wal", backup->backup_path);
    snprintf(src_shm, sizeof(src_shm), "%s-shm", backup->db->path);
    snprintf(dst_shm, sizeof(dst_shm), "%s/backup.db-shm", backup->backup_path);

    copy_file(src_wal, dst_wal);   /* 可能不存在，忽略错误 */
    copy_file(src_shm, dst_shm);   /* 可能不存在，忽略错误 */

    backup->progress = 95;

    /* 写入 metadata.json */
    uint32_t db_size = file_size(backup->db->path);
    write_metadata(backup, db_size);

    backup->progress = 100;
    backup->state = MMDB_BACKUP_COMPLETED;
    return MMDB_OK;
}

/**
 * @brief 获取备份状态
 */
mmdb_backup_state_t mmdb_backup_get_state(const mmdb_backup_t* backup) {
    if (!backup) return MMDB_BACKUP_FAILED;
    return backup->state;
}

/**
 * @brief 获取备份进度
 */
uint32_t mmdb_backup_get_progress(const mmdb_backup_t* backup) {
    if (!backup) return 0;
    return backup->progress;
}

/**
 * @brief 释放备份句柄
 */
void mmdb_backup_free(mmdb_backup_t* backup) {
    if (!backup) return;
    free(backup->backup_path);
    free(backup->description);
    free(backup);
}

/**
 * @brief 恢复数据库
 */
int mmdb_backup_restore(mmdb_t* db, const char* backup_path) {
    if (!db || !backup_path) return MMDB_ERR_INVALID;

    /* 构建备份数据库文件路径 */
    char src_db_path[1024];
    snprintf(src_db_path, sizeof(src_db_path), "%s/backup.db", backup_path);

    /* 验证备份文件存在 */
    FILE* f = fopen(src_db_path, "rb");
    if (!f) {
        mmdb_set_error(db, MMDB_ERR_NOT_FOUND, "备份文件不存在");
        return MMDB_ERR_NOT_FOUND;
    }
    fclose(f);

    /* 1. 关闭当前数据库 */
    sqlite3* old_sqlite = db->db;
    db->db = NULL;

    /* 2. WAL checkpoint（尽力而为） */
    char* err_msg = NULL;
    sqlite3_exec(old_sqlite, "PRAGMA wal_checkpoint(TRUNCATE)", NULL, NULL, &err_msg);
    if (err_msg) sqlite3_free(err_msg);

    /* 3. 关闭旧数据库 */
    sqlite3_close(old_sqlite);

    /* 4. 删除当前数据库文件 */
    remove_file(db->path);
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s-wal", db->path);
    remove_file(buf);
    snprintf(buf, sizeof(buf), "%s-shm", db->path);
    remove_file(buf);

    /* 5. 复制备份文件到当前路径 */
    int rc = copy_file(src_db_path, db->path);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, MMDB_ERR_IO, "复制备份文件失败");
        return rc;
    }

    /* 复制 WAL 和 SHM（如果存在） */
    char src_wal[1024], dst_wal[1024];
    char src_shm[1024], dst_shm[1024];
    snprintf(src_wal, sizeof(src_wal), "%s/backup.db-wal", backup_path);
    snprintf(dst_wal, sizeof(dst_wal), "%s-wal", db->path);
    snprintf(src_shm, sizeof(src_shm), "%s/backup.db-shm", backup_path);
    snprintf(dst_shm, sizeof(dst_shm), "%s-shm", db->path);
    copy_file(src_wal, dst_wal);
    copy_file(src_shm, dst_shm);

    /* 6. 重新打开数据库 */
    rc = sqlite3_open(db->path, &db->db);
    if (rc != SQLITE_OK) {
        mmdb_set_error(db, MMDB_ERR_INTERNAL, "重新打开数据库失败");
        return MMDB_ERR_INTERNAL;
    }

    return MMDB_OK;
}

/**
 * @brief 列出指定目录下的所有可用备份
 */
int mmdb_backup_list(const char* dir_path, mmdb_backup_info_t* infos, size_t* count) {
    if (!dir_path || !count) return MMDB_ERR_INVALID;

    size_t max_count = *count;
    *count = 0;

    /* 扫描目录下的子目录，查找含 metadata.json 的备份 */
    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "dir /b /ad \"%s\" 2>nul", dir_path);
#else
    snprintf(cmd, sizeof(cmd), "ls -1 \"%s\" 2>/dev/null", dir_path);
#endif

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return MMDB_ERR_INTERNAL;

    char line[512];
    while (fgets(line, sizeof(line), pipe) && *count < max_count) {
        /* 去除换行符 */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        /* 构建子目录路径 */
        char sub_dir[1024];
        snprintf(sub_dir, sizeof(sub_dir), "%s/%s", dir_path, line);

        /* 检查是否含 metadata.json */
        char meta_file[1024];
        snprintf(meta_file, sizeof(meta_file), "%s/metadata.json", sub_dir);
        struct stat st;
        if (stat(meta_file, &st) != 0) continue;

        /* 读取元数据 */
        mmdb_backup_info_t* info = &infos[*count];
        memset(info, 0, sizeof(*info));
        strncpy(info->path, sub_dir, sizeof(info->path) - 1);
        read_metadata(sub_dir, &info->metadata);

        (*count)++;
    }

    pclose(pipe);
    return MMDB_OK;
}

/**
 * @brief 删除备份
 */
int mmdb_backup_delete(const char* backup_path) {
    if (!backup_path) return MMDB_ERR_INVALID;

    /* 验证目录存在 */
    struct stat st;
    if (stat(backup_path, &st) != 0) return MMDB_ERR_NOT_FOUND;

    remove_dir_recursive(backup_path);
    return MMDB_OK;
}
