/**
 * @file test_copy_text.cpp
 * @brief TEXT 格式 COPY 测试
 *
 * 测试 TEXT 格式的解析和生成功能。
 */
#include <gtest/gtest.h>
#include <db/tools/copy.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/* 辅助函数：创建临时文件 */
static FILE *create_temp_file(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "/tmp/test_copy_text_%d.tmp", getpid());
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
 * TEXT 导出测试
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief TEXT 导出测试类
 */
class CopyTextExportTest : public ::testing::Test {
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
 * @brief 测试 TEXT 表头导出
 */
TEST_F(CopyTextExportTest, WriteHeader)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name", "age"};
    int ret = text_write_header(temp_fp, cols, 3);
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("id\tname\tage\n", buf);
}

/**
 * @brief 测试 TEXT 行导出（普通值）
 */
TEST_F(CopyTextExportTest, WriteRow)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice", "25"};
    int ret = text_write_row(temp_fp, vals, 3, '\t', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1\tAlice\t25\n", buf);
}

/**
 * @brief 测试 TEXT 行导出（NULL 值）
 */
TEST_F(CopyTextExportTest, WriteRowWithNull)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", NULL, "25"};
    int ret = text_write_row(temp_fp, vals, 3, '\t', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1\t\\N\t25\n", buf);
}

/**
 * @brief 测试自定义分隔符
 */
TEST_F(CopyTextExportTest, CustomDelimiter)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *vals[] = {"1", "Alice", "25"};
    int ret = text_write_row(temp_fp, vals, 3, '|', "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_STREQ("1|Alice|25\n", buf);
}

/* ─────────────────────────────────────────────────────────────────
 * TEXT 导入测试
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief TEXT 导入测试类
 */
class CopyTextImportTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试 TEXT 行解析（普通值）
 */
TEST_F(CopyTextImportTest, ParseLine)
{
    char line[] = "1\tAlice\t25";
    char *fields[10];
    int num_fields = 0;

    int ret = text_parse_line(line, '\t', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 TEXT 行解析（包含换行符）
 */
TEST_F(CopyTextImportTest, ParseLineWithNewline)
{
    char line[] = "1\tAlice\t25\n";
    char *fields[10];
    int num_fields = 0;

    int ret = text_parse_line(line, '\t', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 TEXT 行解析（包含回车换行）
 */
TEST_F(CopyTextImportTest, ParseLineWithCRLF)
{
    char line[] = "1\tAlice\t25\r\n";
    char *fields[10];
    int num_fields = 0;

    int ret = text_parse_line(line, '\t', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 TEXT 行解析（空字段）
 */
TEST_F(CopyTextImportTest, ParseLineWithEmptyField)
{
    char line[] = "1\t\t25";
    char *fields[10];
    int num_fields = 0;

    int ret = text_parse_line(line, '\t', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试 TEXT 行解析（空行）
 */
TEST_F(CopyTextImportTest, ParseEmptyLine)
{
    char line[] = "";
    char *fields[10];
    int num_fields = 0;

    int ret = text_parse_line(line, '\t', fields, 10, &num_fields);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, num_fields);
    EXPECT_STREQ("", fields[0]);
}

/**
 * @brief 测试 TEXT 行解析（参数错误）
 */
TEST_F(CopyTextImportTest, ParseLineNullParams)
{
    char line[] = "1\tAlice\t25";
    char *fields[10];
    int num_fields = 0;

    EXPECT_NE(0, text_parse_line(NULL, '\t', fields, 10, &num_fields));
    EXPECT_NE(0, text_parse_line(line, '\t', NULL, 10, &num_fields));
    EXPECT_NE(0, text_parse_line(line, '\t', fields, 10, NULL));
}

/* ─────────────────────────────────────────────────────────────────
 * TEXT 导入导出往返测试
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief TEXT 往返测试类
 */
class CopyTextRoundtripTest : public ::testing::Test {
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
TEST_F(CopyTextRoundtripTest, RoundtripNormal)
{
    const char *original[] = {"1", "Alice", "25"};

    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);
    text_write_row(temp_fp, original, 3, '\t', "\\N");
    rewind(temp_fp);

    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);

    char *fields[10];
    int num_fields = 0;
    text_parse_line(buf, '\t', fields, 10, &num_fields);

    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("Alice", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}

/**
 * @brief 测试往返：NULL 值
 */
TEST_F(CopyTextRoundtripTest, RoundtripNull)
{
    const char *original[] = {"1", NULL, "25"};

    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);
    text_write_row(temp_fp, original, 3, '\t', "\\N");
    rewind(temp_fp);

    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);

    char *fields[10];
    int num_fields = 0;
    text_parse_line(buf, '\t', fields, 10, &num_fields);

    EXPECT_EQ(3, num_fields);
    EXPECT_STREQ("1", fields[0]);
    EXPECT_STREQ("\\N", fields[1]);
    EXPECT_STREQ("25", fields[2]);
}