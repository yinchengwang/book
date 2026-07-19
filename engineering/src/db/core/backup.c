/**
 * @file backup.c
 * @brief 备份恢复实现
 *
 * 实现数据库冷备/恢复功能：
 * - 文件级拷贝（fread/fwrite）
 * - FNV-1a 32 位校验和（轻量，无外部依赖）
 * - meta.json 元数据管理
 */
#include "db/backup.h"
#include "db/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* 文件拷贝 + 校验和计算 */
static int copy_file(const char *src, const char *dst, uint32_t *out_checksum) {
    FILE *fs = fopen(src, "rb");
    if (!fs) {
        LOG_ERROR("无法打开源文件: %s", src);
        return -1;
    }
    FILE *fd = fopen(dst, "wb");
    if (!fd) {
        LOG_ERROR("无法创建目标文件: %s", dst);
        fclose(fs);
        return -1;
    }

    uint8_t buf[8192];
    size_t n;
    uint32_t checksum = 2166136261u;

    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
        fwrite(buf, 1, n, fd);
        for (size_t i = 0; i < n; i++) {
            checksum ^= buf[i];
            checksum *= 16777619u;
        }
    }

    fclose(fs);
    fclose(fd);

    if (out_checksum) *out_checksum = checksum;
    return 0;
}

/* 生成 ISO8601 时间戳 */
static void timestamp_iso8601(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, size, "%Y-%m-%dT%H:%M:%S", tm);
}

/* 获取文件大小 */
static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

int db_backup(const char *db_path, const char *backup_dir) {
    if (!db_path || !backup_dir) {
        LOG_ERROR("db_backup: 参数无效");
        return -1;
    }

    /* 1. 创建备份目录 */
    if (mkdir(backup_dir, 0755) != 0 && errno != EEXIST) {
        LOG_ERROR("无法创建备份目录: %s", backup_dir);
        return -1;
    }

    /* 2. 提取数据库名称 */
    char db_name[256] = {0};
    const char *slash = strrchr(db_path, '/');
    const char *bslash = strrchr(db_path, '\\');
    const char *base = (slash || bslash) ? (slash > bslash ? slash + 1 : bslash + 1) : db_path;
    strncpy(db_name, base, sizeof(db_name) - 1);
    /* 去掉 .db 后缀 */
    char *dot = strrchr(db_name, '.');
    if (dot) *dot = '\0';

    /* 构造 WAL 路径：.db → .wal */
    char wal_path[512];
    size_t db_len = strlen(db_path);
    if (db_len > 3 && strcmp(db_path + db_len - 3, ".db") == 0) {
        snprintf(wal_path, sizeof(wal_path), "%.*s.wal", (int)(db_len - 3), db_path);
    } else {
        snprintf(wal_path, sizeof(wal_path), "%s.wal", db_path);
    }

    /* 3. 拷贝数据文件 */
    char dst_db[512], dst_wal[512], meta_path[512];
    snprintf(dst_db, sizeof(dst_db), "%s/%s.db", backup_dir, db_name);
    snprintf(dst_wal, sizeof(dst_wal), "%s/%s.wal", backup_dir, db_name);
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", backup_dir);

    uint32_t db_checksum = 0, wal_checksum = 0;

    if (copy_file(db_path, dst_db, &db_checksum) != 0) {
        LOG_ERROR("拷贝数据库文件失败");
        return -1;
    }

    /* WAL 文件可能不存在（新建库没有 WAL） */
    long wal_sz = 0;
    FILE *fwal = fopen(wal_path, "rb");
    if (fwal) {
        fclose(fwal);
        if (copy_file(wal_path, dst_wal, &wal_checksum) != 0) {
            LOG_ERROR("拷贝 WAL 文件失败");
            return -1;
        }
        wal_sz = file_size(wal_path);
    }

    /* 4. 写入 meta.json */
    char ts[64];
    timestamp_iso8601(ts, sizeof(ts));
    long db_sz = file_size(db_path);

    FILE *fm = fopen(meta_path, "w");
    if (!fm) {
        LOG_ERROR("无法创建 meta.json");
        return -1;
    }
    fprintf(fm, "{\n");
    fprintf(fm, "    \"version\": %d,\n", BACKUP_VERSION);
    fprintf(fm, "    \"db_name\": \"%s\",\n", db_name);
    fprintf(fm, "    \"created_at\": \"%s\",\n", ts);
    fprintf(fm, "    \"db_size\": %ld,\n", db_sz);
    fprintf(fm, "    \"wal_size\": %ld,\n", wal_sz < 0 ? 0 : wal_sz);
    fprintf(fm, "    \"db_checksum\": %u,\n", db_checksum);
    fprintf(fm, "    \"wal_checksum\": %u,\n", wal_checksum);
    fprintf(fm, "    \"db_version\": 1,\n");
    fprintf(fm, "    \"engine_version\": \"0.1.0\"\n");
    fprintf(fm, "}\n");
    fclose(fm);

    LOG_INFO("备份完成: %s/", backup_dir);
    LOG_INFO("  数据文件大小: %ld 字节", db_sz);
    LOG_INFO("  WAL 文件大小: %ld 字节", wal_sz < 0 ? 0 : wal_sz);
    LOG_INFO("  校验和: %u", db_checksum);

    return 0;
}

int db_restore(const char *backup_dir, const char *db_dir) {
    if (!backup_dir || !db_dir) {
        LOG_ERROR("db_restore: 参数无效");
        return -1;
    }

    /* 1. 读取 meta.json */
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", backup_dir);
    FILE *fm = fopen(meta_path, "r");
    if (!fm) {
        LOG_ERROR("备份元数据文件未找到: %s", meta_path);
        return -1;
    }

    /* 解析 meta.json 获取 db_name */
    char line[256];
    char db_name[256] = {0};
    while (fgets(line, sizeof(line), fm)) {
        if (strstr(line, "\"db_name\"")) {
            char *p = strchr(line, ':');
            if (p) {
                p++;
                while (*p == ' ' || *p == '\t') p++;
                char *q = strchr(p, '\"');
                if (q) {
                    q++;
                    char *r = strchr(q, '\"');
                    if (r) { *r = '\0'; strncpy(db_name, q, sizeof(db_name) - 1); }
                }
            }
        }
    }
    fclose(fm);

    if (strlen(db_name) == 0) {
        LOG_ERROR("无效的 meta.json: 未找到 db_name");
        return -1;
    }

    /* 2. 创建目标目录 */
    if (mkdir(db_dir, 0755) != 0 && errno != EEXIST) {
        LOG_ERROR("无法创建目标目录: %s", db_dir);
        return -1;
    }

    /* 3. 拷贝文件到目标目录 */
    char src_db[512], src_wal[512];
    char dst_db[512], dst_wal[512];
    snprintf(src_db, sizeof(src_db), "%s/%s.db", backup_dir, db_name);
    snprintf(src_wal, sizeof(src_wal), "%s/%s.wal", backup_dir, db_name);
    snprintf(dst_db, sizeof(dst_db), "%s/%s.db", db_dir, db_name);
    snprintf(dst_wal, sizeof(dst_wal), "%s/%s.wal", db_dir, db_name);

    uint32_t restored_db_cs = 0;
    if (copy_file(src_db, dst_db, &restored_db_cs) != 0) {
        LOG_ERROR("恢复数据库文件失败");
        return -1;
    }

    /* WAL 可选 */
    FILE *fwal = fopen(src_wal, "rb");
    if (fwal) {
        fclose(fwal);
        uint32_t restored_wal_cs = 0;
        if (copy_file(src_wal, dst_wal, &restored_wal_cs) != 0) {
            LOG_ERROR("恢复 WAL 文件失败");
            return -1;
        }
    }

    /* 4. 校验恢复的文件是否存在 */
    if (file_size(dst_db) <= 0) {
        LOG_ERROR("恢复的数据库文件为空");
        return -1;
    }

    LOG_INFO("恢复完成: %s/%s.db", db_dir, db_name);
    LOG_INFO("  文件大小: %ld 字节", file_size(dst_db));

    return 0;
}

int db_backup_meta(const char *backup_dir, backup_meta_t *meta) {
    if (!backup_dir || !meta) return -1;
    memset(meta, 0, sizeof(*meta));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", backup_dir);
    FILE *fm = fopen(meta_path, "r");
    if (!fm) return -1;

    char line[256];
    while (fgets(line, sizeof(line), fm)) {
        char key[64], sval[256];
        int ival;
        long lval;
        if (sscanf(line, "    \"%63[^\"]\": \"%255[^\"]\",", key, sval) == 2) {
            if (strcmp(key, "db_name") == 0) strncpy(meta->db_name, sval, sizeof(meta->db_name) - 1);
            if (strcmp(key, "created_at") == 0) strncpy(meta->created_at, sval, sizeof(meta->created_at) - 1);
            if (strcmp(key, "engine_version") == 0) strncpy(meta->engine_version, sval, sizeof(meta->engine_version) - 1);
        } else if (sscanf(line, "    \"%63[^\"]\": %d,", key, &ival) == 2) {
            if (strcmp(key, "version") == 0) meta->version = (uint32_t)ival;
            if (strcmp(key, "db_version") == 0) meta->db_version = (uint32_t)ival;
            if (strcmp(key, "db_checksum") == 0) meta->db_checksum = (uint32_t)ival;
            if (strcmp(key, "wal_checksum") == 0) meta->wal_checksum = (uint32_t)ival;
        } else if (sscanf(line, "    \"%63[^\"]\": %ld,", key, &lval) == 2) {
            if (strcmp(key, "db_size") == 0) meta->db_size = (uint64_t)lval;
            if (strcmp(key, "wal_size") == 0) meta->wal_size = (uint64_t)lval;
        }
    }
    fclose(fm);
    return 0;
}