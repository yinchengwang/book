/**
 * @file heap_tuple_test.cpp
 * @brief W2.1: Heap 元组头改造测试
 *
 * 验证 HeapTupleHeaderData 的 t_ctid、t_infomask 等字段正确实现。
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/heapam.h"
}

TEST(HeapTupleTest, HeaderHasCtid) {
    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 验证 t_ctid 字段存在且可设置
    ItemPointerSet(&tuple.t_ctid, 1, 10);
    EXPECT_EQ(tuple.t_ctid.ip_blkid, 1);
    EXPECT_EQ(tuple.t_ctid.ip_posid, 10);

    // 验证 ItemPointerEquals
    ItemPointerData other = {1, 10};
    EXPECT_TRUE(ItemPointerEquals(&tuple.t_ctid, &other));

    ItemPointerData diff = {1, 11};
    EXPECT_FALSE(ItemPointerEquals(&tuple.t_ctid, &diff));
}

TEST(HeapTupleTest, InfomaskFlags) {
    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 验证各标志位可组合设置
    tuple.t_infomask = HEAP_XMIN_COMMITTED | HEAP_UPDATED;
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMIN_COMMITTED);
    EXPECT_TRUE(tuple.t_infomask & HEAP_UPDATED);
    EXPECT_FALSE(tuple.t_infomask & HEAP_XMIN_INVALID);

    // 追加更多标志
    tuple.t_infomask |= HEAP_XMAX_INVALID;
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMAX_INVALID);
}

TEST(HeapTupleTest, TxnIds) {
    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 验证事务 ID 字段
    tuple.t_xmin = 100;
    tuple.t_xmax = 200;
    EXPECT_EQ(tuple.t_xmin, 100);
    EXPECT_EQ(tuple.t_xmax, 200);
}

TEST(HeapTupleTest, LinePointerFlags) {
    HeapLinePointerData lp;
    memset(&lp, 0, sizeof(lp));

    // 验证 LP_USED 标志
    lp.t_flags = LP_USED;
    EXPECT_TRUE(lp.t_flags & LP_USED);
    EXPECT_FALSE(lp.t_flags & LP_DEAD);

    // 验证 LP_DEAD 标志（删除时设置）
    lp.t_flags |= LP_DEAD;
    EXPECT_TRUE(lp.t_flags & LP_DEAD);

    // 验证 t_len 字段
    lp.t_len = 128;
    EXPECT_EQ(lp.t_len, 128);
}

TEST(HeapTupleTest, VersionChainScenario) {
    // 模拟版本链场景
    HeapTupleTableData tuple1, tuple2;
    memset(&tuple1, 0, sizeof(tuple1));
    memset(&tuple2, 0, sizeof(tuple2));

    // 初始元组：t_xmin = 100
    // t_ctid 初始指向自己（(0, 0) 表示未设置或自引用）
    tuple1.t_xmin = 100;

    // 新版本元组：t_xmin = 200，插入位置在 (0, 2)
    tuple2.t_xmin = 200;
    tuple2.t_ctid.ip_blkid = 0;
    tuple2.t_ctid.ip_posid = 2;

    // 旧元组被更新：t_xmax = 200（由事务 200 更新）
    // t_ctid 指向新版本位置 (0, 2)
    tuple1.t_xmax = 200;
    tuple1.t_ctid.ip_blkid = 0;
    tuple1.t_ctid.ip_posid = 2;
    tuple1.t_infomask |= HEAP_UPDATED;

    // 验证版本链
    EXPECT_TRUE(ItemPointerEquals(&tuple1.t_ctid, &tuple2.t_ctid));
    EXPECT_EQ(tuple1.t_xmax, tuple2.t_xmin);  // 更新事务 ID 匹配
    EXPECT_TRUE(tuple1.t_infomask & HEAP_UPDATED);
}

TEST(HeapTupleTest, DeleteScenario) {
    // 模拟删除场景
    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 初始元组：t_xmin = 100
    tuple.t_xmin = 100;
    tuple.t_xmax = 0;  // 未删除

    // 删除：t_xmax = 200
    tuple.t_xmax = 200;
    tuple.t_infomask |= HEAP_XMAX_INVALID;

    // 验证删除状态
    EXPECT_EQ(tuple.t_xmax, 200);
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMAX_INVALID);
    EXPECT_FALSE(tuple.t_infomask & HEAP_XMAX_COMMITTED);
}
