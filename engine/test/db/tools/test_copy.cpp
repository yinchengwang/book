/**
 * @file test_copy.cpp
 * @brief COPY 工具测试
 *
 * 测试 COPY 数据导入导出工具的接口实现。
 */
#include <gtest/gtest.h>
#include <db/tools/copy.h>

/**
 * @brief COPY 工具测试类
 */
class CopyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试默认选项
 */
TEST(CopyTest, DefaultOptions)
{
    CopyOptions opts = copy_default_options();
    EXPECT_EQ(COPY_FORMAT_CSV, opts.format);
    EXPECT_EQ(',', opts.delimiter);
    EXPECT_STREQ("\\N", opts.null_string);
    EXPECT_TRUE(opts.header);
    EXPECT_EQ('"', opts.quote_char);
    EXPECT_EQ('"', opts.escape_char);
}

/**
 * @brief 测试创建和销毁上下文
 */
TEST(CopyTest, CreateDestroy)
{
    CopyOptions opts = copy_default_options();
    CopyContext *ctx = copy_create(&opts);
    ASSERT_NE(nullptr, ctx);
    copy_destroy(ctx);
}

/**
 * @brief 测试使用空选项创建
 */
TEST(CopyTest, CreateWithNullOptions)
{
    CopyContext *ctx = copy_create(NULL);
    ASSERT_NE(nullptr, ctx);
    EXPECT_EQ(0, copy_get_row_count(ctx));
    copy_destroy(ctx);
}

/**
 * @brief 测试行计数和错误计数
 */
TEST(CopyTest, RowCountAndErrors)
{
    CopyOptions opts = copy_default_options();
    CopyContext *ctx = copy_create(&opts);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, copy_get_row_count(ctx));
    EXPECT_EQ(0, copy_get_error_count(ctx));

    copy_destroy(ctx);
}

/**
 * @brief 测试空字符串解析
 */
TEST(CopyTest, ParseOptionsEmpty)
{
    CopyOptions opts;
    int ret = copy_parse_options("", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_CSV, opts.format);
}

/**
 * @brief 测试 NULL 字符串解析
 */
TEST(CopyTest, ParseOptionsNull)
{
    CopyOptions opts;
    int ret = copy_parse_options(NULL, &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_CSV, opts.format);
}

/**
 * @brief 测试解析 FORMAT csv
 */
TEST(CopyTest, ParseOptionsFormatCsv)
{
    CopyOptions opts;
    int ret = copy_parse_options("FORMAT csv", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_CSV, opts.format);
}

/**
 * @brief 测试解析 FORMAT text
 */
TEST(CopyTest, ParseOptionsFormatText)
{
    CopyOptions opts;
    int ret = copy_parse_options("FORMAT text", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_TEXT, opts.format);
}

/**
 * @brief 测试解析 FORMAT json
 */
TEST(CopyTest, ParseOptionsFormatJson)
{
    CopyOptions opts;
    int ret = copy_parse_options("FORMAT json", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_JSON, opts.format);
}

/**
 * @brief 测试解析 FORMAT binary
 */
TEST(CopyTest, ParseOptionsFormatBinary)
{
    CopyOptions opts;
    int ret = copy_parse_options("FORMAT binary", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_BINARY, opts.format);
}

/**
 * @brief 测试解析 FORMAT sql
 */
TEST(CopyTest, ParseOptionsFormatSql)
{
    CopyOptions opts;
    int ret = copy_parse_options("FORMAT sql", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_SQL, opts.format);
}

/**
 * @brief 测试解析分隔符
 */
TEST(CopyTest, ParseOptionsDelimiterTab)
{
    CopyOptions opts;
    int ret = copy_parse_options("DELIMITER '\\t'", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ('\t', opts.delimiter);
}

/**
 * @brief 测试解析单字符分隔符
 */
TEST(CopyTest, ParseOptionsDelimiterChar)
{
    CopyOptions opts;
    int ret = copy_parse_options("DELIMITER '|'", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ('|', opts.delimiter);
}

/**
 * @brief 测试解析 HEADER true
 */
TEST(CopyTest, ParseOptionsHeaderTrue)
{
    CopyOptions opts;
    int ret = copy_parse_options("HEADER true", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(opts.header);
}

/**
 * @brief 测试解析 HEADER false
 */
TEST(CopyTest, ParseOptionsHeaderFalse)
{
    CopyOptions opts;
    int ret = copy_parse_options("HEADER false", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(opts.header);
}

/**
 * @brief 测试解析多个选项
 */
TEST(CopyTest, ParseOptionsMultiple)
{
    CopyOptions opts;
    int ret = copy_parse_options("FORMAT text DELIMITER '|' HEADER false", &opts);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(COPY_FORMAT_TEXT, opts.format);
    EXPECT_EQ('|', opts.delimiter);
    EXPECT_FALSE(opts.header);
}

/**
 * @brief 测试 NULL 选项参数
 */
TEST(CopyTest, ParseOptionsNullParam)
{
    int ret = copy_parse_options("", NULL);
    EXPECT_NE(0, ret);
}
