/**
 * @file resp_test.cpp
 * @brief RESP 协议解析与构建单元测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/protocol/resp.h"
}

#include <cstring>
#include <cstdlib>

/* ============================================================
 * RESP 构建器测试
 * ============================================================ */

TEST(RespBuildTest, SimpleString) {
    char buf[64];
    int n = resp_build_string(buf, sizeof(buf), "OK");
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "+OK\r\n");
}

TEST(RespBuildTest, Error) {
    char buf[64];
    int n = resp_build_error(buf, sizeof(buf), "ERR something");
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "-ERR something\r\n");
}

TEST(RespBuildTest, Integer) {
    char buf[64];
    int n = resp_build_integer(buf, sizeof(buf), 42);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, ":42\r\n");
}

TEST(RespBuildTest, IntegerNegative) {
    char buf[64];
    int n = resp_build_integer(buf, sizeof(buf), -100);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, ":-100\r\n");
}

TEST(RespBuildTest, BulkString) {
    char buf[64];
    int n = resp_build_bulk_string(buf, sizeof(buf), "hello", 5);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "$5\r\nhello\r\n");
}

TEST(RespBuildTest, BulkStringNull) {
    char buf[64];
    int n = resp_build_bulk_string(buf, sizeof(buf), NULL, -1);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "$-1\r\n");
}

TEST(RespBuildTest, ArrayHeader) {
    char buf[64];
    int n = resp_build_array_header(buf, sizeof(buf), 3);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "*3\r\n");
}

TEST(RespBuildTest, Boolean) {
    char buf[64];
    int n = resp_build_boolean(buf, sizeof(buf), true);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "#t\r\n");

    n = resp_build_boolean(buf, sizeof(buf), false);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "#f\r\n");
}

TEST(RespBuildTest, Double) {
    char buf[64];
    int n = resp_build_double(buf, sizeof(buf), 3.14);
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, ",3.14\r\n");
}

TEST(RespBuildTest, OkAndPong) {
    char buf[64];
    int n = resp_build_ok(buf, sizeof(buf));
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "+OK\r\n");

    n = resp_build_pong(buf, sizeof(buf));
    ASSERT_GT(n, 0);
    EXPECT_STREQ(buf, "+PONG\r\n");
}

/* ============================================================
 * RESP 解析器测试
 * ============================================================ */

TEST(RespParseTest, SimpleString) {
    const char *input = "+OK\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_STRING);
    EXPECT_EQ(value.data.string.len, 2);
    EXPECT_EQ(memcmp(value.data.string.str, "OK", 2), 0);
}

TEST(RespParseTest, Error) {
    const char *input = "-ERR something\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_ERROR);
}

TEST(RespParseTest, Integer) {
    const char *input = ":42\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_INTEGER);
    EXPECT_EQ(value.data.integer, 42);
}

TEST(RespParseTest, BulkString) {
    const char *input = "$5\r\nhello\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_BULK_STRING);
    EXPECT_EQ(value.data.bulk_string.len, 5);
    EXPECT_EQ(memcmp(value.data.bulk_string.str, "hello", 5), 0);
}

TEST(RespParseTest, NullBulkString) {
    const char *input = "$-1\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_NULL);
}

TEST(RespParseTest, Array) {
    const char *input = "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value.type, RESP_ARRAY);
    ASSERT_EQ(value.data.collection.count, 2);

    /* 第一个元素：GET */
    resp_value_t *elem0 = &value.data.collection.elements[0];
    ASSERT_EQ(elem0->type, RESP_BULK_STRING);
    EXPECT_EQ(elem0->data.bulk_string.len, 3);
    EXPECT_EQ(memcmp(elem0->data.bulk_string.str, "GET", 3), 0);

    /* 第二个元素：mykey */
    resp_value_t *elem1 = &value.data.collection.elements[1];
    ASSERT_EQ(elem1->type, RESP_BULK_STRING);
    EXPECT_EQ(elem1->data.bulk_string.len, 5);
    EXPECT_EQ(memcmp(elem1->data.bulk_string.str, "mykey", 5), 0);

    resp_free_value(&value);
}

TEST(RespParseTest, Boolean) {
    const char *input = "#t\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_BOOLEAN);
    EXPECT_TRUE(value.data.boolean);
}

TEST(RespParseTest, Double) {
    const char *input = ",3.14\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_DOUBLE);
    EXPECT_FLOAT_EQ(value.data.dval, 3.14f);
}

/* ============================================================
 * RESP 命令解析测试
 * ============================================================ */

TEST(RespCommandTest, ParseSetCommand) {
    const char *input = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    int rc = resp_parse_command(&parser, &cmd);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(cmd.argc, 3);
    EXPECT_STREQ(cmd.argv[0], "SET");
    EXPECT_STREQ(cmd.argv[1], "key");
    EXPECT_STREQ(cmd.argv[2], "value");

    resp_free_command(&cmd);
}

TEST(RespCommandTest, ParseGetCommand) {
    const char *input = "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n";
    resp_parser_t parser;
    resp_parser_init(&parser, input, (int)strlen(input));

    resp_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    int rc = resp_parse_command(&parser, &cmd);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(cmd.argc, 2);
    EXPECT_STREQ(cmd.argv[0], "GET");
    EXPECT_STREQ(cmd.argv[1], "mykey");

    resp_free_command(&cmd);
}

/* ============================================================
 * RESP 构建-解析往返测试
 * ============================================================ */

TEST(RespRoundtripTest, StringRoundtrip) {
    char buf[64];
    int n = resp_build_string(buf, sizeof(buf), "Hello World");
    ASSERT_GT(n, 0);

    resp_parser_t parser;
    resp_parser_init(&parser, buf, n);

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_STRING);
    EXPECT_EQ(value.data.string.len, 11);
    EXPECT_EQ(memcmp(value.data.string.str, "Hello World", 11), 0);
}

TEST(RespRoundtripTest, IntegerRoundtrip) {
    char buf[64];
    int n = resp_build_integer(buf, sizeof(buf), 999);
    ASSERT_GT(n, 0);

    resp_parser_t parser;
    resp_parser_init(&parser, buf, n);

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_INTEGER);
    EXPECT_EQ(value.data.integer, 999);
}

TEST(RespRoundtripTest, BulkStringRoundtrip) {
    const char *test_str = "Hello RESP!";
    int test_len = (int)strlen(test_str);

    char buf[64];
    int n = resp_build_bulk_string(buf, sizeof(buf), test_str, test_len);
    ASSERT_GT(n, 0);

    resp_parser_t parser;
    resp_parser_init(&parser, buf, n);

    resp_value_t value;
    memset(&value, 0, sizeof(value));
    int rc = resp_parse_next(&parser, &value);

    ASSERT_EQ(rc, 0);
    EXPECT_EQ(value.type, RESP_BULK_STRING);
    EXPECT_EQ(value.data.bulk_string.len, test_len);
    EXPECT_EQ(memcmp(value.data.bulk_string.str, test_str, (size_t)test_len), 0);
}