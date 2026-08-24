/**
 * @file backup_test.cpp
 * @brief 数据库备份/恢复测试
 *
 * 覆盖：
 *   1. BackupAndRestore - 备份并恢复数据库
 *   2. BackupProgress - 备份进度跟踪
 *   3. BackupInvalidPath - 无效路径处理
 *   4. BackupStateTracking - 状态跟踪
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_backup.h"
#include "sdk/impl/sqlite_backend.h"
}

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kDbPath = "test_backup.db";
constexpr const char* kBackupPath = "test_backup.bak.db";

void cleanup_db() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
    std::remove(kBackupPath);
    std::remove((std::string(kBackupPath) + "-wal").c_str());
    std::remove((std::string(kBackupPath) + "-shm").c_str());
}

}  // namespace

class BackupTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* 创建测试表并插入数据 */
        ASSERT_EQ(sqlite3_exec(db_->db,
            "CREATE TABLE test_backup (id INTEGER PRIMARY KEY, value TEXT)",
            nullptr, nullptr, nullptr), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(db_->db,
            "INSERT INTO test_backup (id, value) VALUES (1, 'hello')",
            nullptr, nullptr, nullptr), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(db_->db,
            "INSERT INTO test_backup (id, value) VALUES (2, 'world')",
            nullptr, nullptr, nullptr), SQLITE_OK);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* 测试 1: 备份并恢复数据库 */
TEST_F(BackupTest, BackupAndRestore) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, &backup), MMDB_OK);
    ASSERT_NE(backup, nullptr);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_COMPLETED);
    ASSERT_EQ(mmdb_backup_get_progress(backup), 100);

    mmdb_backup_free(backup);

    /* 验证备份文件存在 */
    FILE* f = fopen(kBackupPath, "rb");
    ASSERT_NE(f, nullptr);
    fclose(f);

    /* 修改原数据库 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_backup (id, value) VALUES (3, 'new')",
        nullptr, nullptr, nullptr), SQLITE_OK);

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

/* 测试 2: 备份进度跟踪 */
TEST_F(BackupTest, BackupProgress) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, &backup), MMDB_OK);

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

/* 测试 3: 无效路径处理 */
TEST_F(BackupTest, BackupInvalidPath) {
    mmdb_backup_t* backup = nullptr;

    /* NULL 参数 */
    ASSERT_EQ(mmdb_backup_create(nullptr, kBackupPath, &backup), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_backup_create(db_, nullptr, &backup), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, nullptr), MMDB_ERR_INVALID);

    /* 创建有效备份句柄 */
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, &backup), MMDB_OK);

    /* 重复执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_ERR_INVALID);  /* 已完成，不能重复 */

    mmdb_backup_free(backup);
}

/* 测试 4: 状态跟踪 */
TEST_F(BackupTest, BackupStateTracking) {
    mmdb_backup_t* backup = nullptr;
    ASSERT_EQ(mmdb_backup_create(db_, kBackupPath, &backup), MMDB_OK);

    /* 初始状态 */
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_IDLE);

    /* 执行备份 */
    ASSERT_EQ(mmdb_backup_run(backup), MMDB_OK);
    ASSERT_EQ(mmdb_backup_get_state(backup), MMDB_BACKUP_COMPLETED);

    /* 释放句柄 */
    mmdb_backup_free(backup);
}
