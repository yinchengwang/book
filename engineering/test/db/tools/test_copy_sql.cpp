/**
 * @file test_copy_sql.cpp
 * @brief SQL INSERT 格式 COPY 测试
 */

#include <gtest/gtest.h>

extern "C" {
#include <db/tools/copy.h>
}

#include <cstdio>
#include <cstring>
#include <unistd.h>

/* 辅助函数：创建临时文件 */
static FILE *create_temp_file(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "/tmp/test_copy_sql_%d.tmp", getpid());
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
 * SQL 导出测试
 * ───────────────────────────────────────────────────────────────── */

class CopySqlTest : public ::testing::Test {
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

TEST_F(CopySqlTest, WriteInsert)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name", "age"};
    const char *vals[] = {"1", "Alice", "30"};

    int ret = sql_write_insert(temp_fp, "users", cols, vals, 3, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "INSERT INTO users") != nullptr);
    EXPECT_TRUE(strstr(buf, "VALUES") != nullptr);
    EXPECT_TRUE(strstr(buf, "'Alice'") != nullptr);
}

TEST_F(CopySqlTest, WriteInsertWithNull)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name"};
    const char *vals[] = {"1", NULL};

    int ret = sql_write_insert(temp_fp, "users", cols, vals, 2, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "NULL") != nullptr);
}

TEST_F(CopySqlTest, WriteInsertWithNullString)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name"};
    const char *vals[] = {"1", "\\N"};

    int ret = sql_write_insert(temp_fp, "users", cols, vals, 2, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    /* \\N 应被识别为 NULL */
    EXPECT_TRUE(strstr(buf, "NULL") != nullptr);
}

TEST_F(CopySqlTest, EscapeQuotes)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"name"};
    const char *vals[] = {"O'Brien"};

    int ret = sql_write_insert(temp_fp, "users", cols, vals, 1, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    /* 单引号应被转义为两个单引号 */
    EXPECT_TRUE(strstr(buf, "O''Brien") != nullptr);
}

TEST_F(CopySqlTest, MultipleQuotes)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"msg"};
    const char *vals[] = {"It's a test, isn't it?"};

    int ret = sql_write_insert(temp_fp, "users", cols, vals, 1, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    /* 两个单引号都应被转义 */
    EXPECT_TRUE(strstr(buf, "It''s") != nullptr);
    EXPECT_TRUE(strstr(buf, "isn''t") != nullptr);
}

TEST_F(CopySqlTest, ColumnNamesInOutput)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"user_id", "user_name", "email"};
    const char *vals[] = {"1", "Bob", "bob@test.com"};

    int ret = sql_write_insert(temp_fp, "accounts", cols, vals, 3, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "user_id") != nullptr);
    EXPECT_TRUE(strstr(buf, "user_name") != nullptr);
    EXPECT_TRUE(strstr(buf, "email") != nullptr);
    EXPECT_TRUE(strstr(buf, "INSERT INTO accounts") != nullptr);
}

TEST_F(CopySqlTest, StatementEndsWithSemicolon)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id"};
    const char *vals[] = {"1"};

    int ret = sql_write_insert(temp_fp, "t", cols, vals, 1, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    /* 语句应以 );\n 结尾 */
    EXPECT_TRUE(strstr(buf, ");") != nullptr);
}

TEST_F(CopySqlTest, NullPointerReturnsError)
{
    const char *cols[] = {"id"};
    const char *vals[] = {"1"};

    EXPECT_NE(0, sql_write_insert(NULL, "t", cols, vals, 1, "\\N"));
    EXPECT_NE(0, sql_write_insert(stdout, NULL, cols, vals, 1, "\\N"));
    EXPECT_NE(0, sql_write_insert(stdout, "t", NULL, vals, 1, "\\N"));
    EXPECT_NE(0, sql_write_insert(stdout, "t", cols, NULL, 1, "\\N"));
}