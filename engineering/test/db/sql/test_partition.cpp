/**
 * @file test_partition.cpp
 * @brief 分区表路由单元测试
 *
 * 测试范围：
 *   - 分区描述符创建/销毁
 *   - 范围分区路由
 *   - 列表分区路由
 *   - 哈希分区路由
 *   - 边界值比较
 *   - NULL 分区检测
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/partition.h"
}

namespace {

/* ========================================================================
 * 分区描述符测试
 * ======================================================================== */

/**
 * @brief 测试分区描述符创建
 */
TEST(PartitionTest, DescriptorCreate) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 1234, 3);
    ASSERT_NE(partdesc, nullptr);
    EXPECT_EQ(partdesc->type, T_PartitionDesc);
    EXPECT_EQ(partdesc->parttype, PARTITION_TYPE_RANGE);
    EXPECT_EQ(partdesc->relid, 1234);
    EXPECT_EQ(partdesc->nparts, 3);
    EXPECT_EQ(partdesc->partnatts, 1);

    DestroyPartitionDesc(partdesc);
}

/**
 * @brief 测试分区描述符销毁（NULL 安全）
 */
TEST(PartitionTest, DescriptorNullDestroy) {
    /* NULL 输入不应崩溃 */
    DestroyPartitionDesc(NULL);

    /* 边界值测试 */
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_LIST, 5678, 0);
    ASSERT_NE(partdesc, nullptr);
    EXPECT_EQ(partdesc->nparts, 0);

    DestroyPartitionDesc(partdesc);
}

/**
 * @brief 测试获取分区类型
 */
TEST(PartitionTest, GetPartitionType) {
    PartitionDesc *range_desc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 1, 2);
    ASSERT_NE(range_desc, nullptr);
    EXPECT_EQ(GetPartitionType(range_desc), PARTITION_TYPE_RANGE);
    DestroyPartitionDesc(range_desc);

    PartitionDesc *list_desc = CreatePartitionDesc(
        PARTITION_TYPE_LIST, 2, 2);
    ASSERT_NE(list_desc, nullptr);
    EXPECT_EQ(GetPartitionType(list_desc), PARTITION_TYPE_LIST);
    DestroyPartitionDesc(list_desc);

    PartitionDesc *hash_desc = CreatePartitionDesc(
        PARTITION_TYPE_HASH, 3, 2);
    ASSERT_NE(hash_desc, nullptr);
    EXPECT_EQ(GetPartitionType(hash_desc), PARTITION_TYPE_HASH);
    DestroyPartitionDesc(hash_desc);
}

/**
 * @brief 测试获取分区数量
 */
TEST(PartitionTest, GetPartitionCount) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 1, 5);
    ASSERT_NE(partdesc, nullptr);
    EXPECT_EQ(GetPartitionCount(partdesc), 5);
    DestroyPartitionDesc(partdesc);

    /* NULL 描述符 */
    EXPECT_EQ(GetPartitionCount(NULL), 0);
}

/* ========================================================================
 * 范围分区路由测试
 * ======================================================================== */

/**
 * @brief 测试范围分区路由 - 基本场景
 */
TEST(PartitionTest, RangeRouting) {
    /* 创建分区描述符：4 个分区，边界为 [100, 200, 300] */
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 1000, 4);
    ASSERT_NE(partdesc, nullptr);

    /* 创建边界信息：分区 0: <100, 分区 1: <200, 分区 2: <300, 分区 3: >=300 */
    Datum bounds[] = {100, 200, 300};
    int indexes[] = {0, 1, 2};

    partdesc->boundinfo = CreatePartitionBoundInfo(
        PARTITION_TYPE_RANGE, 3, bounds, indexes);
    ASSERT_NE(partdesc->boundinfo, nullptr);

    /* 测试各分区边界 */
    Datum key;
    key = 50;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 100;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 150;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 200;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 250;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = 300;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = 350;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 3);
    key = 1000;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 3);

    DestroyPartitionDesc(partdesc);
}

/**
 * @brief 测试范围分区路由 - 负数边界
 */
TEST(PartitionTest, RangeRoutingNegative) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 1001, 3);
    ASSERT_NE(partdesc, nullptr);

    /* 负数边界：分区 0: <0, 分区 1: <100, 分区 2: >=100 */
    Datum bounds[] = {0, 100};
    int indexes[] = {0, 1};

    partdesc->boundinfo = CreatePartitionBoundInfo(
        PARTITION_TYPE_RANGE, 2, bounds, indexes);
    ASSERT_NE(partdesc->boundinfo, nullptr);

    Datum key;
    key = (Datum)(-100);
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = (Datum)(-1);
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 0;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 50;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 100;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 200;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);

    DestroyPartitionDesc(partdesc);
}

/* ========================================================================
 * 列表分区路由测试
 * ======================================================================== */

/**
 * @brief 测试列表分区路由 - 基本场景
 */
TEST(PartitionTest, ListRouting) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_LIST, 2000, 3);
    ASSERT_NE(partdesc, nullptr);

    /* 列表分区：分区 0: (1), 分区 1: (2,3), 分区 2: (4,5,6) */
    Datum values[] = {1, 2, 3, 4, 5, 6};
    int indexes[] = {0, 1, 1, 2, 2, 2};

    partdesc->boundinfo = CreatePartitionBoundInfo(
        PARTITION_TYPE_LIST, 6, values, indexes);
    ASSERT_NE(partdesc->boundinfo, nullptr);

    /* 验证路由结果 */
    Datum key;
    key = 1;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 2;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 3;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 4;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = 5;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = 6;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);

    /* 不存在的值 */
    key = 7;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), -1);
    key = 0;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), -1);

    DestroyPartitionDesc(partdesc);
}

/**
 * @brief 测试列表分区路由 - 字符串枚举
 */
TEST(PartitionTest, ListRoutingStrings) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_LIST, 2001, 3);
    ASSERT_NE(partdesc, nullptr);

    /* 字符串分区（使用指针值模拟） */
    Datum values[] = {(Datum)"A", (Datum)"B", (Datum)"C"};
    int indexes[] = {0, 1, 2};

    partdesc->boundinfo = CreatePartitionBoundInfo(
        PARTITION_TYPE_LIST, 3, values, indexes);
    ASSERT_NE(partdesc->boundinfo, nullptr);

    /* 由于是字符串比较，需要线性搜索 */
    Datum key;
    key = (Datum)"A";
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = (Datum)"B";
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = (Datum)"C";
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = (Datum)"D";
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), -1);

    DestroyPartitionDesc(partdesc);
}

/* ========================================================================
 * 哈希分区路由测试
 * ======================================================================== */

/**
 * @brief 测试哈希分区路由 - 基本场景
 */
TEST(PartitionTest, HashRouting) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_HASH, 3000, 4);
    ASSERT_NE(partdesc, nullptr);

    Datum key;
    key = 0;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 4;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 8;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);
    key = 1;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 5;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    key = 2;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = 6;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    key = 3;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 3);
    key = 7;
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 3);

    DestroyPartitionDesc(partdesc);
}

/**
 * @brief 测试哈希分区路由 - 负数处理
 */
TEST(PartitionTest, HashRoutingNegative) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_HASH, 3001, 3);
    ASSERT_NE(partdesc, nullptr);

    /* 负数取模测试 */
    Datum key;
    /* -1 % 3 = -1, 修正后 = 2 */
    key = (Datum)(int64_t)(-1);
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    /* -4 % 3 = -1, 修正后 = 2 */
    key = (Datum)(int64_t)(-4);
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 2);
    /* -2 % 3 = -2, 修正后 = 1 */
    key = (Datum)(int64_t)(-2);
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 1);
    /* -3 % 3 = 0 */
    key = (Datum)(int64_t)(-3);
    EXPECT_EQ(PartitionRouting(partdesc, &key, 1), 0);

    DestroyPartitionDesc(partdesc);
}

/* ========================================================================
 * 边界值比较测试
 * ======================================================================== */

/**
 * @brief 测试边界值比较 - 整数
 */
TEST(PartitionTest, BoundCompare) {
    /* 整数比较 */
    EXPECT_EQ(PartitionBoundCmp((Datum)10, (Datum)20, true), -1);
    EXPECT_EQ(PartitionBoundCmp((Datum)20, (Datum)10, true), 1);
    EXPECT_EQ(PartitionBoundCmp((Datum)15, (Datum)15, true), 0);

    /* 负数比较 */
    EXPECT_EQ(PartitionBoundCmp((Datum)(int64_t)(-5), (Datum)5, true), -1);
    EXPECT_EQ(PartitionBoundCmp((Datum)5, (Datum)(int64_t)(-5), true), 1);
    EXPECT_EQ(PartitionBoundCmp((Datum)(int64_t)(-10), (Datum)(int64_t)(-10), true), 0);
}

/**
 * @brief 测试边界值比较 - 浮点数
 */
TEST(PartitionTest, BoundCompareFloat) {
    /* 浮点比较 */
    EXPECT_EQ(PartitionBoundCmp((Datum)(uint64_t)3, (Datum)(uint64_t)4, false), -1);
    EXPECT_EQ(PartitionBoundCmp((Datum)(uint64_t)4, (Datum)(uint64_t)3, false), 1);
    EXPECT_EQ(PartitionBoundCmp((Datum)(uint64_t)3, (Datum)(uint64_t)3, false), 0);
}

/* ========================================================================
 * NULL 分区测试
 * ======================================================================== */

/**
 * @brief 测试 NULL 分区检测
 */
TEST(PartitionTest, NullPartition) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 4000, 2);
    ASSERT_NE(partdesc, nullptr);

    Datum key;
    /* NULL 值检测（使用 0 模拟 NULL） */
    key = 0;
    EXPECT_TRUE(IsNullPartition(partdesc, &key));
    key = 1;
    EXPECT_FALSE(IsNullPartition(partdesc, &key));
    key = 100;
    EXPECT_FALSE(IsNullPartition(partdesc, &key));

    /* NULL 参数测试 */
    EXPECT_FALSE(IsNullPartition(NULL, &key));
    EXPECT_FALSE(IsNullPartition(partdesc, NULL));

    DestroyPartitionDesc(partdesc);
}

/* ========================================================================
 * 分区路由上下文测试
 * ======================================================================== */

/**
 * @brief 测试分区路由上下文创建
 */
TEST(PartitionTest, RoutingContextCreate) {
    PartitionDesc *partdesc = CreatePartitionDesc(
        PARTITION_TYPE_RANGE, 5000, 3);
    ASSERT_NE(partdesc, nullptr);

    PartitionRoutingCtx *prctx = CreatePartitionRoutingCtx(partdesc, NULL);
    ASSERT_NE(prctx, nullptr);
    EXPECT_EQ(prctx->type, T_PartitionRoutingCtx);
    EXPECT_EQ(prctx->partdesc, partdesc);
    EXPECT_EQ(prctx->nparts, 3);
    EXPECT_EQ(prctx->estate, nullptr);

    DestroyPartitionRoutingCtx(prctx);
    DestroyPartitionDesc(partdesc);
}

/**
 * @brief 测试分区路由上下文销毁（NULL 安全）
 */
TEST(PartitionTest, RoutingContextNullDestroy) {
    /* NULL 输入不应崩溃 */
    DestroyPartitionRoutingCtx(NULL);
}

/* ========================================================================
 * 边界条件测试
 * ======================================================================== */

/**
 * @brief 测试无效参数
 */
TEST(PartitionTest, InvalidParameters) {
    Datum key = 10;

    /* 无效分区数 */
    EXPECT_EQ(PartitionHashRouting(0, key), -1);
    EXPECT_EQ(PartitionHashRouting(-1, key), -1);

    /* 无效边界信息 */
    PartitionBoundInfo *empty_bound = CreatePartitionBoundInfo(
        PARTITION_TYPE_RANGE, 0, NULL, NULL);
    ASSERT_NE(empty_bound, nullptr);
    EXPECT_EQ(PartitionRangeRouting(empty_bound, key), -1);
    DestroyPartitionBoundInfo(empty_bound);

    /* 无效分区描述符 */
    EXPECT_EQ(PartitionRouting(NULL, &key, 1), -1);
    EXPECT_EQ(PartitionRouting(NULL, NULL, 0), -1);
}

}  /* namespace */
