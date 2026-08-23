// mmdb_timeseries_test.cpp — Task 10：时序模型（append/query/aggregate）测试
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

#include "sdk/mmdb.h"
#include "sdk/mmdb_timeseries.h"

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