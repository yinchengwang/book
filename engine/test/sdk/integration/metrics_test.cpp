/**
 * @file metrics_test.cpp
 * @brief P6-M1.2：监控指标测试
 *
 * 覆盖：
 *   1. InitialState - 启动后初始指标全为 0
 *   2. Reset - reset() 后所有计数器清零
 *   3. PrometheusFormat - 格式输出包含 HELP/TYPE 注释且包含关键指标名
 *   4. EndToEnd - 通过 SDK API 调用验证指标埋点生效
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb_metrics.h"
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
}

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

/* 辅助函数：检查输出是否包含子串 */
bool contains(const std::string& s, const char* substr) {
    return s.find(substr) != std::string::npos;
}

}  // namespace

/* ================================================================== */
/* 测试 1：初始状态                                                      */
/* ================================================================== */
TEST(MetricsTest, InitialState) {
    /* 先重置保证测试起点一致（避免上一次测试残留） */
    mmdb_metrics_reset();

    const mmdb_metrics_t* m = mmdb_metrics_get();
    ASSERT_NE(m, nullptr);

    /* 所有计数器应为 0 */
    EXPECT_EQ(m->vectors_total, 0u);
    EXPECT_EQ(m->queries_total, 0u);
    EXPECT_EQ(m->queries_success, 0u);
    EXPECT_EQ(m->queries_failed, 0u);
    EXPECT_EQ(m->cache_hits, 0u);
    EXPECT_EQ(m->cache_misses, 0u);
    EXPECT_EQ(m->hnsw_build_total, 0u);

    /* 浮点字段应为 0.0 */
    EXPECT_DOUBLE_EQ(m->query_latency_avg_ms, 0.0);
    EXPECT_DOUBLE_EQ(m->query_latency_p50_ms, 0.0);
    EXPECT_DOUBLE_EQ(m->query_latency_p99_ms, 0.0);
    EXPECT_DOUBLE_EQ(m->cache_hit_rate, 0.0);
    EXPECT_DOUBLE_EQ(m->hnsw_build_time_ms, 0.0);

    /* 资源字段应为 0 */
    EXPECT_EQ(m->memory_used_bytes, 0u);
    EXPECT_EQ(m->memory_total_bytes, 0u);
    EXPECT_EQ(m->disk_used_bytes, 0u);
    EXPECT_EQ(m->disk_total_bytes, 0u);
}

/* ================================================================== */
/* 测试 2：reset 重置                                                  */
/* ================================================================== */
TEST(MetricsTest, ResetClearsCounters) {
    /* 调用 reset 后所有字段应归零（即使之前有非零值） */
    const mmdb_metrics_t* before = mmdb_metrics_get();
    /* 注：before->queries_total 可能来自前面测试或集成测试，本测试不依赖具体值 */

    mmdb_metrics_reset();

    const mmdb_metrics_t* m = mmdb_metrics_get();
    EXPECT_EQ(m->vectors_total, 0u);
    EXPECT_EQ(m->queries_total, 0u);
    EXPECT_EQ(m->queries_success, 0u);
    EXPECT_EQ(m->queries_failed, 0u);
    EXPECT_EQ(m->cache_hits, 0u);
    EXPECT_EQ(m->cache_misses, 0u);
    EXPECT_EQ(m->hnsw_build_total, 0u);
    EXPECT_EQ(m->memory_used_bytes, 0u);
    EXPECT_EQ(m->memory_total_bytes, 0u);
    EXPECT_EQ(m->disk_used_bytes, 0u);
    EXPECT_EQ(m->disk_total_bytes, 0u);

    /* 抑制 unused variable 警告 */
    (void)before;
}

/* ================================================================== */
/* 测试 3：Prometheus 格式输出                                          */
/* ================================================================== */
TEST(MetricsTest, PrometheusFormat) {
    mmdb_metrics_reset();

    char buf[4096];
    size_t n = mmdb_metrics_prometheus_format(buf, sizeof(buf));

    /* 输出应非空 */
    EXPECT_GT(n, 0u);
    EXPECT_LT(n, sizeof(buf));

    /* 输出应以 '\0' 结尾（C 字符串语义） */
    EXPECT_EQ(buf[n], '\0');

    std::string out(buf, n);

    /* 应包含 HELP 注释（关键指标） */
    EXPECT_TRUE(contains(out, "# HELP mmdb_vectors_total"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_queries_total"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_queries_success"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_queries_failed"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_query_latency_avg_ms"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_cache_hit_rate"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_memory_used_bytes"));
    EXPECT_TRUE(contains(out, "# HELP mmdb_hnsw_build_total"));

    /* 应包含 TYPE 注释 */
    EXPECT_TRUE(contains(out, "# TYPE mmdb_vectors_total counter"));
    EXPECT_TRUE(contains(out, "# TYPE mmdb_query_latency_avg_ms gauge"));
    EXPECT_TRUE(contains(out, "# TYPE mmdb_cache_hit_rate gauge"));

    /* 应包含具体指标行（格式：name value） */
    EXPECT_TRUE(contains(out, "mmdb_vectors_total 0.000000"));
    EXPECT_TRUE(contains(out, "mmdb_queries_total 0.000000"));
    EXPECT_TRUE(contains(out, "mmdb_cache_hit_rate 0.000000"));
}

/* ================================================================== */
/* 测试 4：缓冲区边界                                                  */
/* ================================================================== */
TEST(MetricsTest, BufferBoundaryHandling) {
    /* NULL 缓冲应安全返回 0 */
    EXPECT_EQ(mmdb_metrics_prometheus_format(nullptr, 100), 0u);

    /* 0 容量应安全返回 0 */
    char dummy;
    EXPECT_EQ(mmdb_metrics_prometheus_format(&dummy, 0), 0u);

    /* 容量过小：截断时 snprintf 返回安全值，不崩溃 */
    char small[64];
    size_t n = mmdb_metrics_prometheus_format(small, sizeof(small));
    /* 即使截断也应写入部分内容且不会越界访问 */
    EXPECT_LE(n, sizeof(small));
    EXPECT_EQ(small[sizeof(small) - 1], '\0');
}

/* ================================================================== */
/* 测试 5：连续 get 的一致性                                           */
/* ================================================================== */
TEST(MetricsTest, GetReturnsConsistentSnapshot) {
    mmdb_metrics_reset();

    /* 第一次 get */
    const mmdb_metrics_t* m1 = mmdb_metrics_get();
    EXPECT_EQ(m1->queries_total, 0u);

    /* 第二次 get 应返回同一对象（单例） */
    const mmdb_metrics_t* m2 = mmdb_metrics_get();
    EXPECT_EQ(m1, m2);

    /* 字段值应一致 */
    EXPECT_EQ(m1->vectors_total, m2->vectors_total);
    EXPECT_EQ(m1->queries_total, m2->queries_total);
}

/* ================================================================== */
/* 测试 6：端到端集成 - SDK API 调用触发指标埋点                       */
/* ================================================================== */
class MetricsIntegrationTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;

    static constexpr const char* kDbPath = "test_metrics_e2e.db";
    static constexpr size_t kDim = 4;

    void SetUp() override {
        std::remove(kDbPath);
        mmdb_metrics_reset();

        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
        mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
        coll_ = mmdb_collection_create(db_, "vec", &s);
        ASSERT_NE(coll_, nullptr);

        /* 预先添加 3 个向量建立初始向量数 */
        const char* ids[] = {"a", "b", "c"};
        float vecs[][kDim] = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        };
        for (int i = 0; i < 3; i++) {
            mmdb_vector_t v = {(const uint8_t*)ids[i], 1, vecs[i], kDim,
                               nullptr, nullptr};
            ASSERT_EQ(mmdb_vectors_add(coll_, &v, 1), MMDB_OK);
        }
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }
};

TEST_F(MetricsIntegrationTest, SearchIncrementsQueryCounters) {
    /* 基线：3 个 add 已计入 vectors_total */
    /* 注意：mmdb_metrics_get() 返回静态对象，必须立即拷贝值（避免后续快照覆盖） */
    const mmdb_metrics_t* before = mmdb_metrics_get();
    uint64_t base_queries = before->queries_total;
    uint64_t base_success = before->queries_success;
    uint64_t base_failed = before->queries_failed;
    uint64_t base_vectors = before->vectors_total;

    /* 执行 5 次搜索 */
    float q[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 5; i++) {
        mmdb_query_t query = {q, kDim, 2, nullptr, 0, 0};
        mmdb_result_t result;
        ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
        mmdb_result_free(&result);
    }

    /* 重新获取快照（覆盖之前的 before 指针指向的静态对象） */
    const mmdb_metrics_t* after = mmdb_metrics_get();
    /* queries_total 应增加 5 次 */
    EXPECT_EQ(after->queries_total, base_queries + 5);
    /* queries_success 应增加 5 次（全部成功） */
    EXPECT_EQ(after->queries_success, base_success + 5);
    /* queries_failed 应保持不变 */
    EXPECT_EQ(after->queries_failed, base_failed);
    /* 平均延迟应大于 0 */
    EXPECT_GT(after->query_latency_avg_ms, 0.0);
    /* vectors_total 应保持为 3（搜索不修改向量数） */
    EXPECT_EQ(after->vectors_total, base_vectors);
}

TEST_F(MetricsIntegrationTest, AddDeleteUpdatesVectorTotal) {
    /* 同样立即拷贝快照值 */
    uint64_t base_vectors = mmdb_metrics_get()->vectors_total;

    /* 添加 2 个向量 */
    float v1[kDim] = {0.1f, 0.2f, 0.3f, 0.4f};
    float v2[kDim] = {0.5f, 0.6f, 0.7f, 0.8f};
    mmdb_vector_t add_batch[2] = {
        {(const uint8_t*)"d", 1, v1, kDim, nullptr, nullptr},
        {(const uint8_t*)"e", 1, v2, kDim, nullptr, nullptr},
    };
    ASSERT_EQ(mmdb_vectors_add(coll_, add_batch, 2), MMDB_OK);

    EXPECT_EQ(mmdb_metrics_get()->vectors_total, base_vectors + 2);

    /* 删除 1 个向量 */
    ASSERT_EQ(mmdb_vectors_delete(coll_, (const uint8_t*)"d", 1), MMDB_OK);
    EXPECT_EQ(mmdb_metrics_get()->vectors_total, base_vectors + 1);
}

TEST_F(MetricsIntegrationTest, PrometheusOutputReflectsActivity) {
    /* 执行几次查询 */
    float q[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; i++) {
        mmdb_query_t query = {q, kDim, 1, nullptr, 0, 0};
        mmdb_result_t result;
        ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
        mmdb_result_free(&result);
    }

    /* 导出 Prometheus 格式 */
    char buf[4096];
    size_t n = mmdb_metrics_prometheus_format(buf, sizeof(buf));
    std::string out(buf, n);

    /* 验证 Prometheus 输出包含真实活动指标（值非 0） */
    const mmdb_metrics_t* m = mmdb_metrics_get();
    EXPECT_GE(m->vectors_total, 3u);
    EXPECT_GE(m->queries_total, 3u);

    /* 验证 queries_total 的格式化值（保留 6 位小数） */
    char expected[64];
    snprintf(expected, sizeof(expected), "mmdb_queries_total %.6f",
             (double)m->queries_total);
    EXPECT_TRUE(contains(out, expected)) << "expected: " << expected;
}
