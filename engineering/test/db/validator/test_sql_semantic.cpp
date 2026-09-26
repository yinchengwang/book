/**
 * @file test_sql_semantic.cpp
 * @brief SQL 语义分析器单元测试（T5：catalog API 解耦）
 *
 * 验证语义层不再依赖 kv_t，表/列存在性校验走 catalog：
 * - 不存在的表必须被拒绝（关键用例 UnknownTableRejected）
 * - catalog 中存在的表必须通过
 * - 不存在的列必须被拒绝
 * - CREATE/DROP TABLE 的存在性前置校验
 */

#include "gtest/gtest.h"
#include "db/catalog.h"
#include "db/validator/sql_semantic.h"

#include <cstdlib>
#include <cstring>

/* ============================================================
 * 最小 AST 手工构造工具
 *
 * T6 的 canonical parser 落地前，直接按 sql_node_t 结构体布局
 * 手工构造 SELECT/CREATE/DROP 的最小 AST。
 * ============================================================ */

static char *dup_str(const char *s) {
    size_t len = std::strlen(s) + 1;
    char *p = (char *)std::malloc(len);
    std::memcpy(p, s, len);
    return p;
}

static sql_node_t *make_column_ref(const char *name) {
    sql_node_t *node = (sql_node_t *)std::calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_COLUMN_REF;
    node->u.column_ref.name = dup_str(name);
    return node;
}

static sql_node_t *make_expr_list(sql_node_t **items, size_t count) {
    sql_node_t *node = (sql_node_t *)std::calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_EXPR_LIST;
    node->u.list.items = items;
    node->u.list.count = count;
    node->u.list.capacity = count;
    return node;
}

/* SELECT col FROM table_name（col 传 "*" 即 SELECT *） */
static sql_node_t *make_select(const char *table_name, const char *col) {
    sql_node_t **items = (sql_node_t **)std::malloc(sizeof(sql_node_t *));
    items[0] = make_column_ref(col);

    sql_node_t *node = (sql_node_t *)std::calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_SELECT;
    node->u.select.table_name = dup_str(table_name);
    node->u.select.columns = make_expr_list(items, 1);
    node->u.select.where_cond = NULL;
    return node;
}

static sql_node_t *make_create_table(const char *table_name) {
    sql_node_t *node = (sql_node_t *)std::calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_CREATE_TABLE;
    node->u.create_table.table_name = dup_str(table_name);
    node->u.create_table.columns = NULL;
    return node;
}

static sql_node_t *make_drop_table(const char *table_name) {
    sql_node_t *node = (sql_node_t *)std::calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_DROP_TABLE;
    node->u.drop_table.table_name = dup_str(table_name);
    return node;
}

/* 释放手工构造的 AST（仅覆盖上述 helper 产生的形状） */
static void free_ast(sql_node_t *node) {
    if (!node) return;
    switch (node->type) {
    case SQL_NODE_SELECT:
        std::free(node->u.select.table_name);
        free_ast(node->u.select.columns);
        break;
    case SQL_NODE_CREATE_TABLE:
        std::free(node->u.create_table.table_name);
        break;
    case SQL_NODE_DROP_TABLE:
        std::free(node->u.drop_table.table_name);
        break;
    case SQL_NODE_EXPR_LIST:
        for (size_t i = 0; i < node->u.list.count; i++) {
            free_ast(node->u.list.items[i]);
        }
        std::free(node->u.list.items);
        break;
    case SQL_NODE_COLUMN_REF:
        std::free(node->u.column_ref.name);
        break;
    default:
        break;
    }
    std::free(node);
}

/* ============================================================
 * 测试夹具：每个用例独立的空 catalog
 * ============================================================ */

class SqlSemanticTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, catalog_init());
        sem_ = sql_semantic_create();
        ASSERT_NE(nullptr, sem_);
    }

    void TearDown() override {
        sql_semantic_destroy(sem_);
        catalog_shutdown();
    }

    /* 在 catalog 中注册一张两列的表 */
    Oid CreateTable(const char *name, const char *col1, const char *col2) {
        column_def_t cols[2];
        std::memset(cols, 0, sizeof(cols));
        std::strcpy(cols[0].name, col1);
        cols[0].type_oid = 23;  /* int4 */
        std::strcpy(cols[1].name, col2);
        cols[1].type_oid = 25;  /* text */
        return catalog_create_table(name, cols, 2);
    }

    sql_semantic_t *sem_ = nullptr;
};

/* ============================================================
 * 关键用例：不存在的表必须被语义层拒绝
 * ============================================================ */

TEST_F(SqlSemanticTest, UnknownTableRejected) {
    /* "no_such_table" 不在 catalog → 校验必须报错而非通过 */
    sql_node_t *ast = make_select("no_such_table", "*");
    int rc = sql_semantic_analyze_select(sem_, ast, NULL);
    EXPECT_NE(0, rc) << "不存在的表必须被语义层拒绝";
    EXPECT_NE('\0', sql_semantic_errmsg(sem_)[0]);
    free_ast(ast);
}

/* ============================================================
 * catalog 中存在的表必须通过
 * ============================================================ */

TEST_F(SqlSemanticTest, KnownTableAccepted) {
    ASSERT_NE(InvalidOid, CreateTable("users", "id", "name"));

    sql_node_t *ast = make_select("users", "*");
    const table_info_t *table = NULL;
    int rc = sql_semantic_analyze_select(sem_, ast, &table);
    EXPECT_EQ(0, rc) << sql_semantic_errmsg(sem_);
    ASSERT_NE(nullptr, table);
    EXPECT_STREQ("users", table->name);
    free_ast(ast);
}

TEST_F(SqlSemanticTest, KnownColumnAccepted) {
    ASSERT_NE(InvalidOid, CreateTable("users", "id", "name"));

    sql_node_t *ast = make_select("users", "name");
    int rc = sql_semantic_analyze_select(sem_, ast, NULL);
    EXPECT_EQ(0, rc) << sql_semantic_errmsg(sem_);
    free_ast(ast);
}

/* ============================================================
 * 不存在的列必须被拒绝
 * ============================================================ */

TEST_F(SqlSemanticTest, UnknownColumnRejected) {
    ASSERT_NE(InvalidOid, CreateTable("users", "id", "name"));

    sql_node_t *ast = make_select("users", "no_such_column");
    int rc = sql_semantic_analyze_select(sem_, ast, NULL);
    EXPECT_NE(0, rc) << "不存在的列必须被语义层拒绝";
    free_ast(ast);
}

/* ============================================================
 * CREATE / DROP TABLE 前置存在性校验
 * ============================================================ */

TEST_F(SqlSemanticTest, CreateDuplicateTableRejected) {
    ASSERT_NE(InvalidOid, CreateTable("users", "id", "name"));

    sql_node_t *ast = make_create_table("users");
    int rc = sql_semantic_analyze_create_table(sem_, ast);
    EXPECT_NE(0, rc) << "重复建表必须被语义层拒绝";
    free_ast(ast);
}

TEST_F(SqlSemanticTest, CreateNewTableAccepted) {
    sql_node_t *ast = make_create_table("brand_new_table");
    int rc = sql_semantic_analyze_create_table(sem_, ast);
    EXPECT_EQ(0, rc) << sql_semantic_errmsg(sem_);
    free_ast(ast);
}

TEST_F(SqlSemanticTest, DropUnknownTableRejected) {
    sql_node_t *ast = make_drop_table("no_such_table");
    int rc = sql_semantic_analyze_drop_table(sem_, ast);
    EXPECT_NE(0, rc) << "DROP 不存在的表必须被语义层拒绝";
    free_ast(ast);
}

TEST_F(SqlSemanticTest, DropKnownTableAccepted) {
    ASSERT_NE(InvalidOid, CreateTable("users", "id", "name"));

    sql_node_t *ast = make_drop_table("users");
    int rc = sql_semantic_analyze_drop_table(sem_, ast);
    EXPECT_EQ(0, rc) << sql_semantic_errmsg(sem_);
    free_ast(ast);
}
