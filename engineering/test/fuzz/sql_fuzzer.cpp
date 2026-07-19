/**
 * @file sql_fuzzer.cpp
 * @brief SQL 解析器模糊测试
 *
 * 生成随机 SQL 字符串并调用 sql_parse_one() 解析，验证不崩溃。
 * SQL 解析器接受任意输入，失败时返回错误而非崩溃。
 *
 * @note sql_parser.h 中有 C++ 关键字冲突（typename, delete），
 * 通过分别包含 gtest 和 sql_parser.h 避免污染。
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>

/* 在包含 sql_parser.h 前重定义 C++ 关键字冲突 */
#define typename type_name
#define delete delete_stmt
extern "C" {
#include "db/sql/sql_parser.h"
}
#undef delete
#undef typename

/**
 * @brief SQL 模糊测试夹具
 */
class SQLFuzzTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 使用固定种子保证可重复 */
        srand(42);
    }

    /**
     * @brief 生成随机 SQL 字符串
     *
     * 从预定义的 SQL 片段中选择组合，生成语法上可能正确的 SQL。
     * 同时也生成完全的随机字符串。
     */
    std::string generate_random_sql() {
        const char *keywords[] = {
            "SELECT", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP",
            "FROM", "INTO", "VALUES", "SET", "WHERE", "AND", "OR",
            "NOT", "NULL", "TRUE", "FALSE", "IN", "LIKE", "BETWEEN",
            "ORDER BY", "GROUP BY", "HAVING", "LIMIT", "OFFSET",
            "JOIN", "LEFT JOIN", "RIGHT JOIN", "ON", "AS",
            "DISTINCT", "ALL", "UNION", "EXISTS",
            "TABLE", "INDEX", "VIEW", "INT", "VARCHAR", "TEXT",
            "PRIMARY KEY", "FOREIGN KEY", "REFERENCES", "NOT NULL",
            "COUNT", "SUM", "AVG", "MIN", "MAX"
        };
        const char *idents[] = {
            "id", "name", "age", "email", "phone", "address",
            "user", "users", "order", "orders", "product", "products",
            "t1", "t2", "a", "b", "x", "y", "z"
        };
        const char *ops[] = { "=", "<>", ">", "<", ">=", "<=", "+", "-", "*", "/" };
        const char *vals[] = { "1", "0", "42", "100", "'hello'", "'world'", "NULL" };

        int choice = rand() % 8;
        std::string sql;

        switch (choice) {
            case 0: /* SELECT */
                sql = "SELECT ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                sql += " FROM ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                if (rand() % 2) {
                    sql += " WHERE ";
                    sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                    sql += ops[rand() % (sizeof(ops)/sizeof(ops[0]))];
                    sql += vals[rand() % (sizeof(vals)/sizeof(vals[0]))];
                }
                break;

            case 1: /* INSERT */
                sql = "INSERT INTO ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                sql += " VALUES (";
                sql += vals[rand() % (sizeof(vals)/sizeof(vals[0]))];
                sql += ")";
                break;

            case 2: /* UPDATE */
                sql = "UPDATE ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                sql += " SET ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                sql += " = ";
                sql += vals[rand() % (sizeof(vals)/sizeof(vals[0]))];
                break;

            case 3: /* DELETE */
                sql = "DELETE FROM ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                break;

            case 4: /* CREATE TABLE */
                sql = "CREATE TABLE ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                sql += " (id INT, name VARCHAR(100))";
                break;

            case 5: /* DROP */
                sql = "DROP TABLE ";
                sql += idents[rand() % (sizeof(idents)/sizeof(idents[0]))];
                break;

            case 6: /* 随机关键词组合 */
                {
                    int n = rand() % 5 + 1;
                    for (int i = 0; i < n; i++) {
                        sql += keywords[rand() % (sizeof(keywords)/sizeof(keywords[0]))];
                        sql += " ";
                    }
                }
                break;

            case 7: /* 完全随机字符串 */
                {
                    int len = rand() % 256 + 1;
                    for (int i = 0; i < len; i++) {
                        char c = (char)(rand() % 95 + 32); /* 可打印 ASCII */
                        sql += c;
                    }
                }
                break;
        }

        return sql;
    }
};

/**
 * @brief 随机 SQL 输入，验证不崩溃
 */
TEST_F(SQLFuzzTest, RandomInputNoCrash) {
    for (int i = 0; i < 10000; i++) {
        std::string sql = generate_random_sql();

        /* 调用解析器，只验证不崩溃 */
        SqlParseResult_s *result = sql_parse_one(sql.c_str(), (int)sql.length());

        /* 释放结果（注意：result->stmt 指向的 AST 内部通过 parser 管理，
         * 这里只释放 result 结构本身，不释放 AST——因为 sql_parse_one
         * 内部 parser 已销毁 */
        if (result) {
            if (result->errmsg) {
                /* errmsg 是 parser 内部指针，parser 已销毁，不释放 */
            }
            free(result);
        }
    }
}

/**
 * @brief 边界情况 SQL 输入
 */
TEST_F(SQLFuzzTest, BoundaryInputNoCrash) {
    const char *inputs[] = {
        "",
        " ",
        ";",
        "SELECT",
        "INSERT",
        "'",
        "\"",
        "/*",
        "--",
        "\0",
        NULL
    };

    for (int i = 0; inputs[i] != NULL; i++) {
        SqlParseResult_s *result = sql_parse_one(inputs[i], (int)strlen(inputs[i]));
        if (result) {
            free(result);
        }
    }
}

/**
 * @brief 超长 SQL 输入
 */
TEST_F(SQLFuzzTest, LongInputNoCrash) {
    /* 生成一个 10KB 的随机字符串 */
    std::string long_sql;
    for (int i = 0; i < 10000; i++) {
        long_sql += (char)(rand() % 95 + 32);
    }

    SqlParseResult_s *result = sql_parse_one(long_sql.c_str(), (int)long_sql.length());
    if (result) {
        free(result);
    }
}