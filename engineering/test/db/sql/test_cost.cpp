/**
 * @file test_cost.cpp
 * @brief Selinger 代价模型单元测试
 *
 * 测试范围：
 * - 默认代价参数
 * - SeqScan 代价计算
 * - IndexScan 代价计算
 * - HashJoin 代价计算
 * - Sort 代价计算
 * - Agg 代价计算
 * - 选择率估算
 * - 分组数估算
 */
#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "db/sql/cost.h"
}

namespace {

/* ========================================================================
 * 代价参数测试
 * ======================================================================== */

TEST(CostTest, DefaultCostParams) {
    CostParams params = get_default_cost_params();

    /* 验证默认值 */
    EXPECT_DOUBLE_EQ(params.cpu_tuple_cost, 0.01);
    EXPECT_DOUBLE_EQ(params.cpu_index_tuple_cost, 0.005);
    EXPECT_DOUBLE_EQ(params.cpu_operator_cost, 0.0025);
    EXPECT_DOUBLE_EQ(params.random_page_cost, 4.0);
    EXPECT_DOUBLE_EQ(params.seq_page_cost, 1.0);
    EXPECT_DOUBLE_EQ(params.parallel_tuple_cost, 0.1);
    EXPECT_DOUBLE_EQ(params.parallel_setup_cost, 1000.0);
}

/* ========================================================================
 * SeqScan 代价测试
 * ======================================================================== */

TEST(CostTest, SeqScanCostBasic) {
    /* 100 页，10000 行 */
    double relpages = 100;
    double reltuples = 10000;

    Cost cost = compute_seqscan_cost(relpages, reltuples);

    /* 预期代价：seq_page_cost * relpages + cpu_tuple_cost * reltuples */
    double expected = 1.0 * 100 + 0.01 * 10000;
    EXPECT_DOUBLE_EQ(cost, expected);
}

TEST(CostTest, SeqScanCostNullStats) {
    Cost cost = compute_seqscan_cost(0, 0);
    EXPECT_DOUBLE_EQ(cost, 0.0);
}

TEST(CostTest, SeqScanCostLargeTable) {
    /* 大表：10000 页，1M 行 */
    double relpages = 10000;
    double reltuples = 1000000;

    Cost cost = compute_seqscan_cost(relpages, reltuples);

    /* 预期代价 */
    double expected = 1.0 * 10000 + 0.01 * 1000000;
    EXPECT_DOUBLE_EQ(cost, expected);

    /* 验证代价随规模线性增长 */
    EXPECT_GT(cost, 10000.0);
}

/* ========================================================================
 * IndexScan 代价测试
 * ======================================================================== */

TEST(CostTest, IndexScanCostBasic) {
    double relpages = 100;
    double reltuples = 10000;
    double ndistinct = 100;  /* 100 个不同值 */

    /* 选择率 = 1/100 = 0.01 */
    double selectivity = 1.0 / ndistinct;

    Cost cost = compute_indexscan_cost(relpages, reltuples, selectivity);

    /* 预期代价：random_page_cost * pages + cpu_index_tuple_cost * tuples */
    double expected_pages = 100 * selectivity;
    if (expected_pages < 1.0) expected_pages = 1.0;

    double expected_tuples = 10000 * selectivity;
    double expected = 4.0 * expected_pages + 0.005 * expected_tuples;

    EXPECT_DOUBLE_EQ(cost, expected);
}

TEST(CostTest, IndexScanCostNullStats) {
    Cost cost = compute_indexscan_cost(0, 0, 0.5);
    EXPECT_DOUBLE_EQ(cost, 0.0);
}

TEST(CostTest, IndexScanCostLowSelectivity) {
    /* ndistinct = 2，选择率 = 0.5（高选择率，不适合索引扫描） */
    double relpages = 100;
    double reltuples = 10000;
    double selectivity = 0.5;

    Cost cost = compute_indexscan_cost(relpages, reltuples, selectivity);

    /* 验证高选择率情况下代价较高 */
    EXPECT_GT(cost, 0.0);
}

/* ========================================================================
 * HashJoin 代价测试
 * ======================================================================== */

TEST(CostTest, HashJoinCostBasic) {
    double outer_rows = 1000;
    double inner_rows = 500;

    Cost cost = compute_hashjoin_cost(outer_rows, inner_rows);

    /* 构建代价 + 探测代价 */
    double build_cost = 0.01 * inner_rows + 0.0025 * inner_rows;
    double probe_cost = 0.01 * outer_rows + 0.0025 * outer_rows;
    double expected = build_cost + probe_cost;

    EXPECT_DOUBLE_EQ(cost, expected);
}

TEST(CostTest, HashJoinCostNullStats) {
    EXPECT_DOUBLE_EQ(compute_hashjoin_cost(0, 1000), 0.0);
    EXPECT_DOUBLE_EQ(compute_hashjoin_cost(1000, 0), 0.0);
}

TEST(CostTest, HashJoinCostAsymmetric) {
    /* 外表大，内表小 */
    Cost cost_large_outer = compute_hashjoin_cost(10000, 100);

    /* 外表小，内表大 */
    Cost cost_small_outer = compute_hashjoin_cost(100, 10000);

    /* 代价应该接近（因为总工作量相同） */
    EXPECT_NEAR(cost_large_outer, cost_small_outer, 100.0);
}

/* ========================================================================
 * Sort 代价测试
 * ======================================================================== */

TEST(CostTest, SortCostBasic) {
    double num_tuples = 10000;

    Cost cost = compute_sort_cost(num_tuples);

    /* 排序代价：O(n log n) */
    double comparisons = num_tuples * log2(num_tuples + 1);
    double expected = 0.0025 * comparisons + 0.01 * num_tuples;

    EXPECT_DOUBLE_EQ(cost, expected);
}

TEST(CostTest, SortCostZero) {
    EXPECT_DOUBLE_EQ(compute_sort_cost(0), 0.0);
}

TEST(CostTest, SortCostSmall) {
    /* 小数据集排序 */
    Cost cost_small = compute_sort_cost(100);
    Cost cost_large = compute_sort_cost(10000);

    /* 验证代价随规模增长 */
    EXPECT_LT(cost_small, cost_large);
}

/* ========================================================================
 * Agg 代价测试
 * ======================================================================== */

TEST(CostTest, AggCostBasic) {
    double num_groups = 100;

    Cost cost = compute_agg_cost(num_groups);

    /* 聚合代价：每组一个哈希表条目 */
    double expected = 0.01 * num_groups + 0.0025 * num_groups;

    EXPECT_DOUBLE_EQ(cost, expected);
}

TEST(CostTest, AggCostZeroGroups) {
    /* 0 分组时默认为 1 */
    Cost cost = compute_agg_cost(0);
    EXPECT_GT(cost, 0.0);
}

TEST(CostTest, AggCostLargeGroups) {
    Cost cost_small = compute_agg_cost(10);
    Cost cost_large = compute_agg_cost(10000);

    EXPECT_LT(cost_small, cost_large);
}

/* ========================================================================
 * 选择率估算测试
 * ======================================================================== */

TEST(CostTest, EstimateSelectivityBasic) {
    AttStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.ndistinct = 100;

    double selectivity = estimate_selectivity(&stats, (struct Expr*)0x1);  /* 传入非 NULL 的 Expr */

    /* 等值条件选择率 = 1 / ndistinct */
    EXPECT_DOUBLE_EQ(selectivity, 1.0 / 100);
}

TEST(CostTest, EstimateSelectivityNull) {
    EXPECT_DOUBLE_EQ(estimate_selectivity(NULL, NULL), 0.5);
}

TEST(CostTest, EstimateSelectivityNoDistinct) {
    AttStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.ndistinct = 0;

    /* ndistinct = 0 时使用默认选择率 */
    EXPECT_DOUBLE_EQ(estimate_selectivity(&stats, NULL), 0.5);
}

/* ========================================================================
 * 分组数估算测试
 * ======================================================================== */

TEST(CostTest, EstimateNumGroupsBasic) {
    RelStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.reltuples = 10000;

    AttStats key1;
    memset(&key1, 0, sizeof(key1));
    key1.ndistinct = 10;

    AttStats key2;
    memset(&key2, 0, sizeof(key2));
    key2.ndistinct = 20;

    AttStats *group_keys[] = { &key1, &key2 };

    double num_groups = estimate_num_groups(&stats, group_keys, 2);

    /* 分组数 = min(reltuples, Π ndistinct) = min(10000, 10*20) = 200 */
    EXPECT_DOUBLE_EQ(num_groups, 200.0);
}

TEST(CostTest, EstimateNumGroupsExceedTuples) {
    RelStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.reltuples = 100;

    AttStats key1;
    memset(&key1, 0, sizeof(key1));
    key1.ndistinct = 100;

    AttStats key2;
    memset(&key2, 0, sizeof(key2));
    key2.ndistinct = 100;

    AttStats *group_keys[] = { &key1, &key2 };

    double num_groups = estimate_num_groups(&stats, group_keys, 2);

    /* 分组数不超过总行数 */
    EXPECT_DOUBLE_EQ(num_groups, 100.0);
}

TEST(CostTest, EstimateNumGroupsNull) {
    EXPECT_DOUBLE_EQ(estimate_num_groups(NULL, NULL, 0), 1.0);
}

TEST(CostTest, EstimateNumGroupsNoKeys) {
    RelStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.reltuples = 10000;

    /* 无分组键时，分组数 = 总行数 */
    EXPECT_DOUBLE_EQ(estimate_num_groups(&stats, NULL, 0), 10000.0);
}

}  // namespace
