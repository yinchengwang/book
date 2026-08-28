#ifndef DB_VECTOR_CONCURRENCY_E2E_H
#define DB_VECTOR_CONCURRENCY_E2E_H
/* C4-2 T13: Vector 并发插入搜索端到端测试骨架（依赖 C1-2 已修） */
#include <gtest/gtest.h>
#include <atomic>
#include <thread>

extern "C" {
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
}

namespace {
const int kDim = 32;
const int kInsertCount = 100;
}  // namespace

TEST(VectorConcurrentE2E, InsertSearchStability) {
    faiss_hnsw_t *idx = faiss_hnsw_index_create(8, kDim, 16,
                                                 DISTANCE_METRIC_L2,
                                                 QUANTIZATION_TYPE_NONE);
    ASSERT_NE(idx, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};

    std::thread writer([&]() {
        float v[kDim];
        for (int i = 0; i < kInsertCount && running; ++i) {
            for (int j = 0; j < kDim; ++j) v[j] = (float)(i + j);
            if (faiss_hnsw_index_add(idx, 1, v) != 0) errors.fetch_add(1);
        }
    });

    std::thread reader([&]() {
        float q[kDim] = {0};
        float dists[4]; int32_t ids[4];
        for (int t = 0; t < 50 && running; ++t) {
            if (faiss_hnsw_index_search(idx, q, 4, 16, dists, ids) < 0)
                errors.fetch_add(1);
        }
    });

    writer.join();
    running.store(false);
    reader.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ((int)faiss_hnsw_index_ntotal(idx), kInsertCount);
    faiss_hnsw_index_drop(idx);
}
#endif
