/**
 * @file txn_test.cpp
 * @brief ACID 事务测试
 *
 * 覆盖：
 *   1. BeginCommit - 开始并提交事务
 *   2. BeginAbort - 开始并回滚事务
 *   3. ReadOnlyTxn - 只读事务
 *   4. NestedSavepoint - 嵌套 Savepoint
 *   5. AutoRollback - 自动回滚
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_txn.h"
#include "sdk/impl/sqlite_backend.h"
}

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kDbPath = "test_txn.db";

void cleanup_db() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
}

}  // namespace

class TxnTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* 创建测试表（使用 SQLite 直接操作） */
        ASSERT_EQ(sqlite3_exec(db_->db,
            "CREATE TABLE test_txn (id INTEGER PRIMARY KEY, value TEXT)",
            nullptr, nullptr, nullptr), SQLITE_OK);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* 测试 1: 开始并提交事务 */
TEST_F(TxnTest, BeginCommit) {
    mmdb_txn_t* txn = nullptr;
    ASSERT_EQ(mmdb_txn_begin(db_, MMDB_TXN_READWRITE, &txn), MMDB_OK);
    ASSERT_NE(txn, nullptr);
    ASSERT_EQ(mmdb_txn_get_state(txn), MMDB_TXN_ACTIVE);

    /* 插入数据 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_txn (id, value) VALUES (1, 'hello')",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 提交 */
    ASSERT_EQ(mmdb_txn_commit(txn), MMDB_OK);
    ASSERT_EQ(mmdb_txn_get_state(txn), MMDB_TXN_COMMITTED);

    mmdb_txn_free(txn);

    /* 验证数据持久化 */
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT value FROM test_txn WHERE id = 1", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STREQ((const char*)sqlite3_column_text(stmt, 0), "hello");
    sqlite3_finalize(stmt);
}

/* 测试 2: 开始并回滚事务 */
TEST_F(TxnTest, BeginAbort) {
    mmdb_txn_t* txn = nullptr;
    ASSERT_EQ(mmdb_txn_begin(db_, MMDB_TXN_READWRITE, &txn), MMDB_OK);

    /* 插入数据 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_txn (id, value) VALUES (1, 'hello')",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 回滚 */
    ASSERT_EQ(mmdb_txn_abort(txn), MMDB_OK);
    ASSERT_EQ(mmdb_txn_get_state(txn), MMDB_TXN_ABORTED);

    mmdb_txn_free(txn);

    /* 验证数据已回滚 */
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT COUNT(*) FROM test_txn", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
}

/* 测试 3: 只读事务 */
TEST_F(TxnTest, ReadOnlyTxn) {
    /* 先插入测试数据 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_txn (id, value) VALUES (1, 'hello')",
        nullptr, nullptr, nullptr), SQLITE_OK);

    mmdb_txn_t* txn = nullptr;
    ASSERT_EQ(mmdb_txn_begin(db_, MMDB_TXN_READONLY, &txn), MMDB_OK);
    ASSERT_TRUE(mmdb_txn_is_readonly(txn));

    /* 读取数据 */
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT value FROM test_txn WHERE id = 1", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STREQ((const char*)sqlite3_column_text(stmt, 0), "hello");
    sqlite3_finalize(stmt);

    /* 提交 */
    ASSERT_EQ(mmdb_txn_commit(txn), MMDB_OK);
    mmdb_txn_free(txn);
}

/* 测试 4: 嵌套 Savepoint（简化测试） */
TEST_F(TxnTest, NestedSavepoint) {
    mmdb_txn_t* txn = nullptr;
    ASSERT_EQ(mmdb_txn_begin(db_, MMDB_TXN_READWRITE, &txn), MMDB_OK);

    /* 插入数据 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_txn (id, value) VALUES (1, 'hello')",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 创建 Savepoint */
    ASSERT_EQ(sqlite3_exec(db_->db, "SAVEPOINT sp1",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 在 Savepoint 内插入数据 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_txn (id, value) VALUES (2, 'world')",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 回滚到 Savepoint */
    ASSERT_EQ(sqlite3_exec(db_->db, "ROLLBACK TO SAVEPOINT sp1",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 提交外层事务 */
    ASSERT_EQ(mmdb_txn_commit(txn), MMDB_OK);
    mmdb_txn_free(txn);

    /* 验证只有第一条数据 */
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT COUNT(*) FROM test_txn", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 1);
    sqlite3_finalize(stmt);
}

/* 测试 5: 自动回滚（释放未提交事务） */
TEST_F(TxnTest, AutoRollback) {
    mmdb_txn_t* txn = nullptr;
    ASSERT_EQ(mmdb_txn_begin(db_, MMDB_TXN_READWRITE, &txn), MMDB_OK);

    /* 插入数据 */
    ASSERT_EQ(sqlite3_exec(db_->db,
        "INSERT INTO test_txn (id, value) VALUES (1, 'hello')",
        nullptr, nullptr, nullptr), SQLITE_OK);

    /* 直接释放（未提交/回滚） */
    mmdb_txn_free(txn);

    /* 验证数据已回滚 */
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db_->db,
        "SELECT COUNT(*) FROM test_txn", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
}
