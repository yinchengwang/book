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

}  // namespace
