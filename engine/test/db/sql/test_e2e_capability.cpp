/**
 * @file test_e2e_capability.cpp
 * @brief 主路径算子 e2e 能力测试（T9，SQL 栈全收敛 WS-D）
 *
 * 逐能力兑现 spec §6.2-3：
 *   1. ORDER BY + LIMIT（Sort→SeqScan 多节点计划树）
 *   2. LIMIT + OFFSET
 *   3. GROUP BY + 聚合（HashAgg→SeqScan）
 *   4. INNER JOIN（HashJoin→(SeqScan,SeqScan)，等值连接）
 *   5. FROM 子查询（SubqueryScan 折叠：Agg→SeqScan 作为根）
 *
 * bootstrap 序列同 DriverSmoke（catalog/buf/heapam/btree/rel）。
 * 每个测试自带建表与数据，互不依赖执行顺序。
 */

#include <gtest/gtest.h>
#include <string.h>

extern "C" {
#include "db/sql/sql_driver.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"
}

namespace {

/* 按列名定位结果集列下标（catalog 列序不保证与声明一致） */
static int col_index_by_name(const QueryResult *r, const char *name) {
    for (int j = 0; j < r->ncols; j++) {
        if (r->col_names[j] != nullptr && strcmp(r->col_names[j], name) == 0) {
            return j;
        }
    }
    return -1;
}

class E2ECapability : public ::testing::Test {
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

/* 执行 SQL 并断言无错误；返回 QueryResult（调用方负责 FreeQueryResult） */
static QueryResult *run(const char *sql) {
    QueryResult *r = execute_sql(sql, nullptr);
    EXPECT_NE(r, nullptr);
    EXPECT_EQ(r->error_msg, nullptr) << (r && r->error_msg ? r->error_msg : "");
    return r;
}

TEST_F(E2ECapability, OrderByLimit) {
    run("CREATE TABLE t_ord (id INT, v INT)");
    run("INSERT INTO t_ord VALUES (1, 30)");
    run("INSERT INTO t_ord VALUES (2, 10)");
    run("INSERT INTO t_ord VALUES (3, 20)");

    QueryResult *r = run("SELECT id FROM t_ord ORDER BY v DESC LIMIT 2");
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    ASSERT_EQ(r->nrows, 2);          /* v=30,20 → id=1,3 */
    ASSERT_EQ(r->ncols, 1);
    EXPECT_STREQ(r->rows[0][0], "1");
    EXPECT_STREQ(r->rows[1][0], "3");
    FreeQueryResult(r);
}

TEST_F(E2ECapability, LimitOffset) {
    run("CREATE TABLE t_off (id INT, v INT)");
    run("INSERT INTO t_off VALUES (1, 30)");
    run("INSERT INTO t_off VALUES (2, 10)");
    run("INSERT INTO t_off VALUES (3, 20)");

    QueryResult *r = run("SELECT id FROM t_off ORDER BY v LIMIT 1 OFFSET 1");
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    ASSERT_EQ(r->nrows, 1);          /* v 升序第 2 个 → id=3 */
    EXPECT_STREQ(r->rows[0][0], "3");
    FreeQueryResult(r);
}

TEST_F(E2ECapability, GroupByAggregate) {
    run("CREATE TABLE emp (dept INT, salary INT)");
    run("INSERT INTO emp VALUES (1, 100)");
    run("INSERT INTO emp VALUES (1, 200)");
    run("INSERT INTO emp VALUES (2, 50)");

    QueryResult *r = run(
        "SELECT dept, COUNT(*), SUM(salary) FROM emp GROUP BY dept ORDER BY dept");
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    ASSERT_EQ(r->nrows, 2);
    int idx_dept = col_index_by_name(r, "dept");
    int idx_count = col_index_by_name(r, "count");
    int idx_sum = col_index_by_name(r, "sum");
    ASSERT_GE(idx_dept, 0);
    ASSERT_GE(idx_count, 0);
    ASSERT_GE(idx_sum, 0);
    /* dept=1：2 行，salary 合计 300 */
    EXPECT_STREQ(r->rows[0][idx_dept], "1");
    EXPECT_STREQ(r->rows[0][idx_count], "2");
    EXPECT_STREQ(r->rows[0][idx_sum], "300");
    /* dept=2：1 行，salary 合计 50 */
    EXPECT_STREQ(r->rows[1][idx_dept], "2");
    EXPECT_STREQ(r->rows[1][idx_count], "1");
    EXPECT_STREQ(r->rows[1][idx_sum], "50");
    FreeQueryResult(r);
}

TEST_F(E2ECapability, InnerJoin) {
    run("CREATE TABLE j_a (id INT, x INT)");
    run("CREATE TABLE j_b (id INT, y INT)");
    run("INSERT INTO j_a VALUES (1, 10)");
    run("INSERT INTO j_b VALUES (1, 20)");
    run("INSERT INTO j_b VALUES (2, 30)");

    QueryResult *r = run("SELECT j_a.id, j_b.y FROM j_a JOIN j_b ON j_a.id = j_b.id");
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    ASSERT_EQ(r->nrows, 1);
    int idx_id = col_index_by_name(r, "id");
    int idx_y = col_index_by_name(r, "y");
    ASSERT_GE(idx_id, 0);
    ASSERT_GE(idx_y, 0);
    EXPECT_STREQ(r->rows[0][idx_id], "1");
    EXPECT_STREQ(r->rows[0][idx_y], "20");
    FreeQueryResult(r);
}

TEST_F(E2ECapability, SubqueryInFrom) {
    run("CREATE TABLE sub_emp (dept INT, salary INT)");
    run("INSERT INTO sub_emp VALUES (1, 100)");
    run("INSERT INTO sub_emp VALUES (1, 200)");
    run("INSERT INTO sub_emp VALUES (2, 50)");

    QueryResult *r = run("SELECT s.c FROM (SELECT COUNT(*) AS c FROM sub_emp) s");
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    ASSERT_EQ(r->nrows, 1);
    ASSERT_EQ(r->ncols, 1);
    EXPECT_STREQ(r->rows[0][0], "3");
    FreeQueryResult(r);
}

}  // namespace
