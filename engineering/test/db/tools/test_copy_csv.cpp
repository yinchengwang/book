/**
 * @file test_copy_csv.cpp
 * @brief CSV 格式 COPY 测试
 *
 * 测试 CSV 格式的解析和生成功能。
 */
#include <gtest/gtest.h>
#include <db/tools/copy.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/* 辅助函数：创建临时文件 */
static FILE *create_temp_file(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "/tmp/test_copy_csv_%d.tmp", getpid());
    FILE *fp = fopen(path_buf, "w+");
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

/* ─────────────────────────────────────────────────────────────────
 * CSV 导出测试
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief CSV 导出测试类
 */
class CopyCsvExportTest : public ::testing::Test {
protected:
    char temp_path[256];
    FILE *temp_fp;

    void SetUp() override {
        temp_fp = nullptr;
        temp_path[0] = '\0';
    }

    void TearDown() override {
        cleanup_temp_file(temp_fp, temp_path);
    }
};

/**
 * @brief 测试 CSV 表头导出
 */
TEST_F(CopyCsvExportTest, WriteHeader)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name", "age"};
    int ret = csv_write_header(temp_fp, cols, 3, ',');
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("id,name,age\n", buf);
}

/**
 * @brief 测试 CSV 行导出（普通值）
 */
TEST_F(CopyCsvExportTest, WriteRow)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice", "25"};
    int ret = csv_write_row(temp_fp, vals, 3, ',', '"', '"', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1,Alice,25\n", buf);
}

/**
 * @brief 测试 CSV 行导出（包含分隔符）
 */
TEST_F(CopyCsvExportTest, WriteRowWithDelimiter)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice,Bob", "25"};
    int ret = csv_write_row(temp_fp, vals, 3, ',', '"', '"', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1,\"Alice,Bob\",25\n", buf);
}

/**
 * @brief 测试 CSV 行导出（包含换行）
 */
TEST_F(CopyCsvExportTest, WriteRowWithNewline)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice\nBob", "25"};
    int ret = csv_write_row(temp_fp, vals, 3, ',', '"', '"', "\\N");
    EXPECT_EQ(0, ret);

    /* 包含换行的字段会被分成多行 */
    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1,\"Alice\n", buf);
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("Bob\",25\n", buf);
}

/**
 * @brief 测试 CSV 行导出（包含引号）
 */
TEST_F(CopyCsvExportTest, WriteRowWithQuote)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice\"Bob", "25"};
    int ret = csv_write_row(temp_fp, vals, 3, ',', '"', '"', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1,\"Alice\"\"Bob\",25\n", buf);
}

/**
 * @brief 测试 CSV 行导出（NULL 值）
 */
TEST_F(CopyCsvExportTest, WriteRowWithNull)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", NULL, "25"};
    int ret = csv_write_row(temp_fp, vals, 3, ',', '"', '"', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1,\\N,25\n", buf);
}

/**
 * @brief 测试自定义分隔符
 */
TEST_F(CopyCsvExportTest, CustomDelimiter)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice", "25"};
    int ret = csv_write_row(temp_fp, vals, 3, '|', '"', '"', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1|Alice|25\n", buf);
}

/* ─────────────────────────────────────────────────────────────────
 * CSV 导入测试
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief CSV 导入测试类
 */
class CopyCsvImportTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试 CSV 行解析（普通值）
 */
TEST_F(CopyCsvImportTest, ParseLine)
{
    char line[] = "1,Alice,25";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 CSV 行解析（包含引号字段）
 */
TEST_F(CopyCsvImportTest, ParseLineWithQuote)
{
    char line[] = "1,\"Alice,Bob\",25";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice,Bob", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 CSV 行解析（包含转义引号）
 */
TEST_F(CopyCsvImportTest, ParseLineWithEscapedQuote)
{
    char line[] = "1,\"Alice\"\"Bob\",25";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice\"Bob", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 CSV 行解析（空字段）
 */
TEST_F(CopyCsvImportTest, ParseLineWithEmptyField)
{
    char line[] = "1,,25";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_EQ(nullptr, fields[1]);  /* 空字段为 NULL */
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 CSV 行解析（开头空字段）
 */
TEST_F(CopyCsvImportTest, ParseLineWithLeadingEmptyField)
{
    char line[] = ",Alice,25";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_EQ(nullptr, fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 CSV 行解析（结尾空字段）
 */
TEST_F(CopyCsvImportTest, ParseLineWithTrailingEmptyField)
{
    char line[] = "1,Alice,";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_EQ(nullptr, fields[2]);  /* 结尾空字段为 NULL */
}

/**
 * @brief 测试 CSV 行解析（空行）
 */
TEST_F(CopyCsvImportTest, ParseEmptyLine)
{
    char line[] = "";
    char *fields[10];
    int num_fields = 0;

    int ret = csv_parse_line(line, ',', '"', '"', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, num_fields);
}

/**
 * @brief 测试 CSV 行解析（参数错误）
 */
TEST_F(CopyCsvImportTest, ParseLineNullParams)
{
    char line[] = "1,Alice,25";
    char *fields[10];
    int num_fields = 0;

    /* line 为 NULL */
    EXPECT_NE(0, csv_parse_line(NULL, ',', '"', '"', fields, 10, &num_fields));

    /* fields 为 NULL */
    EXPECT_NE(0, csv_parse_line(line, ',', '"', '"', NULL, 10, &num_fields));

    /* num_fields 为 NULL */
    EXPECT_NE(0, csv_parse_line(line, ',', '"', '"', fields, 10, NULL));
}

/* ─────────────────────────────────────────────────────────────────
 * CSV 导入导出往返测试
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief CSV 往返测试类
 */
class CopyCsvRoundtripTest : public ::testing::Test {
protected:
    char temp_path[256];
    FILE *temp_fp;

    void SetUp() override {
        temp_fp = nullptr;
        temp_path[0] = '\0';
    }

    void TearDown() override {
        cleanup_temp_file(temp_fp, temp_path);
    }
};

/**
 * @brief 测试往返：普通值
 */
TEST_F(CopyCsvRoundtripTest, RoundtripNormal)
{
    const char *original[] = {"1", "Alice", "25"};

    /* 导出 */
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);
    csv_write_row(temp_fp, original, 3, ',', '"', '"', "\\N");
    rewind(temp_fp);

    /* 导入 */
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);

    char *fields[10];
    int num_fields = 0;
    csv_parse_line(buf, ',', '"', '"', fields, 10, &num_fields);

    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试往返：包含特殊字符
 */
TEST_F(CopyCsvRoundtripTest, RoundtripSpecialChars)
{
    const char *original[] = {"1", "Alice,Bob", "25"};

    /* 导出 */
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);
    csv_write_row(temp_fp, original, 3, ',', '"', '"', "\\N");
    rewind(temp_fp);

    /* 导入 */
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);

    char *fields[10];
    int num_fields = 0;
    csv_parse_line(buf, ',', '"', '"', fields, 10, &num_fields);

    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice,Bob", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}