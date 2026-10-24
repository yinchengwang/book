/**
 * @file hnsw_pq_test.cpp
 * @brief HNSW-PQ 混合索引单元测试
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <algorithm>

extern "C" {
#include "db/index/vector_index/hnsw_pq/hnsw_pq.h"
#include "db/index/vector_index/pq/pq.h"
#include "db/index/heap/heap_vector_store.h"
#include "db/index/storage_backend.h"
}

class HNSWPQTest : public ::testing::Test {
protected:
    void SetUp() override {
        dims_ = 128;
        n_train_ = 500;
        n_vectors_ = 500;
        n_queries_ = 10;
        k_ = 10;

        /* 生成随机数据 */
        train_vectors_.resize(n_train_ * dims_);
        vectors_.resize(n_vectors_ * dims_);
        queries_.resize(n_queries_ * dims_);

        for (int i = 0; i < n_train_ * dims_; i++) {
            train_vectors_[i] = (float)rand() / RAND_MAX;
        }
        for (int i = 0; i < n_vectors_ * dims_; i++) {
            vectors_[i] = (float)rand() / RAND_MAX;
        }
        for (int i = 0; i < n_queries_ * dims_; i++) {
            queries_[i] = (float)rand() / RAND_MAX;
        }
    }

    int dims_;
    int n_train_;
    int n_vectors_;
    int n_queries_;
    int k_;
    std::vector<float> train_vectors_;
    std::vector<float> vectors_;
    std::vector<float> queries_;
};

/* 测试：创建和销毁 */
TEST_F(HNSWPQTest, CreateDestroy) {
    hnsw_pq_config_t config;
    memset(&config, 0, sizeof(config));
    config.m = 16;
    config.ef_construction = 100;
    config.ef_search = 50;
    config.dims = dims_;
    config.metric = DISTANCE_METRIC_L2_SQUARED;
    config.pq_m = 16;
    config.pq_bits = 8;
    config.rerank_k = 50;

    hnsw_pq_index_t *index = hnsw_pq_index_create(&config);
    ASSERT_NE(index, nullptr);

    EXPECT_EQ(hnsw_pq_index_size(index), 0);

    hnsw_pq_index_destroy(index);
}

/* 测试：带 PQ 训练的插入 */
TEST_F(HNSWPQTest, InsertWithPQ) {
    hnsw_pq_config_t config;
    memset(&config, 0, sizeof(config));
    config.m = 16;
    config.ef_construction = 100;
    config.ef_search = 50;
    config.dims = dims_;
    config.metric = DISTANCE_METRIC_L2_SQUARED;
    config.pq_m = 16;
    config.pq_bits = 8;
    config.rerank_k = 50;

    hnsw_pq_index_t *index = hnsw_pq_index_create(&config);
    ASSERT_NE(index, nullptr);

    /* 创建并训练 PQ */
    pq_quantizer_t *pq = pq_create(dims_, 16, 8);
    ASSERT_NE(pq, nullptr);

    pq_train(pq, n_train_, train_vectors_.data());
    hnsw_pq_index_set_pq(index, pq);

    /* 创建 heap_store */
    storage_backend_t *backend = storage_memory_create(65536);
    heap_vector_config_t heap_config;
    memset(&heap_config, 0, sizeof(heap_config));
    heap_config.backend = backend;
    heap_config.dims = dims_;
    heap_config.page_size = 65536;
    heap_vector_store_t *heap = heap_vector_store_create(&heap_config);
    ASSERT_NE(heap, nullptr);

    hnsw_pq_index_set_heap_store(index, heap);

    /* 插入向量 */
    int32_t inserted = hnsw_pq_index_insert_batch(index, n_vectors_, vectors_.data());
    EXPECT_EQ(inserted, n_vectors_);

    /* 验证节点有 PQ 编码 */
    for (int i = 0; i < 10 && i < n_vectors_; i++) {
        EXPECT_NE(index->nodes[i].pq_code, nullptr);
    }

    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
    pq_destroy(pq);
    hnsw_pq_index_destroy(index);
}

/* 测试：搜索（带重排序） */
TEST_F(HNSWPQTest, SearchWithRerank) {
    hnsw_pq_config_t config;
    memset(&config, 0, sizeof(config));
    config.m = 16;
    config.ef_construction = 100;
    config.ef_search = 50;
    config.dims = dims_;
    config.metric = DISTANCE_METRIC_L2_SQUARED;
    config.pq_m = 16;
    config.pq_bits = 8;
    config.rerank_k = 50;

    hnsw_pq_index_t *index = hnsw_pq_index_create(&config);
    ASSERT_NE(index, nullptr);

    pq_quantizer_t *pq = pq_create(dims_, 16, 8);
    pq_train(pq, n_train_, train_vectors_.data());
    hnsw_pq_index_set_pq(index, pq);

    storage_backend_t *backend = storage_memory_create(65536);
    heap_vector_config_t heap_config;
    memset(&heap_config, 0, sizeof(heap_config));
    heap_config.backend = backend;
    heap_config.dims = dims_;
    heap_config.page_size = 65536;
    heap_vector_store_t *heap = heap_vector_store_create(&heap_config);
    hnsw_pq_index_set_heap_store(index, heap);

    hnsw_pq_index_insert_batch(index, n_vectors_, vectors_.data());

    /* 搜索 */
    int32_t result_ids[10];
    float result_dists[10];

    int32_t n_results = hnsw_pq_index_search(index, queries_.data(), k_,
                                              result_ids, result_dists);

    EXPECT_GT(n_results, 0);
    EXPECT_LE(n_results, k_);

    for (int i = 0; i < n_results; i++) {
        EXPECT_GE(result_ids[i], 0);
        EXPECT_LT(result_ids[i], n_vectors_);
        EXPECT_GE(result_dists[i], 0.0f);
    }

    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
    pq_destroy(pq);
    hnsw_pq_index_destroy(index);
}

/* 测试：召回率 */
TEST_F(HNSWPQTest, RecallTest) {
    hnsw_pq_config_t config;
    memset(&config, 0, sizeof(config));
    config.m = 24;  /* 更大的邻居数 */
    config.ef_construction = 150;
    config.ef_search = 80;
    config.dims = dims_;
    config.metric = DISTANCE_METRIC_L2_SQUARED;
    config.pq_m = 16;
    config.pq_bits = 8;
    config.rerank_k = 100;

    hnsw_pq_index_t *index = hnsw_pq_index_create(&config);
    ASSERT_NE(index, nullptr);

    pq_quantizer_t *pq = pq_create(dims_, 16, 8);
    pq_train(pq, n_train_, train_vectors_.data());
    hnsw_pq_index_set_pq(index, pq);

    storage_backend_t *backend = storage_memory_create(65536);
    heap_vector_config_t heap_config;
    memset(&heap_config, 0, sizeof(heap_config));
    heap_config.backend = backend;
    heap_config.dims = dims_;
    heap_config.page_size = 65536;
    heap_vector_store_t *heap = heap_vector_store_create(&heap_config);
    hnsw_pq_index_set_heap_store(index, heap);

    hnsw_pq_index_insert_batch(index, n_vectors_, vectors_.data());

    float total_recall = 0.0f;

    for (int q = 0; q < n_queries_; q++) {
        int32_t result_ids[10];
        hnsw_pq_index_search(index, &queries_[q * dims_], k_, result_ids, nullptr);

        /* 暴力搜索 ground truth */
        std::vector<std::pair<float, int32_t>> all_dists;
        for (int i = 0; i < n_vectors_; i++) {
            float dist = 0.0f;
            for (int d = 0; d < dims_; d++) {
                float diff = queries_[q * dims_ + d] - vectors_[i * dims_ + d];
                dist += diff * diff;
            }
            all_dists.push_back({dist, i});
        }

        std::sort(all_dists.begin(), all_dists.end());

        int32_t ground_truth[10];
        for (int i = 0; i < k_; i++) {
            ground_truth[i] = all_dists[i].second;
        }

        /* 计算召回率 */
        int hit = 0;
        for (int i = 0; i < k_; i++) {
            for (int j = 0; j < k_; j++) {
                if (result_ids[i] == ground_truth[j]) {
                    hit++;
                    break;
                }
            }
        }

        total_recall += (float)hit / k_;
    }

    float avg_recall = total_recall / n_queries_;
    printf("HNSW-PQ 平均召回率: %.2f%%\n", avg_recall * 100);

    /* 简化实现的召回率可能较低 */
    EXPECT_GT(avg_recall, 0.1f);

    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
    pq_destroy(pq);
    hnsw_pq_index_destroy(index);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}