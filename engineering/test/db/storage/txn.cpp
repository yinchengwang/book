/**
 * @file test_transaction.cpp
 * @brief 事务测试
 */

#include <gtest/gtest.h>
#include "db/kv.h"
#include "db/txn.h"
#include <cstdio>
#include <cstring>

const char* test_txn_file = "test_txn.db";

class TransactionTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(test_txn_file);
        std::remove("test_txn.db.wal");
    }

    void TearDown() override {
    }
};

TEST_F(TransactionTest, SimpleTransaction) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 在事务中插入数据 */
    EXPECT_EQ(TXN_OK, txn_put(txn, "key1", 4, "value1", 6));
    EXPECT_EQ(TXN_OK, txn_put(txn, "key2", 4, "value2", 6));

    /* 提交事务 */
    EXPECT_EQ(TXN_OK, txn_commit(txn));
    EXPECT_EQ(TXN_STATE_COMMITTED, txn_state(txn));

    txn_free(txn);
    kv_close(db);
}

TEST_F(TransactionTest, TransactionCommit) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 插入数据 */
    txn_put(txn, "a", 1, "1", 1);
    txn_put(txn, "b", 1, "2", 1);
    txn_put(txn, "c", 1, "3", 1);

    /* 提交后数据应该可见 */
    txn_commit(txn);
    txn_free(txn);

    /* 在事务外验证 */
    void *val = nullptr;
    size_t len = 0;
    EXPECT_EQ(TXN_OK, kv_get(db, "a", 1, &val, &len));
    EXPECT_EQ(1, len);
    EXPECT_EQ('1', ((char*)val)[0]);
    free(val);

    kv_close(db);
}

TEST_F(TransactionTest, TransactionRollback) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    /* 先插入一条初始数据 */
    kv_put(db, "initial", 7, "data", 4);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 修改数据 */
    txn_put(txn, "initial", 7, "modified", 8);

    /* 回滚 */
    txn_rollback(txn);
    txn_free(txn);

    /* 验证数据恢复 */
    void *val = nullptr;
    size_t len = 0;
    EXPECT_EQ(TXN_OK, kv_get(db, "initial", 7, &val, &len));
    EXPECT_EQ(4, len);
    EXPECT_EQ(0, memcmp(val, "data", 4));
    free(val);

    kv_close(db);
}

TEST_F(TransactionTest, TransactionRollbackInsert) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 插入新数据 */
    txn_put(txn, "new_key", 7, "new_value", 9);
    txn_rollback(txn);
    txn_free(txn);

    /* 数据不应该存在 */
    EXPECT_FALSE(kv_exists(db, "new_key", 7));

    kv_close(db);
}

TEST_F(TransactionTest, TransactionRollbackDelete) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    /* 先插入数据 */
    kv_put(db, "to_delete", 10, "will_restore", 12);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 删除数据 */
    txn_delete(txn, "to_delete", 10);
    txn_rollback(txn);
    txn_free(txn);

    /* 数据应该恢复 */
    EXPECT_TRUE(kv_exists(db, "to_delete", 10));

    kv_close(db);
}

TEST_F(TransactionTest, MultipleTransactions) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn1 = txn_begin(db);
    txn_t *txn2 = txn_begin(db);

    ASSERT_NE(nullptr, txn1);
    ASSERT_NE(nullptr, txn2);

    /* 各自插入数据 */
    txn_put(txn1, "from_txn1", 10, "value1", 6);
    txn_put(txn2, "from_txn2", 10, "value2", 6);

    /* 提交事务1 */
    txn_commit(txn1);
    txn_free(txn1);

    /* 事务2 回滚 */
    txn_rollback(txn2);
    txn_free(txn2);

    /* 只有 txn1 的数据应该存在 */
    EXPECT_TRUE(kv_exists(db, "from_txn1", 10));
    EXPECT_FALSE(kv_exists(db, "from_txn2", 10));

    kv_close(db);
}

TEST_F(TransactionTest, TransactionGet) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    /* 先插入数据 */
    kv_put(db, "existing", 8, "old_value", 9);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 在事务中读取 */
    void *val = nullptr;
    size_t len = 0;
    EXPECT_EQ(TXN_OK, txn_get(txn, "existing", 8, &val, &len));
    EXPECT_EQ(0, memcmp(val, "old_value", len));
    free(val);

    /* 在事务中更新 */
    txn_put(txn, "existing", 8, "new_value", 9);

    /* 再次读取应该看到新值 */
    EXPECT_EQ(TXN_OK, txn_get(txn, "existing", 8, &val, &len));
    EXPECT_EQ(0, memcmp(val, "new_value", len));
    free(val);

    txn_commit(txn);
    txn_free(txn);

    kv_close(db);
}

TEST_F(TransactionTest, TransactionExists) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 插入前不存在 */
    EXPECT_FALSE(txn_exists(txn, "new", 3));

    txn_put(txn, "new", 3, "val", 3);

    /* 插入后存在 */
    EXPECT_TRUE(txn_exists(txn, "new", 3));

    txn_commit(txn);
    txn_free(txn);

    kv_close(db);
}

TEST_F(TransactionTest, TransactionDelete) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    /* 先插入数据 */
    kv_put(db, "to_delete", 10, "value", 5);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    EXPECT_EQ(TXN_OK, txn_delete(txn, "to_delete", 10));
    EXPECT_FALSE(txn_exists(txn, "to_delete", 10));

    txn_commit(txn);
    txn_free(txn);

    /* 确认删除成功 */
    EXPECT_FALSE(kv_exists(db, "to_delete", 10));

    kv_close(db);
}

TEST_F(TransactionTest, TransactionId) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    /* 事务ID应该大于0 */
    EXPECT_GT(txn_id(txn), 0);
    EXPECT_EQ(TXN_STATE_ACTIVE, txn_state(txn));

    txn_commit(txn);
    txn_free(txn);

    kv_close(db);
}

TEST_F(TransactionTest, ActiveTransactionCount) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(0, txn_active_count());

    txn_t *txn1 = txn_begin(db);
    txn_t *txn2 = txn_begin(db);
    txn_t *txn3 = txn_begin(db);

    EXPECT_EQ(3, txn_active_count());

    txn_commit(txn1);
    EXPECT_EQ(2, txn_active_count());

    txn_rollback(txn2);
    EXPECT_EQ(1, txn_active_count());

    txn_free(txn3);
    EXPECT_EQ(0, txn_active_count());

    kv_close(db);
}

TEST_F(TransactionTest, DoubleCommit) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    txn_put(txn, "k", 1, "v", 1);

    /* 第一次提交成功 */
    EXPECT_EQ(TXN_OK, txn_commit(txn));

    /* 第二次提交失败 */
    EXPECT_EQ(TXN_ERROR, txn_commit(txn));

    txn_free(txn);
    kv_close(db);
}

TEST_F(TransactionTest, DoubleRollback) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    txn_put(txn, "k", 1, "v", 1);

    /* 第一次回滚成功 */
    EXPECT_EQ(TXN_OK, txn_rollback(txn));

    /* 第二次回滚失败 */
    EXPECT_EQ(TXN_ERROR, txn_rollback(txn));

    txn_free(txn);
    kv_close(db);
}

TEST_F(TransactionTest, ErrorMessage) {
    kv_t *db = kv_open(test_txn_file);
    ASSERT_NE(nullptr, db);

    txn_t *txn = txn_begin(db);
    ASSERT_NE(nullptr, txn);

    txn_put(txn, "k", 1, "v", 1);
    txn_commit(txn);

    /* 提交后不能再操作 */
    EXPECT_EQ(TXN_ERROR, txn_put(txn, "k2", 2, "v2", 2));
    EXPECT_STRNE(nullptr, txn_errmsg(txn));

    txn_free(txn);
    kv_close(db);
}