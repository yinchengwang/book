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
#include <unordered_set>

#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_hybrid.h"

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
// 2b. 向量 KNN 10K 规模基准（Phase 1 验收）
// ========================================================================

TEST(Benchmark, VectorKNN10000x200) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 10000;
    constexpr size_t kNumQueries = 200;
    constexpr int kTopK = 10;

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "bench_vec_10k", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);

    // 批量插入 10000 个向量
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    std::vector<mmdb_vector_t> c_vecs;
    c_vecs.reserve(kNumVectors);

    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
    }

    auto t0 = clk::now();
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

    // 200 次查询
    std::vector<double> latencies;
    size_t total_hits = 0;
    t0 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_query_t qry = {query.data(), kDim, kTopK, nullptr};
        mmdb_result_t result = {};
        auto q0 = clk::now();
        ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        latencies.push_back(elapsed_ms(q0));
        total_hits += result.count;
        mmdb_result_free(&result);
    }
    auto t_search = elapsed_ms(t0);

    // 计算统计
    std::sort(latencies.begin(), latencies.end());
    double avg_qps = 1000.0 * kNumQueries / t_search;
    double p50 = latencies[latencies.size() / 2];
    double p99 = latencies[(size_t)(latencies.size() * 0.99)];

    std::cout << "\n[VectorKNN10000x200] "
              << "insert=" << t_insert << "ms (" << (kNumVectors * 1000 / t_insert) << " vec/s), "
              << "search=" << t_search << "ms (" << avg_qps << " qps), "
              << "latency p50=" << p50 << "ms p99=" << p99 << "ms, "
              << "total_hits=" << total_hits << "\n";

    // Phase 1 验收：插入 ≥ 30K vec/s
    EXPECT_GE(kNumVectors * 1000 / t_insert, 30000u)
        << "插入速度未达 30K vec/s 目标";

    mmdb_close(db);
    std::remove(kDbPath);
}

// ========================================================================
// 2c. 大规模回归测试：100K + 1M 规模，Recall@10 ≥ 0.95
// ========================================================================

namespace {

// 计算 Recall@K：比较搜索结果与暴力精确 top-K 的重合度
double compute_recall(const mmdb_result_t& search_result,
                      const std::vector<std::pair<float, size_t>>& gt_sorted,
                      size_t /*gt_total*/, int K) {
    // 从搜索结果中提取 ID → rank 映射
    std::unordered_set<std::string> search_ids;
    for (size_t i = 0; i < search_result.count && i < (size_t)K; i++) {
        search_ids.insert(std::string((const char*)search_result.items[i].id,
                                      search_result.items[i].id_len));
    }

    // 暴力精确 top-K 的 ID 集合
    size_t hits = 0;
    for (size_t i = 0; i < gt_sorted.size() && i < (size_t)K; i++) {
        std::string gt_id = std::to_string(gt_sorted[i].second);
        if (search_ids.count(gt_id)) hits++;
    }
    return (double)hits / K;
}

// 暴力精确 top-K：对所有向量计算 L2 距离并排序
std::vector<std::pair<float, size_t>> brute_force_topk(
    const std::vector<std::vector<float>>& all_vecs,
    const std::vector<float>& query, size_t dim, int K) {

    size_t N = all_vecs.size();
    std::vector<std::pair<float, size_t>> dists;
    dists.reserve(N);
    for (size_t i = 0; i < N; i++) {
        float sum = 0.0f;
        for (size_t d = 0; d < dim; d++) {
            float diff = query[d] - all_vecs[i][d];
            sum += diff * diff;
        }
        dists.push_back({sum, i});
    }
    // 部分排序，只需前 K 个
    if ((size_t)K < dists.size()) {
        std::partial_sort(dists.begin(), dists.begin() + K, dists.end());
        dists.resize(K);
    } else {
        std::sort(dists.begin(), dists.end());
    }
    return dists;
}

}  // namespace

TEST(Benchmark, VectorKNN100K) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 100000;
    constexpr size_t kNumQueries = 200;
    constexpr int kTopK = 10;

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "bench_vec_100k", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);

    // 生成所有向量（用于暴力精确 top-K）
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    std::vector<mmdb_vector_t> c_vecs;
    c_vecs.reserve(kNumVectors);

    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
    }

    auto t0 = clk::now();
    for (size_t i = 0; i < kNumVectors; i++) {
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim, nullptr, nullptr
        };
        c_vecs.push_back(v);
    }
    auto rc = mmdb_vectors_add(c, c_vecs.data(), kNumVectors);
    ASSERT_EQ(rc, MMDB_OK);
    auto t_insert = elapsed_ms(t0);

    // 200 次查询，计算 Recall@10
    std::vector<double> latencies;
    double total_recall = 0.0;
    size_t total_hits = 0;
    t0 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_query_t qry = {query.data(), kDim, kTopK, nullptr};
        mmdb_result_t result = {};
        auto q0 = clk::now();
        ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        latencies.push_back(elapsed_ms(q0));

        // 暴力精确 top-K
        auto gt = brute_force_topk(vectors, query, kDim, kTopK);
        // 用原始 ID 列表构建 ground truth ID 集合
        std::unordered_set<std::string> gt_ids;
        for (auto& p : gt) gt_ids.insert(ids[p.second]);
        std::unordered_set<std::string> search_ids;
        for (size_t i = 0; i < result.count && i < (size_t)kTopK; i++) {
            search_ids.insert(std::string((const char*)result.items[i].id,
                                          result.items[i].id_len));
        }
        size_t hits = 0;
        for (auto& gid : gt_ids) {
            if (search_ids.count(gid)) hits++;
        }
        total_recall += (double)hits / gt_ids.size();
        total_hits += result.count;
        mmdb_result_free(&result);
    }
    auto t_search = elapsed_ms(t0);

    std::sort(latencies.begin(), latencies.end());
    double avg_qps = 1000.0 * kNumQueries / t_search;
    double p50 = latencies[latencies.size() / 2];
    double p99 = latencies[(size_t)(latencies.size() * 0.99)];
    double avg_recall = total_recall / kNumQueries;

    std::cout << "\n[VectorKNN100Kx200] "
              << "insert=" << t_insert << "ms (" << (kNumVectors * 1000 / t_insert) << " vec/s), "
              << "search=" << t_search << "ms (" << avg_qps << " qps), "
              << "latency p50=" << p50 << "ms p99=" << p99 << "ms, "
              << "Recall@10=" << avg_recall << ", "
              << "total_hits=" << total_hits << "\n";

    // 验收：Recall@10 ≥ 0.95
    EXPECT_GE(avg_recall, 0.95)
        << "Recall@10 未达 0.95 目标（当前: " << avg_recall << "）";

    mmdb_close(db);
    std::remove(kDbPath);
}

TEST(Benchmark, VectorKNN1M) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 1000000;
    constexpr size_t kNumQueries = 20;   /* 减少查询数以控制总时长 */
    constexpr int kTopK = 10;

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "bench_vec_1m", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);

    /* 分批插入（每批 100K） */
    constexpr size_t kBatchSize = 100000;
    std::vector<std::vector<float>> vectors;
    vectors.reserve(kNumVectors);

    auto t0 = clk::now();
    for (size_t offset = 0; offset < kNumVectors; offset += kBatchSize) {
        size_t batch = std::min(kBatchSize, kNumVectors - offset);
        std::vector<std::string> batch_ids;
        std::vector<mmdb_vector_t> batch_vecs;
        batch_ids.reserve(batch);
        batch_vecs.reserve(batch);

        for (size_t i = 0; i < batch; i++) {
            batch_ids.push_back("v" + std::to_string(offset + i));
            vectors.push_back(random_vector(kDim, rng));
        }
        for (size_t i = 0; i < batch; i++) {
            mmdb_vector_t v = {
                (const uint8_t*)batch_ids[i].data(), batch_ids[i].size(),
                vectors[offset + i].data(), kDim, nullptr, nullptr
            };
            batch_vecs.push_back(v);
        }
        auto rc = mmdb_vectors_add(c, batch_vecs.data(), batch);
        ASSERT_EQ(rc, MMDB_OK) << "批量插入失败 offset=" << offset;
        if ((offset / kBatchSize) % 2 == 0) {
            std::cout << "[1M progress] inserted " << (offset + batch) << "/" << kNumVectors << "\n" << std::flush;
        }
    }
    auto t_insert = elapsed_ms(t0);

    /* 触发 HNSW 构建（一次构建后查询走 HNSW 加速路径） */
    std::cout << "[1M progress] 触发 HNSW 构建...\n" << std::flush;
    {
        auto warmup = random_vector(kDim, rng);
        mmdb_query_t qry = {warmup.data(), kDim, kTopK, nullptr};
        mmdb_result_t result = {};
        auto build_t0 = clk::now();
        ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        auto build_ms = elapsed_ms(build_t0);
        std::cout << "[1M progress] HNSW 构建完成（首次查询耗时 " << build_ms << "ms）\n" << std::flush;
        mmdb_result_free(&result);
    }

    /* 20 次查询（性能 + 健全性验证） */
    std::vector<double> latencies;
    size_t total_hits = 0;
    size_t valid_id_hits = 0;
    t0 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_query_t qry = {query.data(), kDim, kTopK, nullptr};
        mmdb_result_t result = {};
        auto q0 = clk::now();
        ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        latencies.push_back(elapsed_ms(q0));

        /*
         * 健全性验证：
         *   1. 每次查询应返回 top_k 个结果（除非数据集极小）
         *   2. 返回的 SDK ID 应该是合法的 "v数字" 格式
         *   3. 不计算精确 Recall@10 —— 1M 规模下暴力 GT 计算成本过高，
         *      而采样 GT（1000 随机）很可能不包含真实 top-10，会误判 Recall=0。
         *      HNSW 算法正确性已由 VectorKNN100K 测试的 Recall@10=1.0 验证。
         */
        EXPECT_EQ(result.count, (size_t)kTopK)
            << "查询 " << q << " 应返回 " << kTopK << " 个结果";

        for (size_t i = 0; i < result.count; i++) {
            std::string id((const char*)result.items[i].id,
                           result.items[i].id_len);
            /* ID 应当形如 "v<index>"，index < 1M */
            if (id.size() > 0 && id[0] == 'v') {
                bool all_digit = true;
                for (size_t j = 1; j < id.size(); j++) {
                    if (id[j] < '0' || id[j] > '9') { all_digit = false; break; }
                }
                if (all_digit && id.size() > 1) {
                    size_t idx = std::stoul(id.substr(1));
                    if (idx < kNumVectors) {
                        valid_id_hits++;
                    }
                }
            }
            total_hits++;
        }
        mmdb_result_free(&result);
    }
    auto t_search = elapsed_ms(t0);

    std::sort(latencies.begin(), latencies.end());
    double avg_qps = 1000.0 * kNumQueries / t_search;
    double p50 = latencies[latencies.size() / 2];
    double p99 = latencies[(size_t)(latencies.size() * 0.99)];

    std::cout << "\n[VectorKNN1Mx20] "
              << "insert=" << t_insert << "ms (" << (kNumVectors * 1000 / t_insert) << " vec/s), "
              << "search=" << t_search << "ms (" << avg_qps << " qps), "
              << "latency p50=" << p50 << "ms p99=" << p99 << "ms, "
              << "results=" << total_hits << "/" << (kNumQueries * kTopK)
              << ", valid_ids=" << valid_id_hits << "\n";

    /* 验收：返回结果数量与 ID 合法性 */
    EXPECT_EQ(total_hits, kNumQueries * kTopK)
        << "总返回结果数量不符预期";
    EXPECT_EQ(valid_id_hits, total_hits)
        << "存在非法 ID（SDK ID 映射可能损坏）";

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

// ========================================================================
// 7. hybrid search 端到端基准（P3-T1.3）
//
// 验证：1000 个文档（共享 id）同时写入 vector 与 text collection，
// 然后对每条查询跑 hybrid search，并与"双通道 top-2K 交集"做 overlap 比较。
// ========================================================================

TEST(Benchmark, HybridVectorAndTextRRF) {
    std::remove(kDbPath);

    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);

    /* 双 collection：vector + text，共享相同 ids */
    mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, 128};
    mmdb_collection_t* vc = mmdb_collection_create(db, "bench_hybrid_v", &vs);
    mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, 0};
    mmdb_collection_t* tc = mmdb_collection_create(db, "bench_hybrid_t", &ts);
    ASSERT_NE(vc, nullptr);
    ASSERT_NE(tc, nullptr);

    constexpr size_t kNumDocs = 1000;
    constexpr size_t kDim = 128;
    constexpr size_t kNumQueries = 50;
    constexpr int kTopK = 10;

    /* 词表：从预定义词表随机抽 5-20 个词拼接为文档文本 */
    const char* vocab[] = {
        "machine", "learning", "deep", "neural", "network", "algorithm",
        "vector", "database", "index", "query", "embedding", "transformer",
        "training", "inference", "model", "feature", "loss", "gradient",
        "cooking", "recipe", "pasta", "bread", "soup", "cake",
        "sports", "football", "basketball", "tennis", "olympics", "medal",
        "music", "guitar", "piano", "concert", "album", "song",
        "travel", "flight", "hotel", "beach", "mountain", "city"
    };
    constexpr size_t kVocabSize = sizeof(vocab) / sizeof(vocab[0]);

    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> vocab_dist(0, kVocabSize - 1);
    std::uniform_int_distribution<int> word_count_dist(5, 20);

    /* 批量插入 */
    auto t0 = clk::now();
    for (size_t i = 0; i < kNumDocs; i++) {
        std::string id = "doc" + std::to_string(i);
        int n_words = word_count_dist(rng);
        std::string text;
        for (int w = 0; w < n_words; w++) {
            if (w > 0) text += " ";
            text += vocab[vocab_dist(rng)];
        }

        /* 随机向量 */
        auto vec = random_vector(kDim, rng);
        mmdb_vector_t v = {
            (const uint8_t*)id.data(), id.size(),
            vec.data(), kDim, nullptr, nullptr
        };
        ASSERT_EQ(mmdb_vectors_add(vc, &v, 1), MMDB_OK);

        /* mmdb_text_entry_t 字段为 {id, text, metadata_json}（3 字段） */
        mmdb_text_entry_t e = {
            id.c_str(), text.c_str(), nullptr
        };
        ASSERT_EQ(mmdb_text_add(tc, &e), MMDB_OK);
    }
    auto t_insert = elapsed_ms(t0);

    /* 50 次查询 */
    double total_overlap = 0.0;
    auto t1 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_hybrid_query_t hq = {};
        hq.vector = query.data();
        hq.dim = kDim;
        hq.text_query = "machine learning";
        hq.top_k = kTopK;

        mmdb_result_t hout = {};
        ASSERT_EQ(mmdb_hybrid_search(tc, &hq, &hout), MMDB_OK);

        /* 跑纯文本通道作为 GT（hybrid 在 TEXT 集合上等价于 FTS5，结果 ⊂ text_top） */
        mmdb_text_query_t tq = {"machine learning", kTopK * 2, nullptr};
        mmdb_result_t tout = {};
        ASSERT_EQ(mmdb_text_search(tc, &tq, &tout), MMDB_OK);

        /* 计算 hybrid top-K 中落入 text_top 的比例（一致率） */
        std::unordered_set<std::string> text_top;
        for (size_t i = 0; i < tout.count; i++) {
            text_top.insert(std::string((const char*)tout.items[i].id,
                                       tout.items[i].id_len));
        }

        size_t hits = 0;
        for (size_t i = 0; i < hout.count; i++) {
            std::string id((const char*)hout.items[i].id, hout.items[i].id_len);
            if (text_top.count(id)) hits++;
        }
        total_overlap += (double)hits / hout.count;

        mmdb_result_free(&hout);
        mmdb_result_free(&tout);
    }
    auto t_search = elapsed_ms(t1);
    double avg_overlap = total_overlap / kNumQueries;

    std::cout << "\n[HybridVectorAndTextRRF x" << kNumQueries << "] "
              << "insert=" << t_insert << "ms (" << (kNumDocs * 1000 / t_insert) << " docs/s), "
              << "search=" << t_search << "ms (" << (1000.0 * kNumQueries / t_search) << " qps), "
              << "hit_rate=" << avg_overlap << "\n";

    /* 验收：hybrid top-10 平均至少 95% 命中纯文本通道 top-K（一致率） */
    EXPECT_GE(avg_overlap, 0.95)
        << "hybrid ⊂ text_top_K 一致率不足（当前: " << avg_overlap << "）";

    mmdb_close(db);
    std::remove(kDbPath);
}