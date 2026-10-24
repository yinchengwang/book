/**
 * @file ssg_test.cpp
 * @brief SSG 索引单元测试
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

extern "C" {
#include "db/index/vector_index/ssg/ssg.h"
#include "db/index/heap/heap_vector_store.h"
#include "db/index/storage_backend.h"
#include "algo-prod/distance/distance.h"
}

class SSGTest : public ::testing::Test {
protected:
    void SetUp() override {
        dims_ = 128;
        n_vectors_ = 1000;
        n_queries_ = 10;
        k_ = 10;

        /* 生成随机向量 */
        vectors_.resize(n_vectors_ * dims_);
        queries_.resize(n_queries_ * dims_);

        for (int32_t i = 0; i < n_vectors_ * dims_; i++) {
            vectors_[i] = (float)rand() / RAND_MAX;
        }

        for (int32_t i = 0; i < n_queries_ * dims_; i++) {
            queries_[i] = (float)rand() / RAND_MAX;
        }
    }

    int32_t dims_;
    int32_t n_vectors_;
    int32_t n_queries_;
    int32_t k_;
    std::vector<float> vectors_;
    std::vector<float> queries_;
};

/* 测试：创建和销毁 */
TEST_F(SSGTest, CreateDestroy) {
    ssg_config_t config = {
        .R = 16,
        .L = 100,
        .max_search_L = 50,
        .dims = dims_,
        .alpha = 1.2f,
        .metric = DISTANCE_METRIC_L2_SQUARED
    };

    ssg_index_t *index = ssg_index_create(&config);
    ASSERT_NE(index, nullptr);

    EXPECT_EQ(ssg_index_size(index), 0);

    ssg_index_destroy(index);
}

/* 测试：单向量插入 */
TEST_F(SSGTest, SingleInsert) {
    ssg_config_t config = {
        .R = 16,
        .L = 100,
        .max_search_L = 50,
        .dims = dims_,
        .alpha = 1.2f,
        .metric = DISTANCE_METRIC_L2_SQUARED
    };

    ssg_index_t *index = ssg_index_create(&config);
    ASSERT_NE(index, nullptr);

    int32_t id = ssg_index_insert(index, vectors_.data());
    EXPECT_EQ(id, 0);
    EXPECT_EQ(ssg_index_size(index), 1);

    ssg_index_destroy(index);
}

/* 测试：批量插入 */
TEST_F(SSGTest, BatchInsert) {
    ssg_config_t config = {
        .R = 16,
        .L = 100,
        .max_search_L = 50,
        .dims = dims_,
        .alpha = 1.2f,
        .metric = DISTANCE_METRIC_L2_SQUARED
    };

    ssg_index_t *index = ssg_index_create(&config);
    ASSERT_NE(index, nullptr);

    /* 创建 heap_store */
    storage_backend_t *backend = storage_memory_create(65536);
    ASSERT_NE(backend, nullptr) << "storage_memory_create failed";

    heap_vector_config_t heap_config = {};
    heap_config.backend = backend;
    heap_config.dims = dims_;
    heap_config.page_size = 65536;  /* 必须与 backend->page_size 一致 */
    heap_config.vectors_per_page = 0;

    heap_vector_store_t *heap = heap_vector_store_create(&heap_config);
    ASSERT_NE(heap, nullptr) << "heap_vector_store_create failed";

    ssg_index_set_heap_store(index, heap);

    int32_t inserted = ssg_index_insert_batch(index, n_vectors_, vectors_.data());
    EXPECT_EQ(inserted, n_vectors_);
    EXPECT_EQ(ssg_index_size(index), n_vectors_);

    /* 检查平均出度 */
    float avg_degree = ssg_index_avg_out_degree(index);
    printf("SSG 平均出度: %.2f\n", avg_degree);
    EXPECT_LT(avg_degree, config.R);  /* 应该小于最大邻居数 */

    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
    ssg_index_destroy(index);
}

/* 测试：基础搜索 */
TEST_F(SSGTest, BasicSearch) {
    ssg_config_t config = {
        .R = 16,
        .L = 100,
        .max_search_L = 50,
        .dims = dims_,
        .alpha = 1.2f,
        .metric = DISTANCE_METRIC_L2_SQUARED
    };

    ssg_index_t *index = ssg_index_create(&config);
    ASSERT_NE(index, nullptr);

    /* 创建 heap_store */
    storage_backend_t *backend = storage_memory_create(65536);
    ASSERT_NE(backend, nullptr);

    heap_vector_config_t heap_config = {};
    heap_config.backend = backend;
    heap_config.dims = dims_;
    heap_config.page_size = 65536;
    heap_config.vectors_per_page = 0;

    heap_vector_store_t *heap = heap_vector_store_create(&heap_config);
    ASSERT_NE(heap, nullptr);

    ssg_index_set_heap_store(index, heap);

    ssg_index_insert_batch(index, n_vectors_, vectors_.data());

    int32_t result_ids[10];
    float result_dists[10];

    int32_t n_results = ssg_index_search(index, queries_.data(), k_,
                                          result_ids, result_dists);

    EXPECT_GT(n_results, 0);
    EXPECT_LE(n_results, k_);

    for (int32_t i = 0; i < n_results; i++) {
        EXPECT_GE(result_ids[i], 0);
        EXPECT_LT(result_ids[i], n_vectors_);
        EXPECT_GE(result_dists[i], 0.0f);
    }

    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
    ssg_index_destroy(index);
}

/* 测试：召回率 */
TEST_F(SSGTest, RecallTest) {
    ssg_config_t config = {
        .R = 32,          /* 更大的 R 提高召回率 */
        .L = 200,         /* 更大的 L 提高构建质量 */
        .max_search_L = 100,
        .dims = dims_,
        .alpha = 1.2f,
        .metric = DISTANCE_METRIC_L2_SQUARED
    };

    ssg_index_t *index = ssg_index_create(&config);
    ASSERT_NE(index, nullptr);

    /* 创建 heap_store */
    storage_backend_t *backend = storage_memory_create(65536);
    ASSERT_NE(backend, nullptr);

    heap_vector_config_t heap_config = {};
    heap_config.backend = backend;
    heap_config.dims = dims_;
    heap_config.page_size = 65536;
    heap_config.vectors_per_page = 0;

    heap_vector_store_t *heap = heap_vector_store_create(&heap_config);
    ASSERT_NE(heap, nullptr);

    ssg_index_set_heap_store(index, heap);

    ssg_index_insert_batch(index, n_vectors_, vectors_.data());

    float total_recall = 0.0f;

    for (int32_t q = 0; q < n_queries_; q++) {
        int32_t result_ids[10];
        float result_dists[10];

        ssg_index_search(index, &queries_[q * dims_], k_,
                         result_ids, result_dists);

        /* 暴力搜索 ground truth */
        std::vector<std::pair<float, int32_t>> all_dists;
        for (int32_t i = 0; i < n_vectors_; i++) {
            float dist = distance_l2sqr(&queries_[q * dims_],
                                         &vectors_[i * dims_],
                                         dims_);
            all_dists.push_back({dist, i});
        }

        std::sort(all_dists.begin(), all_dists.end());

        int32_t ground_truth[10];
        for (int32_t i = 0; i < k_; i++) {
            ground_truth[i] = all_dists[i].second;
        }

        /* 计算召回率 */
        int32_t hit = 0;
        for (int32_t i = 0; i < k_; i++) {
            for (int32_t j = 0; j < k_; j++) {
                if (result_ids[i] == ground_truth[j]) {
                    hit++;
                    break;
                }
            }
        }

        total_recall += (float)hit / k_;
    }

    float avg_recall = total_recall / n_queries_;
    printf("SSG 平均召回率: %.2f%%\n", avg_recall * 100);

    EXPECT_GE(avg_recall, 0.40f);  /* 随机数据召回率阈值 */

    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
    ssg_index_destroy(index);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
