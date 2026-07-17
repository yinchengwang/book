/**
 * @file benchmark_join.cpp
 * @brief HashJoin 执行器节点性能基准测试
 *
 * 测试场景：
 * 1. HashJoin10Kx10K - 10K x 10K HashJoin
 * 2. HashJoin100Kx100K - 100K x 100K HashJoin（目标 < 5s）
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "benchmark_config.h"

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodeHashjoin.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"
}

namespace {

/**
 * @brief 连接基准测试环境
 */
class BenchmarkJoinTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化存储子系统 */
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(2048), 0) << "Buffer Pool 初始化失败";
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
     * @param start_id 起始 ID
     * @return 表 OID
     */
    Oid create_and_populate_table(const char *name, int64_t nrows, int64_t start_id = 0) {
        /* 创建表 */
        column_def_t columns[3];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN);
        columns[0].type_oid = 23;  /* int4 */
        columns[0].not_null = true;

        strncpy(columns[1].name, "join_key", NAMEDATALEN);
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
                (int)(start_id + i),      /* id */
                (int)(i % 10000),         /* join_key: 0-9999 循环，确保部分匹配 */
                (int)(i * 10)             /* value */
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
 * @brief 基准测试: HashJoin 10K x 10K
 *
 * 目标：10K x 10K HashJoin，测量吞吐量
 */
TEST_F(BenchmarkJoinTest, HashJoin10Kx10K) {
    const int64_t nrows = 10000;
    const char *left_table = "bench_join_left_10k";
    const char *right_table = "bench_join_right_10k";

    /* 创建并填充左表 */
    Oid left_oid = create_and_populate_table(left_table, nrows, 0);
    ASSERT_NE(left_oid, InvalidOid) << "左表创建失败";

    /* 创建并填充右表 */
    Oid right_oid = create_and_populate_table(right_table, nrows, 0);
    ASSERT_NE(right_oid, InvalidOid) << "右表创建失败";

    /* 使用低层 API 执行 HashJoin（避免 SQL 解析器问题） */
    /* 创建外表（左表）扫描计划 */
    SeqScanPlan outer_scan;
    memset(&outer_scan, 0, sizeof(outer_scan));
    outer_scan.type = EXEC_SEQ_SCAN;
    outer_scan.scanrelid = left_oid;

    /* 创建内表（右表）扫描计划 */
    SeqScanPlan inner_scan;
    memset(&inner_scan, 0, sizeof(inner_scan));
    inner_scan.type = EXEC_SEQ_SCAN;
    inner_scan.scanrelid = right_oid;

    /* 创建 HashJoin 计划 */
    HashJoinPlan hj_plan;
    memset(&hj_plan, 0, sizeof(hj_plan));
    hj_plan.type = EXEC_HASHJOIN;

    /* 测量 HashJoin 性能 */
    BenchmarkTimer timer;
    int64_t total_tuples = 0;

    timer.Start();

    /* 初始化 HashJoin 状态（简化版：直接使用底层 API） */
    HashJoinState *hj_state = ExecInitHashJoin(&h_plan, nullptr, 0);
    ASSERT_NE(hj_state, nullptr);

    /* 执行 HashJoin（构建 + 探测） */
    TupleTableSlot *slot = nullptr;
    while ((slot = ExecHashJoin((PlanState *)hj_state)) != nullptr) {
        total_tuples++;
    }

    ExecEndHashJoin(hj_state);

    timer.Stop();

    double duration_ms = timer.GetElapsedMs();
    double rows_per_sec = (nrows * 1000.0) / duration_ms;

    printf("HashJoin 10Kx10K 结果:\n");
    printf("  耗时: %.3f ms\n", duration_ms);
    printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);
    printf("  返回行数: %lld\n", (long long)total_tuples);
}

/**
 * @brief 基准测试: HashJoin 100K x 100K
 *
 * 目标：100K x 100K HashJoin，耗时 < 5000 ms
 */
TEST_F(BenchmarkJoinTest, HashJoin100Kx100K) {
    const int64_t nrows = 100000;
    const char *left_table = "bench_join_left_100k";
    const char *right_table = "bench_join_right_100k";

    /* 创建并填充左表 */
    Oid left_oid = create_and_populate_table(left_table, nrows, 0);
    ASSERT_NE(left_oid, InvalidOid) << "左表创建失败";

    /* 创建并填充右表 */
    Oid right_oid = create_and_populate_table(right_table, nrows, 0);
    ASSERT_NE(right_oid, InvalidOid) << "右表创建失败";

    /* 使用低层 API 执行 HashJoin（避免 SQL 解析器问题） */
    /* 创建外表（左表）扫描计划 */
    SeqScanPlan outer_scan;
    memset(&outer_scan, 0, sizeof(outer_scan));
    outer_scan.type = EXEC_SEQ_SCAN;
    outer_scan.scanrelid = left_oid;

    /* 创建内表（右表）扫描计划 */
    SeqScanPlan inner_scan;
    memset(&inner_scan, 0, sizeof(inner_scan));
    inner_scan.type = EXEC_SEQ_SCAN;
    inner_scan.scanrelid = right_oid;

    /* 创建 HashJoin 计划 */
    HashJoinPlan hj_plan;
    memset(&hj_plan, 0, sizeof(hj_plan));
    hj_plan.type = EXEC_HASHJOIN;

    /* 测量 HashJoin 性能 */
    BenchmarkTimer timer;
    int64_t total_tuples = 0;

    timer.Start();

    /* 初始化 HashJoin 状态（简化版：直接使用底层 API） */
    HashJoinState *hj_state = ExecInitHashJoin(&h_plan, nullptr, 0);
    ASSERT_NE(hj_state, nullptr);

    /* 执行 HashJoin（构建 + 探测） */
    TupleTableSlot *slot = nullptr;
    while ((slot = ExecHashJoin((PlanState *)hj_state)) != nullptr) {
        total_tuples++;
    }

    ExecEndHashJoin(hj_state);

    timer.Stop();

    double duration_ms = timer.GetElapsedMs();
    double rows_per_sec = (nrows * 1000.0) / duration_ms;

    printf("HashJoin 100Kx100K 结果:\n");
    printf("  耗时: %.3f ms\n", duration_ms);
    printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);
    printf("  返回行数: %lld\n", (long long)total_tuples);

    /* 性能目标：耗时 < 5000 ms */
    EXPECT_LT(duration_ms, 5000.0) << "100K x 100K 连接超过 5 秒";
}

}  // namespace