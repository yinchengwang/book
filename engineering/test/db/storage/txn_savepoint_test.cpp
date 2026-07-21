/**
 * @file txn_savepoint_test.cpp
 * @brief W5.3: 子事务/保存点测试
 *
 * 测试场景：
 *   1. 创建保存点
 *   2. 回滚到保存点
 *   3. 释放保存点
 *   4. 嵌套保存点
 *   5. 事务结束后保存点自动清理
 *   6. 保存点名称操作
 *   7. 保存点深度查询
 *   8. 保存点数量上限
 */

#include <gtest/gtest.h>
#include "db/storage/txn/mvcc.h"
#include "db/storage/txn/txn_savepoint.h"
#include <cstdio>
#include <cstring>

extern "C" {
    int txn_start(isolation_level_t isolation);
    int txn_end(void);
    int txn_abort(void);
    TxnContext *txn_get_context(void);
    void txn_reset_slots(void);
}

class SavepointTest : public ::testing::Test {
protected:
    void SetUp() override {
        txn_reset_slots();
        mvcc_reset();
    }

    void TearDown() override {
        /* 确保清理事务 */
        if (txn_in_progress()) {
            txn_abort();
        }
    }
};

/* ========================================================================
 * 测试 1: 创建保存点
 * ======================================================================== */
TEST_F(SavepointTest, CreateSavepoint) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(0, txn_create_savepoint("sp1"));
    ASSERT_EQ(1, txn_savepoint_depth());
    ASSERT_STREQ("sp1", txn_savepoint_name(0));
    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 2: 回滚到保存点
 * ======================================================================== */
TEST_F(SavepointTest, RollbackToSavepoint) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(0, txn_create_savepoint("sp1"));

    /* 再创建一个保存点 */
    ASSERT_EQ(0, txn_create_savepoint("sp2"));
    ASSERT_EQ(2, txn_savepoint_depth());

    /* 回滚到 sp1：sp2 被撤销 */
    ASSERT_EQ(0, txn_rollback_to_savepoint("sp1"));
    ASSERT_EQ(1, txn_savepoint_depth());

    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 3: 释放保存点
 * ======================================================================== */
TEST_F(SavepointTest, ReleaseSavepoint) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(0, txn_create_savepoint("sp1"));

    ASSERT_EQ(0, txn_release_savepoint("sp1"));
    ASSERT_EQ(0, txn_savepoint_depth());

    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 4: 嵌套保存点
 * ======================================================================== */
TEST_F(SavepointTest, NestedSavepoints) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));

    /* 三层嵌套 */
    ASSERT_EQ(0, txn_create_savepoint("outer"));
    ASSERT_EQ(1, txn_savepoint_depth());
    ASSERT_STREQ("outer", txn_savepoint_name(0));

    ASSERT_EQ(0, txn_create_savepoint("middle"));
    ASSERT_EQ(2, txn_savepoint_depth());
    ASSERT_STREQ("middle", txn_savepoint_name(1));

    ASSERT_EQ(0, txn_create_savepoint("inner"));
    ASSERT_EQ(3, txn_savepoint_depth());
    ASSERT_STREQ("inner", txn_savepoint_name(2));

    /* 回滚到 middle：撤销 inner */
    ASSERT_EQ(0, txn_rollback_to_savepoint("middle"));
    ASSERT_EQ(2, txn_savepoint_depth());
    ASSERT_STREQ("middle", txn_savepoint_name(1));

    /* 释放 middle */
    ASSERT_EQ(0, txn_release_savepoint("middle"));
    ASSERT_EQ(1, txn_savepoint_depth());
    ASSERT_STREQ("outer", txn_savepoint_name(0));

    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 5: 事务结束后保存点自动清理
 * ======================================================================== */
TEST_F(SavepointTest, SavepointsClearedOnEnd) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(0, txn_create_savepoint("sp1"));
    ASSERT_EQ(0, txn_create_savepoint("sp2"));
    ASSERT_EQ(2, txn_savepoint_depth());

    ASSERT_EQ(0, txn_end());

    /* 新事务，保存点应该为空 */
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(0, txn_savepoint_depth());
    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 6: 保存点名称操作
 * ======================================================================== */
TEST_F(SavepointTest, SavepointByName) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));

    /* 命名保存点 */
    ASSERT_EQ(0, txn_create_savepoint("my_savepoint_1"));
    ASSERT_STREQ("my_savepoint_1", txn_savepoint_name(0));

    /* 回滚到指定名称 */
    ASSERT_EQ(0, txn_create_savepoint("temp"));
    ASSERT_EQ(2, txn_savepoint_depth());

    ASSERT_EQ(0, txn_rollback_to_savepoint("my_savepoint_1"));
    ASSERT_EQ(1, txn_savepoint_depth());

    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 7: 保存点深度查询
 * ======================================================================== */
TEST_F(SavepointTest, SavepointDepth) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(0, txn_savepoint_depth());

    ASSERT_EQ(0, txn_create_savepoint("sp1"));
    ASSERT_EQ(1, txn_savepoint_depth());

    ASSERT_EQ(0, txn_create_savepoint("sp2"));
    ASSERT_EQ(2, txn_savepoint_depth());

    ASSERT_EQ(0, txn_create_savepoint("sp3"));
    ASSERT_EQ(3, txn_savepoint_depth());

    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 8: 保存点数量上限
 * ======================================================================== */
TEST_F(SavepointTest, MaxSavepoints) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));

    /* 创建 MAX_SAVEPOINTS 个保存点 */
    for (int i = 0; i < MAX_SAVEPOINTS; i++) {
        ASSERT_EQ(0, txn_create_savepoint(NULL));
    }
    ASSERT_EQ(MAX_SAVEPOINTS, txn_savepoint_depth());

    /* 第 MAX_SAVEPOINTS+1 个应该失败 */
    ASSERT_EQ(-1, txn_create_savepoint("extra"));

    /* 回滚释放后可以再创建 */
    ASSERT_EQ(0, txn_rollback_to_savepoint("sp_0"));
    ASSERT_EQ(1, txn_savepoint_depth());

    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 9: 不在事务中创建保存点应该失败
 * ======================================================================== */
TEST_F(SavepointTest, CreateOutsideTransaction) {
    ASSERT_EQ(-1, txn_create_savepoint("sp1"));
    ASSERT_EQ(-1, txn_rollback_to_savepoint("sp1"));
    ASSERT_EQ(-1, txn_release_savepoint("sp1"));
}

/* ========================================================================
 * 测试 10: 释放不存在的保存点
 * ======================================================================== */
TEST_F(SavepointTest, ReleaseNonExistent) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(-1, txn_release_savepoint("nonexistent"));
    ASSERT_EQ(0, txn_end());
}

/* ========================================================================
 * 测试 11: 回滚到不存在的保存点
 * ======================================================================== */
TEST_F(SavepointTest, RollbackToNonExistent) {
    ASSERT_EQ(0, txn_start(ISOLATION_READ_COMMITTED));
    ASSERT_EQ(-1, txn_rollback_to_savepoint("nonexistent"));
    ASSERT_EQ(0, txn_end());
}