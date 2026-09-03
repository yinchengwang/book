/**
 * @file test_hnsw_persist.cpp
 * @brief HNSW 持久化测试
 */

#include <gtest/gtest.h>
#include "rag/hnsw_persist.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace rag;

class HNSWPersistTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_hnsw_index.bin";
    }

    void TearDown() override {
        // 清理测试文件
        std::remove(test_file_.c_str());
    }

    std::string test_file_;
};

TEST_F(HNSWPersistTest, VerifyHeader) {
    // 测试魔数验证
    unsigned char valid_header[64] = {0};
    // 写入魔数 "HNSW" = 0x484E5357
    valid_header[0] = 0x57;
    valid_header[1] = 0x53;
    valid_header[2] = 0x4E;
    valid_header[3] = 0x48;

    EXPECT_TRUE(hnsw_persist_verify_header(valid_header));

    // 无效魔数
    unsigned char invalid_header[64] = {0};
    EXPECT_FALSE(hnsw_persist_verify_header(invalid_header));

    // NULL 指针
    EXPECT_FALSE(hnsw_persist_verify_header(nullptr));
}

TEST_F(HNSWPersistTest, FileSize) {
    // 不存在的文件
    EXPECT_EQ(hnsw_persist_file_size("nonexistent_file.bin"), -1);
}

TEST_F(HNSWPersistTest, IsValid) {
    // 不存在的文件
    EXPECT_FALSE(hnsw_persist_is_valid("nonexistent_file.bin"));

    // 创建测试文件并写入无效头
    FILE *fp = fopen(test_file_.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    unsigned char data[64] = {0};
    fwrite(data, 1, 64, fp);
    fclose(fp);

    EXPECT_FALSE(hnsw_persist_is_valid(test_file_.c_str()));
}

TEST_F(HNSWPersistTest, IsValidWithMagic) {
    // 创建测试文件并写入有效的魔数
    FILE *fp = fopen(test_file_.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    unsigned char data[64] = {0};
    // 写入魔数 "HNSW" = 0x484E5357
    data[0] = 0x57;
    data[1] = 0x53;
    data[2] = 0x4E;
    data[3] = 0x48;
    // 写入版本
    data[4] = 1;  // version = 1
    fwrite(data, 1, 64, fp);
    fclose(fp);

    EXPECT_TRUE(hnsw_persist_is_valid(test_file_.c_str()));
}

TEST_F(HNSWPersistTest, GetMeta) {
    // 不存在的文件
    hnsw_persist_meta_t meta;
    EXPECT_NE(hnsw_persist_get_meta("nonexistent.bin", &meta), 0);

    // 有效文件
    FILE *fp = fopen(test_file_.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    unsigned char data[64] = {0};
    // 魔数
    data[0] = 0x57; data[1] = 0x53; data[2] = 0x4E; data[3] = 0x48;
    // 版本
    data[4] = 1;
    // 维度 = 128
    data[8] = 128;
    // 向量数量 = 100
    data[12] = 100;
    fwrite(data, 1, 64, fp);
    fclose(fp);

    EXPECT_EQ(hnsw_persist_get_meta(test_file_.c_str(), &meta), 0);
    EXPECT_EQ(meta.magic, HNSW_PERSIST_MAGIC);
    EXPECT_EQ(meta.version, HNSW_PERSIST_VERSION);
    EXPECT_EQ(meta.dims, 128);
    EXPECT_EQ(meta.n_total, 100);
}

TEST_F(HNSWPersistTest, Stats) {
    // NULL 索引
    int32_t total = -1, deleted = -1;
    uint64_t mem = 0;
    hnsw_persist_stats(nullptr, &total, &deleted, &mem);
    EXPECT_EQ(total, 0);
    EXPECT_EQ(deleted, 0);
    EXPECT_EQ(mem, 0);
}

TEST_F(HNSWPersistTest, SaveLoad) {
    // 测试保存/加载 NULL 索引（占位实现）
    hnsw_persist_result_t save_result = {0};
    int ret = hnsw_persist_save(nullptr, test_file_.c_str(), &save_result);
    EXPECT_EQ(ret, -1);  // 应该失败
}

TEST_F(HNSWPersistTest, LoadInvalidPath) {
    hnsw_persist_result_t result = {0};
    void *index = hnsw_persist_load("", &result);
    EXPECT_EQ(index, nullptr);
}

TEST_F(HNSWPersistTest, HeaderFormat) {
    // 验证头格式与规范一致
    EXPECT_EQ(HNSW_PERSIST_MAGIC, 0x484E5357);
    EXPECT_EQ(HNSW_PERSIST_VERSION, 1);
    EXPECT_EQ(HNSW_PERSIST_HEADER_SIZE, 64);

    // 验证头格式布局
    // 0-3: magic (4 bytes)
    // 4-7: version (4 bytes)
    // 8-11: dims (4 bytes)
    // 12-15: n_total (4 bytes)
    // 16-19: M (4 bytes)
    // 20-23: ef_construction (4 bytes)
    // 24-27: ef_search (4 bytes)
    // 28-31: max_level (4 bytes)
    // 32-35: entry_pointer (4 bytes)
    // 36-39: metric (4 bytes)
    // 40-43: quantization_type (4 bytes)
    // 44-47: code_size (4 bytes)
    // 48-55: created_at (8 bytes)
    // 56-63: modified_at (8 bytes)
    // 总计: 4*14 + 8*2 = 56 + 16 = 72? 不对...

    // 重新计算: 4*14 = 56, 8*2 = 16, 56 + 16 = 72 > 64?
    // 让我重新检查布局...

    // 实际上应该是:
    // 0-3: magic (4)
    // 4-7: version (4)
    // 8-11: dims (4)
    // 12-15: n_total (4)
    // 16-19: M (4)
    // 20-23: ef_construction (4)
    // 24-27: ef_search (4)
    // 28-31: max_level (4)
    // 32-35: entry_pointer (4)
    // 36-39: metric (4)
    // 40-43: quantization_type (4)
    // 44-47: code_size (4)
    // 48-55: created_at (8)
    // 56-63: modified_at (8)
    // 总计 = 64 bytes ✓

    // 验证常量
    EXPECT_EQ(HNSW_PERSIST_HEADER_SIZE, 64);
}
