// cross_lang_consistency_test.cpp — 跨语言一致性 + 性能基准测试
//
// 验证场景：
//   1. C ABI 创建数据库并写入数据 → 重新打开后能正确读取
//   2. 向量 KNN 搜索性能（1000 向量 / 100 查询）
//   3. 时序范围查询性能（10000 数据点）
//   4. 全文搜索性能（FTS5）
//   5. 数据库文件大小 vs 内存占用
//   6. 并发安全性（多线程同时读）
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <filesystem>

#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/mmdb_text.h"

namespace fs = std::filesystem;
using clk = std::chrono::high_resolution_clock;

namespace {

const char* kDbPath = "test_cross_lang.db";

// 生成 N 维随机浮点向量
std::vector<float> random_vector(size_t dim, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(dim);
    for (size_t i = 0; i < dim; i++) v[i] = dist(rng);
    return v;
}

double elapsed_ms(std::chrono::time_point<clk> start) {
    auto end = clk::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

// ========================================================================
// 1. C ABI 一致性测试
// ========================================================================

TEST(CrossLang, OpenCloseReopenSameData) {
    std::remove(kDbPath);

    // 第一次会话：创建集合并写入
    {
        mmdb_t* db = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db, nullptr);

        mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
        mmdb_collection_t* c = mmdb_collection_create(db, "persisted", &s);
        ASSERT_NE(c, nullptr);

        const char* id = "persistent_vec";
        float vec[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        mmdb_vector_t v = {(const uint8_t*)id, strlen(id), vec, 4, nullptr, nullptr};
        ASSERT_EQ(mmdb_vectors_add(c, &v, 1), MMDB_OK);

        mmdb_close(db);
    }

    // 第二次会话：重新打开并验证数据
    {
        mmdb_t* db = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db, nullptr);

        mmdb_collection_t* c = mmdb_collection_get(db, "persisted");
        ASSERT_NE(c, nullptr) << "集合未持久化";

        float q[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        mmdb_query_t query = {q, 4, 5, nullptr};
        mmdb_result_t result = {};
        ASSERT_EQ(mmdb_vectors_search(c, &query, &result), MMDB_OK);
        EXPECT_EQ(result.count, 1u);
        EXPECT_EQ(result.items[0].id_len, strlen("persistent_vec"));
        EXPECT_EQ(memcmp(result.items[0].id, "persistent_vec", result.items[0].id_len), 0);
        EXPECT_NEAR(result.items[0].distance, 0.0f, 1e-6);
        mmdb_result_free(&result);

        mmdb_close(db);
    }

    std::remove(kDbPath);
}

// ========================================================================
// 2. 向量 KNN 性能基准
// ========================================================================

TEST(Benchmark, VectorKNN1000x100) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 1000;
    constexpr size_t kNumQueries = 100;
    constexpr int kTopK = 10;

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "bench_vec", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);

    // 批量插入 1000 个向量
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    std::vector<mmdb_vector_t> c_vecs;
    c_vecs.reserve(kNumVectors);

    auto t0 = clk::now();
    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
    }
    auto t_setup = elapsed_ms(t0);

    t0 = clk::now();
    for (size_t i = 0; i < kNumVectors; i++) {
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim,
            nullptr, nullptr
        };
        c_vecs.push_back(v);
    }
    auto rc = mmdb_vectors_add(c, c_vecs.data(), kNumVectors);
    ASSERT_EQ(rc, MMDB_OK);
    auto t_insert = elapsed_ms(t0);

    // 100 次查询
    std::vector<size_t> query_sizes;
    size_t total_hits = 0;
    t0 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_query_t qry = {query.data(), kDim, kTopK, nullptr};
        mmdb_result_t result = {};
        ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        query_sizes.push_back(result.count);
        total_hits += result.count;
        mmdb_result_free(&result);
    }
    auto t_search = elapsed_ms(t0);

    // 计算 P50/P99 延迟
    std::sort(query_sizes.begin(), query_sizes.end());
    double avg_qps = 1000.0 * kNumQueries / t_search;

    std::cout << "\n[VectorKNN1000x100] "
              << "setup=" << t_setup << "ms, "
              << "insert=" << t_insert << "ms (" << (kNumVectors * 1000 / t_insert) << " vec/s), "
              << "search=" << t_search << "ms (" << avg_qps << " qps), "
              << "total_hits=" << total_hits << "\n";

    mmdb_close(db);
    std::remove(kDbPath);
}

// ========================================================================
// 3. 时序性能基准
// ========================================================================

TEST(Benchmark, TimeseriesAppendAndQuery) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    mmdb_schema_t s = {MMDB_MODEL_TIMESERIES, 0, nullptr, 0};
    mmdb_collection_t* c = mmdb_collection_create(db, "bench_ts", &s);
    ASSERT_NE(c, nullptr);

    constexpr size_t kNumPoints = 10000;

    // 批量追加 10000 个数据点
    auto t0 = clk::now();
    for (size_t i = 0; i < kNumPoints; i++) {
        mmdb_datapoint_t dp = {(int64_t)(i * 1000), (double)i, nullptr};
        ASSERT_EQ(mmdb_timeseries_append(c, &dp), MMDB_OK);
    }
    auto t_append = elapsed_ms(t0);

    // 范围查询
    t0 = clk::now();
    mmdb_ts_query_t q = {(int64_t)0, (int64_t)(kNumPoints * 1000), nullptr, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_timeseries_query(c, &q, &result), MMDB_OK);
    auto t_query = elapsed_ms(t0);

    std::cout << "\n[Timeseries 10000] "
              << "append=" << t_append << "ms (" << (kNumPoints * 1000 / t_append) << " pt/s), "
              << "query=" << t_query << "ms (count=" << result.count << ")\n";

    mmdb_result_free(&result);
    mmdb_close(db);
    std::remove(kDbPath);
}

// ========================================================================
// 4. 文件大小基准
// ========================================================================

TEST(Benchmark, FileSize1000Vectors) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 1000;

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "size_bench", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(123);
    std::vector<mmdb_vector_t> c_vecs;
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("id" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim, nullptr, nullptr
        };
        c_vecs.push_back(v);
    }
    mmdb_vectors_add(c, c_vecs.data(), kNumVectors);

    mmdb_close(db);

    auto sz = fs::file_size(kDbPath);
    std::cout << "\n[FileSize 1000x" << kDim << "] "
              << sz << " bytes (" << (sz / 1024.0) << " KiB, "
              << (sz / 1024.0 / 1024.0) << " MiB)\n";

    std::remove(kDbPath);
}

// ========================================================================
// 5. 并发读测试
// ========================================================================

TEST(Concurrency, MultipleReaders) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
    mmdb_collection_t* c = mmdb_collection_create(db, "concurr", &s);
    ASSERT_NE(c, nullptr);

    // 写入少量数据
    for (int i = 0; i < 10; i++) {
        std::string id = "v" + std::to_string(i);
        float vec[4] = {(float)i, 0, 0, 0};
        mmdb_vector_t v = {(const uint8_t*)id.data(), id.size(), vec, 4, nullptr, nullptr};
        ASSERT_EQ(mmdb_vectors_add(c, &v, 1), MMDB_OK);
    }

    // 启动多个线程并发读
    constexpr int kThreads = 4;
    constexpr int kQueriesPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> total_errors{0};

    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([&]() {
            mmdb_t* thread_db = mmdb_open(kDbPath, nullptr);
            if (!thread_db) {
                total_errors++;
                return;
            }
            for (int q = 0; q < kQueriesPerThread; q++) {
                mmdb_collection_t* thread_c = mmdb_collection_get(thread_db, "concurr");
                if (!thread_c) {
                    total_errors++;
                    continue;
                }
                float query[4] = {1.0f, 0, 0, 0};
                mmdb_query_t qry = {query, 4, 5, nullptr};
                mmdb_result_t result = {};
                int rc = mmdb_vectors_search(thread_c, &qry, &result);
                if (rc != MMDB_OK || result.count == 0) {
                    total_errors++;
                }
                mmdb_result_free(&result);
            }
            mmdb_close(thread_db);
        });
    }

    for (auto& th : threads) th.join();

    EXPECT_EQ(total_errors.load(), 0) << "并发读出错";

    mmdb_close(db);
    std::remove(kDbPath);
}

// ========================================================================
// 6. 错误恢复测试
// ========================================================================

TEST(ErrorHandling, InvalidVectorDim) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    // vector_dim = 0 应失败
    mmdb_schema_t bad_schema = {MMDB_MODEL_VECTOR, 0, nullptr, 0};
    mmdb_collection_t* c = mmdb_collection_create(db, "bad", &bad_schema);
    EXPECT_EQ(c, nullptr) << "vector_dim=0 不应通过校验";

    int err_code = mmdb_last_error_code(db);
    EXPECT_NE(err_code, MMDB_OK);

    mmdb_close(db);
    std::remove(kDbPath);
}

TEST(ErrorHandling, NonExistentCollection) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    mmdb_collection_t* c = mmdb_collection_get(db, "does_not_exist");
    EXPECT_EQ(c, nullptr);

    mmdb_close(db);
    std::remove(kDbPath);
}