/**
 * @file backup_test.cpp
 * @brief 数据库备份/恢复测试（P6-M2.1 增强版）
 *
 * 覆盖：
 *   1. BackupAndRestore         - 在线备份并恢复数据库
 *   2. BackupProgress           - 备份进度跟踪
 *   3. BackupInvalidPath        - 无效路径处理
 *   4. BackupStateTracking      - 状态跟踪
 *   5. BackupFullMode           - 全量备份模式
 *   6. BackupList               - 备份列表
 *   7. BackupMetadata           - 备份元数据验证
 *   8. BackupMultiple           - 多次备份管理
 *   9. BackupDelete             - 删除备份
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_backup.h"
#include "sqlite3.h"
}

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {

constexpr const char* kDbPath = "test_backup.db";
constexpr const char* kBackupDir = "test_backup_dir";
constexpr const char* kBackupPath = "test_backup_dir/bak1";

void cleanup_db() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
}

void cleanup_backup() {
    /* 删除备份目录及内容 */
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>nul", kBackupDir);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" 2>/dev/null", kBackupDir);
#endif
    system(cmd);
}

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* 辅助函数：从目录读取元数据 */
static int read_metadata_from_dir(const char* dir_path, mmdb_backup_metadata_t* meta) {
    char meta_file[1024];
    snprintf(meta_file, sizeof(meta_file), "%s/metadata.json", dir_path);

    FILE* f = fopen(meta_file, "r");
    if (!f) return MMDB_ERR_IO;

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

}  // namespace

class BackupTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;

    void SetUp() override {
        cleanup_db();
        cleanup_backup();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* 创建测试表并插入数据 */
        char* err_msg = nullptr;
        ASSERT_EQ(sqlite3_exec(db_->db,
            "CREATE TABLE test_backup (id INTEGER PRIMARY KEY, value TEXT)",
            nullptr, nullptr, &err_msg), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(db_->db,
            "INSERT INTO test_backup (id, value) VALUES (1, 'hello')",
            nullptr, nullptr, &err_msg), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(db_->db,
            "INSERT INTO test_backup (id, value) VALUES (2, 'world')",
            nullptr, nullptr, &err_msg), SQLITE_OK);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        cleanup_db();
        cleanup_backup();
    }
};

/* ====================================================================
 * 测试 1: 在线备份并恢复数据库
 * ==================================================================== */
TEST_F(BackupTest, BackupAndRestore) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_OK);
    ASSERT_NE(backup, nullptr);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_COMPLETED);
    ASSERT_EQ(mmdb_backup_get_progress(backup), 100);

    mmdb_backup_free(backup);

    /* 验证备份文件存在 */
    std::string db_file = std::string(kBackupPath) + "/backup.db";
    ASSERT_TRUE(file_exists(db_file.c_str()));

    /* 修改原数据库 */
    char* err_msg = nullptr;
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_backup (id, value) VALUES (3, 'new')",
        nullptr, nullptr, &err_msg), SQLITE_OK);

    /* 从备份恢复 */
    ASSERT_EQ(mmdb_backup_restore(db_, kBackupPath), MMDB_OK);

    /* 验证恢复后的数据 */
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT COUNT(*) FROM test_backup", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 2);  /* 恢复后只有 2 条记录 */
    sqlite3_finalize(stmt);
}

/* ====================================================================
 * 测试 2: 备份进度跟踪
 * ==================================================================== */
TEST_F(BackupTest, BackupProgress) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_OK);

    /* 初始状态 */
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_IDLE);
    ASSERT_EQ(mmdb_backup_get_progress(backup), 0);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);

    /* 最终状态 */
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_COMPLETED);
    ASSERT_EQ(mmdb_backup_get_progress(backup), 100);

    mmdb_backup_free(backup);
}

/* ====================================================================
 * 测试 3: 无效路径处理
 * ==================================================================== */
TEST_F(BackupTest, BackupInvalidPath) {
    mmdb_backup_t* backup = nullptr;

    /* NULL 参数 */
    ASSERT_EQ(mmdb_backup_create(nullptr, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_backup_create(db_, nullptr, MMDB_BACKUP_ONLINE, &backup), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, nullptr), MMDB_ERR_INVALID);

    /* 创建有效备份句柄 */
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_OK);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);

    /* 重复执行备份（已完成） */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_ERR_ALREADY);

    mmdb_backup_free(backup);
}

/* ====================================================================
 * 测试 4: 状态跟踪
 * ==================================================================== */
TEST_F(BackupTest, BackupStateTracking) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_OK);

    /* 初始状态 */
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_IDLE);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_COMPLETED);

    /* 释放句柄 */
    mmdb_backup_free(backup);
}

/* ====================================================================
 * 测试 5: 全量备份模式
 * ==================================================================== */
TEST_F(BackupTest, BackupFullMode) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_FULL, &backup), MMDB_OK);

    /* 执行全量备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_COMPLETED);

    mmdb_backup_free(backup);

    /* 验证元数据中记录了 full 模式 */
    std::string meta_file = std::string(kBackupPath) + "/metadata.json";
    ASSERT_TRUE(file_exists(meta_file.c_str()));

    /* 读取元数据验证 */
    FILE* f = fopen(meta_file.c_str(), "r");
    ASSERT_NE(f, nullptr);
    char content[4096];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);

    ASSERT_NE(strstr(content, "\"full\""), nullptr);  /* 包含 "full" 模式标记 */
}

/* ====================================================================
 * 测试 6: 备份列表
 * ==================================================================== */
TEST_F(BackupTest, BackupList) {
    /* 创建两个备份 */
    mmdb_backup_t* bak1 = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &bak1), MMDB_OK);
    ASSERT_EQ(mmdb_backup_run(bak1), MMDB_OK);
    mmdb_backup_free(bak1);

    std::string bak2_path = std::string(kBackupDir) + "/bak2";
    mmdb_backup_t* bak2 = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, bak2_path.c_str(), MMDB_BACKUP_ONLINE, &bak2), MMDB_OK);
    ASSERT_EQ(mmdb_backup_run(bak2), MMDB_OK);
    mmdb_backup_free(bak2);

    /* 列出备份 */
    mmdb_backup_info_t infos[10];
    size_t count = 10;
    ASSERT_EQ(mmdb_backup_list(kBackupDir, infos, &count), MMDB_OK);
    ASSERT_GE(count, 2u);  /* 至少 2 个备份 */

    /* 验证元数据非空 */
    ASSERT_GT(strlen(infos[0].metadata.created_at), 0u);
    ASSERT_GT(infos[0].metadata.db_size_bytes, 0u);
}

/* ====================================================================
 * 测试 7: 备份元数据验证
 * ==================================================================== */
TEST_F(BackupTest, BackupMetadata) {
    /* 设置描述 */
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_set_description(backup, "测试备份"), MMDB_OK);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    mmdb_backup_free(backup);

    /* 读取元数据 */
    mmdb_backup_metadata_t meta;
    ASSERT_EQ(read_metadata_from_dir(kBackupPath, &meta), MMDB_OK);

    /* 验证元数据字段 */
    ASSERT_GT(strlen(meta.created_at), 0u);
    ASSERT_GT(meta.db_size_bytes, 0u);
    ASSERT_GT(meta.backup_size_bytes, 0u);
    ASSERT_EQ(meta.mode, MMDB_BACKUP_ONLINE);
    ASSERT_STREQ(meta.description, "测试备份");
}

/* ====================================================================
 * 测试 8: 多次备份管理
 * ==================================================================== */
TEST_F(BackupTest, BackupMultiple) {
    /* 第一次备份 */
    mmdb_backup_t* bak1 = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &bak1), MMDB_OK);
    ASSERT_EQ(mmdb_backup_run(bak1), MMDB_OK);
    mmdb_backup_free(bak1);

    /* 插入新数据 */
    char* err_msg = nullptr;
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_backup (id, value) VALUES (10, 'new_data')",
        nullptr, nullptr, &err_msg), SQLITE_OK);

    /* 第二次备份（应该包含新数据） */
    std::string bak2_path = std::string(kBackupDir) + "/bak2";
    mmdb_backup_t* bak2 = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, bak2_path.c_str(), MMDB_BACKUP_ONLINE, &bak2), MMDB_OK);
    ASSERT_EQ(mmdb_backup_run(bak2), MMDB_OK);
    mmdb_backup_free(bak2);

    /* 从第一次备份恢复 */
    ASSERT_EQ(mmdb_backup_restore(db_, kBackupPath), MMDB_OK);
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT COUNT(*) FROM test_backup", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 2);  /* 恢复后只有原始 2 条 */
    sqlite3_finalize(stmt);

    /* 从第二次备份恢复 */
    ASSERT_EQ(mmdb_backup_restore(db_, bak2_path.c_str()), MMDB_OK);
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT COUNT(*) FROM test_backup", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 3);  /* 包含新数据共 3 条 */
    sqlite3_finalize(stmt);
}

/* ====================================================================
 * 测试 9: 删除备份
 * ==================================================================== */
TEST_F(BackupTest, BackupDelete) {
    /* 创建备份 */
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, MMDB_BACKUP_ONLINE, &backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    mmdb_backup_free(backup);

    /* 验证备份存在 */
    std::string db_file = std::string(kBackupPath) + "/backup.db";
    ASSERT_TRUE(file_exists(db_file.c_str()));

    /* 删除备份 */
    ASSERT_EQ(mmdb_backup_delete(kBackupPath), MMDB_OK);

    /* 验证备份已删除 */
    ASSERT_FALSE(file_exists(db_file.c_str()));

    /* 删除不存在的备份 */
    ASSERT_EQ(mmdb_backup_delete(kBackupPath), MMDB_ERR_NOT_FOUND);
}
