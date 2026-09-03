/**
 * @file test_copy_json.cpp
 * @brief JSON Lines 格式 COPY 测试
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
    snprintf(path_buf, buf_size, "/tmp/test_copy_json_%d.tmp", getpid());
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
 * JSON 导出测试
 * ───────────────────────────────────────────────────────────────── */

class CopyJsonTest : public ::testing::Test {
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

TEST_F(CopyJsonTest, WriteRow)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name", "age"};
    const char *vals[] = {"1", "Alice", "30"};

    int ret = json_write_row(temp_fp, cols, vals, 3, "\\N");
    EXPECT_EQ(0, ret);

    /* 验证输出 */
    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "\"id\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Alice\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"name\"") != nullptr);
}

TEST_F(CopyJsonTest, WriteRowWithNull)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name"};
    const char *vals[] = {"1", NULL};

    int ret = json_write_row(temp_fp, cols, vals, 2, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "null") != nullptr);
}

TEST_F(CopyJsonTest, WriteRowWithNullString)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"id", "name"};
    const char *vals[] = {"1", "\\N"};

    int ret = json_write_row(temp_fp, cols, vals, 2, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    /* \\N 应被识别为 null */
    EXPECT_TRUE(strstr(buf, "null") != nullptr);
}

TEST_F(CopyJsonTest, EscapeSpecialChars)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"text"};
    const char *vals[] = {"hello\"world\n"};

    int ret = json_write_row(temp_fp, cols, vals, 1, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    /* 双引号应被转义 */
    EXPECT_TRUE(strstr(buf, "\\\"") != nullptr);
    /* 换行应被转义 */
    EXPECT_TRUE(strstr(buf, "\\n") != nullptr);
}

TEST_F(CopyJsonTest, EscapeBackslash)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"path"};
    const char *vals[] = {"C:\\Users\\test"};

    int ret = json_write_row(temp_fp, cols, vals, 1, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    /* 反斜杠应被转义 */
    EXPECT_TRUE(strstr(buf, "\\\\") != nullptr);
}

TEST_F(CopyJsonTest, EscapeTab)
{
    temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    ASSERT_NE(temp_fp, nullptr);

    const char *cols[] = {"data"};
    const char *vals[] = {"col1\tcol2"};

    int ret = json_write_row(temp_fp, cols, vals, 1, "\\N");
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[256];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "\\t") != nullptr);
}

TEST_F(CopyJsonTest, NullPointerReturnsError)
{
    const char *cols[] = {"id"};
    const char *vals[] = {"1"};

    EXPECT_NE(0, json_write_row(NULL, cols, vals, 1, "\\N"));
    EXPECT_NE(0, json_write_row(stdout, NULL, vals, 1, "\\N"));
    EXPECT_NE(0, json_write_row(stdout, cols, NULL, 1, "\\N"));
}

/* ─────────────────────────────────────────────────────────────────
 * JSON 导入测试
 * ───────────────────────────────────────────────────────────────── */

class CopyJsonParseTest : public ::testing::Test {
protected:
    /* 每个键/值缓冲区大小 */
    static const int BUF_SIZE = 256;
    static const int MAX_PAIRS = 16;
    char keys[MAX_PAIRS][BUF_SIZE];
    char *key_ptrs[MAX_PAIRS];
    char vals[MAX_PAIRS][BUF_SIZE];
    char *val_ptrs[MAX_PAIRS];
    int num_pairs;

    void SetUp() override
    {
        for (int i = 0; i < MAX_PAIRS; i++) {
            key_ptrs[i] = keys[i];
            val_ptrs[i] = vals[i];
        }
    }
};

TEST_F(CopyJsonParseTest, ParseSimpleObject)
{
    const char *line = "{\"id\":\"1\",\"name\":\"Alice\"}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, num_pairs);
    EXPECT_STREQ("id", keys[0]);
    EXPECT_STREQ("1", vals[0]);
    EXPECT_STREQ("name", keys[1]);
    EXPECT_STREQ("Alice", vals[1]);
}

TEST_F(CopyJsonParseTest, ParseNullValue)
{
    const char *line = "{\"id\":\"1\",\"name\":null}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, num_pairs);
    EXPECT_STREQ("", vals[1]);
}

TEST_F(CopyJsonParseTest, ParseNumberValue)
{
    const char *line = "{\"id\":42,\"score\":3.14}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, num_pairs);
    EXPECT_STREQ("42", vals[0]);
    EXPECT_STREQ("3.14", vals[1]);
}

TEST_F(CopyJsonParseTest, ParseBooleanValue)
{
    const char *line = "{\"active\":true,\"deleted\":false}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, num_pairs);
    EXPECT_STREQ("true", vals[0]);
    EXPECT_STREQ("false", vals[1]);
}

TEST_F(CopyJsonParseTest, ParseEmptyObject)
{
    const char *line = "{}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, num_pairs);
}

TEST_F(CopyJsonParseTest, ParseEscapedChars)
{
    const char *line = "{\"text\":\"hello\\nworld\"}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, num_pairs);
    EXPECT_STREQ("hello\nworld", vals[0]);
}

TEST_F(CopyJsonParseTest, ParseEscapedQuote)
{
    const char *line = "{\"text\":\"say \\\"hi\\\"\"}";
    int ret = json_parse_line(line, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, num_pairs);
    EXPECT_STREQ("say \"hi\"", vals[0]);
}

TEST_F(CopyJsonParseTest, InvalidInputReturnsError)
{
    EXPECT_NE(0, json_parse_line(NULL, key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs));
    EXPECT_NE(0, json_parse_line("{}", NULL, val_ptrs, MAX_PAIRS, &num_pairs));
    EXPECT_NE(0, json_parse_line("{}", key_ptrs, NULL, MAX_PAIRS, &num_pairs));
    EXPECT_NE(0, json_parse_line("{}", key_ptrs, val_ptrs, MAX_PAIRS, NULL));
    /* 非对象格式 */
    EXPECT_NE(0, json_parse_line("[1,2,3]", key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs));
    EXPECT_NE(0, json_parse_line("invalid", key_ptrs, val_ptrs, MAX_PAIRS, &num_pairs));
}
