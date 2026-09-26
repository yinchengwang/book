/**
 * @file test_sql_parse_canonical.cpp
 * @brief canonical SQL 解析器统一入口测试（T6, WS-C）
 *
 * 验证 sql_parse() 统一入口：
 * - 基本 SELECT 解析并返回 T_SelectStmt AST
 * - JOIN / GROUP BY+HAVING+ORDER BY+LIMIT 复杂语句
 * - 语法错误返回 NULL 且错误串带 line:column 位置
 */

#include <gtest/gtest.h>
#include <string.h>

extern "C" {
#include "db/parser/sql/sql_parse.h"
}

TEST(SqlParseCanonical, SelectStar) {
    Node *ast = sql_parse("SELECT * FROM users");
    ASSERT_NE(ast, nullptr) << sql_parse_last_error();
    EXPECT_EQ(ast->type, T_SelectStmt);
}

TEST(SqlParseCanonical, JoinParses) {
    Node *ast = sql_parse("SELECT a.id FROM t1 a JOIN t2 b ON a.id = b.id");
    ASSERT_NE(ast, nullptr) << sql_parse_last_error();
    EXPECT_EQ(ast->type, T_SelectStmt);
}

TEST(SqlParseCanonical, GroupByHavingOrderLimit) {
    /* 注意：gram.y 的 select_stmt 各候选式是累积式的，
     * GROUP BY 之前的 WHERE 子句在语法上不可缺省，故测试 SQL 带 WHERE。 */
    Node *ast = sql_parse(
        "SELECT dept, COUNT(*) FROM emp WHERE id > 0 GROUP BY dept "
        "HAVING COUNT(*) > 1 ORDER BY dept DESC LIMIT 10 OFFSET 5");
    ASSERT_NE(ast, nullptr) << sql_parse_last_error();
    EXPECT_EQ(ast->type, T_SelectStmt);
}

TEST(SqlParseCanonical, SyntaxErrorHasPosition) {
    Node *ast = sql_parse("SELEKT * FORM t1");
    EXPECT_EQ(ast, nullptr);
    const char *err = sql_parse_last_error();
    ASSERT_STRNE(err, "");
    EXPECT_NE(strstr(err, "line"), nullptr) << "错误必须带位置: " << err;
}
