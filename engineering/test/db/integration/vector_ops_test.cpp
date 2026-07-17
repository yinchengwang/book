/**
 * @file vector_ops_test.cpp
 * @brief 向量操作集成测试
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <cstring>
#include <cmath>

// 测试夹具
class VectorOpsIntegrationTest : public ::testing::Test {
protected:
    VectorAPI *api;
    const char *test_data_dir;

    void SetUp() override {
        test_data_dir = "./test_vector_ops_data";
#ifdef _WIN32
        mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
        api = vector_api_create(test_data_dir);
        ASSERT_NE(nullptr, api);

        // 创建测试集合
        VectorCreateParams params = {
            .name = "test_collection",
            .dimension = 4,
            .index_type = VECTOR_INDEX_HNSW,
            .metric_type = VECTOR_METRIC_L2
        };
        EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));
    }

    void TearDown() override {
        if (api) vector_api_destroy(api);
    }
};

// ============================================================
// 向量插入测试
// ============================================================

TEST_F(VectorOpsIntegrationTest, InsertSingleVector) {
    float vector[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float *vectors[1] = {vector};

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = 1,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int64_t out_id;
    int inserted = vector_api_insert(api, &params, &out_id);
    EXPECT_EQ(1, inserted);
    EXPECT_EQ(1, vector_api_size(api, "test_collection"));
}

TEST_F(VectorOpsIntegrationTest, InsertBatchVectors) {
    float *vectors[5];
    for (int i = 0; i < 5; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i;
        vectors[i][2] = (float)i;
        vectors[i][3] = (float)i;
    }

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = 5,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int inserted = vector_api_insert(api, &params, NULL);
    EXPECT_EQ(5, inserted);
    EXPECT_EQ(5, vector_api_size(api, "test_collection"));

    for (int i = 0; i < 5; i++) free(vectors[i]);
}

TEST_F(VectorOpsIntegrationTest, InsertWithCustomIds) {
    float vector[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float *vectors[1] = {vector};
    int64_t ids[1] = {42};

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = 1,
        .ids = ids,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int inserted = vector_api_insert(api, &params, NULL);
    EXPECT_EQ(1, inserted);

    // 搜索验证 ID
    float query[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    VectorSearchParams search_params = {
        .collection = "test_collection",
        .query = query,
        .top_k = 10,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    VectorSearchResults *results = vector_api_search(api, &search_params);
    ASSERT_NE(nullptr, results);
    ASSERT_GT(vector_api_results_count(results), 0);

    const VectorSearchResult *r = vector_api_results_get(results, 0);
    EXPECT_EQ(42, r->id);
    vector_api_results_free(results);
}

// ============================================================
// 向量搜索测试
// ============================================================

TEST_F(VectorOpsIntegrationTest, SearchExactMatch) {
    // 插入向量
    float v1[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float v2[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    float v3[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    float *vectors[3] = {v1, v2, v3};

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = 3,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &params, NULL);

    // 搜索精确匹配
    float query[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    VectorSearchParams search_params = {
        .collection = "test_collection",
        .query = query,
        .top_k = 1,
        .radius = 0.01f,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    VectorSearchResults *results = vector_api_search(api, &search_params);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(1, vector_api_results_count(results));

    const VectorSearchResult *r = vector_api_results_get(results, 0);
    EXPECT_NEAR(0.0f, r->distance, 0.01f);
    vector_api_results_free(results);
}

TEST_F(VectorOpsIntegrationTest, SearchApproximateMatch) {
    // 插入一组向量
    for (int i = 0; i < 10; i++) {
        float v[4] = {(float)i, (float)i, (float)i, (float)i};
        float *vectors[1] = {v};
        VectorInsertParams params = {
            .collection = "test_collection",
            .n = 1,
            .ids = NULL,
            .vectors = vectors,
            .metadata = NULL,
            .metadata_sizes = NULL
        };
        vector_api_insert(api, &params, NULL);
    }

    // 搜索近似匹配
    float query[4] = {5.5f, 5.5f, 5.5f, 5.5f};
    VectorSearchParams search_params = {
        .collection = "test_collection",
        .query = query,
        .top_k = 3,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    VectorSearchResults *results = vector_api_search(api, &search_params);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(3, vector_api_results_count(results));

    // 验证排序正确（距离从小到大）
    float prev_dist = -1;
    for (int32_t i = 0; i < vector_api_results_count(results); i++) {
        const VectorSearchResult *r = vector_api_results_get(results, i);
        EXPECT_LE(prev_dist, r->distance);
        prev_dist = r->distance;
    }
    vector_api_results_free(results);
}

TEST_F(VectorOpsIntegrationTest, SearchEmptyCollection) {
    float query[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    VectorSearchParams search_params = {
        .collection = "test_collection",
        .query = query,
        .top_k = 10,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    VectorSearchResults *results = vector_api_search(api, &search_params);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(0, vector_api_results_count(results));
    vector_api_results_free(results);
}

// ============================================================
// 向量删除测试
// ============================================================

TEST_F(VectorOpsIntegrationTest, DeleteVector) {
    // 插入向量并获取 ID
    float v[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float *vectors[1] = {v};
    int64_t out_id;

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = 1,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &params, &out_id);
    EXPECT_EQ(1, vector_api_size(api, "test_collection"));

    // 删除向量
    int deleted = vector_api_delete(api, "test_collection", &out_id, 1);
    EXPECT_EQ(1, deleted);
    EXPECT_EQ(0, vector_api_size(api, "test_collection"));
}

TEST_F(VectorOpsIntegrationTest, DeleteNonexistentVector) {
    int64_t fake_id = 99999;
    int deleted = vector_api_delete(api, "test_collection", &fake_id, 1);
    EXPECT_EQ(0, deleted);
}

TEST_F(VectorOpsIntegrationTest, DeleteMultipleVectors) {
    // 插入多个向量
    float *vectors[5];
    int64_t ids[5];
    for (int i = 0; i < 5; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i;
        vectors[i][2] = (float)i;
        vectors[i][3] = (float)i;
    }

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = 5,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &params, ids);
    EXPECT_EQ(5, vector_api_size(api, "test_collection"));

    for (int i = 0; i < 5; i++) free(vectors[i]);

    // 删除前 3 个
    int deleted = vector_api_delete(api, "test_collection", ids, 3);
    EXPECT_EQ(3, deleted);
    EXPECT_EQ(2, vector_api_size(api, "test_collection"));
}

// ============================================================
// 性能基准测试
// ============================================================

TEST_F(VectorOpsIntegrationTest, BulkInsertPerformance) {
    const int n = 1000;
    float **vectors = (float **)malloc(n * sizeof(float *));
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)(i % 100);
        vectors[i][1] = (float)(i % 100);
        vectors[i][2] = (float)(i % 100);
        vectors[i][3] = (float)(i % 100);
    }

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int64_t start = clock();
    int inserted = vector_api_insert(api, &params, NULL);
    int64_t elapsed = clock() - start;

    EXPECT_EQ(n, inserted);
    printf("\n批量插入 %d 个向量耗时: %.2f ms\n", n, elapsed * 1000.0 / CLOCKS_PER_SEC);

    for (int i = 0; i < n; i++) free(vectors[i]);
    free(vectors);
}

TEST_F(VectorOpsIntegrationTest, SearchPerformance) {
    // 插入 1000 个向量
    const int n = 1000;
    float **vectors = (float **)malloc(n * sizeof(float *));
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)(i % 100);
        vectors[i][1] = (float)(i % 100);
        vectors[i][2] = (float)(i % 100);
        vectors[i][3] = (float)(i % 100);
    }

    VectorInsertParams params = {
        .collection = "test_collection",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &params, NULL);

    for (int i = 0; i < n; i++) free(vectors[i]);
    free(vectors);

    // 搜索性能测试
    float query[4] = {50.0f, 50.0f, 50.0f, 50.0f};
    VectorSearchParams search_params = {
        .collection = "test_collection",
        .query = query,
        .top_k = 10,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    const int iterations = 100;
    int64_t start = clock();
    for (int i = 0; i < iterations; i++) {
        VectorSearchResults *results = vector_api_search(api, &search_params);
        if (results) vector_api_results_free(results);
    }
    int64_t elapsed = clock() - start;

    printf("\n搜索 %d 次耗时: %.2f ms (平均每次 %.2f us)\n",
           iterations, elapsed * 1000.0 / CLOCKS_PER_SEC,
           elapsed * 1000000.0 / CLOCKS_PER_SEC / iterations);
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
