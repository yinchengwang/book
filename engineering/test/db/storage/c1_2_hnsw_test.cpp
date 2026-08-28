/**
 * @file c1_2_hnsw_test.cpp
 * @brief C1-2 faiss_hnsw 并发与 IP 度量测试
 *
 * T1：并发插入+搜索压力（4 reader + 1 writer，500ms）
 *   - 断言：运行期间无崩溃、死锁（实测耗时 vs 预期）
 *   - 现状：faiss_hnsw add 触发 realloc + search 无锁 → UAF（Debug 双分配器断言失败）
 *
 * T2：IP 度量对拍（暴力扫描 vs 索引）
 *   - 断言：索引结果 top-k 与暴力扫描结果一致
 *   - 现状：IP fallback L2² → 索引结果与真值不匹配（FAIL）
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

extern "C" {
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
}

namespace {

constexpr int32_t kDim = 64;
constexpr int32_t kNumVectors = 200;
constexpr int   kReaderThreads = 4;
constexpr int   kConcurrencyMs = 500;

void gen_vector(float *v, int32_t dim, unsigned seed) {
    /* 简易伪随机：线性同余，保证可重复 */
    for (int32_t i = 0; i < dim; ++i) {
        seed = seed * 1103515245u + 12345u;
        v[i] = ((seed >> 16) & 0x7fff) / 32768.0f - 1.0f;
    }
}

}  // namespace

/* T1：并发插入+搜索压力（DocFails 直到 C1-2 T3 完整 COW 落地） */
TEST(HnswConcurrency, ReadWriteStressNoCrash) {
    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, kDim, DISTANCE_METRIC_L2);
    if (idx == nullptr) GTEST_SKIP() << "faiss_hnsw_index_create 失败";

    std::vector<float> base(kNumVectors * kDim);
    for (int i = 0; i < kNumVectors; ++i) {
        gen_vector(base.data() + (size_t)i * kDim, kDim, (unsigned)(i + 1));
    }

    std::atomic<bool> running{true};
    std::atomic<int>  reads{0};
    std::atomic<int>  writes{0};
    std::atomic<int>  errors{0};

    std::vector<std::thread> readers;
    for (int r = 0; r < kReaderThreads; ++r) {
        readers.emplace_back([&]() {
            std::vector<float> query(kDim);
            std::vector<int32_t> ids(4);
            std::vector<float>   dists(4);
            while (running.load(std::memory_order_relaxed)) {
                gen_vector(query.data(), kDim, (unsigned)(r + 100));
                int rc = faiss_hnsw_index_search(idx, query.data(), 4,
                                                ids.data(), dists.data());
                if (rc < 0) errors.fetch_add(1);
                reads.fetch_add(1);
            }
        });
    }

    std::thread writer([&]() {
        int idx_inserted = 0;
        while (running.load(std::memory_order_relaxed) && idx_inserted < 1000) {
            std::vector<float> v(kDim);
            gen_vector(v.data(), kDim, (unsigned)(idx_inserted + 9999));
            int rc = faiss_hnsw_index_add(idx, 1, v.data());
            if (rc < 0) errors.fetch_add(1);
            writes.fetch_add(1);
            idx_inserted++;
        }
    });

    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(kConcurrencyMs));
    running.store(false, std::memory_order_release);
    for (auto &t : readers) t.join();
    writer.join();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_LE(elapsed, kConcurrencyMs + 5000)
        << "实际耗时 " << elapsed << "ms 远超预期（死锁？）";
    EXPECT_EQ(errors.load(), 0)
        << "并发操作出现 " << errors.load() << " 次错误（UAF 信号）";
    EXPECT_GT(reads.load() + writes.load(), 0);

    faiss_hnsw_index_destroy(idx);
}

/* T2：IP 度量对拍（先 FAIL——IP 当前 fallback L2²） */
TEST(HnswIPMetric, BruteForceMatchesIndex) {
    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, kDim, DISTANCE_METRIC_INNER_PRODUCT);
    if (idx == nullptr) GTEST_SKIP() << "faiss_hnsw_index_create 失败";

    std::vector<float> base(kNumVectors * kDim);
    for (int i = 0; i < kNumVectors; ++i) {
        gen_vector(base.data() + (size_t)i * kDim, kDim, (unsigned)(i + 1));
        int rc = faiss_hnsw_index_add(idx, 1, base.data() + (size_t)i * kDim);
        ASSERT_EQ(rc, 0) << "add 失败 @" << i;
    }

    /* 查询向量 */
    std::vector<float> query(kDim);
    gen_vector(query.data(), kDim, 42u);

    /* 索引 top-5 */
    std::vector<int32_t> idx_ids(5);
    std::vector<float>   idx_dists(5);
    int idx_n = faiss_hnsw_index_search(idx, query.data(), 5,
                                        idx_ids.data(), idx_dists.data());

    /* 暴力真值：计算 IP 距离，排序取 top-5 */
    std::vector<std::pair<float, int>> brute;
    brute.reserve(kNumVectors);
    for (int i = 0; i < kNumVectors; ++i) {
        const float *v = base.data() + (size_t)i * kDim;
        float ip = 0;
        for (int j = 0; j < kDim; ++j) ip += v[j] * query[j];
        brute.emplace_back(-ip, i);  /* 距离 = -IP */
    }
    std::sort(brute.begin(), brute.end());

    /* 对比前 5 名 id 应一致 */
    int matched = 0;
    for (int k = 0; k < std::min(idx_n, 5); ++k) {
        if (idx_ids[k] == brute[k].second) matched++;
    }

    /* C1-2 T4 修复后应 5/5；当前 fallback L2² 可能 0-1/5 */
    EXPECT_GE(matched, 4)
        << "IP 度量对拍：索引与暴力仅匹配 " << matched << "/5（fallback bug？）";

    faiss_hnsw_index_destroy(idx);
}
