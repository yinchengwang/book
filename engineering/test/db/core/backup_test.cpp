/**
 * @file backup_test.cpp
 * @brief 备份恢复测试
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

extern "C" {
#include "db/backup.h"
#include "db/kv.h"
}

class BackupTest : public ::testing::Test {
protected:
    std::string test_dir;
    std::string db_path;
    std::string backup_dir;

    void SetUp() override {
        test_dir = "test-results/backup_test";
        backup_dir = test_dir + "/backup";
        db_path = test_dir + "/test_kv.db";

        system(("rmdir /s /q " + test_dir + " 2>nul").c_str());
        MKDIR(test_dir.c_str());
    }

    void TearDown() override {
        system(("rmdir /s /q " + test_dir + " 2>nul").c_str());
    }
};

TEST_F(BackupTest, BackupRestoreBasic) {
    /* 1. 写入数据 */
    kv_t *db = kv_open(db_path.c_str());
    ASSERT_NE(db, nullptr);

    kv_put(db, "key1", 4, "value1", 6);
    kv_put(db, "key2", 4, "value2", 6);
    kv_put(db, "key3", 4, "value3", 6);
    kv_close(db);

    /* 2. 备份 */
    ASSERT_EQ(db_backup(db_path.c_str(), backup_dir.c_str()), 0);

    /* 3. 删除原库 */
    remove(db_path.c_str());
    std::string wal_path = db_path + ".wal";
    remove(wal_path.c_str());

    /* 4. 恢复 */
    ASSERT_EQ(db_restore(backup_dir.c_str(), test_dir.c_str()), 0);

    /* 5. 验证数据 */
    db = kv_open(db_path.c_str());
    ASSERT_NE(db, nullptr);

    void *val = NULL;
    size_t val_len = 0;
    ASSERT_EQ(kv_get(db, "key1", 4, &val, &val_len), KV_OK);
    ASSERT_EQ(val_len, 6);
    ASSERT_STREQ((const char*)val, "value1");
    free(val);

    ASSERT_EQ(kv_get(db, "key2", 4, &val, &val_len), KV_OK);
    ASSERT_STREQ((const char*)val, "value2");
    free(val);

    ASSERT_EQ(kv_get(db, "key3", 4, &val, &val_len), KV_OK);
    ASSERT_STREQ((const char*)val, "value3");
    free(val);

    kv_close(db);
}

TEST_F(BackupTest, BackupEmptyDB) {
    /* 1. 创建空数据库 */
    kv_t *db = kv_open(db_path.c_str());
    ASSERT_NE(db, nullptr);
    kv_close(db);

    /* 2. 备份恢复 */
    ASSERT_EQ(db_backup(db_path.c_str(), backup_dir.c_str()), 0);

    remove(db_path.c_str());
    std::string wal_path = db_path + ".wal";
    remove(wal_path.c_str());

    ASSERT_EQ(db_restore(backup_dir.c_str(), test_dir.c_str()), 0);

    /* 3. 验证空库可打开 */
    db = kv_open(db_path.c_str());
    ASSERT_NE(db, nullptr);
    kv_close(db);
}

TEST_F(BackupTest, BackupMetaCheck) {
    /* 写入数据 + 备份 */
    kv_t *db = kv_open(db_path.c_str());
    ASSERT_NE(db, nullptr);
    kv_put(db, "k1", 2, "v1", 2);
    kv_close(db);

    ASSERT_EQ(db_backup(db_path.c_str(), backup_dir.c_str()), 0);

    /* 读取元数据 */
    backup_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    ASSERT_EQ(db_backup_meta(backup_dir.c_str(), &meta), 0);

    ASSERT_EQ(meta.version, 1);
    ASSERT_GT(meta.db_size, 0);
    ASSERT_GT(meta.db_checksum, 0);
    ASSERT_NE(strlen(meta.db_name), 0);
    ASSERT_NE(strlen(meta.created_at), 0);
}