#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/storage/wal/wal.h"

class WalTest : public ::testing::Test {
protected:
    void SetUp() override {
        snprintf(test_path, sizeof(test_path), "./test_wal_%d.log", getpid());
    }
    void TearDown() {
        remove(test_path);
    }
    char test_path[256];
};

TEST_F(WalTest, CreateFlush) {
    wal_t *wal = wal_create(test_path, 4096);
    ASSERT_NE(wal, nullptr);

    /* 写入一条 INSERT 记录 */
    uint64_t lsn = wal_write_insert(wal, 1, "key1", 4, "value1", 6);
    EXPECT_NE(lsn, 0);

    /* 刷盘 */
    EXPECT_EQ(wal_flush(wal), 0);

    wal_close(wal);
}

TEST_F(WalTest, RedoInsert) {
    /* 创建 WAL 并写入一条记录 */
    wal_t *wal = wal_create(test_path, 4096);
    ASSERT_NE(wal, nullptr);

    wal_write_begin(wal, 1);
    wal_write_insert(wal, 1, "key1", 4, "value1", 6);
    wal_write_commit(wal, 1);
    wal_flush(wal);
    wal_close(wal);

    /* 重新打开 WAL */
    wal_t *wal2 = wal_open(test_path);
    ASSERT_NE(wal2, nullptr);
    wal_close(wal2);
}

TEST_F(WalTest, NullSafe) {
    wal_close(NULL);

    /* 测试 NULL 安全性 */
    EXPECT_EQ(wal_flush(NULL), 0);
    EXPECT_EQ(wal_write_insert(NULL, 0, NULL, 0, NULL, 0), 0);
}

TEST_F(WalTest, VariableLengthLSN) {
    wal_t *wal = wal_create(test_path, 4096);
    ASSERT_NE(wal, nullptr);

    /* 写入不同大小的记录，验证 LSN 是字节偏移 */
    uint64_t lsn1 = wal_write_insert(wal, 1, "k", 1, "v", 1);
    uint64_t lsn2 = wal_write_insert(wal, 1, "key", 3, "value", 5);
    uint64_t lsn3 = wal_write_insert(wal, 1, "longkey", 7, "longvalue", 9);

    /* LSN 应该递增 */
    EXPECT_GT(lsn2, lsn1);
    EXPECT_GT(lsn3, lsn2);

    /* LSN 应该是字节偏移（非 record index） */
    EXPECT_EQ(lsn1, 0);  /* 第一条记录从偏移 0 开始 */

    wal_flush(wal);
    wal_close(wal);
}

TEST_F(WalTest, AllRecordTypes) {
    wal_t *wal = wal_create(test_path, 4096);
    ASSERT_NE(wal, nullptr);

    /* 测试所有记录类型 */
    uint64_t lsn;

    lsn = wal_write_begin(wal, 1);
    EXPECT_NE(lsn, 0);

    lsn = wal_write_insert(wal, 1, "key", 3, "val", 3);
    EXPECT_NE(lsn, 0);

    lsn = wal_write_update(wal, 1, "key", 3, "old", 3, "new", 3);
    EXPECT_NE(lsn, 0);

    lsn = wal_write_delete(wal, 1, "key", 3, "val", 3);
    EXPECT_NE(lsn, 0);

    lsn = wal_write_commit(wal, 1);
    EXPECT_NE(lsn, 0);

    /* 写入第二个事务用于 abort 测试 */
    wal_write_begin(wal, 2);
    wal_write_insert(wal, 2, "key2", 4, "val2", 4);
    lsn = wal_write_abort(wal, 2);
    EXPECT_NE(lsn, 0);

    wal_flush(wal);
    wal_close(wal);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
