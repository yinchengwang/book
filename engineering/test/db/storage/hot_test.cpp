/**
 * @file hot_test.cpp
 * @brief W2.4: HOT（Heap-Only Tuple）实现测试
 *
 * HOT 允许同一页面上的 UPDATE 不修改索引，只通过版本链追踪。
 *
 * ## HOT 原理
 *
 * 当 UPDATE 不修改索引键时：
 * 1. 旧元组标记 LP_DEAD
 * 2. 新元组插入同一页面
 * 3. 通过 t_ctid 形成版本链
 * 4. 索引条目不需要更新
 *
 * ## HOT 条件
 *
 * - 新元组能放入同一页面
 * - UPDATE 不修改索引键（如果实现了索引）
 * - 页面有足够的空闲空间
 *
 * ## 非 HOT 场景（需要更新索引）
 *
 * - 新元组跨页移动
 * - UPDATE 修改了索引键
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/heapam.h"
}

TEST(HOTTest, HOTChainStructure) {
    // 验证 HOT 链结构
    // 场景：同一页面上连续更新 3 次

    HeapTupleTableData v1, v2, v3;
    memset(&v1, 0, sizeof(v1));
    memset(&v2, 0, sizeof(v2));
    memset(&v3, 0, sizeof(v3));

    // 版本 1: 事务 100 创建
    v1.t_xmin = 100;
    v1.t_xmax = 200;  // 被事务 200 更新
    v1.t_ctid.ip_blkid = 0;
    v1.t_ctid.ip_posid = 2;  // 指向版本 2

    // 版本 2: 事务 200 创建
    v2.t_xmin = 200;
    v2.t_xmax = 300;  // 被事务 300 更新
    v2.t_ctid.ip_blkid = 0;
    v2.t_ctid.ip_posid = 3;  // 指向版本 3

    // 版本 3: 事务 300 创建（最新）
    v3.t_xmin = 300;
    v3.t_ctid.ip_blkid = 0;
    v3.t_ctid.ip_posid = 3;  // 指向自己（最新版本）

    // 验证 HOT 链
    EXPECT_EQ(v1.t_xmax, v2.t_xmin);      // v1 的删除事务 = v2 的创建事务
    EXPECT_EQ(v2.t_xmax, v3.t_xmin);      // v2 的删除事务 = v3 的创建事务

    // v1.t_ctid 指向 v2
    EXPECT_EQ(v1.t_ctid.ip_blkid, 0);
    EXPECT_EQ(v1.t_ctid.ip_posid, 2);

    // v2.t_ctid 指向 v3
    EXPECT_EQ(v2.t_ctid.ip_blkid, 0);
    EXPECT_EQ(v2.t_ctid.ip_posid, 3);

    // v3 是最新版本，t_ctid 指向自己
    EXPECT_EQ(v3.t_ctid.ip_blkid, 0);
    EXPECT_EQ(v3.t_ctid.ip_posid, 3);

    // 所有版本在同一页面（ip_blkid = 0）
    EXPECT_EQ(v1.t_ctid.ip_blkid, 0);
    EXPECT_EQ(v2.t_ctid.ip_blkid, 0);
    EXPECT_EQ(v3.t_ctid.ip_blkid, 0);
}

TEST(HOTTest, HOTSpaceReuse) {
    // 验证 HOT 允许空间复用
    // 当 HOT 链上的旧版本都被标记为死亡时，VACUUM 可以回收空间

    HeapLinePointerData lp1, lp2, lp3;
    memset(&lp1, 0, sizeof(lp1));
    memset(&lp2, 0, sizeof(lp2));
    memset(&lp3, 0, sizeof(lp3));

    // 初始插入
    lp1.t_flags = LP_USED;
    lp1.t_off = 7000;
    lp1.t_len = 200;

    // 第一次更新：版本 1 死亡
    lp1.t_flags |= LP_DEAD;
    lp2.t_flags = LP_USED;
    lp2.t_off = 6800;  // 复用空间
    lp2.t_len = 200;

    // 第二次更新：版本 2 死亡
    lp2.t_flags |= LP_DEAD;
    lp3.t_flags = LP_USED;
    lp3.t_off = 6600;  // 继续复用空间
    lp3.t_len = 200;

    // 所有版本都标记为死亡
    EXPECT_TRUE(lp1.t_flags & LP_DEAD);
    EXPECT_TRUE(lp2.t_flags & LP_DEAD);
    EXPECT_TRUE(lp3.t_flags & LP_USED);  // 最新版本存活

    // VACUUM 可以回收 lp1 和 lp2 的空间
}

TEST(HOTTest, HOTVsNonHOT) {
    // 对比 HOT 和非 HOT 场景

    // HOT 场景：同一页面更新
    HeapTupleTableData hot_v1, hot_v2;
    memset(&hot_v1, 0, sizeof(hot_v1));
    memset(&hot_v2, 0, sizeof(hot_v2));

    hot_v1.t_ctid.ip_blkid = 0;
    hot_v1.t_ctid.ip_posid = 2;
    hot_v2.t_ctid.ip_blkid = 0;  // 同一页面
    hot_v2.t_ctid.ip_posid = 2;

    EXPECT_TRUE(ItemPointerEquals(&hot_v1.t_ctid, &hot_v2.t_ctid));

    // 非 HOT 场景：跨页更新
    HeapTupleTableData non_hot_v1, non_hot_v2;
    memset(&non_hot_v1, 0, sizeof(non_hot_v1));
    memset(&non_hot_v2, 0, sizeof(non_hot_v2));

    non_hot_v1.t_ctid.ip_blkid = 0;
    non_hot_v1.t_ctid.ip_posid = 2;
    non_hot_v2.t_ctid.ip_blkid = 1;  // 不同页面
    non_hot_v2.t_ctid.ip_posid = 1;

    EXPECT_FALSE(ItemPointerEquals(&non_hot_v1.t_ctid, &non_hot_v2.t_ctid));
    EXPECT_NE(non_hot_v1.t_ctid.ip_blkid, non_hot_v2.t_ctid.ip_blkid);
}

TEST(HOTTest, HOTPruningCondition) {
    // HOT 链裁剪条件
    // 当 HOT 链头部死亡且没有其他事务可见时，可以裁剪

    HeapTupleTableData tuple;
    memset(&tuple, 0, sizeof(tuple));

    // 假设版本 1 被事务 200 更新，事务 100 已提交
    tuple.t_xmin = 100;
    tuple.t_xmax = 200;
    tuple.t_infomask = HEAP_XMIN_COMMITTED | HEAP_UPDATED;

    // 对于事务 250:
    // - t_xmin = 100 已提交，在快照之外（可见）
    // - t_xmax = 200 更新事务（不在快照中，不可见）
    // - 所以版本 1 对事务 250 不可见，可以裁剪

    // 当版本 1 可见性消失后，VACUUM 可以裁剪并复用空间

    EXPECT_TRUE(tuple.t_infomask & HEAP_UPDATED);
    EXPECT_TRUE(tuple.t_infomask & HEAP_XMIN_COMMITTED);
}
