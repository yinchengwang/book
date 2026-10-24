/**
 * @file test_txn.c
 * @brief MVCC 事务测试
 *
 * 测试 MVCC 快照隔离和可见性判断
 */

#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/kv.h"
#include "db/storage/txn/txn_internal.h"

/* 简化的 tuple_header 结构（与 txn_internal.h 中一致） */
typedef struct test_tuple_s {
    uint32_t t_xmin;       /**< 创建事务 ID */
    uint32_t t_xmax;       /**< 删除事务 ID (0 = 未删除) */
    uint64_t t_csn;        /**< 提交序列号 */
    bool     committed;    /**< 是否已提交 */
} test_tuple_t;

/* 测试夹具 */
class TxnTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建临时数据库用于测试 */
        db = kv_open(":memory:");
        ASSERT_NE(db, nullptr);
    }

    void TearDown() override {
        if (db) {
            kv_close(db);
        }
    }

    kv_t *db;
};

/* ============================================================
 * 基础事务测试
 * ============================================================ */

TEST_F(TxnTest, BeginBasic) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    EXPECT_EQ(txn_state(txn), TXN_STATE_ACTIVE);
    EXPECT_GT(txn_id(txn), 0u);
    txn_free(txn);
}

TEST_F(TxnTest, CommitBasic) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);

    /* 写入数据 */
    const char *key = "test_key";
    const char *value = "test_value";
    EXPECT_EQ(txn_put(txn, key, strlen(key), value, strlen(value)), TXN_OK);

    /* 提交事务 */
    EXPECT_EQ(txn_commit(txn), TXN_OK);
    EXPECT_EQ(txn_state(txn), TXN_STATE_COMMITTED);

    txn_free(txn);
}

TEST_F(TxnTest, RollbackBasic) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);

    /* 写入数据 */
    const char *key = "test_key";
    const char *value = "test_value";
    EXPECT_EQ(txn_put(txn, key, strlen(key), value, strlen(value)), TXN_OK);

    /* 回滚事务 */
    EXPECT_EQ(txn_rollback(txn), TXN_OK);
    EXPECT_EQ(txn_state(txn), TXN_STATE_ABORTED);

    txn_free(txn);
}

TEST_F(TxnTest, PutGetDelete) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);

    /* 插入 */
    const char *key = "k1";
    const char *value = "v1";
    EXPECT_EQ(txn_put(txn, key, strlen(key), value, strlen(value)), TXN_OK);

    /* 获取 */
    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(txn_get(txn, key, strlen(key), &out_value, &out_len), TXN_OK);
    ASSERT_NE(out_value, nullptr);
    EXPECT_EQ(memcmp(out_value, value, out_len), 0);
    free(out_value);

    /* 删除 */
    EXPECT_EQ(txn_delete(txn, key, strlen(key)), TXN_OK);

    /* 再次获取应该不存在 */
    EXPECT_EQ(txn_get(txn, key, strlen(key), &out_value, &out_len), TXN_NOT_FOUND);

    txn_free(txn);
}

/* ============================================================
 * MVCC 可见性测试
 * ============================================================ */

TEST_F(TxnTest, MvccBegin) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);

    /* 开启 MVCC 模式 */
    EXPECT_EQ(txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED), TXN_OK);
    EXPECT_TRUE(txn_is_mvcc(txn));
    EXPECT_EQ(txn_get_isolation_level(txn), TXN_ISOLATION_READ_COMMITTED);
    EXPECT_GT(txn_get_snapshot_lsn(txn), 0u);

    txn_free(txn);
}

TEST_F(TxnTest, MvccVisibilityOwnWrite) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED);

    /* 模拟一个由当前事务创建的元组 */
    test_tuple_t tuple;
    tuple.t_xmin = txn_id(txn);  /* 当前事务创建的 */
    tuple.t_xmax = 0;
    tuple.committed = true;

    /* 自己的修改应该可见 */
    EXPECT_EQ(txn_visibility_check(txn, (const tuple_header_t*)&tuple), 1);

    txn_free(txn);
}

TEST_F(TxnTest, MvccVisibilityCommittedBeforeSnapshot) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED);

    uint64_t snapshot_lsn = txn_get_snapshot_lsn(txn);

    /* 模拟一个在快照之前已提交的元组 */
    test_tuple_t tuple;
    tuple.t_xmin = 1;  /* 早期事务 */
    tuple.t_xmax = 0;
    tuple.committed = true;
    /* tuple.t_xmin < snapshot_lsn 由调用者保证 */

    /* 已提交且在快照之前的创建应该可见 */
    EXPECT_EQ(txn_visibility_check(txn, (const tuple_header_t*)&tuple), 1);

    txn_free(txn);
}

TEST_F(TxnTest, MvccVisibilityDeletedBeforeSnapshot) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED);

    uint64_t snapshot_lsn = txn_get_snapshot_lsn(txn);

    /* 模拟一个在快照之前被删除且删除已提交的元组 */
    test_tuple_t tuple;
    tuple.t_xmin = 1;  /* 早期事务创建 */
    tuple.t_xmax = 2;  /* 早期事务删除 */
    tuple.committed = true;
    tuple.t_xmax = snapshot_lsn - 1;  /* 删除在快照之前 */

    /* 已被删除且删除在快照之前，不可见 */
    EXPECT_EQ(txn_visibility_check(txn, (const tuple_header_t*)&tuple), 0);

    txn_free(txn);
}

TEST_F(TxnTest, MvccVisibilityNotCommitted) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED);

    /* 模拟一个未提交的创建 */
    test_tuple_t tuple;
    tuple.t_xmin = 999;  /* 其他未提交事务 */
    tuple.t_xmax = 0;
    tuple.committed = false;

    /* 未提交的创建不可见 */
    EXPECT_EQ(txn_visibility_check(txn, (const tuple_header_t*)&tuple), 0);

    txn_free(txn);
}

TEST_F(TxnTest, MvccVisibilityNullArgs) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED);

    /* 空参数应该返回不可见 */
    EXPECT_EQ(txn_visibility_check(nullptr, nullptr), 0);
    EXPECT_EQ(txn_visibility_check(txn, nullptr), 0);
    EXPECT_EQ(txn_visibility_check(nullptr, (const tuple_header_t*)1), 0);

    txn_free(txn);
}

/* ============================================================
 * MVCC 隔离级别测试
 * ============================================================ */

TEST_F(TxnTest, MvccIsolationLevels) {
    txn_t *txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);

    /* 测试读已提交 */
    EXPECT_EQ(txn_begin_mvcc(txn, TXN_ISOLATION_READ_COMMITTED), TXN_OK);
    EXPECT_EQ(txn_get_isolation_level(txn), TXN_ISOLATION_READ_COMMITTED);
    txn_free(txn);

    /* 测试可重复读 */
    txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    EXPECT_EQ(txn_begin_mvcc(txn, TXN_ISOLATION_REPEATABLE_READ), TXN_OK);
    EXPECT_EQ(txn_get_isolation_level(txn), TXN_ISOLATION_REPEATABLE_READ);
    txn_free(txn);

    /* 测试串行化 */
    txn = txn_begin(db);
    ASSERT_NE(txn, nullptr);
    EXPECT_EQ(txn_begin_mvcc(txn, TXN_ISOLATION_SERIALIZABLE), TXN_OK);
    EXPECT_EQ(txn_get_isolation_level(txn), TXN_ISOLATION_SERIALIZABLE);
    txn_free(txn);
}

/* ============================================================
 * 并发事务测试
 * ============================================================ */

TEST_F(TxnTest, ConcurrentTransactions) {
    /* 事务1 */
    txn_t *txn1 = txn_begin(db);
    ASSERT_NE(txn1, nullptr);

    /* 事务2 */
    txn_t *txn2 = txn_begin(db);
    ASSERT_NE(txn2, nullptr);

    /* 两个事务可以同时读取 */
    const char *key = "shared_key";
    const char *value = "shared_value";

    EXPECT_EQ(txn_put(txn1, key, strlen(key), value, strlen(value)), TXN_OK);
    EXPECT_EQ(txn_put(txn2, key, strlen(key), value, strlen(value)), TXN_OK);

    /* 各自提交 */
    EXPECT_EQ(txn_commit(txn1), TXN_OK);
    EXPECT_EQ(txn_commit(txn2), TXN_OK);

    txn_free(txn1);
    txn_free(txn2);
}

/* ============================================================
 * 错误处理测试
 * ============================================================ */

TEST_F(TxnTest, NullDb) {
    txn_t *txn = txn_begin(nullptr);
    EXPECT_EQ(txn, nullptr);
}

TEST_F(TxnTest, InvalidTxn) {
    EXPECT_EQ(txn_commit(nullptr), TXN_ERROR);
    EXPECT_EQ(txn_rollback(nullptr), TXN_ERROR);
}

TEST_F(TxnTest, PutWithoutTxn) {
    const char *key = "test_key";
    const char *value = "test_value";
    /* 没有活动事务的 put 应该失败 */
    txn_t *txn = txn_begin(db);
    txn_free(txn);
    /* txn 现在处于非活动状态 */
    EXPECT_EQ(txn_put(txn, key, strlen(key), value, strlen(value)), TXN_ERROR);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
