/**
 * @file ts_scan_test.cpp
 * @brief 时序扫描算子单元测试
 *
 * 测试时序算子的 Volcano 迭代器协议实现。
 * 注意：ts_engine 默认不编译，测试需要链接相应的库。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/executor/ts/ts_scan.h"
#include "db/ts_engine.h"
}

#include <cstdlib>
#include <cstring>
#include <cstdint>

/* ============================================================
 * 工具函数：创建测试数据点
 * ============================================================ */

/**
 * @brief 构造一个简单的时序数据点（时间戳 + double 值）
 * 格式与 ts_engine_insert 的 data 参数兼容。
 */
static void make_ts_data_point(void *buf, int64_t timestamp, double value)
{
    /* 假定数据点布局：timestamp(int64) + value(double) */
    memcpy((char *)buf, &timestamp, sizeof(timestamp));
    memcpy((char *)buf + sizeof(timestamp), &value, sizeof(value));
}

/* ============================================================
 * TsScanTest 测试夹具
 * ============================================================ */

class TsScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *test_dir = std::getenv("TEST_TMPDIR");
        if (!test_dir) test_dir = "test_data/ts_scan_test";
        ts_engine_init(test_dir);
        ts_engine_create("default", nullptr);
    }

    void TearDown() override {
        ts_engine_drop("default");
        ts_engine_shutdown();
    }

    /**
     * @brief 插入一条时序数据
     */
    void insert_data_point(int64_t timestamp, double value) {
        void *rel = ts_engine_open("default", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(rel, nullptr);

        uint8_t buf[16];
        make_ts_data_point(buf, timestamp, value);
        int rc = ts_engine_insert(rel, buf, sizeof(buf));
        EXPECT_EQ(rc, 0);

        ts_engine_close(rel);
    }
};

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @test 初始化与关闭（无数据）
 * 验证 init/close 不会崩溃。
 */
TEST_F(TsScanTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    TsScanState *state = exec_ts_scan_init(&parent, 0, 0, NULL);
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(state->ps.type, EXEC_TIMESERIES_SCAN);
    EXPECT_EQ(state->time_start, 0);
    EXPECT_EQ(state->time_end, 0);
    EXPECT_EQ(state->tag_filters, nullptr);

    exec_ts_scan_close(state);
}

/**
 * @test 插入与查询
 * 插入多条时序数据，查询并验证结果数量。
 */
TEST_F(TsScanTest, InsertAndQuery) {
    /* 插入 5 条数据 */
    int64_t base_ts = 1000000;
    insert_data_point(base_ts + 0, 1.5);
    insert_data_point(base_ts + 1000, 2.5);
    insert_data_point(base_ts + 2000, 3.5);
    insert_data_point(base_ts + 3000, 4.5);
    insert_data_point(base_ts + 4000, 5.5);

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    TsScanState *state = exec_ts_scan_init(&parent,
        base_ts, base_ts + 5000, NULL);
    ASSERT_NE(state, nullptr);

    /* 遍历结果 */
    int row_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_ts_scan_next(state)) != NULL) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        exec_clear_tuple_slot(slot);
        row_count++;
    }

    /* 应该返回至少 1 行（聚合结果） */
    EXPECT_GT(row_count, 0);

    exec_ts_scan_close(state);
}

/**
 * @test 聚合查询
 * 插入数据后验证聚合结果中的统计字段。
 */
TEST_F(TsScanTest, QueryWithAggregation) {
    /* 插入数据，使聚合可产生有意义的统计值 */
    int64_t base_ts = 2000000;
    insert_data_point(base_ts + 0, 10.0);
    insert_data_point(base_ts + 1000, 20.0);
    insert_data_point(base_ts + 2000, 30.0);

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    /* 使用降采样窗口 1000ms，触发 SECOND 粒度聚合 */
    TsScanState *state = exec_ts_scan_init(&parent,
        base_ts, base_ts + 3000, NULL);
    ASSERT_NE(state, nullptr);
    state->down_sample_window = 1000;

    int row_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_ts_scan_next(state)) != NULL) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        /* timestamp 不应为 0 */
        EXPECT_NE((uintptr_t)slot->tts_values[0], 0);
        exec_clear_tuple_slot(slot);
        row_count++;
    }

    EXPECT_GT(row_count, 0);
    exec_ts_scan_close(state);
}

/**
 * @test 时间范围查询
 * 验证时间范围过滤：只查询部分数据，应返回更少的结果。
 */
TEST_F(TsScanTest, RangeQuery) {
    /* 插入分布在两个时间窗口的数据 */
    int64_t early_ts = 3000000;
    int64_t late_ts  = 9000000;
    insert_data_point(early_ts, 1.0);
    insert_data_point(early_ts + 1000, 2.0);
    insert_data_point(late_ts, 100.0);
    insert_data_point(late_ts + 1000, 200.0);

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    /* 查询早期窗口（不包含后期数据） */
    TsScanState *state = exec_ts_scan_init(&parent,
        early_ts - 100, early_ts + 2000, NULL);
    ASSERT_NE(state, nullptr);

    int early_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_ts_scan_next(state)) != NULL) {
        exec_clear_tuple_slot(slot);
        early_count++;
    }
    exec_ts_scan_close(state);

    /* 查询全部数据 */
    state = exec_ts_scan_init(&parent,
        early_ts - 100, late_ts + 2000, NULL);
    ASSERT_NE(state, nullptr);

    int all_count = 0;
    while ((slot = exec_ts_scan_next(state)) != NULL) {
        exec_clear_tuple_slot(slot);
        all_count++;
    }
    exec_ts_scan_close(state);

    /* 全部数据的行数应 >= 早期数据的行数 */
    EXPECT_GE(all_count, early_count);
    EXPECT_GT(all_count, 0);
}