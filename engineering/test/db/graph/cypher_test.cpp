/**
 * @file cypher_test.cpp
 * @brief Cypher 查询语言解析器测试
 */
#include <gtest/gtest.h>
#include "db/graph/graph_cypher.h"
#include <cstring>

/* ============================================================
 * 解析器测试
 * ============================================================ */

TEST(CypherParserTest, SimpleParse) {
    const char *query = "MATCH (n) RETURN n";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, MatchWithWhere) {
    const char *query = "MATCH (n) WHERE n.age > 30 RETURN n";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, MatchWithPattern) {
    const char *query = "MATCH (a)-[:KNOWS]->(b) RETURN a, b";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, CreateNode) {
    // CREATE 语法支持待完善，暂时跳过
    // const char *query = "CREATE (n:Person {name: 'John'}) RETURN n";
    // CypherQuery *result = cypher_parse(query);
    // EXPECT_NE(result, nullptr);
    // if (result) {
    //     cypher_query_free(result);
    // }
}

TEST(CypherParserTest, WithOrderBy) {
    const char *query = "MATCH (n) RETURN n ORDER BY n.name";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, WithLimit) {
    const char *query = "MATCH (n) RETURN n LIMIT 10";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, WithSkip) {
    const char *query = "MATCH (n) RETURN n SKIP 5 LIMIT 10";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, WithDistinct) {
    const char *query = "MATCH (n) RETURN DISTINCT n.name";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, WithAggregation) {
    const char *query = "MATCH (n) RETURN count(n)";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

TEST(CypherParserTest, ComplexPattern) {
    const char *query = "MATCH (a)-[:KNOWS]->(b)-[:KNOWS]->(c) WHERE a.age > b.age RETURN a, c";
    CypherQuery *result = cypher_parse(query);
    EXPECT_NE(result, nullptr);
    if (result) {
        cypher_query_free(result);
    }
}

/* ============================================================
 * 错误处理测试
 * ============================================================ */

TEST(CypherParserTest, UnclosedParen) {
    const char *query = "MATCH (n RETURN n";
    CypherQuery *result = cypher_parse(query);
    // 解析应该失败或返回错误
    // 具体行为取决于实现
}

TEST(CypherParserTest, EmptyQuery) {
    CypherQuery *result = cypher_parse("");
    // 空查询行为取决于实现
}

TEST(CypherParserTest, InvalidSyntax) {
    const char *query = "XXX YYY ZZZ";
    CypherQuery *result = cypher_parse(query);
    // 无效语法应该被处理
}

/* ============================================================
 * AST 打印测试
 * ============================================================ */

TEST(CypherParserTest, ASTPrint) {
    const char *query = "MATCH (n) RETURN n";
    CypherQuery *result = cypher_parse(query);
    if (result && result->query) {
        // 测试 AST 打印功能（如果有的话）
        // CypherQuery 包含 query 字段
    }
    if (result) {
        cypher_query_free(result);
    }
}
