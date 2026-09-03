/**
 * @file e2e_test.cpp
 * @brief 端到端场景集成测试
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <cstring>

// 测试夹具
class E2EIntegrationTest : public ::testing::Test {
protected:
    VectorAPI *api;
    const char *test_data_dir;

    void SetUp() override {
        test_data_dir = "./test_e2e_data";
#ifdef _WIN32
        mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
        api = vector_api_create(test_data_dir);
        ASSERT_NE(nullptr, api);
    }

    void TearDown() override {
        if (api) vector_api_destroy(api);
    }
};

// ============================================================
// 完整工作流测试
// ============================================================

TEST_F(E2EIntegrationTest, CompleteWorkflow) {
    // Step 1: 创建集合
    VectorCreateParams params = {
        .name = "products",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_COSINE
    };
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // Step 2: 插入向量
    const int n = 100;
    float **vectors = (float **)malloc(n * sizeof(float *));
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(128 * sizeof(float));
        for (int d = 0; d < 128; d++) {
            vectors[i][d] = (float)(i + d) / 128.0f;
        }
    }

    VectorInsertParams insert_params = {
        .collection = "products",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int inserted = vector_api_insert(api, &insert_params, NULL);
    EXPECT_EQ(n, inserted);

    for (int i = 0; i < n; i++) free(vectors[i]);
    free(vectors);

    // Step 3: 验证大小
    EXPECT_EQ(n, vector_api_size(api, "products"));

    // Step 4: 搜索
    float query[128];
    for (int d = 0; d < 128; d++) query[d] = 0.5f;

    VectorSearchParams search_params = {
        .collection = "products",
        .query = query,
        .top_k = 10,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    VectorSearchResults *results = vector_api_search(api, &search_params);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(10, vector_api_results_count(results));
    vector_api_results_free(results);

    // Step 5: 删除部分向量
    // 获取前 10 个结果的 ID
    results = vector_api_search(api, &search_params);
    int64_t ids_to_delete[5];
    for (int i = 0; i < 5 && i < vector_api_results_count(results); i++) {
        const VectorSearchResult *r = vector_api_results_get(results, i);
        ids_to_delete[i] = r->id;
    }
    vector_api_results_free(results);

    int deleted = vector_api_delete(api, "products", ids_to_delete, 5);
    EXPECT_EQ(5, deleted);
    EXPECT_EQ(n - 5, vector_api_size(api, "products"));

    // Step 6: 保存
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));

    // Step 7: 重新加载
    vector_api_destroy(api);
    api = vector_api_create(test_data_dir);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    // Step 8: 验证数据一致性
    EXPECT_EQ(n - 5, vector_api_size(api, "products"));

    // Step 9: 清理
    EXPECT_EQ(VECTOR_API_OK, vector_api_drop_collection(api, "products"));
    EXPECT_EQ(0, vector_api_size(api, "products"));
}

// ============================================================
// 多集合隔离测试
// ============================================================

TEST_F(E2EIntegrationTest, MultiCollectionIsolation) {
    // 创建多个集合
    VectorCreateParams params1 = {"images", 128, VECTOR_INDEX_HNSW, VECTOR_METRIC_COSINE};
    VectorCreateParams params2 = {"documents", 256, VECTOR_INDEX_IVF, VECTOR_METRIC_L2};
    VectorCreateParams params3 = {"audio", 64, VECTOR_INDEX_DISKANN, VECTOR_METRIC_IP};

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params1));
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params2));
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params3));

    // 插入向量到每个集合
    float v1[128] = {0}, v2[256] = {0}, v3[64] = {0};
    float *v1_ptr = v1, *v2_ptr = v2, *v3_ptr = v3;

    VectorInsertParams p1 = {"images", 1, NULL, &v1_ptr, NULL, NULL};
    VectorInsertParams p2 = {"documents", 1, NULL, &v2_ptr, NULL, NULL};
    VectorInsertParams p3 = {"audio", 1, NULL, &v3_ptr, NULL, NULL};

    EXPECT_EQ(1, vector_api_insert(api, &p1, NULL));
    EXPECT_EQ(1, vector_api_insert(api, &p2, NULL));
    EXPECT_EQ(1, vector_api_insert(api, &p3, NULL));

    // 验证大小
    EXPECT_EQ(1, vector_api_size(api, "images"));
    EXPECT_EQ(1, vector_api_size(api, "documents"));
    EXPECT_EQ(1, vector_api_size(api, "audio"));

    // 验证维度正确
    VectorCollectionInfo info;
    EXPECT_EQ(VECTOR_API_OK, vector_api_get_collection(api, "images", &info));
    EXPECT_EQ(128, info.dimension);
    EXPECT_EQ(VECTOR_API_OK, vector_api_get_collection(api, "documents", &info));
    EXPECT_EQ(256, info.dimension);
    EXPECT_EQ(VECTOR_API_OK, vector_api_get_collection(api, "audio", &info));
    EXPECT_EQ(64, info.dimension);

    // 删除一个集合不影响其他
    EXPECT_EQ(VECTOR_API_OK, vector_api_drop_collection(api, "documents"));
    EXPECT_EQ(1, vector_api_size(api, "images"));
    EXPECT_EQ(0, vector_api_size(api, "documents"));  // 已删除
    EXPECT_EQ(1, vector_api_size(api, "audio"));
}

// ============================================================
// 异常恢复测试
// ============================================================

TEST_F(E2EIntegrationTest, RecoveryAfterError) {
    // 创建集合
    VectorCreateParams params = {
        .name = "recovery_test",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插入一些数据
    float v[128] = {0};
    float *v_ptr = v;
    VectorInsertParams insert_params = {
        .collection = "recovery_test",
        .n = 1,
        .ids = NULL,
        .vectors = &v_ptr,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    EXPECT_EQ(1, vector_api_insert(api, &insert_params, NULL));
    EXPECT_EQ(1, vector_api_size(api, "recovery_test"));

    // 保存
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 模拟错误操作（操作不存在的集合）
    EXPECT_EQ(VECTOR_API_ERR_NOT_FOUND,
              vector_api_drop_collection(api, "nonexistent"));

    // 验证数据仍然正确
    EXPECT_EQ(1, vector_api_size(api, "recovery_test"));

    // 重新加载并验证
    vector_api_destroy(api);
    api = vector_api_create(test_data_dir);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(1, vector_api_size(api, "recovery_test"));
}

// ============================================================
// 并发操作测试（简化版）
// ============================================================

TEST_F(E2EIntegrationTest, SequentialOperations) {
    // 连续创建和删除
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "temp_%d", i);

        VectorCreateParams params = {
            .name = name,
            .dimension = 128,
            .index_type = VECTOR_INDEX_HNSW,
            .metric_type = VECTOR_METRIC_L2
        };
        EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

        // 插入向量
        float v[128] = {0};
        float *v_ptr = v;
        VectorInsertParams insert_params = {
            .collection = name,
            .n = 1,
            .ids = NULL,
            .vectors = &v_ptr,
            .metadata = NULL,
            .metadata_sizes = NULL
        };
        EXPECT_EQ(1, vector_api_insert(api, &insert_params, NULL));
        EXPECT_EQ(1, vector_api_size(api, name));

        // 删除
        EXPECT_EQ(VECTOR_API_OK, vector_api_drop_collection(api, name));
    }

    // 验证无残留
    char **names = NULL;
    int32_t n = 0;
    EXPECT_EQ(VECTOR_API_OK, vector_api_list_collections(api, &names, &n));
    EXPECT_EQ(0, n);
    if (names) free(names);
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
