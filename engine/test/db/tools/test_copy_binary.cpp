/**
 * @file test_copy_binary.cpp
 * @brief 二进制格式 COPY 测试
 */

#include <gtest/gtest.h>
#include <db/tools/copy.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/* 辅助函数：创建临时文件 */
static FILE *create_temp_file(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "/tmp/test_copy_binary_%d.tmp", getpid());
    FILE *fp = fopen(path_buf, "w+b");
    return fp;
}

/* 辅助函数：关闭并删除临时文件 */
static void cleanup_temp_file(FILE *fp, const char *path)
{
    if (fp) {
        fclose(fp);
    }
    if (path) {
        unlink(path);
    }
}

/**
 * @brief 二进制格式 COPY 测试类
 */
class CopyBinaryTest : public ::testing::Test {
protected:
    char temp_path[256];
    FILE *temp_fp;

    void SetUp() override
    {
        temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    }

    void TearDown() override
    {
        cleanup_temp_file(temp_fp, temp_path);
        temp_fp = nullptr;
    }
};

TEST_F(CopyBinaryTest, WriteHeader)
{
    ASSERT_NE(temp_fp, nullptr);

    int ret = binary_write_header(temp_fp, 3);
    EXPECT_EQ(0, ret);
}

TEST_F(CopyBinaryTest, WriteRow)
{
    ASSERT_NE(temp_fp, nullptr);

    binary_write_header(temp_fp, 3);

    const char *vals[] = {"1", "Alice", "30"};
    int ret = binary_write_row(temp_fp, vals, 3);
    EXPECT_EQ(0, ret);
}

TEST_F(CopyBinaryTest, WriteRowWithNull)
{
    ASSERT_NE(temp_fp, nullptr);

    binary_write_header(temp_fp, 2);

    const char *vals[] = {"1", NULL};
    int ret = binary_write_row(temp_fp, vals, 2);
    EXPECT_EQ(0, ret);
}

TEST_F(CopyBinaryTest, ReadHeader)
{
    ASSERT_NE(temp_fp, nullptr);

    binary_write_header(temp_fp, 3);

    rewind(temp_fp);
    int num_cols = 0;
    int ret = binary_read_header(temp_fp, &num_cols);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_cols);
}

TEST_F(CopyBinaryTest, ReadField)
{
    ASSERT_NE(temp_fp, nullptr);

    binary_write_header(temp_fp, 2);

    const char *vals[] = {"hello", "world"};
    binary_write_row(temp_fp, vals, 2);

    rewind(temp_fp);
    int num_cols = 0;
    int ret = binary_read_header(temp_fp, &num_cols);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, num_cols);

    char *value = nullptr;
    uint32_t len = 0;

    ret = binary_read_field(temp_fp, &value, &len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(5, len);
    EXPECT_STREQ("hello", value);
    free(value);

    ret = binary_read_field(temp_fp, &value, &len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(5, len);
    EXPECT_STREQ("world", value);
    free(value);
}

TEST_F(CopyBinaryTest, ReadNullField)
{
    ASSERT_NE(temp_fp, nullptr);

    binary_write_header(temp_fp, 2);

    const char *vals[] = {"test", NULL};
    binary_write_row(temp_fp, vals, 2);

    rewind(temp_fp);
    int num_cols = 0;
    int ret = binary_read_header(temp_fp, &num_cols);
    ASSERT_EQ(0, ret);

    char *value = nullptr;
    uint32_t len = 0;

    ret = binary_read_field(temp_fp, &value, &len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(4, len);
    EXPECT_STREQ("test", value);
    free(value);

    ret = binary_read_field(temp_fp, &value, &len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, len);
    EXPECT_EQ(nullptr, value);
}

TEST_F(CopyBinaryTest, InvalidMagic)
{
    ASSERT_NE(temp_fp, nullptr);

    /* 写入无效数据 */
    const char *invalid = "INVALID";
    fwrite(invalid, 1, strlen(invalid), temp_fp);

    rewind(temp_fp);
    int num_cols = 0;
    int ret = binary_read_header(temp_fp, &num_cols);
    EXPECT_EQ(-1, ret);
}
