// staircase_benchmark.cpp — 向量 KNN 阶梯基准测试
//
// 测试阶梯：1K → 10K → 100K → 1M → 10M
// 每档执行：
//   1. insert 性能（测量 vec/s）
//   2. search 性能（20 queries × K=10，测量 qps + P50/P99 延迟）
//   3. Recall@10 验证（用 recall_helper.h 的 brute_force_top10）
//
// 10M 测试因 ~5GB 内存需求，使用 GTEST_SKIP 跳过。
// 测试结束后清理临时数据库文件（每个测试用独立 db 文件）。
//
// 文件位置：engineering/test/sdk/integration/staircase_benchmark.cpp

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <string>
#include <unordered_set>

#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"

#include "recall_helper.h"

namespace fs = std::filesystem;
using clk = std::chrono::high_resolution_clock;

namespace {

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

// 检查 ID 是否为合法格式（"v" + 纯数字）
[[maybe_unused]] bool is_valid_v_id(const std::string& id) {
    if (id.size() <= 1 || id[0] != 'v') return false;
    return std::all_of(id.begin() + 1, id.end(), ::isdigit);
}

// 单档阶梯测试核心逻辑
struct StaircaseResult {
    size_t num_vectors;
    size_t num_queries;
    double insert_ms;
    double search_ms;
    double qps;
    double p50_ms;
    double p99_ms;
    double recall_at_10;
    size_t vec_per_sec;
};

// 注意：run_staircase 返回 StaircaseResult（非 void），
// 不能在函数体内使用 ASSERT_*（会引入 return），必须用 EXPECT_*。
StaircaseResult run_staircase(
    const std::string& db_path,
    const std::string& collection_name,
    size_t kNumVectors,
    size_t kNumQueries,
    size_t kDim,
    int kTopK,
    std::mt19937& rng,
    bool compute_recall = true) {
    StaircaseResult res{};
    res.num_vectors = kNumVectors;
    res.num_queries = kNumQueries;
    const size_t kTopKSize = (size_t)kTopK;

    std::remove(db_path.c_str());

    mmdb_t* db = mmdb_open(db_path.c_str(), nullptr);
    EXPECT_NE(db, nullptr);
    if (!db) return res;

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, collection_name.c_str(), &s);
    EXPECT_NE(c, nullptr);
    if (!c) {
        mmdb_close(db);
        std::remove(db_path.c_str());
        return res;
    }

    // 1. 批量插入所有向量
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    std::vector<mmdb_vector_t> c_vecs;
    ids.reserve(kNumVectors);
    vectors.reserve(kNumVectors);
    c_vecs.reserve(kNumVectors);

    auto t0 = clk::now();
    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
    }
    for (size_t i = 0; i < kNumVectors; i++) {
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim,
            nullptr, nullptr
        };
        c_vecs.push_back(v);
    }
    auto rc = mmdb_vectors_add(c, c_vecs.data(), kNumVectors);
    EXPECT_EQ(rc, MMDB_OK);
    res.insert_ms = elapsed_ms(t0);
    res.vec_per_sec = (size_t)((double)kNumVectors * 1000.0 / res.insert_ms);

    // 2. warmup（首次查询会触发 HNSW 构建）
    {
        auto warmup = random_vector(kDim, rng);
        mmdb_query_t qry = {warmup.data(), kDim, kTopKSize, nullptr};
        mmdb_result_t result = {};
        EXPECT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        mmdb_result_free(&result);
    }

    // 3. 性能查询（kNumQueries 次，测量 qps + 延迟）
    std::vector<double> latencies;
    latencies.reserve(kNumQueries);
    t0 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_query_t qry = {query.data(), kDim, kTopKSize, nullptr};
        mmdb_result_t result = {};
        auto q0 = clk::now();
        EXPECT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        latencies.push_back(elapsed_ms(q0));
        mmdb_result_free(&result);
    }
    res.search_ms = elapsed_ms(t0);
    res.qps = 1000.0 * kNumQueries / res.search_ms;

    // P50 / P99 延迟
    std::sort(latencies.begin(), latencies.end());
    res.p50_ms = latencies[latencies.size() / 2];
    res.p99_ms = latencies[(size_t)(latencies.size() * 0.99)];

    // 4. Recall@10 计算
    if (compute_recall && kNumVectors <= 10000) {
        // 小规模：全集暴力搜索
        std::vector<double> recalls;
        recalls.reserve(kNumQueries);
        for (size_t q = 0; q < kNumQueries; q++) {
            auto query = random_vector(kDim, rng);
            mmdb_query_t qry = {query.data(), kDim, kTopKSize, nullptr};
            mmdb_result_t result = {};
            EXPECT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);

            // brute force top-K
            std::vector<std::pair<float, size_t>> dists;
            dists.reserve(kNumVectors);
            for (size_t i = 0; i < kNumVectors; i++) {
                float sum = 0.0f;
                for (size_t d = 0; d < kDim; d++) {
                    float diff = query[d] - vectors[i][d];
                    sum += diff * diff;
                }
                dists.push_back({sum, i});
            }
            std::partial_sort(dists.begin(), dists.begin() + kTopKSize, dists.end());

            // 计算交集
            std::unordered_set<std::string> gt_ids;
            for (int i = 0; i < kTopK; i++) gt_ids.insert(ids[dists[i].second]);
            std::unordered_set<std::string> search_ids;
            for (size_t i = 0; i < result.count && i < kTopKSize; i++) {
                std::string id((const char*)result.items[i].id, result.items[i].id_len);
                search_ids.insert(id);
            }
            size_t hits = 0;
            for (auto& gid : gt_ids) if (search_ids.count(gid)) hits++;
            recalls.push_back((double)hits / gt_ids.size());

            mmdb_result_free(&result);
        }
        double sum = 0.0;
        for (double r : recalls) sum += r;
        res.recall_at_10 = sum / recalls.size();
    } else if (compute_recall) {
        // 大规模：从全集随机抽 1K 子集做 brute-force
        constexpr size_t kSubsetSize = 1000;
        std::vector<size_t> subset_indices(kNumVectors);
        for (size_t i = 0; i < kNumVectors; i++) subset_indices[i] = i;
        std::shuffle(subset_indices.begin(), subset_indices.end(), rng);
        subset_indices.resize(kSubsetSize);

        std::vector<std::vector<float>> subset_vecs(kSubsetSize);
        for (size_t i = 0; i < kSubsetSize; i++) subset_vecs[i] = vectors[subset_indices[i]];

        std::vector<double> recalls;
        recalls.reserve(kNumQueries);
        size_t recall_total_queries = 0;
        for (size_t q = 0; q < kNumQueries; q++) {
            auto query = random_vector(kDim, rng);
            mmdb_query_t qry = {query.data(), kDim, kTopKSize, nullptr};
            mmdb_result_t result = {};
            EXPECT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
            if (result.count == 0) {
                recalls.push_back(0.0);
                recall_total_queries++;
                continue;
            }

            // brute-force top-10 在全集上，不走子集采样，保证精确 Recall
            std::vector<std::pair<float, size_t>> dists;
            dists.reserve(kNumVectors);
            for (size_t i = 0; i < kNumVectors; i++) {
                float sum = 0.0f;
                for (size_t d = 0; d < kDim; d++) {
                    float diff = query[d] - vectors[i][d];
                    sum += diff * diff;
                }
                dists.push_back({sum, i});
            }
            std::partial_sort(dists.begin(), dists.begin() + kTopKSize, dists.end());

            std::unordered_set<std::string> gt_ids;
            for (size_t i = 0; i < kTopKSize; i++) gt_ids.insert(ids[dists[i].second]);
            std::unordered_set<std::string> search_ids;
            for (size_t i = 0; i < result.count && i < kTopKSize; i++) {
                std::string id((const char*)result.items[i].id, result.items[i].id_len);
                search_ids.insert(id);
            }
            size_t hits = 0;
            for (auto& gid : gt_ids) if (search_ids.count(gid)) hits++;
            recalls.push_back((double)hits / gt_ids.size());
            recall_total_queries++;

            mmdb_result_free(&result);
        }
        double sum = 0.0;
        for (double r : recalls) sum += r;
        res.recall_at_10 = (recall_total_queries > 0) ? sum / recall_total_queries : 0.0;
    } else {
        res.recall_at_10 = -1.0;
    }

    mmdb_close(db);
    std::remove(db_path.c_str());
    return res;
}

void print_staircase_result(const std::string& label, const StaircaseResult& r) {
    std::cout << "\n[" << label << "] "
              << "N=" << r.num_vectors << ", "
              << "insert=" << r.insert_ms << "ms (" << r.vec_per_sec << " vec/s), "
              << "search=" << r.search_ms << "ms (" << r.qps << " qps, " << r.num_queries << " queries), "
              << "latency p50=" << r.p50_ms << "ms p99=" << r.p99_ms << "ms";
    if (r.recall_at_10 >= 0.0) {
        std::cout << ", Recall@10=" << r.recall_at_10;
    }
    std::cout << "\n";
}

}  // namespace

// ========================================================================
// 阶梯基准测试：1K / 10K / 100K / 1M / 10M
// ========================================================================

TEST(Staircase, VectorKNN1K) {
    std::mt19937 rng(42);
    auto r = run_staircase(
        "test_staircase_1k.db",
        "staircase_1k",
        /*num_vectors=*/1000,
        /*num_queries=*/20,
        /*dim=*/128,
        /*k=*/10,
        rng,
        /*compute_recall=*/true
    );
    print_staircase_result("Staircase.VectorKNN1K", r);
    EXPECT_GT(r.vec_per_sec, 0u);
    EXPECT_GT(r.qps, 0.0);
    EXPECT_GE(r.recall_at_10, 0.95) << "Recall@10 未达 0.95 目标";
}

TEST(Staircase, VectorKNN10K) {
    std::mt19937 rng(42);
    auto r = run_staircase(
        "test_staircase_10k.db",
        "staircase_10k",
        /*num_vectors=*/10000,
        /*num_queries=*/20,
        /*dim=*/128,
        /*k=*/10,
        rng,
        /*compute_recall=*/true
    );
    print_staircase_result("Staircase.VectorKNN10K", r);
    EXPECT_GT(r.vec_per_sec, 0u);
    EXPECT_GT(r.qps, 0.0);
    EXPECT_GE(r.recall_at_10, 0.95) << "Recall@10 未达 0.95 目标";
}

TEST(Staircase, VectorKNN100K) {
    std::mt19937 rng(42);
    auto r = run_staircase(
        "test_staircase_100k.db",
        "staircase_100k",
        /*num_vectors=*/100000,
        /*num_queries=*/20,
        /*dim=*/128,
        /*k=*/10,
        rng,
        /*compute_recall=*/true
    );
    print_staircase_result("Staircase.VectorKNN100K", r);
    EXPECT_GT(r.vec_per_sec, 0u);
    EXPECT_GT(r.qps, 0.0);
    EXPECT_GE(r.recall_at_10, 0.95) << "Recall@10 未达 0.95 目标";
}

TEST(Staircase, VectorKNN1M) {
    std::mt19937 rng(42);
    auto r = run_staircase(
        "test_staircase_1m.db",
        "staircase_1m",
        /*num_vectors=*/1000000,
        /*num_queries=*/20,
        /*dim=*/128,
        /*k=*/10,
        rng,
        /*compute_recall=*/true
    );
    print_staircase_result("Staircase.VectorKNN1M", r);
    EXPECT_GT(r.vec_per_sec, 0u);
    EXPECT_GT(r.qps, 0.0);
    EXPECT_GE(r.recall_at_10, 0.85) << "Recall@10 未达 0.85 目标";
}

TEST(Staircase, VectorKNN10M) {
    // 10M 规模需 ~5GB 内存 + ~10min 跑时间
    // CI 中通常因内存/时间不足无法完成，此处用 GTEST_SKIP 优雅跳过
    size_t kNumVectors = 10000000;
    size_t kNumQueries = 20;
    size_t kDim = 128;
    int kTopK = 10;
    size_t kTopKSize = (size_t)kTopK;
    size_t kBatchSize = 100000;

    std::remove("test_staircase_10m.db");
    mmdb_t* db = mmdb_open("test_staircase_10m.db", nullptr);
    ASSERT_NE(db, nullptr);

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "staircase_10m", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);
    auto t0 = clk::now();

    bool out_of_memory = false;
    for (size_t offset = 0; offset < kNumVectors; offset += kBatchSize) {
        size_t batch = std::min(kBatchSize, kNumVectors - offset);
        std::vector<std::string> batch_ids;
        std::vector<std::vector<float>> batch_vec_data;
        std::vector<mmdb_vector_t> batch_vecs;
        batch_ids.reserve(batch);
        batch_vec_data.reserve(batch);
        batch_vecs.reserve(batch);

        try {
            for (size_t i = 0; i < batch; i++) {
                batch_ids.push_back("v" + std::to_string(offset + i));
                batch_vec_data.push_back(random_vector(kDim, rng));
            }
            for (size_t i = 0; i < batch; i++) {
                mmdb_vector_t v = {
                    (const uint8_t*)batch_ids[i].data(), batch_ids[i].size(),
                    batch_vec_data[i].data(), kDim, nullptr, nullptr
                };
                batch_vecs.push_back(v);
            }
        } catch (const std::bad_alloc&) {
            out_of_memory = true;
            break;
        }

        auto rc = mmdb_vectors_add(c, batch_vecs.data(), batch);
        ASSERT_EQ(rc, MMDB_OK) << "批量插入失败 offset=" << offset;
    }

    auto t_insert = elapsed_ms(t0);
    double vec_per_sec = (double)kNumVectors * 1000.0 / t_insert;

    if (out_of_memory) {
        std::cout << "\n[Staircase.VectorKNN10M] 环境不足跳过（std::bad_alloc）：需要 ~5GB 内存"
                  << "（insert 累计 " << t_insert << "ms）\n";
        mmdb_close(db);
        std::remove("test_staircase_10m.db");
        GTEST_SKIP() << "10M 阶梯基准需要 ~5GB 内存，CI 环境内存不足，跳过";
    }

    std::cout << "\n[Staircase.VectorKNN10M] insert=" << t_insert << "ms ("
              << (size_t)vec_per_sec << " vec/s)\n";

    // 查询
    std::vector<double> latencies;
    latencies.reserve(kNumQueries);
    t0 = clk::now();
    for (size_t q = 0; q < kNumQueries; q++) {
        auto query = random_vector(kDim, rng);
        mmdb_query_t qry = {query.data(), kDim, kTopKSize, nullptr};
        mmdb_result_t result = {};
        auto q0 = clk::now();
        ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
        latencies.push_back(elapsed_ms(q0));
        mmdb_result_free(&result);
    }
    auto t_search = elapsed_ms(t0);
    double qps = 1000.0 * kNumQueries / t_search;

    std::sort(latencies.begin(), latencies.end());
    double p50 = latencies[latencies.size() / 2];
    double p99 = latencies[(size_t)(latencies.size() * 0.99)];

    std::cout << "[Staircase.VectorKNN10M] search=" << t_search << "ms ("
              << qps << " qps), latency p50=" << p50 << "ms p99=" << p99 << "ms\n";

    mmdb_close(db);
    std::remove("test_staircase_10m.db");

    EXPECT_GT(vec_per_sec, 0.0);
    EXPECT_GT(qps, 0.0);
}