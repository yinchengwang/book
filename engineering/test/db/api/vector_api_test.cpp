/**
 * @file vector_api_test.cpp
 * @brief 向量 API 单元测试
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <cstring>
#include <sys/stat.h>

// 测试夹具
class VectorAPITest : public ::testing::Test {
protected:
    VectorAPI *api;
    const char *test_data_dir;

    void SetUp() override {
        test_data_dir = "./test_vector_data";
        // 创建测试目录
#ifdef _WIN32
        mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
        api = vector_api_create(test_data_dir);
        ASSERT_NE(nullptr, api);
    }

    void TearDown() override {
        if (api) {
            vector_api_destroy(api);
        }
    }
};

// ============================================================
// 生命周期测试
// ============================================================

TEST_F(VectorAPITest, CreateAndDestroy) {
    // api 已在 SetUp 创建
    EXPECT_NE(nullptr, api);
    EXPECT_STREQ("", vector_api_error(api));
}

// ============================================================
// 集合管理测试
// ============================================================

TEST_F(VectorAPITest, CreateCollection) {
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2,
        .index_ef_search = 100,
        .index_m = 16,
        .index_ef_construction = 200
    };

    int ret = vector_api_create_collection(api, &params);
    EXPECT_EQ(VECTOR_API_OK, ret);
}

TEST_F(VectorAPITest, CreateDuplicateCollection) {
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));
    EXPECT_EQ(VECTOR_API_ERR_EXISTS, vector_api_create_collection(api, &params));
}

TEST_F(VectorAPITest, DropCollection) {
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));
    EXPECT_EQ(VECTOR_API_OK, vector_api_drop_collection(api, "test_collection"));
    EXPECT_EQ(VECTOR_API_ERR_NOT_FOUND, vector_api_drop_collection(api, "test_collection"));
}

TEST_F(VectorAPITest, GetCollection) {
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    VectorCollectionInfo info;
    EXPECT_EQ(VECTOR_API_OK, vector_api_get_collection(api, "test_collection", &info));
    EXPECT_STREQ("test_collection", info.name);
    EXPECT_EQ(128, info.dimension);
    EXPECT_EQ(0, info.size);
    EXPECT_EQ(VECTOR_INDEX_HNSW, info.index_type);
    EXPECT_EQ(VECTOR_METRIC_L2, info.metric_type);
}

TEST_F(VectorAPITest, ListCollections) {
    VectorCreateParams params1 = {
        .name = "collection1",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    VectorCreateParams params2 = {
        .name = "collection2",
        .dimension = 256,
        .index_type = VECTOR_INDEX_IVF,
        .metric_type = VECTOR_METRIC_COSINE
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params1));
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params2));

    char **names = NULL;
    int32_t n = 0;
    EXPECT_EQ(VECTOR_API_OK, vector_api_list_collections(api, &names, &n));
    EXPECT_EQ(2, n);

    // 验证集合名称
    bool found1 = false, found2 = false;
    for (int32_t i = 0; i < n; i++) {
        if (strcmp(names[i], "collection1") == 0) found1 = true;
        if (strcmp(names[i], "collection2") == 0) found2 = true;
        free(names[i]);
    }
    free(names);
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// ============================================================
// 向量操作测试
// ============================================================

TEST_F(VectorAPITest, InsertVectors) {
    // 创建集合
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 4,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 准备向量数据
    int32_t n = 3;
    float *vectors[3];
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i + 0.1f;
        vectors[i][2] = (float)i + 0.2f;
        vectors[i][3] = (float)i + 0.3f;
    }

    VectorInsertParams insert_params = {
        .collection = "test_collection",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int inserted = vector_api_insert(api, &insert_params, NULL);
    EXPECT_EQ(3, inserted);

    // 验证集合大小
    EXPECT_EQ(3, vector_api_size(api, "test_collection"));

    // 释放内存
    for (int i = 0; i < n; i++) {
        free(vectors[i]);
    }
}

TEST_F(VectorAPITest, SearchVectors) {
    // 创建集合
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 4,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插入向量
    float *vectors[3];
    for (int i = 0; i < 3; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i;
        vectors[i][2] = (float)i;
        vectors[i][3] = (float)i;
    }

    VectorInsertParams insert_params = {
        .collection = "test_collection",
        .n = 3,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &insert_params, NULL);

    for (int i = 0; i < 3; i++) free(vectors[i]);

    // 搜索
    float query[4] = {0.0f, 0.0f, 0.0f, 0.0f};
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
    EXPECT_GT(vector_api_results_count(results), 0);
    vector_api_results_free(results);
}

TEST_F(VectorAPITest, DeleteVectors) {
    // 创建集合
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 4,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插入向量并获取 ID
    float *vectors[3];
    for (int i = 0; i < 3; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i;
        vectors[i][2] = (float)i;
        vectors[i][3] = (float)i;
    }

    int64_t ids[3];
    VectorInsertParams insert_params = {
        .collection = "test_collection",
        .n = 3,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &insert_params, ids);

    for (int i = 0; i < 3; i++) free(vectors[i]);

    EXPECT_EQ(3, vector_api_size(api, "test_collection"));

    // 删除一个向量
    int deleted = vector_api_delete(api, "test_collection", &ids[1], 1);
    EXPECT_EQ(1, deleted);
    EXPECT_EQ(2, vector_api_size(api, "test_collection"));
}

// ============================================================
// 持久化测试
// ============================================================

TEST_F(VectorAPITest, SaveAndLoad) {
    // 创建集合并插入数据
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 4,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    float *vectors[2];
    for (int i = 0; i < 2; i++) {
        vectors[i] = (float *)malloc(4 * sizeof(float));
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i;
        vectors[i][2] = (float)i;
        vectors[i][3] = (float)i;
    }

    VectorInsertParams insert_params = {
        .collection = "test_collection",
        .n = 2,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &insert_params, NULL);

    for (int i = 0; i < 2; i++) free(vectors[i]);

    // 保存
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));
}

// ============================================================
// 错误处理测试
// ============================================================

TEST_F(VectorAPITest, ErrorHandling) {
    // 操作不存在的集合
    VectorCollectionInfo info;
    EXPECT_EQ(VECTOR_API_ERR_NOT_FOUND, vector_api_get_collection(api, "nonexistent", &info));

    // 无效参数
    EXPECT_NE(nullptr, api);
    EXPECT_NE(nullptr, vector_api_error(api));
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
