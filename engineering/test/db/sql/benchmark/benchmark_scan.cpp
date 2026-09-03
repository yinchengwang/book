/**
 * @file benchmark_scan.cpp
 * @brief SeqScan 执行器节点性能基准测试
 *
 * 测试场景：
 * 1. SeqScan100K - 100K 行全表扫描
 * 2. SeqScan1M - 1M 行全表扫描（目标 < 1s）
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "benchmark_config.h"

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"
}

namespace {

/**
 * @brief 扫描基准测试环境
 */
class BenchmarkScanTest : public ::testing::Test {
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
     * @return 表 OID
     */
    Oid create_and_populate_table(const char *name, int64_t nrows) {
        /* 创建表 */
        column_def_t columns[2];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN);
        columns[0].type_oid = 23;  /* int4 */
        columns[0].not_null = true;

        strncpy(columns[1].name, "value", NAMEDATALEN);
        columns[1].type_oid = 23;  /* int4 */

        Oid table_oid = catalog_create_table(name, columns, 2);
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
            int data[2] = { (int)(i + 1), (int)(i * 10) };
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
 * @brief 基准测试: SeqScan 100K 行
 *
 * 目标：100K 行全表扫描，测量吞吐量
 */
TEST_F(BenchmarkScanTest, SeqScan100K) {
    const int64_t nrows = 100000;
    const char *table_name = "bench_scan_100k";

    /* 创建并填充表 */
    Oid table_oid = create_and_populate_table(table_name, nrows);
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    /* 创建 SeqScan 计划 */
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    /* 测量扫描性能 */
    BenchmarkTimer timer;
    int64_t total_tuples = 0;

    timer.Start();

    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = nullptr;
    while ((slot = ExecSeqScan((PlanState *)state)) != nullptr) {
        total_tuples++;
    }

    ExecEndSeqScan(state);

    timer.Stop();

    /* 验证结果 */
    EXPECT_EQ(total_tuples, nrows) << "扫描行数不匹配";

    double duration_ms = timer.GetElapsedMs();
    double rows_per_sec = (nrows * 1000.0) / duration_ms;

    printf("SeqScan 100K 结果:\n");
    printf("  耗时: %.3f ms\n", duration_ms);
    printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);
}

/**
 * @brief 基准测试: SeqScan 1M 行
 *
 * 目标：1M 行全表扫描，耗时 < 1000 ms
 */
TEST_F(BenchmarkScanTest, SeqScan1M) {
    const int64_t nrows = 1000000;
    const char *table_name = "bench_scan_1m";

    /* 创建并填充表 */
    Oid table_oid = create_and_populate_table(table_name, nrows);
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    /* 创建 SeqScan 计划 */
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    /* 测量扫描性能 */
    BenchmarkTimer timer;
    int64_t total_tuples = 0;

    timer.Start();

    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = nullptr;
    while ((slot = ExecSeqScan((PlanState *)state)) != nullptr) {
        total_tuples++;
    }

    ExecEndSeqScan(state);

    timer.Stop();

    /* 验证结果 */
    EXPECT_EQ(total_tuples, nrows) << "扫描行数不匹配";

    double duration_ms = timer.GetElapsedMs();
    double rows_per_sec = (nrows * 1000.0) / duration_ms;

    printf("SeqScan 1M 结果:\n");
    printf("  耗时: %.3f ms\n", duration_ms);
    printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);

    /* 性能目标：耗时 < 1000 ms */
    EXPECT_LT(duration_ms, 1000.0) << "1M 行扫描超过 1 秒";
}

}  // namespace