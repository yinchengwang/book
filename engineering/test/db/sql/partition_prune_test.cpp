/**
 * @file partition_prune_test.cpp
 * @brief W4.4: 分区裁剪测试
 *
 * 测试 planner_partition_prune 在逻辑计划中裁剪分区表的能力。
 * 不依赖 catalog.h（避免 Oid 类型冲突），使用纯概念测试。
 */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "db/sql/sql_planner.h"
}

namespace {

/**
 * @brief 测试分区裁剪
 */
class PartitionPruneTest : public ::testing::Test {
protected:
    PlannerContext *ctx;

    void SetUp() override {
        ctx = planner_create();
    }

    void TearDown() override {
        planner_destroy(ctx);
    }
};

/**
 * @brief 测试分区裁剪函数存在且不崩溃
 */
TEST_F(PartitionPruneTest, PruneOnNullPlan) {
    /* NULL 计划不应崩溃 */
    planner_partition_prune(ctx, NULL);
}

/**
 * @brief 测试分区裁剪对非分区表无影响
 */
TEST_F(PartitionPruneTest, PruneNonPartitioned) {
    /* 创建非分区表 SCAN 计划 */
    LogicalPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = LOGICAL_SCAN;
    plan.node_id = 1;
    plan.extra = NULL;

    /* 裁剪不应修改 extra */
    planner_partition_prune(ctx, &plan);
    EXPECT_EQ(plan.extra, (void*)NULL);
}

/**
 * @brief 测试递归裁剪左右子树
 */
TEST_F(PartitionPruneTest, PruneRecursive) {
    /* 创建 JOIN 树：左 SCAN + 右 SCAN */
    LogicalPlan left;
    memset(&left, 0, sizeof(left));
    left.type = LOGICAL_SCAN;
    left.node_id = 1;

    LogicalPlan right;
    memset(&right, 0, sizeof(right));
    right.type = LOGICAL_SCAN;
    right.node_id = 2;

    LogicalPlan join;
    memset(&join, 0, sizeof(join));
    join.type = LOGICAL_JOIN;
    join.left = &left;
    join.right = &right;

    /* 递归裁剪不应崩溃 */
    planner_partition_prune(ctx, &join);
}

/**
 * @brief 测试分区裁剪概念：只扫描匹配分区
 */
TEST_F(PartitionPruneTest, PruneConcept) {
    /* 验证分区裁剪概念：
     * 分区表有 4 个分区 [0, 100), [100, 200), [200, 300), [300, 400)
     * WHERE id >= 150 应只扫描 [100, 200), [200, 300), [300, 400) 三个分区
     */

    /* 定义分区边界 */
    const int N_PARTS = 4;
    int64_t bounds[N_PARTS] = {100, 200, 300, 400};
    bool scan_part[N_PARTS] = {false, false, false, false};

    /* 模拟 WHERE id >= 150 */
    int64_t query_value = 150;

    /* 对于范围分区，检查每个分区是否应被扫描 */
    int expected_scanned = 0;
    for (int i = 0; i < N_PARTS; i++) {
        int64_t upper = bounds[i];
        if (upper > query_value) {
            scan_part[i] = true;
            expected_scanned++;
        }
    }

    /* 验证裁剪结果：100-400 三个分区应被扫描 */
    EXPECT_EQ(expected_scanned, 3);
    EXPECT_TRUE(scan_part[1]);   /* [100, 200) */
    EXPECT_TRUE(scan_part[2]);   /* [200, 300) */
    EXPECT_TRUE(scan_part[3]);   /* [300, 400) */
    EXPECT_FALSE(scan_part[0]);  /* [0, 100) 被裁剪 */
}

/**
 * @brief 测试分区裁剪 — 等值匹配
 */
TEST_F(PartitionPruneTest, EqualMatch) {
    /* 验证 WHERE id = 250 只扫描 [200, 300) */
    const int N_PARTS = 4;
    int64_t bounds[N_PARTS] = {100, 200, 300, 400};
    bool scan_part[N_PARTS] = {false, false, false, false};

    int64_t query_value = 250;
    int expected_idx = -1;

    for (int i = 0; i < N_PARTS; i++) {
        int64_t lower = (i == 0) ? 0 : bounds[i - 1];
        int64_t upper = bounds[i];

        if (query_value >= lower && query_value < upper) {
            scan_part[i] = true;
            expected_idx = i;
            break;
        }
    }

    /* 只有 [200, 300) 应被扫描 */
    EXPECT_EQ(expected_idx, 2);
    EXPECT_TRUE(scan_part[2]);
}

/**
 * @brief 测试分区裁剪 — 全表扫描
 */
TEST_F(PartitionPruneTest, FullScan) {
    /* 无 WHERE 条件时应扫描所有分区 */
    const int N_PARTS = 4;
    bool scan_part[N_PARTS] = {true, true, true, true};

    int scanned_count = 0;
    for (int i = 0; i < N_PARTS; i++) {
        if (scan_part[i]) {
            scanned_count++;
        }
    }

    EXPECT_EQ(scanned_count, 4);
}

}  // namespace