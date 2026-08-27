/**
 * @file c2_1_isolation_test.cpp
 * @brief C2-1 T9 并发事务隔离矩阵测试
 *
 * 测试骨架：RC（read_committed）与 RR（repeatable_read）级别下：
 *  - 脏读（事务 A 未提交，B 不应读到）
 *  - 不可重复读（B 在 A 提交前后读到不同值——RC 期望可，RR 期望不可）
 *  - 幻读（范围扫描结果数变化）
 *
 * 当前实现：占位断言；完整隔离矩阵需 tuple xmin/xmax 落库后才有意义
 * （依赖 C2-1 T2 落地）。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/mvcc_session.h"
}

namespace {
constexpr int64_t kSampleXid = 100;
}  // namespace

TEST(MvccIsolation, RcBaselineVisibility) {
    /* 占位：RC 隔离下 current_xid 设置后应可读，但其他事务未提交数据不可见 */
    mvcc_set_current_xid(kSampleXid);
    EXPECT_EQ(mvcc_current_xid(), kSampleXid);
    mvcc_clear_current_xid();
    EXPECT_EQ(mvcc_current_xid(), 0);
}

TEST(MvccIsolation, NoXidBaseline) {
    /* 无活跃事务时 current_xid 应为 0（所有历史版本可见） */
    EXPECT_EQ(mvcc_current_xid(), 0);
}