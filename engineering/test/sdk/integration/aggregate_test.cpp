/**
 * @file aggregate_test.cpp
 * @brief P6-M4.1 通用聚合框架测试
 *
 * 覆盖：
 *   1. CountAll - COUNT(*) 全局聚合
 *   2. GroupByCount - GROUP BY + COUNT 分组计数
 *   3. SumAvgMinMax - AVG/MIN/MAX 计算
 *   4. Histogram - 直方图 bucket 分配
 *   5. Pagination - 分页 offset/limit
 *   6. FilterWithAgg - 带过滤条件的聚合
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_aggregate.h"
}

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kDbPath = "test_aggregate.db";
constexpr size_t kDim = 4;

void cleanup_db() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
}

}  // namespace

class AggregateTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* 创建带 metadata 的 collection */
        mmdb_field_def_t fields[] = {
            {"category", MMDB_FIELD_TEXT},
            {"value", MMDB_FIELD_REAL},
            {"region", MMDB_FIELD_TEXT},
        };
        mmdb_schema_t schema = {MMDB_MODEL_VECTOR, 3, fields, kDim};
        coll_ = mmdb_collection_create(db_, "test_agg", &schema);
        ASSERT_NE(coll_, nullptr);

        /* 插入测试数据：10 条记录 */
        struct TestData {
            const char* id;
            const char* category;
            double value;
            const char* region;
        };

        TestData data[] = {
            {"v0", "A", 10.0, "east"},
            {"v1", "A", 20.0, "east"},
            {"v2", "A", 30.0, "west"},
            {"v3", "B", 15.0, "east"},
            {"v4", "B", 25.0, "west"},
            {"v5", "B", 35.0, "west"},
            {"v6", "C", 5.0,  "east"},
            {"v7", "C", 50.0, "west"},
            {"v8", "C", 100.0, "east"},
            {"v9", "C", 200.0, "west"},
        };

        for (const auto& d : data) {
            float vec[kDim] = {0.0f};
            mmdb_vector_t v = {
                reinterpret_cast<const uint8_t*>(d.id), strlen(d.id),
                vec, kDim, nullptr, nullptr
            };
            ASSERT_EQ(mmdb_vectors_add(coll_, &v, 1), MMDB_OK);

            /* 写入 metadata */
            char sql[512];
            snprintf(sql, sizeof(sql),
                "INSERT INTO \"test_agg__metadata\" (\"_id\", \"category\", \"value\", \"region\") "
                "VALUES ('%s', '%s', %.6f, '%s')",
                d.id, d.category, d.value, d.region);
            ASSERT_EQ(mmdb_sqlite_exec(db_->db, sql), 0);
        }
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* ================================================================== */
/* 测试 1：COUNT(*) 全局聚合                                             */
/* ================================================================== */
TEST_F(AggregateTest, CountAll) {
    mmdb_aggregate_query_t query = {0};
    mmdb_agg_expr_t count_expr = {0};
    count_expr.field = NULL;
    count_expr.type = MMDB_AGG_COUNT;
    count_expr.alias = "total";

    query.agg_count = 1;
    query.aggs[0] = count_expr;
    query.limit = 0; /* 无分页限制 */

    mmdb_aggregate_result_set_t* result = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &query, NULL, &result), MMDB_OK);
    ASSERT_NE(result, nullptr);

    /* 全局聚合应只有 1 组 */
    EXPECT_EQ(result->group_count, 1u);
    EXPECT_EQ(result->groups[0].count, 10u);
    EXPECT_STREQ(result->groups[0].key, "ALL");
    EXPECT_EQ(result->total_count, 1u); /* 1 个分组 */
    EXPECT_FALSE(result->has_more);

    mmdb_aggregate_result_free(result);
}

/* ================================================================== */
/* 测试 2：GROUP BY + COUNT 分组计数                                     */
/* ================================================================== */
TEST_F(AggregateTest, GroupByCount) {
    mmdb_aggregate_query_t query = {0};
    query.group_by = "category";

    mmdb_agg_expr_t count_expr = {0};
    count_expr.type = MMDB_AGG_COUNT;
    count_expr.alias = "cnt";
    query.agg_count = 1;
    query.aggs[0] = count_expr;
    query.limit = 0;

    mmdb_aggregate_result_set_t* result = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &query, NULL, &result), MMDB_OK);
    ASSERT_NE(result, nullptr);

    /* 应有 3 个分组：A(3), B(3), C(4) */
    EXPECT_EQ(result->group_count, 3u);
    EXPECT_EQ(result->total_count, 3u);

    /* 验证各分组计数（顺序可能不固定，按 key 查找） */
    uint64_t count_a = 0, count_b = 0, count_c = 0;
    for (size_t i = 0; i < result->group_count; i++) {
        if (strcmp(result->groups[i].key, "A") == 0)
            count_a = result->groups[i].count;
        else if (strcmp(result->groups[i].key, "B") == 0)
            count_b = result->groups[i].count;
        else if (strcmp(result->groups[i].key, "C") == 0)
            count_c = result->groups[i].count;
    }
    EXPECT_EQ(count_a, 3u);
    EXPECT_EQ(count_b, 3u);
    EXPECT_EQ(count_c, 4u);

    mmdb_aggregate_result_free(result);
}

/* ================================================================== */
/* 测试 3：AVG/MIN/MAX 计算                                              */
/* ================================================================== */
TEST_F(AggregateTest, SumAvgMinMax) {
    mmdb_aggregate_query_t query = {0};
    query.group_by = "category";

    mmdb_agg_expr_t avg_e = {0};
    avg_e.field = "value";
    avg_e.type = MMDB_AGG_AVG;
    avg_e.alias = "avg_val";

    mmdb_agg_expr_t min_e = {0};
    min_e.field = "value";
    min_e.type = MMDB_AGG_MIN;
    min_e.alias = "min_val";

    mmdb_agg_expr_t max_e = {0};
    max_e.field = "value";
    max_e.type = MMDB_AGG_MAX;
    max_e.alias = "max_val";

    query.agg_count = 3;
    query.aggs[0] = avg_e;
    query.aggs[1] = min_e;
    query.aggs[2] = max_e;
    query.limit = 0;

    mmdb_aggregate_result_set_t* result = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &query, NULL, &result), MMDB_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->group_count, 3u);

    for (size_t i = 0; i < result->group_count; i++) {
        const auto& g = result->groups[i];
        if (strcmp(g.key, "A") == 0) {
            /* A: 10, 20, 30 → avg=20, min=10, max=30 */
            EXPECT_DOUBLE_EQ(g.avg, 20.0);
            EXPECT_DOUBLE_EQ(g.min, 10.0);
            EXPECT_DOUBLE_EQ(g.max, 30.0);
        } else if (strcmp(g.key, "B") == 0) {
            /* B: 15, 25, 35 → avg=25, min=15, max=35 */
            EXPECT_DOUBLE_EQ(g.avg, 25.0);
            EXPECT_DOUBLE_EQ(g.min, 15.0);
            EXPECT_DOUBLE_EQ(g.max, 35.0);
        } else if (strcmp(g.key, "C") == 0) {
            /* C: 5, 50, 100, 200 → avg=88.75, min=5, max=200 */
            EXPECT_DOUBLE_EQ(g.avg, 88.75);
            EXPECT_DOUBLE_EQ(g.min, 5.0);
            EXPECT_DOUBLE_EQ(g.max, 200.0);
        }
    }

    mmdb_aggregate_result_free(result);
}

/* ================================================================== */
/* 测试 4：HISTOGRAM 直方图                                               */
/* ================================================================== */
TEST_F(AggregateTest, Histogram) {
    mmdb_aggregate_query_t query = {0};
    query.group_by = NULL; /* 全局聚合 */

    mmdb_agg_expr_t hist = {0};
    hist.field = "value";
    hist.type = MMDB_AGG_HISTOGRAM;
    hist.alias = "hist";
    hist.bucket_count = 4;
    hist.bucket_min = 0.0;
    hist.bucket_max = 100.0;

    query.agg_count = 1;
    query.aggs[0] = hist;
    query.limit = 0;

    mmdb_aggregate_result_set_t* result = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &query, NULL, &result), MMDB_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->group_count, 1u);

    const auto& g = result->groups[0];
    ASSERT_NE(g.histogram_buckets, nullptr);
    EXPECT_EQ(g.histogram_bucket_count, 4u);

    /* bucket_size = 25.0
     * [0, 25): 10, 15, 5 = 3 条
     * [25, 50): 30, 25, 35, 50 = 4 条
     * [50, 75): 无 = 0 条
     * [75, 100]: 100 = 1 条
     * > 100: 200 = 1 条（落入 ELSE，bucket_idx=-1，不计入任何桶）
     */
    EXPECT_EQ(g.histogram_buckets[0], 3u);  /* [0, 25) */
    EXPECT_EQ(g.histogram_buckets[1], 4u);  /* [25, 50) */
    EXPECT_EQ(g.histogram_buckets[2], 0u);  /* [50, 75) */
    EXPECT_EQ(g.histogram_buckets[3], 1u);  /* [75, 100] */

    mmdb_aggregate_result_free(result);
}

/* ================================================================== */
/* 测试 5：分页 offset/limit                                             */
/* ================================================================== */
TEST_F(AggregateTest, Pagination) {
    /* 先查询全量获取 total_count */
    mmdb_aggregate_query_t q_full = {0};
    q_full.group_by = "category";
    mmdb_agg_expr_t cnt = {0};
    cnt.type = MMDB_AGG_COUNT;
    cnt.alias = "c";
    q_full.agg_count = 1;
    q_full.aggs[0] = cnt;
    q_full.limit = 0;

    mmdb_aggregate_result_set_t* full = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &q_full, NULL, &full), MMDB_OK);
    uint64_t total = full->total_count;
    mmdb_aggregate_result_free(full);

    /* 第一页：offset=0, limit=2 */
    mmdb_aggregate_query_t q_page1 = {0};
    q_page1.group_by = "category";
    q_page1.agg_count = 1;
    q_page1.aggs[0] = cnt;
    q_page1.offset = 0;
    q_page1.limit = 2;

    mmdb_aggregate_result_set_t* page1 = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &q_page1, NULL, &page1), MMDB_OK);
    EXPECT_EQ(page1->group_count, 2u);
    EXPECT_EQ(page1->total_count, total);
    EXPECT_TRUE(page1->has_more);
    mmdb_aggregate_result_free(page1);

    /* 第二页：offset=2, limit=2 */
    mmdb_aggregate_query_t q_page2 = {0};
    q_page2.group_by = "category";
    q_page2.agg_count = 1;
    q_page2.aggs[0] = cnt;
    q_page2.offset = 2;
    q_page2.limit = 2;

    mmdb_aggregate_result_set_t* page2 = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &q_page2, NULL, &page2), MMDB_OK);
    EXPECT_EQ(page2->group_count, 1u); /* 只剩 1 个分组 */
    EXPECT_FALSE(page2->has_more);
    mmdb_aggregate_result_free(page2);
}

/* ================================================================== */
/* 测试 6：带过滤条件的聚合                                               */
/* ================================================================== */
TEST_F(AggregateTest, FilterWithAgg) {
    mmdb_aggregate_query_t query = {0};
    mmdb_agg_expr_t cnt = {0};
    cnt.type = MMDB_AGG_COUNT;
    cnt.alias = "total";

    mmdb_agg_expr_t avg_e = {0};
    avg_e.field = "value";
    avg_e.type = MMDB_AGG_AVG;
    avg_e.alias = "avg_val";

    query.agg_count = 2;
    query.aggs[0] = cnt;
    query.aggs[1] = avg_e;
    query.limit = 0;

    /* 过滤 region='east'：v0(10), v1(20), v3(15), v6(5), v8(100) = 5 条 */
    mmdb_aggregate_result_set_t* result = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &query, "region = 'east'", &result), MMDB_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->group_count, 1u);
    EXPECT_EQ(result->groups[0].count, 5u);
    /* avg = (10+20+15+5+100)/5 = 30.0 */
    EXPECT_DOUBLE_EQ(result->groups[0].avg, 30.0);

    mmdb_aggregate_result_free(result);
}

/* ================================================================== */
/* 测试 7：GROUP BY + SUM                                                */
/* ================================================================== */
TEST_F(AggregateTest, GroupBySum) {
    mmdb_aggregate_query_t query = {0};
    query.group_by = "category";

    mmdb_agg_expr_t sum_e = {0};
    sum_e.field = "value";
    sum_e.type = MMDB_AGG_SUM;
    sum_e.alias = "sum_val";

    query.agg_count = 1;
    query.aggs[0] = sum_e;
    query.limit = 0;

    mmdb_aggregate_result_set_t* result = NULL;
    ASSERT_EQ(mmdb_aggregate(coll_, &query, NULL, &result), MMDB_OK);
    ASSERT_NE(result, nullptr);

    for (size_t i = 0; i < result->group_count; i++) {
        const auto& g = result->groups[i];
        if (strcmp(g.key, "A") == 0) {
            /* A: 10+20+30 = 60 */
            EXPECT_DOUBLE_EQ(g.sum, 60.0);
        } else if (strcmp(g.key, "B") == 0) {
            /* B: 15+25+35 = 75 */
            EXPECT_DOUBLE_EQ(g.sum, 75.0);
        } else if (strcmp(g.key, "C") == 0) {
            /* C: 5+50+100+200 = 355 */
            EXPECT_DOUBLE_EQ(g.sum, 355.0);
        }
    }

    mmdb_aggregate_result_free(result);
}
