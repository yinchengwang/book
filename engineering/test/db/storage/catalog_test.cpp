/**
 * @file catalog_test.cpp
 * @brief Catalog 系统单元测试
 */

#include "gtest/gtest.h"
#include "db/catalog.h"

/* ============================================================
 * 测试夹具
 * ============================================================ */

class CatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, catalog_init());
    }

    void TearDown() override {
        catalog_shutdown();
    }
};

/* ============================================================
 * 基础测试
 * ============================================================ */

TEST_F(CatalogTest, InitAndShutdown) {
    /* 重复初始化应该成功 */
    EXPECT_EQ(0, catalog_init());
}

/* ============================================================
 * 表操作测试
 * ============================================================ */

TEST_F(CatalogTest, CreateTable) {
    column_def_t cols[3];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;  /* int4 */
    cols[0].not_null = true;
    cols[0].has_default = false;

    strcpy(cols[1].name, "name");
    cols[1].type_oid = 25;  /* text */
    cols[1].not_null = false;
    cols[1].has_default = false;

    strcpy(cols[2].name, "age");
    cols[2].type_oid = 23;  /* int4 */
    cols[2].not_null = false;
    cols[2].has_default = true;
    cols[2].default_expr = NULL;

    Oid table_oid = catalog_create_table("users", cols, 3);
    EXPECT_NE(InvalidOid, table_oid);
    EXPECT_GE(table_oid, 1u);
}

TEST_F(CatalogTest, GetTable) {
    column_def_t cols[2];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;
    cols[0].not_null = true;

    strcpy(cols[1].name, "value");
    cols[1].type_oid = 25;
    cols[1].not_null = false;

    Oid table_oid = catalog_create_table("test_table", cols, 2);
    ASSERT_NE(InvalidOid, table_oid);

    table_info_t *info = catalog_get_table(table_oid);
    ASSERT_NE(nullptr, info);
    EXPECT_STREQ("test_table", info->name);
    EXPECT_EQ(2, info->natts);
    EXPECT_EQ(OBJECT_TABLE, info->relkind);
}

TEST_F(CatalogTest, LookupTable) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("lookup_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    Oid found = catalog_lookup_table("lookup_test");
    EXPECT_EQ(table_oid, found);

    Oid not_found = catalog_lookup_table("non_existent");
    EXPECT_EQ(InvalidOid, not_found);
}

TEST_F(CatalogTest, DropTable) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("to_drop", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    EXPECT_EQ(CATALOG_SUCCESS, catalog_drop_table(table_oid));

    table_info_t *info = catalog_get_table(table_oid);
    EXPECT_EQ(nullptr, info);
}

TEST_F(CatalogTest, DuplicateTableName) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid oid1 = catalog_create_table("dup_test", cols, 1);
    ASSERT_NE(InvalidOid, oid1);

    Oid oid2 = catalog_create_table("dup_test", cols, 1);
    EXPECT_EQ(InvalidOid, oid2);
}

TEST_F(CatalogTest, GetAllTables) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    catalog_create_table("t1", cols, 1);
    catalog_create_table("t2", cols, 1);
    catalog_create_table("t3", cols, 1);

    int count = 0;
    table_info_t *tables = catalog_get_all_tables(&count);
    EXPECT_GE(count, 3);
    free(tables);
}

/* ============================================================
 * 列操作测试
 * ============================================================ */

TEST_F(CatalogTest, GetColumns) {
    column_def_t cols[3];
    strcpy(cols[0].name, "col1");
    cols[0].type_oid = 23;
    strcpy(cols[1].name, "col2");
    cols[1].type_oid = 25;
    strcpy(cols[2].name, "col3");
    cols[2].type_oid = 16;  /* bool */

    Oid table_oid = catalog_create_table("columns_test", cols, 3);
    ASSERT_NE(InvalidOid, table_oid);

    int ncols = 0;
    column_info_t *columns = catalog_get_columns(table_oid, &ncols);
    ASSERT_NE(nullptr, columns);
    EXPECT_EQ(3, ncols);
    // 列顺序可能不是插入顺序，只检查名称存在
    bool has_col1 = false, has_col2 = false, has_col3 = false;
    for (int i = 0; i < ncols; i++) {
        if (strcmp(columns[i].name, "col1") == 0) has_col1 = true;
        if (strcmp(columns[i].name, "col2") == 0) has_col2 = true;
        if (strcmp(columns[i].name, "col3") == 0) has_col3 = true;
    }
    EXPECT_TRUE(has_col1);
    EXPECT_TRUE(has_col2);
    EXPECT_TRUE(has_col3);

    free(columns);
}

TEST_F(CatalogTest, AddColumn) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("add_col_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    column_def_t new_col;
    strcpy(new_col.name, "new_col");
    new_col.type_oid = 25;
    new_col.not_null = false;

    EXPECT_EQ(CATALOG_SUCCESS, catalog_add_column(table_oid, &new_col));

    int ncols = 0;
    column_info_t *columns = catalog_get_columns(table_oid, &ncols);
    EXPECT_EQ(2, ncols);
    free(columns);
}

TEST_F(CatalogTest, DropColumn) {
    column_def_t cols[2];
    strcpy(cols[0].name, "col1");
    cols[0].type_oid = 23;
    strcpy(cols[1].name, "col2");
    cols[1].type_oid = 25;

    Oid table_oid = catalog_create_table("drop_col_test", cols, 2);
    ASSERT_NE(InvalidOid, table_oid);

    EXPECT_EQ(CATALOG_SUCCESS, catalog_drop_column(table_oid, "col2"));

    int ncols = 0;
    column_info_t *columns = catalog_get_columns(table_oid, &ncols);
    EXPECT_EQ(1, ncols);
    EXPECT_STREQ("col1", columns[0].name);
    free(columns);
}

/* ============================================================
 * 索引操作测试
 * ============================================================ */

TEST_F(CatalogTest, CreateIndex) {
    column_def_t cols[2];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;
    strcpy(cols[1].name, "name");
    cols[1].type_oid = 25;

    Oid table_oid = catalog_create_table("index_test", cols, 2);
    ASSERT_NE(InvalidOid, table_oid);

    const char *idx_cols[] = {"id"};
    Oid index_oid = catalog_create_index("idx_id", table_oid, idx_cols, 1, true);
    EXPECT_NE(InvalidOid, index_oid);
}

TEST_F(CatalogTest, GetIndexes) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("idx_multi_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    const char *col1[] = {"id"};
    catalog_create_index("idx1", table_oid, col1, 1, true);
    catalog_create_index("idx2", table_oid, col1, 1, false);

    int nindexes = 0;
    index_info_t *indexes = catalog_get_indexes(table_oid, &nindexes);
    EXPECT_EQ(2, nindexes);

    for (int i = 0; i < nindexes; i++) {
        free(indexes[i].key_nums);
    }
    free(indexes);
}

TEST_F(CatalogTest, DropIndex) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("drop_idx_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    const char *cols_idx[] = {"id"};
    Oid index_oid = catalog_create_index("to_drop_idx", table_oid, cols_idx, 1, false);
    ASSERT_NE(InvalidOid, index_oid);

    EXPECT_EQ(CATALOG_SUCCESS, catalog_drop_index(index_oid));

    index_info_t *info = catalog_get_index(index_oid);
    EXPECT_EQ(nullptr, info);
}

/* ============================================================
 * 缓存测试
 * ============================================================ */

TEST_F(CatalogTest, CacheHitMiss) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("cache_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    // 先获取统计
    uint64_t hits_before = 0, misses_before = 0;
    catalog_get_cache_stats(&hits_before, &misses_before);

    // 多次访问同一表
    catalog_get_table(table_oid);
    catalog_get_table(table_oid);
    catalog_get_table(table_oid);

    uint64_t hits = 0, misses = 0;
    catalog_get_cache_stats(&hits, &misses);

    // 至少有一些命中
    EXPECT_GE(hits, hits_before + 2u);
}

TEST_F(CatalogTest, InvalidateTable) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("invalidate_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    /* 验证可以获取 */
    ASSERT_NE(nullptr, catalog_get_table(table_oid));

    /* 使缓存失效 */
    catalog_invalidate_table(table_oid);

    /* 再次获取应该返回 NULL（需要重新加载） */
    EXPECT_EQ(nullptr, catalog_get_table(table_oid));
}
