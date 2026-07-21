/**
 * @file twophase_test.cpp
 * @brief W5.1: 两阶段提交 2PC PREPARE/COMMIT PREPARED/ROLLBACK PREPARED 测试
 *
 * 测试场景：
 *   1. txn_prepare — 将事务标记为 PREPARED
 *   2. txn_commit_prepared — 提交已 PREPARE 的事务
 *   3. txn_rollback_prepared — 回滚已 PREPARE 的事务
 *   4. PREPARE 后不能再次 PREPARE 或普通提交/回滚
 *   5. 未 PREPARE 的事务不能 COMMIT PREPARED
 *   6. 多个 PREPARED 事务列表查询
 *   7. CLOG 持久化验证 PREPARED 状态
 *   8. WAL PREPARE 日志写入
 */

#include <gtest/gtest.h>
#include "db/storage/txn/mvcc.h"
#include "db/storage/txn/clog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

/* 从 txn_xact.c 引入的函数声明（非公开 API，测试直接使用） */
extern "C" {
    int txn_begin(TransactionId *xid);
    int txn_commit(TransactionId xid);
    int txn_rollback(TransactionId xid);
    int txn_prepare(TransactionId xid, const char *gid);
    int txn_commit_prepared(TransactionId xid);
    int txn_rollback_prepared(TransactionId xid);
    int txn_get_prepared_xids(TransactionId *xids, int max_count);
    transaction_status_t txn_get_status(TransactionId xid);
    void txn_reset_slots(void);
}

class TwoPhaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 重置事务槽位和 MVCC 子系统 */
        txn_reset_slots();
        mvcc_reset();
    }

    void TearDown() override {
    }
};

/* ========================================================================
 * 测试 1: txn_prepare — 基本 PREPARE 功能
 * ======================================================================== */
TEST_F(TwoPhaseTest, BasicPrepare) {
    TransactionId xid;

    /* 开始事务 */
    ASSERT_EQ(0, txn_begin(&xid));
    ASSERT_NE(INVALID_TRANSACTION_ID, xid);
    ASSERT_EQ(TRANSACTION_STATUS_IN_PROGRESS, txn_get_status(xid));

    /* PREPARE 事务 */
    ASSERT_EQ(0, txn_prepare(xid, NULL));
    ASSERT_EQ(TRANSACTION_STATUS_PREPARED, txn_get_status(xid));
}

/* ========================================================================
 * 测试 2: txn_commit_prepared — 提交已 PREPARE 的事务
 * ======================================================================== */
TEST_F(TwoPhaseTest, CommitPrepared) {
    TransactionId xid;

    ASSERT_EQ(0, txn_begin(&xid));
    ASSERT_EQ(0, txn_prepare(xid, NULL));
    ASSERT_EQ(TRANSACTION_STATUS_PREPARED, txn_get_status(xid));

    /* COMMIT PREPARED */
    ASSERT_EQ(0, txn_commit_prepared(xid));
    ASSERT_EQ(TRANSACTION_STATUS_COMMITTED, txn_get_status(xid));
}

/* ========================================================================
 * 测试 3: txn_rollback_prepared — 回滚已 PREPARE 的事务
 * ======================================================================== */
TEST_F(TwoPhaseTest, RollbackPrepared) {
    TransactionId xid;

    ASSERT_EQ(0, txn_begin(&xid));
    ASSERT_EQ(0, txn_prepare(xid, NULL));
    ASSERT_EQ(TRANSACTION_STATUS_PREPARED, txn_get_status(xid));

    /* ROLLBACK PREPARED */
    ASSERT_EQ(0, txn_rollback_prepared(xid));
    ASSERT_EQ(TRANSACTION_STATUS_ABORTED, txn_get_status(xid));
}

/* ========================================================================
 * 测试 4: PREPARE 后不能再次 PREPARE 或普通提交/回滚
 * ======================================================================== */
TEST_F(TwoPhaseTest, PreparedCannotRedoOperations) {
    TransactionId xid;

    ASSERT_EQ(0, txn_begin(&xid));
    ASSERT_EQ(0, txn_prepare(xid, NULL));

    /* 不能再次 PREPARE */
    ASSERT_EQ(-1, txn_prepare(xid, NULL));

    /* 不能普通提交（PREPARE 后只能用 COMMIT PREPARED） */
    ASSERT_EQ(-1, txn_commit(xid));

    /* 不能普通回滚（PREPARE 后只能用 ROLLBACK PREPARED） */
    ASSERT_EQ(-1, txn_rollback(xid));
}

/* ========================================================================
 * 测试 5: 未 PREPARE 的事务不能 COMMIT PREPARED / ROLLBACK PREPARED
 * ======================================================================== */
TEST_F(TwoPhaseTest, NonPreparedCannotCommitPrepared) {
    TransactionId xid;

    ASSERT_EQ(0, txn_begin(&xid));

    /* 未 PREPARE 的事务不能 COMMIT PREPARED */
    ASSERT_EQ(-1, txn_commit_prepared(xid));

    /* 未 PREPARE 的事务不能 ROLLBACK PREPARED */
    ASSERT_EQ(-1, txn_rollback_prepared(xid));

    /* 正常提交仍然可以 */
    ASSERT_EQ(0, txn_commit(xid));
}

/* ========================================================================
 * 测试 6: 多个 PREPARED 事务列表查询
 * ======================================================================== */
TEST_F(TwoPhaseTest, MultiplePreparedTransactions) {
    TransactionId xid1, xid2, xid3;
    TransactionId xids[16];

    ASSERT_EQ(0, txn_begin(&xid1));
    ASSERT_EQ(0, txn_begin(&xid2));
    ASSERT_EQ(0, txn_begin(&xid3));

    /* PREPARE 其中两个 */
    ASSERT_EQ(0, txn_prepare(xid1, NULL));
    ASSERT_EQ(0, txn_prepare(xid3, NULL));

    /* 查询 PREPARED 列表 */
    int count = txn_get_prepared_xids(xids, 16);
    ASSERT_EQ(2, count);

    /* 验证列表包含 xid1 和 xid3 */
    bool found1 = false, found3 = false;
    for (int i = 0; i < count; i++) {
        if (xids[i] == xid1) found1 = true;
        if (xids[i] == xid3) found3 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found3);

    /* 提交一个 PREPARED 事务 */
    ASSERT_EQ(0, txn_commit_prepared(xid1));

    /* 验证只剩一个 */
    count = txn_get_prepared_xids(xids, 16);
    ASSERT_EQ(1, count);
    EXPECT_EQ(xid3, xids[0]);
}

/* ========================================================================
 * 测试 7: CLOG 持久化验证 PREPARED 状态
 * ======================================================================== */
TEST_F(TwoPhaseTest, ClogPreparedPersistence) {
    TransactionId xid;

    ASSERT_EQ(0, txn_begin(&xid));
    ASSERT_EQ(0, txn_prepare(xid, NULL));

    /* 通过 CLOG 直接验证状态 */
    uint8_t clog_status = clog_get_status(xid);
    ASSERT_EQ(CLOG_STATUS_PREPARED, clog_status);

    /* COMMIT PREPARED 后验证 CLOG */
    ASSERT_EQ(0, txn_commit_prepared(xid));
    clog_status = clog_get_status(xid);
    ASSERT_EQ(CLOG_STATUS_COMMITTED, clog_status);
}

/* ========================================================================
 * 测试 8: PREPARE 后 COMMIT 的完整 2PC 流程
 * ======================================================================== */
TEST_F(TwoPhaseTest, FullTwoPhaseCommit) {
    TransactionId xid;

    /* Phase 1: 正常事务操作 */
    ASSERT_EQ(0, txn_begin(&xid));

    /* Phase 2: PREPARE */
    ASSERT_EQ(0, txn_prepare(xid, "global_txn_001"));
    ASSERT_EQ(TRANSACTION_STATUS_PREPARED, txn_get_status(xid));

    /* Phase 3: COMMIT PREPARED */
    ASSERT_EQ(0, txn_commit_prepared(xid));
    ASSERT_EQ(TRANSACTION_STATUS_COMMITTED, txn_get_status(xid));
}

/* ========================================================================
 * 测试 9: PREPARE 后 ROLLBACK 的完整 2PC 流程
 * ======================================================================== */
TEST_F(TwoPhaseTest, FullTwoPhaseRollback) {
    TransactionId xid;

    ASSERT_EQ(0, txn_begin(&xid));
    ASSERT_EQ(0, txn_prepare(xid, "global_txn_002"));
    ASSERT_EQ(TRANSACTION_STATUS_PREPARED, txn_get_status(xid));

    ASSERT_EQ(0, txn_rollback_prepared(xid));
    ASSERT_EQ(TRANSACTION_STATUS_ABORTED, txn_get_status(xid));
}

/* ========================================================================
 * 测试 10: 边界 — 空事务列表
 * ======================================================================== */
TEST_F(TwoPhaseTest, EmptyPreparedList) {
    TransactionId xids[16];

    int count = txn_get_prepared_xids(xids, 16);
    ASSERT_EQ(0, count);
}

/* ========================================================================
 * 测试 11: 边界 — 传入 NULL 参数
 * ======================================================================== */
TEST_F(TwoPhaseTest, NullParameters) {
    /* 空 xid 的 PREPARE */
    ASSERT_EQ(-1, txn_prepare(INVALID_TRANSACTION_ID, NULL));

    /* 空 xid 的 COMMIT PREPARED */
    ASSERT_EQ(-1, txn_commit_prepared(INVALID_TRANSACTION_ID));

    /* 空 xid 的 ROLLBACK PREPARED */
    ASSERT_EQ(-1, txn_rollback_prepared(INVALID_TRANSACTION_ID));

    /* NULL 输出数组 */
    TransactionId dummy;
    ASSERT_EQ(0, txn_get_prepared_xids(NULL, 16));
    ASSERT_EQ(0, txn_get_prepared_xids(&dummy, 0));
}