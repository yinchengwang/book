/**
 * @file test_e2e_query.cpp
 * @brief SQL 端到端查询测试
 *
 * Task 5.2: SQL 解析器端到端集成
 *
 * 验证完整的 SQL 执行流程：
 * 1. 简单 SELECT 查询
 * 2. 常量查询
 * 3. INSERT + SELECT 流程
 * 4. WHERE 条件过滤
 * 5. 聚合函数
 * 6. 连接查询
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/sql_driver.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"
}

namespace {

/**
 * @brief 端到端查询测试环境
 *
 * 初始化存储子系统和清理环境
 */
class E2EQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化存储子系统
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(1024), 0) << "Buffer Pool 初始化失败";
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    void TearDown() override {
        // 清理子系统
        rel_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }

    // 辅助函数：创建测试表
    Oid create_test_table(const char *name, int num_columns = 2) {
        column_def_t columns[3];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN);
        columns[0].type_oid = 23;  // int4
        columns[0].not_null = true;

        if (num_columns >= 2) {
            strncpy(columns[1].name, "value", NAMEDATALEN);
            columns[1].type_oid = 23;  // int4
        }

        if (num_columns >= 3) {
            strncpy(columns[2].name, "name", NAMEDATALEN);
            columns[2].type_oid = 25;  // text
        }

        return catalog_create_table(name, columns, num_columns);
    }
};

/**
 * @brief 测试 1: 简单 SELECT 查询
 *
 * 测试场景：SELECT * FROM table
 */
TEST_F(E2EQueryTest, SimpleSelect) {
    // 创建测试表
    Oid table_oid = create_test_table("test_select");
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    // 执行简单 SELECT 查询
    const char *sql = "SELECT * FROM test_select";
    QueryResult *result = execute_sql(sql, nullptr);

    ASSERT_NE(result, nullptr) << "execute_sql 返回 NULL";

    // 由于表为空，应返回 0 行
    EXPECT_EQ(result->nrows, 0);
    EXPECT_EQ(result->ncols, 2);  // id, value

    // 不应有错误
    EXPECT_EQ(result->error_msg, nullptr) << "SQL 执行错误: " << result->error_msg;

    FreeQueryResult(result);
}

/**
 * @brief 测试 2: 常量查询
 *
 * 测试场景：SELECT 1, 2, 3
 */
TEST_F(E2EQueryTest, SelectConstant) {
    // 执行常量查询
    const char *sql = "SELECT 1, 2, 3";
    QueryResult *result = execute_sql(sql, nullptr);

    ASSERT_NE(result, nullptr) << "execute_sql 返回 NULL";

    // 应返回 1 行 3 列
    EXPECT_EQ(result->nrows, 1);
    EXPECT_EQ(result->ncols, 3);

    // 不应有错误
    EXPECT_EQ(result->error_msg, nullptr) << "SQL 执行错误: " << result->error_msg;

    FreeQueryResult(result);
}

/**
 * @brief 测试 3: INSERT + SELECT 流程
 *
 * 测试场景：
 * 1. INSERT 插入数据
 * 2. SELECT 查询数据
 */
TEST_F(E2EQueryTest, InsertAndSelect) {
    // 创建测试表
    Oid table_oid = create_test_table("test_insert_select");
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    // Step 1: INSERT 数据
    const char *insert_sql = "INSERT INTO test_insert_select VALUES (1, 100)";
    int rows_affected = execute_dml(insert_sql, nullptr);
    EXPECT_EQ(rows_affected, 1) << "INSERT 应影响 1 行";

    // Step 2: SELECT 查询
    const char *select_sql = "SELECT * FROM test_insert_select";
    QueryResult *result = execute_sql(select_sql, nullptr);

    ASSERT_NE(result, nullptr) << "execute_sql 返回 NULL";

    // 应返回 1 行
    EXPECT_EQ(result->nrows, 1);
    EXPECT_EQ(result->ncols, 2);

    // 不应有错误
    EXPECT_EQ(result->error_msg, nullptr) << "SQL 执行错误: " << result->error_msg;

    FreeQueryResult(result);
}

/**
 * @brief 测试 4: SELECT with WHERE 条件
 *
 * 测试场景：SELECT * FROM table WHERE id = 1
 */
TEST_F(E2EQueryTest, SelectWithWhere) {
    // 创建测试表并插入数据
    Oid table_oid = create_test_table("test_where");
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    // 插入 3 条数据
    execute_dml("INSERT INTO test_where VALUES (1, 100)", nullptr);
    execute_dml("INSERT INTO test_where VALUES (2, 200)", nullptr);
    execute_dml("INSERT INTO test_where VALUES (3, 300)", nullptr);

    // 执行带 WHERE 条件的查询
    const char *sql = "SELECT * FROM test_where WHERE id = 2";
    QueryResult *result = execute_sql(sql, nullptr);

    ASSERT_NE(result, nullptr) << "execute_sql 返回 NULL";

    // 应返回 1 行
    EXPECT_EQ(result->nrows, 1);

    // 不应有错误
    EXPECT_EQ(result->error_msg, nullptr) << "SQL 执行错误: " << result->error_msg;

    FreeQueryResult(result);
}

/**
 * @brief 测试 5: SELECT with 聚合函数
 *
 * 测试场景：SELECT COUNT(*), SUM(value), AVG(value) FROM table
 */
TEST_F(E2EQueryTest, SelectWithAggregate) {
    // 创建测试表并插入数据
    Oid table_oid = create_test_table("test_agg");
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

    // 插入数据
    execute_dml("INSERT INTO test_agg VALUES (1, 100)", nullptr);
    execute_dml("INSERT INTO test_agg VALUES (2, 200)", nullptr);
    execute_dml("INSERT INTO test_agg VALUES (3, 300)", nullptr);

    // 执行聚合查询
    const char *sql = "SELECT COUNT(*), SUM(value), AVG(value) FROM test_agg";
    QueryResult *result = execute_sql(sql, nullptr);

    ASSERT_NE(result, nullptr) << "execute_sql 返回 NULL";

    // 应返回 1 行
    EXPECT_EQ(result->nrows, 1);
    EXPECT_EQ(result->ncols, 3);  // COUNT, SUM, AVG

    // 不应有错误
    EXPECT_EQ(result->error_msg, nullptr) << "SQL 执行错误: " << result->error_msg;

    FreeQueryResult(result);
}

/**
 * @brief 测试 6: 连接查询
 *
 * 测试场景：SELECT * FROM t1 JOIN t2 ON t1.id = t2.id
 */
TEST_F(E2EQueryTest, JoinQuery) {
    // 创建两个测试表
    Oid table1_oid = create_test_table("test_join_t1");
    ASSERT_NE(table1_oid, InvalidOid) << "表 t1 创建失败";

    Oid table2_oid = create_test_table("test_join_t2");
    ASSERT_NE(table2_oid, InvalidOid) << "表 t2 创建失败";

    // 插入数据
    execute_dml("INSERT INTO test_join_t1 VALUES (1, 100)", nullptr);
    execute_dml("INSERT INTO test_join_t1 VALUES (2, 200)", nullptr);

    execute_dml("INSERT INTO test_join_t2 VALUES (1, 1000)", nullptr);
    execute_dml("INSERT INTO test_join_t2 VALUES (2, 2000)", nullptr);

    // 执行连接查询
    const char *sql = "SELECT * FROM test_join_t1 t1 JOIN test_join_t2 t2 ON t1.id = t2.id";
    QueryResult *result = execute_sql(sql, nullptr);

    ASSERT_NE(result, nullptr) << "execute_sql 返回 NULL";

    // 应返回 2 行（id=1 和 id=2 匹配）
    EXPECT_EQ(result->nrows, 2);

    // 不应有错误
    EXPECT_EQ(result->error_msg, nullptr) << "SQL 执行错误: " << result->error_msg;

    FreeQueryResult(result);
}

}  /* namespace */
