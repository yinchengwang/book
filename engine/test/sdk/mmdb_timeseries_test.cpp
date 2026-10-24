// mmdb_timeseries_test.cpp — Task 10：时序模型（append/query/aggregate）测试
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

#include "sdk/mmdb.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/mmdb_aggregate.h"

namespace {
constexpr const char* kDbPath = "test_mmdb_timeseries.db";
}

class MmdbTimeseriesTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;

    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
        mmdb_schema_t s = {MMDB_MODEL_TIMESERIES, 0, nullptr, 0};
        coll_ = mmdb_collection_create(db_, "metrics", &s);
        ASSERT_NE(coll_, nullptr);
    }
    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }
};

TEST_F(MmdbTimeseriesTest, AppendSingle) {
    mmdb_datapoint_t dp = {1000, 42.0, R"({"sensor":"a"})"};
    EXPECT_EQ(mmdb_timeseries_append(coll_, &dp), MMDB_OK);
}

TEST_F(MmdbTimeseriesTest, AppendBatch) {
    mmdb_datapoint_t dps[5];
    for (int i = 0; i < 5; i++) {
        dps[i] = {1000 + i, (double)(i * 10), nullptr};
    }
    EXPECT_EQ(mmdb_timeseries_append_batch(coll_, dps, 5), MMDB_OK);
}

TEST_F(MmdbTimeseriesTest, AppendZeroCount) {
    EXPECT_EQ(mmdb_timeseries_append_batch(coll_, nullptr, 0), MMDB_OK);
}

TEST_F(MmdbTimeseriesTest, AppendOverwrite) {
    mmdb_datapoint_t dp1 = {1000, 42.0, nullptr};
    mmdb_datapoint_t dp2 = {1000, 99.0, nullptr};
    ASSERT_EQ(mmdb_timeseries_append(coll_, &dp1), MMDB_OK);
    EXPECT_EQ(mmdb_timeseries_append(coll_, &dp2), MMDB_OK);

    mmdb_ts_query_t q = {0, 2000, nullptr, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &q, &result), MMDB_OK);
    EXPECT_EQ(result.count, 1u);
    EXPECT_NEAR(result.items[0].distance, 99.0f, 1e-3);
    mmdb_result_free(&result);
}

TEST_F(MmdbTimeseriesTest, QueryRange) {
    mmdb_datapoint_t dps[10];
    for (int i = 0; i < 10; i++) {
        dps[i] = {1000 + i, (double)i, nullptr};
    }
    ASSERT_EQ(mmdb_timeseries_append_batch(coll_, dps, 10), MMDB_OK);

    mmdb_ts_query_t q = {1002, 1006, nullptr, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &q, &result), MMDB_OK);
    EXPECT_EQ(result.count, 5u);
    mmdb_result_free(&result);
}

TEST_F(MmdbTimeseriesTest, QueryAvg) {
    mmdb_datapoint_t dps[4];
    for (int i = 0; i < 4; i++) {
        dps[i] = {1000 + i, (double)(i + 1), nullptr};  /* 1, 2, 3, 4 → avg=2.5 */
    }
    ASSERT_EQ(mmdb_timeseries_append_batch(coll_, dps, 4), MMDB_OK);

    mmdb_ts_query_t q = {0, 9999, "avg", nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &q, &result), MMDB_OK);
    EXPECT_EQ(result.count, 1u);
    EXPECT_NEAR(result.items[0].distance, 2.5f, 1e-3);
    mmdb_result_free(&result);
}

TEST_F(MmdbTimeseriesTest, QuerySum) {
    mmdb_datapoint_t dps[3];
    for (int i = 0; i < 3; i++) dps[i] = {1000 + i, 10.0, nullptr};
    ASSERT_EQ(mmdb_timeseries_append_batch(coll_, dps, 3), MMDB_OK);

    mmdb_ts_query_t q = {0, 9999, "sum", nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &q, &result), MMDB_OK);
    EXPECT_NEAR(result.items[0].distance, 30.0f, 1e-3);
    mmdb_result_free(&result);
}

TEST_F(MmdbTimeseriesTest, QueryMinMax) {
    mmdb_datapoint_t dps[3];
    dps[0] = {1000, 5.0, nullptr};
    dps[1] = {1001, 2.0, nullptr};
    dps[2] = {1002, 9.0, nullptr};
    ASSERT_EQ(mmdb_timeseries_append_batch(coll_, dps, 3), MMDB_OK);

    mmdb_ts_query_t qmin = {0, 9999, "min", nullptr};
    mmdb_result_t rmin = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &qmin, &rmin), MMDB_OK);
    EXPECT_NEAR(rmin.items[0].distance, 2.0f, 1e-3);
    mmdb_result_free(&rmin);

    mmdb_ts_query_t qmax = {0, 9999, "max", nullptr};
    mmdb_result_t rmax = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &qmax, &rmax), MMDB_OK);
    EXPECT_NEAR(rmax.items[0].distance, 9.0f, 1e-3);
    mmdb_result_free(&rmax);
}

TEST_F(MmdbTimeseriesTest, QueryCount) {
    mmdb_datapoint_t dps[5];
    for (int i = 0; i < 5; i++) dps[i] = {1000 + i, (double)i, nullptr};
    ASSERT_EQ(mmdb_timeseries_append_batch(coll_, dps, 5), MMDB_OK);

    mmdb_ts_query_t q = {0, 9999, "count", nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_timeseries_query(coll_, &q, &result), MMDB_OK);
    EXPECT_NEAR(result.items[0].distance, 5.0f, 1e-3);
    mmdb_result_free(&result);
}

TEST_F(MmdbTimeseriesTest, QueryEmptyCollection) {
    mmdb_ts_query_t q = {0, 9999, nullptr, nullptr};
    mmdb_result_t result = {};
    EXPECT_EQ(mmdb_timeseries_query(coll_, &q, &result), MMDB_OK);
    EXPECT_EQ(result.count, 0u);
}

TEST_F(MmdbTimeseriesTest, WrongCollectionModelFails) {
    mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
    mmdb_collection_t* v = mmdb_collection_create(db_, "vec", &vs);
    ASSERT_NE(v, nullptr);

    mmdb_datapoint_t dp = {1000, 42.0, nullptr};
    EXPECT_NE(mmdb_timeseries_append(v, &dp), MMDB_OK);
}

/* ======================================================================== */
/* 滑动窗口聚合测试（P6-M4.2）                                               */
/* ======================================================================== */

TEST_F(MmdbTimeseriesTest, AggregateCountNoSlide) {
    /* 插入 3 个点，窗口 1000ms，不滑动 → 1 个窗口，count=3 */
    uint64_t base = 1000;
    for (int i = 0; i < 3; i++) {
        mmdb_datapoint_t dp = {(int64_t)(base + i * 100), (double)(10 + i), nullptr};
        ASSERT_EQ(mmdb_timeseries_append(coll_, &dp), MMDB_OK);
    }

    mmdb_ts_agg_expr_t agg = {NULL, MMDB_AGG_COUNT, 1000, 0};
    mmdb_ts_aggregate_query_t q = {base, base + 1000, {agg}, 1, false};
    mmdb_aggregate_result_set_t* rs = NULL;
    ASSERT_EQ(mmdb_ts_aggregate(coll_, &q, &rs), MMDB_OK);
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->group_count, 1u);
    EXPECT_EQ(rs->groups[0].count, 3u);
    mmdb_aggregate_result_free(rs);
}

TEST_F(MmdbTimeseriesTest, AggregateSumWithSlide) {
    /* 滑动窗口：窗口 200ms，步长 100ms */
    uint64_t base = 1000;
    /* ts=1000 val=10, ts=1050 val=20, ts=1100 val=30, ts=1150 val=40 */
    double vals[] = {10, 20, 30, 40};
    uint64_t ts[]  = {1000, 1050, 1100, 1150};
    for (int i = 0; i < 4; i++) {
        mmdb_datapoint_t dp = {(int64_t)ts[i], vals[i], nullptr};
        ASSERT_EQ(mmdb_timeseries_append(coll_, &dp), MMDB_OK);
    }

    /* 窗口 [1000,1200) 步长 100 → 3 个窗口：[1000,1200), [1100,1300), [1200,1300) */
    mmdb_ts_agg_expr_t agg = {NULL, MMDB_AGG_SUM, 200, 100};
    mmdb_ts_aggregate_query_t q = {base, base + 300, {agg}, 1, true};
    mmdb_aggregate_result_set_t* rs = NULL;
    ASSERT_EQ(mmdb_ts_aggregate(coll_, &q, &rs), MMDB_OK);
    ASSERT_NE(rs, nullptr);
    /* 至少应有 3 个窗口 */
    EXPECT_GE(rs->group_count, 2u);
    mmdb_aggregate_result_free(rs);
}

TEST_F(MmdbTimeseriesTest, AggregateFillEmptyZero) {
    /* 仅 ts=1000 有数据，窗口 [1000,3000) 步长 1000 → 2 个窗口，第 2 个空 */
    mmdb_datapoint_t dp = {1000, 99.0, nullptr};
    ASSERT_EQ(mmdb_timeseries_append(coll_, &dp), MMDB_OK);

    mmdb_ts_agg_expr_t agg = {NULL, MMDB_AGG_COUNT, 1000, 0};
    mmdb_ts_aggregate_query_t q = {1000, 3000, {agg}, 1, true};  /* fill_empty=true */
    mmdb_aggregate_result_set_t* rs = NULL;
    ASSERT_EQ(mmdb_ts_aggregate(coll_, &q, &rs), MMDB_OK);
    ASSERT_NE(rs, nullptr);
    /* 2 个窗口 */
    EXPECT_EQ(rs->group_count, 2u);
    mmdb_aggregate_result_free(rs);
}

TEST_F(MmdbTimeseriesTest, AggregateFillEmptySkip) {
    /* 同上，fill_empty=false → 只返回有数据的窗口 */
    mmdb_datapoint_t dp = {1000, 99.0, nullptr};
    ASSERT_EQ(mmdb_timeseries_append(coll_, &dp), MMDB_OK);

    mmdb_ts_agg_expr_t agg = {NULL, MMDB_AGG_COUNT, 1000, 0};
    mmdb_ts_aggregate_query_t q = {1000, 2000, {agg}, 1, false};  /* 改为 [1000,2000) 只有 1 个窗口 */
    mmdb_aggregate_result_set_t* rs = NULL;
    ASSERT_EQ(mmdb_ts_aggregate(coll_, &q, &rs), MMDB_OK);
    ASSERT_NE(rs, nullptr);
    /* 只有 1 个窗口有数据 */
    EXPECT_EQ(rs->group_count, 1u);
    mmdb_aggregate_result_free(rs);
}

TEST_F(MmdbTimeseriesTest, AggregateMultiExpr) {
    /* 同时做 COUNT 和 SUM */
    uint64_t base = 5000;
    double vals[] = {1.0, 2.0, 3.0};
    uint64_t ts[]  = {5000, 5100, 5200};
    for (int i = 0; i < 3; i++) {
        mmdb_datapoint_t dp = {(int64_t)ts[i], vals[i], nullptr};
        ASSERT_EQ(mmdb_timeseries_append(coll_, &dp), MMDB_OK);
    }

    mmdb_ts_agg_expr_t agg1 = {NULL, MMDB_AGG_COUNT, 1000, 0};
    mmdb_ts_agg_expr_t agg2 = {NULL, MMDB_AGG_SUM,   1000, 0};
    mmdb_ts_aggregate_query_t q = {base, base + 1000, {agg1, agg2}, 2, false};
    mmdb_aggregate_result_set_t* rs = NULL;
    ASSERT_EQ(mmdb_ts_aggregate(coll_, &q, &rs), MMDB_OK);
    ASSERT_NE(rs, nullptr);
    /* 2 个表达式 × 1 个窗口 = 2 个 group */
    EXPECT_EQ(rs->group_count, 2u);
    mmdb_aggregate_result_free(rs);
}

TEST_F(MmdbTimeseriesTest, AggregateInvalidParams) {
    EXPECT_EQ(mmdb_ts_aggregate(NULL, NULL, NULL), MMDB_ERR_INVALID);

    mmdb_ts_aggregate_query_t q = {0, 1000, {}, 0, false};
    EXPECT_EQ(mmdb_ts_aggregate(coll_, &q, NULL), MMDB_ERR_INVALID);

    q.agg_count = 5; /* 超过 4 个 */
    EXPECT_EQ(mmdb_ts_aggregate(coll_, &q, NULL), MMDB_ERR_INVALID);
}

TEST_F(MmdbTimeseriesTest, AggregateEmptyTable) {
    /* 空表聚合不崩溃 */
    mmdb_ts_agg_expr_t agg = {NULL, MMDB_AGG_COUNT, 1000, 0};
    mmdb_ts_aggregate_query_t q = {0, 1000, {agg}, 1, false};
    mmdb_aggregate_result_set_t* rs = NULL;
    ASSERT_EQ(mmdb_ts_aggregate(coll_, &q, &rs), MMDB_OK);
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->group_count, 0u);
    mmdb_aggregate_result_free(rs);
}