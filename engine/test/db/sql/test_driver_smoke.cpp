/**
 * @file test_driver_smoke.cpp
 * @brief sql_driver 端到端烟测（T7，SQL 栈全收敛）
 *
 * 验证 execute_sql() 在 canonical Bison AST 上的端到端路径：
 * 1. CREATE TABLE 走 DDL 直通道（语义检查 + catalog_create_table + 存储对象），
 *    建表后 catalog_lookup_table 必须能查到；
 * 2. DROP TABLE 直通道；
 * 3. 解析失败必须经 error_msg 返回真实错误（非静默桩）。
 *
 * bootstrap 序列复制自 test_sql_storage_integration.cpp。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/sql_driver.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"
}

namespace {

/* 按列名在结果集中找列下标（catalog 的 SELECT * 列序不保证与
 * 建表声明一致——见 task-8 报告对 catalog_get_columns 的说明），
 * 断言真实值时一律按列名定位。 */
static int col_index_by_name(const QueryResult *r, const char *name) {
    for (int j = 0; j < r->ncols; j++) {
        if (r->col_names[j] != nullptr && strcmp(r->col_names[j], name) == 0) {
            return j;
        }
    }
    return -1;
}

class DriverSmoke : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(""), 0) << "Buffer Pool 初始化失败";  /* 空路径=内存池 */
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(btreeam_init(), 0) << "BTree AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    static void TearDownTestSuite() {
        rel_shutdown();
        btreeam_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }
};

TEST_F(DriverSmoke, CreateTableDdl) {
    QueryResult *r = execute_sql("CREATE TABLE smoke_t (id INT, name TEXT)", nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    EXPECT_NE(catalog_lookup_table("smoke_t"), InvalidOid)
        << "DDL 后 catalog 必须能查到表";
    FreeQueryResult(r);
}

TEST_F(DriverSmoke, DropTableDdl) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_drop_t (id INT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    ASSERT_NE(catalog_lookup_table("smoke_drop_t"), InvalidOid);
    FreeQueryResult(r1);

    QueryResult *r2 = execute_sql("DROP TABLE smoke_drop_t", nullptr);
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r2->error_msg, nullptr) << (r2->error_msg ? r2->error_msg : "");
    EXPECT_EQ(catalog_lookup_table("smoke_drop_t"), InvalidOid)
        << "DROP 后 catalog 不应再查到表";
    FreeQueryResult(r2);
}

TEST_F(DriverSmoke, DuplicateTableRejected) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_dup_t (id INT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    /* 重复建表必须报真实错误，不得静默成功 */
    QueryResult *r2 = execute_sql("CREATE TABLE smoke_dup_t (id INT)", nullptr);
    ASSERT_NE(r2, nullptr);
    EXPECT_NE(r2->error_msg, nullptr) << "重复建表应返回错误";
    FreeQueryResult(r2);
}

TEST_F(DriverSmoke, ParseErrorReported) {
    QueryResult *r = execute_sql("CREATE TABLEX garbage (", nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_NE(r->error_msg, nullptr) << "解析失败必须经 error_msg 返回真实错误";
    FreeQueryResult(r);
}

TEST_F(DriverSmoke, UnknownTypeRejected) {
    /* 未知列类型必须报真实错误，且不得留下 catalog 垃圾 */
    QueryResult *r = execute_sql("CREATE TABLE smoke_bad_t (id FROBNICATE)", nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_NE(r->error_msg, nullptr) << "未知类型应返回错误";
    EXPECT_EQ(catalog_lookup_table("smoke_bad_t"), InvalidOid);
    FreeQueryResult(r);
}

/* ====================================================================
 * T8：SELECT 链路（执行器四阶段链 + 结果物化）与 DML 直通道
 * ==================================================================*/

TEST_F(DriverSmoke, InsertThenSelect) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_is_t (id INT, name TEXT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    QueryResult *r2 = execute_sql(
        "INSERT INTO smoke_is_t VALUES (1, 'alice')", nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r2->error_msg, nullptr) << r2->error_msg;
    EXPECT_EQ(r2->nrows, 1) << "INSERT 必须上报受影响行数 1";
    FreeQueryResult(r2);

    QueryResult *r3 = execute_sql("SELECT * FROM smoke_is_t", nullptr);
    ASSERT_NE(r3, nullptr);
    ASSERT_EQ(r3->error_msg, nullptr) << (r3->error_msg ? r3->error_msg : "");
    ASSERT_EQ(r3->nrows, 1) << "SELECT 必须物化出 1 行";
    ASSERT_EQ(r3->ncols, 2);
    ASSERT_NE(r3->col_names, nullptr);
    int idx_id = col_index_by_name(r3, "id");
    int idx_name = col_index_by_name(r3, "name");
    ASSERT_GE(idx_id, 0) << "结果集必须包含 id 列";
    ASSERT_GE(idx_name, 0) << "结果集必须包含 name 列";
    ASSERT_NE(r3->rows, nullptr);
    ASSERT_NE(r3->rows[0], nullptr);
    EXPECT_STREQ(r3->rows[0][idx_id], "1") << "INT 列必须按真实值物化";
    EXPECT_STREQ(r3->rows[0][idx_name], "alice") << "TEXT 列必须按真实值物化";
    FreeQueryResult(r3);
}

TEST_F(DriverSmoke, SelectMultipleRowsAndColumnOrder) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_mr_t (id INT, name TEXT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    /* 单语句多行 VALUES */
    QueryResult *r2 = execute_sql(
        "INSERT INTO smoke_mr_t VALUES (1, 'a'), (2, 'b'), (3, 'c')", nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r2->error_msg, nullptr) << r2->error_msg;
    EXPECT_EQ(r2->nrows, 3);
    FreeQueryResult(r2);

    QueryResult *r3 = execute_sql("SELECT * FROM smoke_mr_t", nullptr);
    ASSERT_NE(r3, nullptr);
    ASSERT_EQ(r3->error_msg, nullptr) << (r3->error_msg ? r3->error_msg : "");
    ASSERT_EQ(r3->nrows, 3);
    ASSERT_EQ(r3->ncols, 2);
    int idx_id = col_index_by_name(r3, "id");
    int idx_name = col_index_by_name(r3, "name");
    ASSERT_GE(idx_id, 0);
    ASSERT_GE(idx_name, 0);
    /* 堆扫描按插入顺序返回（单页内 LinePointer 顺序） */
    EXPECT_STREQ(r3->rows[0][idx_id], "1");
    EXPECT_STREQ(r3->rows[0][idx_name], "a");
    EXPECT_STREQ(r3->rows[1][idx_id], "2");
    EXPECT_STREQ(r3->rows[1][idx_name], "b");
    EXPECT_STREQ(r3->rows[2][idx_id], "3");
    EXPECT_STREQ(r3->rows[2][idx_name], "c");
    FreeQueryResult(r3);

    /* 显式列列表：投影顺序按目标列，而非表定义顺序 */
    QueryResult *r4 = execute_sql("SELECT name, id FROM smoke_mr_t", nullptr);
    ASSERT_NE(r4, nullptr);
    ASSERT_EQ(r4->error_msg, nullptr) << (r4->error_msg ? r4->error_msg : "");
    ASSERT_EQ(r4->nrows, 3);
    ASSERT_EQ(r4->ncols, 2);
    EXPECT_STREQ(r4->col_names[0], "name");
    EXPECT_STREQ(r4->col_names[1], "id");
    EXPECT_STREQ(r4->rows[0][0], "a");
    EXPECT_STREQ(r4->rows[0][1], "1");
    FreeQueryResult(r4);
}

TEST_F(DriverSmoke, InsertWithColumnList) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_cl_t (id INT, name TEXT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    /* 列名列表顺序与表定义不同：值必须按列名映射 */
    QueryResult *r2 = execute_sql(
        "INSERT INTO smoke_cl_t (name, id) VALUES ('x', 7)", nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r2->error_msg, nullptr) << r2->error_msg;
    EXPECT_EQ(r2->nrows, 1);
    FreeQueryResult(r2);

    QueryResult *r3 = execute_sql("SELECT id, name FROM smoke_cl_t", nullptr);
    ASSERT_NE(r3, nullptr);
    ASSERT_EQ(r3->error_msg, nullptr) << (r3->error_msg ? r3->error_msg : "");
    ASSERT_EQ(r3->nrows, 1);
    EXPECT_STREQ(r3->rows[0][0], "7");
    EXPECT_STREQ(r3->rows[0][1], "x");
    FreeQueryResult(r3);
}

TEST_F(DriverSmoke, UpdateThenSelect) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_upd_t (id INT, name TEXT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    QueryResult *r2 = execute_sql(
        "INSERT INTO smoke_upd_t VALUES (1, 'a'), (2, 'b')", nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r2->error_msg, nullptr) << r2->error_msg;
    FreeQueryResult(r2);

    QueryResult *r3 = execute_sql(
        "UPDATE smoke_upd_t SET name = 'z'", nullptr);
    ASSERT_NE(r3, nullptr);
    ASSERT_EQ(r3->error_msg, nullptr) << r3->error_msg;
    EXPECT_EQ(r3->nrows, 2) << "UPDATE 必须上报受影响行数 2";
    FreeQueryResult(r3);

    QueryResult *r4 = execute_sql("SELECT id, name FROM smoke_upd_t", nullptr);
    ASSERT_NE(r4, nullptr);
    ASSERT_EQ(r4->error_msg, nullptr) << (r4->error_msg ? r4->error_msg : "");
    ASSERT_EQ(r4->nrows, 2);
    EXPECT_STREQ(r4->rows[0][0], "1");
    EXPECT_STREQ(r4->rows[0][1], "z") << "UPDATE 后的新值必须可见";
    EXPECT_STREQ(r4->rows[1][0], "2");
    EXPECT_STREQ(r4->rows[1][1], "z");
    FreeQueryResult(r4);
}

TEST_F(DriverSmoke, DeleteThenSelect) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_del_t (id INT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    QueryResult *r2 = execute_sql(
        "INSERT INTO smoke_del_t VALUES (1), (2)", nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r2->error_msg, nullptr) << r2->error_msg;
    FreeQueryResult(r2);

    QueryResult *r3 = execute_sql("DELETE FROM smoke_del_t", nullptr);
    ASSERT_NE(r3, nullptr);
    ASSERT_EQ(r3->error_msg, nullptr) << r3->error_msg;
    EXPECT_EQ(r3->nrows, 2) << "DELETE 必须上报受影响行数 2";
    FreeQueryResult(r3);

    QueryResult *r4 = execute_sql("SELECT * FROM smoke_del_t", nullptr);
    ASSERT_NE(r4, nullptr);
    ASSERT_EQ(r4->error_msg, nullptr) << (r4->error_msg ? r4->error_msg : "");
    EXPECT_EQ(r4->nrows, 0) << "DELETE 后表必须为空";
    EXPECT_EQ(r4->ncols, 1) << "0 行时列元数据仍须就位";
    FreeQueryResult(r4);
}

TEST_F(DriverSmoke, UnsupportedClausesExplicitlyRejected) {
    QueryResult *r1 = execute_sql("CREATE TABLE smoke_rej_t (id INT)", nullptr);
    ASSERT_NE(r1, nullptr);
    ASSERT_EQ(r1->error_msg, nullptr) << r1->error_msg;
    FreeQueryResult(r1);

    /* WHERE 未接入：必须显式报错，绝不静默全表返回/全表修改 */
    QueryResult *r2 = execute_sql("SELECT * FROM smoke_rej_t WHERE id = 1", nullptr);
    ASSERT_NE(r2, nullptr);
    EXPECT_NE(r2->error_msg, nullptr) << "SELECT ... WHERE 必须显式拒绝";
    FreeQueryResult(r2);

    QueryResult *r3 = execute_sql("UPDATE smoke_rej_t SET id = 9 WHERE id = 1", nullptr);
    ASSERT_NE(r3, nullptr);
    EXPECT_NE(r3->error_msg, nullptr) << "UPDATE ... WHERE 必须显式拒绝";
    FreeQueryResult(r3);

    QueryResult *r4 = execute_sql("DELETE FROM smoke_rej_t WHERE id = 1", nullptr);
    ASSERT_NE(r4, nullptr);
    EXPECT_NE(r4->error_msg, nullptr) << "DELETE ... WHERE 必须显式拒绝";
    FreeQueryResult(r4);

    /* 不存在的列名也必须显式报错 */
    QueryResult *r5 = execute_sql("SELECT nosuch FROM smoke_rej_t", nullptr);
    ASSERT_NE(r5, nullptr);
    EXPECT_NE(r5->error_msg, nullptr) << "未知列必须显式报错";
    FreeQueryResult(r5);
}

}  // namespace
