/**
 * @file collection_test.cpp
 * @brief Collection CRUD 集成测试
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <cstring>
#include <sys/stat.h>

// 测试夹具
class CollectionIntegrationTest : public ::testing::Test {
protected:
    VectorAPI *api;
    const char *test_data_dir;

    void SetUp() override {
        test_data_dir = "./test_integration_data";
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
// Collection CRUD 测试
// ============================================================

TEST_F(CollectionIntegrationTest, CreateCollection) {
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2,
        .index_ef_search = 100,
        .index_m = 16,
        .index_ef_construction = 200
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 验证集合存在
    VectorCollectionInfo info;
    EXPECT_EQ(VECTOR_API_OK, vector_api_get_collection(api, "test_collection", &info));
    EXPECT_STREQ("test_collection", info.name);
    EXPECT_EQ(128, info.dimension);
    EXPECT_EQ(0, info.size);
}

TEST_F(CollectionIntegrationTest, CreateDuplicateCollection) {
    VectorCreateParams params1 = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    VectorCreateParams params2 = {
        .name = "test_collection",
        .dimension = 256,  // 不同维度
        .index_type = VECTOR_INDEX_IVF,
        .metric_type = VECTOR_METRIC_COSINE
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params1));
    EXPECT_EQ(VECTOR_API_ERR_EXISTS, vector_api_create_collection(api, &params2));

    // 验证第一个集合未被覆盖
    VectorCollectionInfo info;
    EXPECT_EQ(VECTOR_API_OK, vector_api_get_collection(api, "test_collection", &info));
    EXPECT_EQ(128, info.dimension);
}

TEST_F(CollectionIntegrationTest, DropCollection) {
    VectorCreateParams params = {
        .name = "test_collection",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));
    EXPECT_EQ(VECTOR_API_OK, vector_api_drop_collection(api, "test_collection"));

    // 验证集合不存在
    VectorCollectionInfo info;
    EXPECT_EQ(VECTOR_API_ERR_NOT_FOUND, vector_api_get_collection(api, "test_collection", &info));
}

TEST_F(CollectionIntegrationTest, DropNonexistentCollection) {
    EXPECT_EQ(VECTOR_API_ERR_NOT_FOUND, vector_api_drop_collection(api, "nonexistent"));
}

TEST_F(CollectionIntegrationTest, ListCollections) {
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
    VectorCreateParams params3 = {
        .name = "collection3",
        .dimension = 512,
        .index_type = VECTOR_INDEX_DISKANN,
        .metric_type = VECTOR_METRIC_IP
    };

    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params1));
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params2));
    EXPECT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params3));

    char **names = NULL;
    int32_t n = 0;
    EXPECT_EQ(VECTOR_API_OK, vector_api_list_collections(api, &names, &n));
    EXPECT_EQ(3, n);

    // 验证集合名称
    int found = 0;
    for (int32_t i = 0; i < n; i++) {
        if (strcmp(names[i], "collection1") == 0 ||
            strcmp(names[i], "collection2") == 0 ||
            strcmp(names[i], "collection3") == 0) {
            found++;
        }
        free(names[i]);
    }
    free(names);
    EXPECT_EQ(3, found);
}

TEST_F(CollectionIntegrationTest, GetCollectionInfo) {
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
    EXPECT_EQ(VECTOR_INDEX_HNSW, info.index_type);
    EXPECT_EQ(VECTOR_METRIC_L2, info.metric_type);
    EXPECT_EQ(0, info.size);
    EXPECT_GT(info.created_at, 0);
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
