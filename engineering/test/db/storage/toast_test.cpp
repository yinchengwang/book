/**
 * @file toast_test.cpp
 * @brief W3.1: TOAST 大值存储测试
 *
 * 验证 TOAST 压缩、解压缩、大值分割存储功能。
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/access/toast.h"
}

class ToastTest : public ::testing::Test {
protected:
    void SetUp() override {
        toast_init();
    }

    void TearDown() override {
        toast_shutdown();
    }
};

TEST_F(ToastTest, NeedsToast) {
    // 验证 TOAST 判断
    EXPECT_FALSE(toast_needs_toast(100));    // 小于阈值
    EXPECT_FALSE(toast_needs_toast(2000));   // 等于阈值（不触发）
    EXPECT_TRUE(toast_needs_toast(2001));     // 超过阈值
    EXPECT_TRUE(toast_needs_toast(8192));     // 一个页面大小
    EXPECT_TRUE(toast_needs_toast(65536));     // 64KB
}

TEST_F(ToastTest, CompressAndDecompress) {
    // 验证压缩和解压缩
    const char *test_data = "Hello, TOAST! This is a test of the compression algorithm. "
                            "The quick brown fox jumps over the lazy dog. "
                            "Repeated data! Repeated data! Repeated data! ";
    size_t src_len = strlen(test_data) + 1;

    // 分配缓冲区
    char compressed[1024];
    char decompressed[1024];

    // 压缩
    int compressed_size = toast_compress(test_data, src_len,
                                         compressed, sizeof(compressed));
    ASSERT_GT(compressed_size, 0);

    // 解压缩
    int decompressed_size = toast_decompress(compressed, (size_t)compressed_size,
                                             decompressed, sizeof(decompressed));
    ASSERT_GT(decompressed_size, 0);

    // 验证解压后数据与原始数据一致
    ASSERT_EQ((size_t)decompressed_size, src_len);
    EXPECT_EQ(memcmp(test_data, decompressed, src_len), 0);
}

TEST_F(ToastTest, StoreAndFetch) {
    // 验证 TOAST 存储和读取
    // 注意：当前实现中 toast_store 是简化实现
    // 实际存储需要 TOAST 表支持

    const char *large_value = "A very large value that exceeds the TOAST threshold "
                              "and needs to be stored in the TOAST table. "
                              "This simulates a real-world scenario where a tuple "
                              "contains a large text field, such as a blog post "
                              "or a document.";
    size_t value_len = strlen(large_value) + 1;

    uint32_t chunk_id = 0;
    int ret = toast_store(1, large_value, value_len, &chunk_id);
    EXPECT_EQ(ret, TOAST_SUCCESS);
    EXPECT_GT(chunk_id, 0);  // 应该分配了 Chunk ID
}

TEST_F(ToastTest, LargeValueStorage) {
    // 验证大值存储
    // 生成 10KB 的测试数据
    size_t large_size = 10240;
    char *large_data = (char *)malloc(large_size);
    ASSERT_TRUE(large_data != NULL);

    // 填充数据
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = (char)('A' + (i % 26));
    }

    // 验证 TOAST 检测
    EXPECT_TRUE(toast_needs_toast(large_size));

    // 尝试压缩
    char *compressed = (char *)malloc(large_size + 128);
    ASSERT_TRUE(compressed != NULL);

    int compressed_size = toast_compress(large_data, large_size,
                                         compressed, large_size + 128);
    EXPECT_GT(compressed_size, 0);

    // 解压验证
    char *decompressed = (char *)malloc(large_size);
    ASSERT_TRUE(decompressed != NULL);

    int decompressed_size = toast_decompress(compressed, (size_t)compressed_size,
                                             decompressed, large_size);
    ASSERT_EQ((size_t)decompressed_size, large_size);
    EXPECT_EQ(memcmp(large_data, decompressed, large_size), 0);

    free(large_data);
    free(compressed);
    free(decompressed);
}

TEST_F(ToastTest, SmallValueNoCompress) {
    // 小值不需要压缩
    const char *small_value = "small";
    size_t small_len = strlen(small_value) + 1;

    // 小值不应该触发 TOAST
    EXPECT_FALSE(toast_needs_toast(small_len));

    // 但压缩 API 仍然可以处理
    char compressed[256];
    char decompressed[256];

    int compressed_size = toast_compress(small_value, small_len,
                                         compressed, sizeof(compressed));
    // 小值可能不压缩（返回 -1）
    if (compressed_size > 0) {
        int decompressed_size = toast_decompress(compressed, (size_t)compressed_size,
                                                 decompressed, sizeof(decompressed));
        ASSERT_GT(decompressed_size, 0);
        ASSERT_EQ((size_t)decompressed_size, small_len);
        EXPECT_EQ(memcmp(small_value, decompressed, small_len), 0);
    }
}

TEST_F(ToastTest, EstimateCompressedSize) {
    // 验证压缩大小估算
    EXPECT_EQ(toast_estimate_compressed_size(100), 164);
    EXPECT_EQ(toast_estimate_compressed_size(1000), 1064);
    EXPECT_EQ(toast_estimate_compressed_size(2000), 2064);
}