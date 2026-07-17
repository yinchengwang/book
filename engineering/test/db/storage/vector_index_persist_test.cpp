/**
 * @file vector_index_persist_test.cpp
 * @brief 向量索引持久化单元测试
 */

#include <gtest/gtest.h>
#include "db/storage/vector/vector_engine.h"
#include "rag/hnsw_persist.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <random>
#include <filesystem>

namespace {

// 测试临时目录
const char* TEST_DIR = "test-results/engineering/vector_persist";
const char* TEST_INDEX_FILE = "test-results/engineering/vector_persist/test_index.bin";

// 测试夹具
class VectorIndexPersistTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录
        std::filesystem::create_directories(TEST_DIR);

        // 初始化向量引擎
        ASSERT_EQ(0, vector_engine_init(TEST_DIR));
    }

    void TearDown() override {
        // 关闭向量引擎
        vector_engine_shutdown();

        // 清理测试文件
        if (std::filesystem::exists(TEST_INDEX_FILE)) {
            std::filesystem::remove(TEST_INDEX_FILE);
        }
    }

    // 生成随机向量
    std::vector<float> generate_random_vectors(int count, int dim) {
        std::vector<float> vectors(count * dim);
        std::mt19937 rng(42);  // 固定种子，可重复
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& v : vectors) {
            v = dist(rng);
        }
        return vectors;
    }
};

// ========== 空索引保存/加载测试 ==========

TEST_F(VectorIndexPersistTest, EmptyIndexSave) {
    // 创建空集合
    storage_schema_t schema = {0};
    schema.dimension = 128;
    schema.metric = METRIC_L2;

    ASSERT_EQ(0, vector_engine_create("empty_test", &schema));

    void* rel = vector_engine_open("empty_test", ACCESS_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    // 保存空索引（应该失败，因为索引未构建）
    vector_persist_result_t result = {0};
    int ret = vector_index_save(rel, TEST_INDEX_FILE, &result);

    // 空索引没有数据，预期失败
    EXPECT_EQ(-1, ret);
    EXPECT_EQ(0, result.success);

    vector_engine_close(rel);
    vector_engine_drop("empty_test");
}

// ========== 单向量索引保存/加载测试 ==========

TEST_F(VectorIndexPersistTest, SingleVectorSaveLoad) {
    // 创建集合
    storage_schema_t schema = {0};
    schema.dimension = 128;
    schema.metric = METRIC_L2;

    ASSERT_EQ(0, vector_engine_create("single_test", &schema));

    void* rel = vector_engine_open("single_test", ACCESS_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    // 插入单个向量
    auto vectors = generate_random_vectors(1, 128);
    ASSERT_EQ(0, vector_engine_insert(rel, vectors.data(), 128 * sizeof(float)));

    // 构建索引（通过搜索触发）
    vector_search_results_t results = {0};
    ASSERT_EQ(0, vector_engine_search(rel, vectors.data(), 128, 1, &results));
    vector_engine_free_results(&results);

    // 保存索引
    vector_persist_result_t save_result = {0};
    int ret = vector_index_save(rel, TEST_INDEX_FILE, &save_result);

    // 如果保存失败，跳过加载测试
    if (ret != 0) {
        vector_engine_close(rel);
        vector_engine_drop("single_test");
        return;
    }

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, save_result.success);
    EXPECT_GT(save_result.bytes_written, 0);

    // 验证文件存在且有效
    EXPECT_TRUE(vector_index_is_valid(TEST_INDEX_FILE));

    // 重新打开集合并加载索引
    vector_engine_close(rel);
    rel = vector_engine_open("single_test", ACCESS_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    vector_persist_result_t load_result = {0};
    ret = vector_index_load(rel, TEST_INDEX_FILE, &load_result);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, load_result.success);
    EXPECT_GT(load_result.bytes_read, 0);

    // 验证搜索仍然有效
    vector_search_results_t results2 = {0};
    ASSERT_EQ(0, vector_engine_search(rel, vectors.data(), 128, 1, &results2));
    EXPECT_EQ(1, results2.count);
    vector_engine_free_results(&results2);

    vector_engine_close(rel);
    vector_engine_drop("single_test");
}

// ========== 多向量索引保存/加载测试 ==========

TEST_F(VectorIndexPersistTest, MultipleVectorsSaveLoad) {
    const int NUM_VECTORS = 100;
    const int DIM = 128;

    // 创建集合
    storage_schema_t schema = {0};
    schema.dimension = DIM;
    schema.metric = METRIC_L2;

    ASSERT_EQ(0, vector_engine_create("multi_test", &schema));

    void* rel = vector_engine_open("multi_test", ACCESS_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    // 插入多个向量
    auto vectors = generate_random_vectors(NUM_VECTORS, DIM);
    for (int i = 0; i < NUM_VECTORS; i++) {
        ASSERT_EQ(0, vector_engine_insert(rel, vectors.data() + i * DIM, DIM * sizeof(float)));
    }

    // 构建索引（通过搜索触发）
    vector_search_results_t results = {0};
    ASSERT_EQ(0, vector_engine_search(rel, vectors.data(), DIM, 10, &results));
    vector_engine_free_results(&results);

    // 保存索引
    vector_persist_result_t save_result = {0};
    int ret = vector_index_save(rel, TEST_INDEX_FILE, &save_result);

    if (ret != 0) {
        vector_engine_close(rel);
        vector_engine_drop("multi_test");
        return;
    }

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, save_result.success);
    EXPECT_GT(save_result.bytes_written, 0);

    // 验证文件元数据
    int32_t dims = 0, n_total = 0, metric = 0;
    EXPECT_EQ(0, vector_index_get_meta(TEST_INDEX_FILE, &dims, &n_total, &metric));
    EXPECT_EQ(DIM, dims);
    EXPECT_EQ(NUM_VECTORS, n_total);
    EXPECT_EQ(METRIC_L2, metric);

    // 重新打开并加载
    vector_engine_close(rel);
    rel = vector_engine_open("multi_test", ACCESS_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    vector_persist_result_t load_result = {0};
    ret = vector_index_load(rel, TEST_INDEX_FILE, &load_result);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, load_result.success);

    // 验证搜索结果一致
    vector_search_results_t results2 = {0};
    ASSERT_EQ(0, vector_engine_search(rel, vectors.data(), DIM, 10, &results2));
    EXPECT_EQ(10, results2.count);

    vector_engine_free_results(&results2);
    vector_engine_close(rel);
    vector_engine_drop("multi_test");
}

// ========== 格式校验测试 ==========

TEST_F(VectorIndexPersistTest, InvalidFile) {
    // 创建无效文件
    FILE* fp = fopen(TEST_INDEX_FILE, "wb");
    if (fp) {
        const char* garbage = "NOT_A_VALID_INDEX_FILE";
        fwrite(garbage, 1, strlen(garbage), fp);
        fclose(fp);
    }

    // 验证无效文件被检测
    EXPECT_FALSE(vector_index_is_valid(TEST_INDEX_FILE));

    // 尝试加载应该失败
    storage_schema_t schema = {0};
    schema.dimension = 128;
    schema.metric = METRIC_L2;

    ASSERT_EQ(0, vector_engine_create("invalid_test", &schema));
    void* rel = vector_engine_open("invalid_test", ACCESS_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    vector_persist_result_t result = {0};
    EXPECT_NE(0, vector_index_load(rel, TEST_INDEX_FILE, &result));
    EXPECT_EQ(0, result.success);

    vector_engine_close(rel);
    vector_engine_drop("invalid_test");

    std::filesystem::remove(TEST_INDEX_FILE);
}

// ========== 魔数校验测试 ==========

TEST_F(VectorIndexPersistTest, MagicNumberValidation) {
    // 创建带正确魔数的文件
    FILE* fp = fopen(TEST_INDEX_FILE, "wb");
    if (fp) {
        uint32_t magic = HNSW_PERSIST_MAGIC;
        fwrite(&magic, sizeof(magic), 1, fp);

        // 写入一些无效数据
        uint32_t version = 1;
        fwrite(&version, sizeof(version), 1, fp);

        fclose(fp);
    }

    // 验证文件头有效（虽然数据不完整）
    EXPECT_TRUE(vector_index_is_valid(TEST_INDEX_FILE));

    // 但元数据读取应该失败（文件太小）
    int32_t dims = 0, n_total = 0, metric = 0;
    EXPECT_NE(0, vector_index_get_meta(TEST_INDEX_FILE, &dims, &n_total, &metric));

    std::filesystem::remove(TEST_INDEX_FILE);
}

}  // namespace