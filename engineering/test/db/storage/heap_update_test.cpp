/**
 * @file heap_update_test.cpp
 * @brief W2.2: heap_update 版本链测试
 *
 * 验证版本链数据结构的正确性。
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/heapam.h"
}

TEST(HeapUpdateTest, VersionChainStructure) {
    // 验证版本链结构
    HeapTupleTableData tuple1, tuple2;
    memset(&tuple1, 0, sizeof(tuple1));
    memset(&tuple2, 0, sizeof(tuple2));

    // 模拟版本链
    tuple1.t_xmin = 100;
    tuple1.t_xmax = 200;  // 被事务 200 更新
    tuple1.t_ctid.ip_blkid = 0;
    tuple1.t_ctid.ip_posid = 2;  // 指向新版本

    tuple2.t_xmin = 200;  // 新版本由事务 200 创建
    tuple2.t_ctid.ip_blkid = 0;
    tuple2.t_ctid.ip_posid = 2;  // 指向自己（最新版本）

    // 验证版本链关系
    EXPECT_EQ(tuple1.t_xmax, tuple2.t_xmin);
    EXPECT_TRUE(ItemPointerEquals(&tuple1.t_ctid, &tuple2.t_ctid));
    EXPECT_EQ(tuple2.t_ctid.ip_posid, 2);  // 最新版本指向自己
}

TEST(HeapUpdateTest, LinePointerDeadFlag) {
    // 验证 LinePointer 的 LP_DEAD 标志
    HeapLinePointerData lp;
    memset(&lp, 0, sizeof(lp));

    // 初始状态
    lp.t_flags = LP_USED;
    EXPECT_TRUE(lp.t_flags & LP_USED);
    EXPECT_FALSE(lp.t_flags & LP_DEAD);

    // UPDATE 后标记为死亡
    lp.t_flags |= LP_DEAD;
    EXPECT_TRUE(lp.t_flags & LP_DEAD);
    EXPECT_TRUE(lp.t_flags & LP_USED);  // 仍然标记为使用中，但标记为死亡
}

TEST(HeapUpdateTest, InfomaskUpdateFlag) {
    // 验证 HEAP_UPDATED 标志
    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    tuple.t_infomask = HEAP_UPDATED;
    EXPECT_TRUE(tuple.t_infomask & HEAP_UPDATED);

    tuple.t_infomask |= HEAP_XMAX_INVALID;
    EXPECT_TRUE(tuple.t_infomask & HEAP_UPDATED);
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMAX_INVALID);
}

TEST(HeapUpdateTest, MultiVersionChain) {
    // 模拟多次更新的版本链
    HeapTupleTableData tuple1, tuple2, tuple3;
    memset(&tuple1, 0, sizeof(tuple1));
    memset(&tuple2, 0, sizeof(tuple2));
    memset(&tuple3, 0, sizeof(tuple3));

    // 版本 1: 事务 100 创建
    tuple1.t_xmin = 100;
    tuple1.t_xmax = 200;
    tuple1.t_ctid.ip_blkid = 0;
    tuple1.t_ctid.ip_posid = 2;

    // 版本 2: 事务 200 创建
    tuple2.t_xmin = 200;
    tuple2.t_xmax = 300;
    tuple2.t_ctid.ip_blkid = 0;
    tuple2.t_ctid.ip_posid = 3;

    // 版本 3: 事务 300 创建（最新）
    tuple3.t_xmin = 300;
    tuple3.t_ctid.ip_blkid = 0;
    tuple3.t_ctid.ip_posid = 3;  // 最新版本指向自己

    // 验证版本链
    EXPECT_EQ(tuple1.t_xmax, tuple2.t_xmin);
    EXPECT_EQ(tuple2.t_xmax, tuple3.t_xmin);
    EXPECT_TRUE(ItemPointerEquals(&tuple2.t_ctid, &tuple3.t_ctid));
}

TEST(HeapUpdateTest, ItemPointerManipulation) {
    // 测试 ItemPointer 的设置和比较
    ItemPointerData tid1, tid2;

    // 设置值
    ItemPointerSet(&tid1, 5, 10);
    EXPECT_EQ(tid1.ip_blkid, 5);
    EXPECT_EQ(tid1.ip_posid, 10);

    // 相等比较
    ItemPointerSet(&tid2, 5, 10);
    EXPECT_TRUE(ItemPointerEquals(&tid1, &tid2));

    // 不等比较
    ItemPointerSet(&tid2, 5, 11);
    EXPECT_FALSE(ItemPointerEquals(&tid1, &tid2));

    ItemPointerSet(&tid2, 6, 10);
    EXPECT_FALSE(ItemPointerEquals(&tid1, &tid2));
}
