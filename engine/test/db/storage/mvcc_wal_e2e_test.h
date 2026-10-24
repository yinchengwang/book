#ifndef DB_MVCC_WAL_E2E_H
#define DB_MVCC_WAL_E2E_H
/* C4-2 T12: Relational MVCC + WAL 事务端到端测试骨架 */
#include <gtest/gtest.h>

extern "C" {
#include "db/mvcc_session.h"
}

TEST(MvccWalE2E, SessionLifecycleBasic) {
    /* 骨架：模拟事务会话生命周期 */
    int64_t before = mvcc_current_xid();
    mvcc_set_current_xid(42);
    EXPECT_EQ(mvcc_current_xid(), 42);
    mvcc_clear_current_xid();
    EXPECT_EQ(mvcc_current_xid(), 0);
    (void)before;
}
#endif
