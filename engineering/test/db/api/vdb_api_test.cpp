/**
 * @file vdb_api_test.cpp
 * @brief VDB 统一 API 层单元测试
 *
 * 测试覆盖：
 * - vdb_open/vdb_close 往返测试
 * - collection CRUD 测试
 * - 向量插入/查询/删除往返测试
 * - 错误处理测试（无效参数、不存在集合等）
 */
#include <gtest/gtest.h>
#include <db/vdb_api.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

const char* TEST_DIR = "test-results/engineering/vdb_api";
const char* TEST_COLLECTION = "test_collection";

class VdbApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(TEST_DIR);
    }

    void TearDown() override {
        std::filesystem::remove_all(TEST_DIR);
    }
};

/* ========== vdb_open/vdb_close 往返测试 ========== */

TEST_F(VdbApiTest, OpenClose) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    int rc = vdb_close(db);
    EXPECT_EQ(VDB_OK, rc);
}

TEST_F(VdbApiTest, OpenCloseWithConfig) {
    vdb_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.data_dir, TEST_DIR);
    config.enable_wal = true;
    config.max_collections = 10;

    vdb_handle_t *db = vdb_open(TEST_DIR, &config);
    ASSERT_NE(nullptr, db);

    int rc = vdb_close(db);
    EXPECT_EQ(VDB_OK, rc);
}

TEST_F(VdbApiTest, OpenNullPath) {
    vdb_handle_t *db = vdb_open(NULL, NULL);
    EXPECT_EQ(nullptr, db);
}

/* ========== collection CRUD 测试 ========== */

TEST_F(VdbApiTest, CreateCollection) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 128;
    config.index_type = VDB_INDEX_HNSW;
    config.metric_type = VDB_METRIC_L2;

    int rc = vdb_create_collection(db, TEST_COLLECTION, &config);
    EXPECT_EQ(VDB_OK, rc);

    vdb_close(db);
}

TEST_F(VdbApiTest, CreateDuplicateCollection) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 64;
    config.index_type = VDB_INDEX_HNSW;

    EXPECT_EQ(VDB_OK, vdb_create_collection(db, TEST_COLLECTION, &config));
    EXPECT_EQ(VDB_ERR_EXISTS, vdb_create_collection(db, TEST_COLLECTION, &config));

    vdb_close(db);
}

TEST_F(VdbApiTest, GetCollection) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 128;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    vdb_collection_t *coll = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, coll);

    // 验证集合信息
    vdb_collection_config_t info;
    memset(&info, 0, sizeof(info));
    EXPECT_EQ(VDB_OK, vdb_collection_info(coll, &info));
    EXPECT_EQ(128, info.dimension);
    EXPECT_EQ(VDB_METRIC_L2, info.metric_type);

    vdb_close(db);
}

TEST_F(VdbApiTest, GetNonExistentCollection) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_t *coll = vdb_get_collection(db, "nonexistent");
    EXPECT_EQ(nullptr, coll);

    vdb_close(db);
}

TEST_F(VdbApiTest, DropCollection) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 128;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    EXPECT_EQ(VDB_OK, vdb_drop_collection(db, TEST_COLLECTION));
    EXPECT_EQ(nullptr, vdb_get_collection(db, TEST_COLLECTION));

    vdb_close(db);
}

TEST_F(VdbApiTest, ListCollections) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, "coll_a");
    config.dimension = 64;
    vdb_create_collection(db, "coll_a", &config);

    strcpy(config.name, "coll_b");
    config.dimension = 128;
    vdb_create_collection(db, "coll_b", &config);

    char **names = NULL;
    int32_t count = 0;
    EXPECT_EQ(VDB_OK, vdb_list_collections(db, &names, &count));
    EXPECT_EQ(2, count);
    vdb_free_names(names, count);

    vdb_close(db);
}

/* ========== 向量插入/查询/删除往返测试 ========== */

TEST_F(VdbApiTest, InsertAndSearch) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 4;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    vdb_collection_t *coll = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, coll);

    // 插入向量
    float vec1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float vec2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float vec3[] = {0.0f, 0.0f, 1.0f, 0.0f};

    int64_t id1 = vdb_insert(coll, vec1, 4, NULL, 0);
    int64_t id2 = vdb_insert(coll, vec2, 4, NULL, 0);
    int64_t id3 = vdb_insert(coll, vec3, 4, NULL, 0);
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_GT(id3, 0);

    // 查询
    float query[] = {1.0f, 0.1f, 0.0f, 0.0f};
    vdb_search_params_t params;
    memset(&params, 0, sizeof(params));
    params.top_k = 3;
    params.with_distance = true;

    vdb_results_t *results = vdb_search(coll, query, 4, &params);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(3, results->count);

    if (results->count > 0) {
        // 最近邻应为 vec1（距离最小）
        EXPECT_EQ(id1, results->items[0].id);
    }

    vdb_results_free(results);
    vdb_close(db);
}

TEST_F(VdbApiTest, InsertBatch) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 2;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    vdb_collection_t *coll = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, coll);

    // 批量插入
    float vectors[] = {
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.5f, 0.5f
    };
    int64_t ids[3];
    int32_t inserted = vdb_insert_batch(coll, vectors, 2, 3, ids);
    EXPECT_EQ(3, inserted);

    EXPECT_EQ(3, vdb_size(coll));

    vdb_close(db);
}

TEST_F(VdbApiTest, DeleteVector) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 2;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    vdb_collection_t *coll = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, coll);

    float vec[] = {1.0f, 0.0f};
    int64_t id = vdb_insert(coll, vec, 2, NULL, 0);
    EXPECT_GT(id, 0);
    EXPECT_EQ(1, vdb_size(coll));

    EXPECT_EQ(VDB_OK, vdb_delete(coll, id));
    EXPECT_EQ(0, vdb_size(coll));

    vdb_close(db);
}

/* ========== 错误处理测试 ========== */

TEST_F(VdbApiTest, InvalidParams) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    // NULL 参数
    EXPECT_EQ(VDB_ERR_INVALID_PARAM, vdb_create_collection(NULL, "test", NULL));
    EXPECT_EQ(VDB_ERR_INVALID_PARAM, vdb_create_collection(db, NULL, NULL));

    // 删除不存在的集合
    EXPECT_EQ(VDB_ERR_NOT_FOUND, vdb_drop_collection(db, "nonexistent"));

    // 获取不存在的集合
    EXPECT_EQ(nullptr, vdb_get_collection(db, "nonexistent"));

    vdb_close(db);
}

TEST_F(VdbApiTest, SearchEmptyCollection) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 4;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    vdb_collection_t *coll = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, coll);

    float query[] = {1.0f, 0.0f, 0.0f, 0.0f};
    vdb_search_params_t params;
    memset(&params, 0, sizeof(params));
    params.top_k = 5;

    vdb_results_t *results = vdb_search(coll, query, 4, &params);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(0, results->count);

    vdb_results_free(results);
    vdb_close(db);
}

TEST_F(VdbApiTest, SaveAndLoad) {
    vdb_handle_t *db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    // 创建集合
    vdb_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, TEST_COLLECTION);
    config.dimension = 2;
    vdb_create_collection(db, TEST_COLLECTION, &config);

    vdb_collection_t *coll = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, coll);

    float vec[] = {0.5f, 0.5f};
    vdb_insert(coll, vec, 2, NULL, 0);

    // 保存
    EXPECT_EQ(VDB_OK, vdb_save(db));
    vdb_close(db);

    // 重新打开并加载
    db = vdb_open(TEST_DIR, NULL);
    ASSERT_NE(nullptr, db);

    // 应自动加载已有集合
    vdb_collection_t *loaded = vdb_get_collection(db, TEST_COLLECTION);
    ASSERT_NE(nullptr, loaded);
    EXPECT_EQ(1, vdb_size(loaded));

    vdb_close(db);
}

}  // namespace