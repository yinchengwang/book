/**
 * @file benchmark_aggregate.cpp
 * @brief HashAgg 执行器节点性能基准测试
 *
 * 测试场景：
 * 1. CountStar - COUNT(*) 聚合
 * 2. GroupBy100Groups - GROUP BY 100 组
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "benchmark_config.h"

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeHashagg.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"
}

namespace {

/**
 * @brief 聚合基准测试环境
 */
class BenchmarkAggregateTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化存储子系统 */
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(1024), 0) << "Buffer Pool 初始化失败";
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    void TearDown() override {
        rel_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }

    /**
     * @brief 创建测试表并插入数据
     *
     * @param name 表名
     * @param nrows 行数
     * @param num_groups 分组数（用于 GROUP BY 测试）
     * @return 表 OID
     */
    Oid create_and_populate_table(const char *name, int64_t nrows, int64_t num_groups = 1) {
        /* 创建表 */
        column_def_t columns[3];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN);
        columns[0].type_oid = 23;  /* int4 */
        columns[0].not_null = true;

        strncpy(columns[1].name, "group_key", NAMEDATALEN);
        columns[1].type_oid = 23;  /* int4 */

        strncpy(columns[2].name, "value", NAMEDATALEN);
        columns[2].type_oid = 23;  /* int4 */

        Oid table_oid = catalog_create_table(name, columns, 3);
        if (table_oid == InvalidOid) {
            fprintf(stderr, "创建表失败: %s\n", name);
            return InvalidOid;
        }

        /* 打开表并插入数据 */
        Relation rel = relation_open(table_oid, RELMODE_WRITE);
        if (!rel) {
            fprintf(stderr, "打开表失败: %s\n", name);
            return InvalidOid;
        }

        /* 批量插入数据 */
        for (int64_t i = 0; i < nrows; i++) {
            int data[3] = {
                (int)(i + 1),              /* id */
                (int)(i % num_groups),     /* group_key: 循环分配到各组 */
                (int)(i * 10)              /* value */
            };
            int ret = heap_insert(rel, data, sizeof(data), 0, 0, nullptr);
            if (ret != 0) {
                fprintf(stderr, "插入数据失败: i=%lld, ret=%d\n", (long long)i, ret);
                relation_close(rel, RELMODE_WRITE);
                return InvalidOid;
            }
        }

        relation_close(rel, RELMODE_WRITE);
        return table_oid;
    }
};

/**
 * @brief 基准测试: COUNT(*) 聚合
 *
 * 目标：100K 行 COUNT(*) 聚合
 */
TEST_F(BenchmarkAggregateTest, CountStar) {
    const int64_t nrows = 100000;
    const char *table_name = "bench_agg_count";

    /* 创建并填充表 */
    Oid table_oid = create_and_populate_table(table_name, nrows, 1);
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    /* 创建 HashAgg 计划 */
    HashAggPlan agg_plan;
    memset(&agg_plan, 0, sizeof(agg_plan));
    agg_plan.type = EXEC_HASHAGG;
    agg_plan.numGroupCols = 0;  /* COUNT(*) 不分组 */

    /* 测量聚合性能 */
    BenchmarkTimer timer;
    int64_t total_groups = 0;

    timer.Start();

    HashAggState *state = ExecInitHashAgg(&agg_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = nullptr;
    while ((slot = ExecHashAgg((PlanState *)state)) != nullptr) {
        total_groups++;
    }

    ExecEndHashAgg(state);

    timer.Stop();

    double duration_ms = timer.GetElapsedMs();
    double rows_per_sec = (nrows * 1000.0) / duration_ms;

    printf("COUNT(*) 结果:\n");
    printf("  耗时: %.3f ms\n", duration_ms);
    printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);
    printf("  返回组数: %lld\n", (long long)total_groups);
}

/**
 * @brief 基准测试: GROUP BY 100 组
 *
 * 目标：100K 行 GROUP BY 100 组，测量分组性能
 */
TEST_F(BenchmarkAggregateTest, GroupBy100Groups) {
    const int64_t nrows = 100000;
    const int64_t num_groups = 100;
    const char *table_name = "bench_agg_groupby";

    /* 创建并填充表 */
    Oid table_oid = create_and_populate_table(table_name, nrows, num_groups);
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    /* 创建 HashAgg 计划 */
    int grp_col_idx[1] = {1};  /* group_key 列 */
    HashAggPlan agg_plan;
    memset(&agg_plan, 0, sizeof(agg_plan));
    agg_plan.type = EXEC_HASHAGG;
    agg_plan.numGroupCols = 1;
    agg_plan.grpColIdx = grp_col_idx;

    /* 测量聚合性能 */
    BenchmarkTimer timer;
    int64_t total_groups = 0;

    timer.Start();

    HashAggState *state = ExecInitHashAgg(&agg_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = nullptr;
    while ((slot = ExecHashAgg((PlanState *)state)) != nullptr) {
        total_groups++;
    }

    ExecEndHashAgg(state);

    timer.Stop();

    double duration_ms = timer.GetElapsedMs();
    double rows_per_sec = (nrows * 1000.0) / duration_ms;

    printf("GROUP BY 100 组结果:\n");
    printf("  耗时: %.3f ms\n", duration_ms);
    printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);
    printf("  返回组数: %lld\n", (long long)total_groups);

    /* 验证返回的组数 */
    EXPECT_EQ(total_groups, num_groups) << "GROUP BY 应返回 100 组";
}

}  // namespace