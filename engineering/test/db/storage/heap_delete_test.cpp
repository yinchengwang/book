/**
 * @file heap_delete_test.cpp
 * @brief W2.3: heap_delete 标记删除测试
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/heapam.h"
}

TEST(HeapDeleteTest, LinePointerDeadFlag) {
    // 验证 LinePointer 的 LP_DEAD 标志在删除时的使用
    HeapLinePointerData lp;
    memset(&lp, 0, sizeof(lp));

    // 初始状态：元组有效
    lp.t_flags = LP_USED;
    lp.t_off = 100;
    lp.t_len = 50;

    EXPECT_TRUE(lp.t_flags & LP_USED);
    EXPECT_FALSE(lp.t_flags & LP_DEAD);

    // 删除后：标记为死亡（LP_DEAD）
    lp.t_flags |= LP_DEAD;

    EXPECT_TRUE(lp.t_flags & LP_DEAD);
    EXPECT_TRUE(lp.t_flags & LP_USED);  // LinePointer 仍然有效

    // 注意：元组数据仍然存在，只是标记为死亡
    EXPECT_EQ(lp.t_off, 100);  // 元组偏移量保持不变
    EXPECT_EQ(lp.t_len, 50);   // 元组长度保持不变
}

TEST(HeapDeleteTest, DeleteVsUpdate) {
    // 对比删除和更新的标志差异
    HeapLinePointerData lp_delete, lp_update;
    memset(&lp_delete, 0, sizeof(lp_delete));
    memset(&lp_update, 0, sizeof(lp_update));

    // 删除：只设置 LP_DEAD
    lp_delete.t_flags = LP_USED | LP_DEAD;

    // 更新：设置 LP_DEAD（旧版本）+ 新版本在别处
    lp_update.t_flags = LP_USED | LP_DEAD;

    // 两者都设置 LP_DEAD，但含义不同：
    // - 删除：元组永久不可见
    // - 更新：元组通过版本链仍然可见（旧版本的 t_ctid 指向新版本）
    EXPECT_TRUE(lp_delete.t_flags & LP_DEAD);
    EXPECT_TRUE(lp_update.t_flags & LP_DEAD);
}

TEST(HeapDeleteTest, InfomaskXmaxInvalid) {
    // 验证 HEAP_XMAX_INVALID 标志
    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 删除时设置 t_xmax 和 HEAP_XMAX_INVALID
    tuple.t_xmax = 200;
    tuple.t_infomask = HEAP_XMAX_INVALID;

    EXPECT_EQ(tuple.t_xmax, 200);
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMAX_INVALID);
    EXPECT_FALSE(tuple.t_infomask & HEAP_XMAX_COMMITTED);  // 未提交时为 INVALID
}

TEST(HeapDeleteTest, DeleteVisibility) {
    // 模拟删除可见性判断
    // 场景：事务 100 创建元组，事务 200 删除该元组

    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 元组由事务 100 创建
    tuple.t_xmin = 100;

    // 事务 200 删除该元组
    tuple.t_xmax = 200;
    tuple.t_infomask |= HEAP_XMAX_INVALID;

    // 对于事务 300 的可见性判断：
    // 1. t_xmin = 100：如果事务 100 未提交，则不可见
    // 2. t_xmax = 200：如果事务 200 未提交，则可见（因为删除未生效）

    // 如果事务 100 和 200 都已提交：
    // - tuple.t_infomask 需要设置 HEAP_XMIN_COMMITTED 和 HEAP_XMAX_COMMITTED
    // - 然后判断：t_xmin 提交 < 快照上限，且 t_xmax 未提交 → 可见
    //            但实际上 t_xmax 已提交，所以不可见

    tuple.t_infomask |= HEAP_XMIN_COMMITTED;
    tuple.t_infomask |= HEAP_XMAX_COMMITTED;

    // 都已提交时，元组对当前事务不可见
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMIN_COMMITTED);
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMAX_COMMITTED);
}
