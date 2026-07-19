/**
 * @file ddl_test.cpp
 * @brief DDL 执行器测试
 */
#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/sql_parser.h"
#include "db/sql/sql_ddl.h"
#include "db/catalog.h"
}

class DDLTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_init();
    }

    void TearDown() override {
        catalog_shutdown();
    }

    /* 创建一个测试表 */
    Oid create_test_table() {
        column_def_t cols[2];
        memset(cols, 0, sizeof(cols));
        strncpy(cols[0].name, "id", NAMEDATALEN - 1);
        cols[0].type_oid = 1; /* INT */
        strncpy(cols[1].name, "name", NAMEDATALEN - 1);
        cols[1].type_oid = 3; /* VARCHAR */
        return catalog_create_table("test_table", cols, 2);
    }
};

TEST_F(DDLTest, ParseAlterAddColumn) {
    const char *sql = "ALTER TABLE test_table ADD COLUMN email VARCHAR";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    AlterTableStmt *stmt = (AlterTableStmt *)result->stmt;
    ASSERT_EQ(stmt->type, AST_AlterTableStmt);
    ASSERT_STREQ(stmt->relation, "test_table");
    ASSERT_EQ(stmt->num_cmds, 1);
    ASSERT_EQ(stmt->cmds[0]->subtype, AT_AddColumn);
    ASSERT_STREQ(stmt->cmds[0]->name, "email");
    ASSERT_STREQ(stmt->cmds[0]->type_name, "VARCHAR");

    free(result);
}

TEST_F(DDLTest, ParseAlterDropColumn) {
    const char *sql = "ALTER TABLE test_table DROP COLUMN email";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    AlterTableStmt *stmt = (AlterTableStmt *)result->stmt;
    ASSERT_EQ(stmt->cmds[0]->subtype, AT_DropColumn);
    ASSERT_STREQ(stmt->cmds[0]->name, "email");

    free(result);
}

TEST_F(DDLTest, ParseAlterColumnType) {
    const char *sql = "ALTER TABLE test_table ALTER COLUMN name TYPE VARCHAR";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    AlterTableStmt *stmt = (AlterTableStmt *)result->stmt;
    ASSERT_EQ(stmt->cmds[0]->subtype, AT_AlterColumnType);
    ASSERT_STREQ(stmt->cmds[0]->name, "name");
    ASSERT_STREQ(stmt->cmds[0]->type_name, "VARCHAR");

    free(result);
}

TEST_F(DDLTest, ParseAlterRenameColumn) {
    const char *sql = "ALTER TABLE test_table RENAME COLUMN name TO full_name";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    AlterTableStmt *stmt = (AlterTableStmt *)result->stmt;
    ASSERT_EQ(stmt->cmds[0]->subtype, AT_RenameColumn);
    ASSERT_STREQ(stmt->cmds[0]->name, "name");
    ASSERT_STREQ(stmt->cmds[0]->new_name, "full_name");

    free(result);
}

TEST_F(DDLTest, ParseAlterBatchColumns) {
    const char *sql = "ALTER TABLE test_table ADD COLUMN email VARCHAR, DROP COLUMN phone";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    AlterTableStmt *stmt = (AlterTableStmt *)result->stmt;
    ASSERT_EQ(stmt->num_cmds, 2);
    ASSERT_EQ(stmt->cmds[0]->subtype, AT_AddColumn);
    ASSERT_STREQ(stmt->cmds[0]->name, "email");
    ASSERT_EQ(stmt->cmds[1]->subtype, AT_DropColumn);
    ASSERT_STREQ(stmt->cmds[1]->name, "phone");

    free(result);
}

TEST_F(DDLTest, ExecuteAlterAddColumn) {
    Oid oid = create_test_table();
    ASSERT_NE(oid, InvalidOid);

    /* 解析并执行 */
    const char *sql = "ALTER TABLE test_table ADD COLUMN email VARCHAR";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    ASSERT_EQ(exec_alter_table((AlterTableStmt *)result->stmt, NULL), 0);

    /* 验证列已添加 */
    int ncols = 0;
    column_info_t *cols = catalog_get_columns(oid, &ncols);
    ASSERT_GE(ncols, 3);
    bool found = false;
    for (int i = 0; i < ncols; i++) {
        if (strcmp(cols[i].name, "email") == 0 && !cols[i].is_dropped) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
    catalog_free_columns(cols);

    free(result);
}

TEST_F(DDLTest, ExecuteAlterDropColumn) {
    Oid oid = create_test_table();
    ASSERT_NE(oid, InvalidOid);

    const char *sql = "ALTER TABLE test_table DROP COLUMN name";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    ASSERT_EQ(exec_alter_table((AlterTableStmt *)result->stmt, NULL), 0);

    /* 验证列被标记为已删除 */
    int ncols = 0;
    column_info_t *cols = catalog_get_columns(oid, &ncols);
    bool found_dropped = false;
    for (int i = 0; i < ncols; i++) {
        if (strcmp(cols[i].name, "name") == 0) {
            found_dropped = cols[i].is_dropped;
            break;
        }
    }
    ASSERT_TRUE(found_dropped);
    catalog_free_columns(cols);

    free(result);
}

TEST_F(DDLTest, ExecuteAlterNonExistentTable) {
    const char *sql = "ALTER TABLE nonexistent ADD COLUMN x INT";
    SqlParseResult *result = sql_parse_one(sql, (int)strlen(sql));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->success);

    ASSERT_EQ(exec_alter_table((AlterTableStmt *)result->stmt, NULL), -1);

    free(result);
}