/**
 * @file cf_engine_test.cpp
 * @brief 列族存储引擎单元测试
 *
 * 测试覆盖：
 * - 列族创建/删除/列表
 * - 单列 CRUD
 * - 行级 CRUD
 * - 批量操作
 * - 扫描
 * - TTL/序列化
 * - 边界条件
 */

#include <gtest/gtest.h>
#include "db/cf/cf_engine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

/* ============================================================
 * 测试辅助
 * ============================================================ */

/** 测试数据库路径 */
static const char *kTestDbPath = "test_cf_engine.db";

/** 清理数据库文件（含 .wal） */
static void cleanup_db() {
    remove(kTestDbPath);
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", kTestDbPath);
    remove(wal_path);
}

/* ============================================================
 * 测试 1：列族创建与查询
 * ============================================================ */

TEST(CFEngineTest, CreateAndCheckFamily) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(CF_OK, cf_create_family(db, "users"));
    EXPECT_TRUE(cf_family_exists(db, "users"));
    EXPECT_FALSE(cf_family_exists(db, "nonexistent"));

    cf_close(db);
}

/* ============================================================
 * 测试 2：单列 PUT / GET
 * ============================================================ */

TEST(CFEngineTest, PutAndGetSingleColumn) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:001";
    const char *col = "name";
    const char *val = "Alice";
    size_t val_len = strlen(val);

    EXPECT_EQ(CF_OK, cf_put(db, "users", row, strlen(row),
                            col, strlen(col),
                            val, (uint32_t)val_len, 0));

    void *out_val = nullptr;
    uint32_t out_len = 0;
    EXPECT_EQ(CF_OK, cf_get(db, "users", row, strlen(row),
                            col, strlen(col),
                            &out_val, &out_len));
    EXPECT_EQ(val_len, out_len);
    EXPECT_EQ(0, memcmp(val, out_val, out_len));
    if (out_val) free(out_val);

    cf_close(db);
}

/* ============================================================
 * 测试 3：列存在性检查
 * ============================================================ */

TEST(CFEngineTest, ColumnExists) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:001";
    const char *col = "email";
    EXPECT_EQ(CF_OK, cf_put(db, "users", row, strlen(row),
                            col, strlen(col),
                            "alice@example.com", 17, 0));

    EXPECT_TRUE(cf_exists(db, "users", row, strlen(row),
                          col, strlen(col)));
    EXPECT_FALSE(cf_exists(db, "users", row, strlen(row),
                           "nonexistent", 11));

    cf_close(db);
}

/* ============================================================
 * 测试 4：单列删除
 * ============================================================ */

TEST(CFEngineTest, DeleteSingleColumn) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:001";
    const char *col = "temp_field";

    cf_put(db, "users", row, strlen(row), col, strlen(col),
           "delete_me", 9, 0);
    EXPECT_TRUE(cf_exists(db, "users", row, strlen(row), col, strlen(col)));

    EXPECT_EQ(CF_OK, cf_delete_column(db, "users",
                                      row, strlen(row),
                                      col, strlen(col)));
    EXPECT_FALSE(cf_exists(db, "users", row, strlen(row), col, strlen(col)));

    /* 第二次删除应返回 NOT_FOUND */
    EXPECT_EQ(CF_NOT_FOUND, cf_delete_column(db, "users",
                                             row, strlen(row),
                                             col, strlen(col)));

    cf_close(db);
}

/* ============================================================
 * 测试 5：动态列（同行的不同列）
 * ============================================================ */

TEST(CFEngineTest, DynamicColumns) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:007";
    /* 同行的不同列 */
    cf_put(db, "users", row, strlen(row), "name", 4, "Bob", 3, 0);
    cf_put(db, "users", row, strlen(row), "age", 3, "30", 2, 0);
    cf_put(db, "users", row, strlen(row), "city", 4, "Beijing", 7, 0);
    cf_put(db, "users", row, strlen(row), "active", 6, "true", 4, 0);

    /* 全部应可独立读取 */
    void *v; uint32_t l;
    EXPECT_EQ(CF_OK, cf_get(db, "users", row, strlen(row), "name", 4, &v, &l));
    EXPECT_EQ(3u, l); EXPECT_EQ(0, memcmp("Bob", v, 3)); free(v);

    EXPECT_EQ(CF_OK, cf_get(db, "users", row, strlen(row), "age", 3, &v, &l));
    EXPECT_EQ(2u, l); EXPECT_EQ(0, memcmp("30", v, 2)); free(v);

    EXPECT_EQ(CF_OK, cf_get(db, "users", row, strlen(row), "city", 4, &v, &l));
    EXPECT_EQ(7u, l); EXPECT_EQ(0, memcmp("Beijing", v, 7)); free(v);

    EXPECT_EQ(CF_OK, cf_get(db, "users", row, strlen(row), "active", 6, &v, &l));
    EXPECT_EQ(4u, l); EXPECT_EQ(0, memcmp("true", v, 4)); free(v);

    cf_close(db);
}

/* ============================================================
 * 测试 6：覆盖更新（同名列）
 * ============================================================ */

TEST(CFEngineTest, UpdateOverwriteColumn) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:001";
    const char *col = "name";

    cf_put(db, "users", row, strlen(row), col, strlen(col), "Alice", 5, 0);
    cf_put(db, "users", row, strlen(row), col, strlen(col), "AliceV2", 7, 0);

    void *v; uint32_t l;
    EXPECT_EQ(CF_OK, cf_get(db, "users", row, strlen(row),
                            col, strlen(col), &v, &l));
    EXPECT_EQ(7u, l);
    EXPECT_EQ(0, memcmp("AliceV2", v, 7));
    free(v);

    cf_close(db);
}

/* ============================================================
 * 测试 7：行级 GET（多列）
 * ============================================================ */

TEST(CFEngineTest, GetRowMultipleColumns) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:123";
    cf_put(db, "users", row, strlen(row), "name", 4, "Charlie", 7, 0);
    cf_put(db, "users", row, strlen(row), "email", 5, "c@x.com", 7, 0);
    cf_put(db, "users", row, strlen(row), "score", 5, "95", 2, 0);

    cf_row_t *row_data = nullptr;
    EXPECT_EQ(CF_OK, cf_get_row(db, "users", row, strlen(row), &row_data));
    ASSERT_NE(nullptr, row_data);
    EXPECT_EQ(strlen(row), row_data->row_key_len);
    EXPECT_EQ(0, memcmp(row, row_data->row_key, row_data->row_key_len));
    EXPECT_EQ(3u, row_data->num_columns);

    /* 列按字典序：email, name, score */
    EXPECT_STREQ("email", row_data->columns[0]->name);
    EXPECT_STREQ("name", row_data->columns[1]->name);
    EXPECT_STREQ("score", row_data->columns[2]->name);

    cf_row_free(row_data);
    cf_close(db);
}

/* ============================================================
 * 测试 8：行级删除
 * ============================================================ */

TEST(CFEngineTest, DeleteRow) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "user:999";
    cf_put(db, "users", row, strlen(row), "a", 1, "1", 1, 0);
    cf_put(db, "users", row, strlen(row), "b", 1, "2", 1, 0);
    cf_put(db, "users", row, strlen(row), "c", 1, "3", 1, 0);

    EXPECT_TRUE(cf_row_exists(db, "users", row, strlen(row)));

    EXPECT_EQ(CF_OK, cf_delete_row(db, "users", row, strlen(row)));

    EXPECT_FALSE(cf_row_exists(db, "users", row, strlen(row)));
    EXPECT_FALSE(cf_exists(db, "users", row, strlen(row), "a", 1));
    EXPECT_FALSE(cf_exists(db, "users", row, strlen(row), "b", 1));
    EXPECT_FALSE(cf_exists(db, "users", row, strlen(row), "c", 1));

    cf_close(db);
}

/* ============================================================
 * 测试 9：行扫描
 * ============================================================ */

TEST(CFEngineTest, ScanRows) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* 插入多行 */
    cf_put(db, "users", "user:001", 8, "name", 4, "A", 1, 0);
    cf_put(db, "users", "user:002", 8, "name", 4, "B", 1, 0);
    cf_put(db, "users", "user:003", 8, "name", 4, "C", 1, 0);
    cf_put(db, "users", "user:004", 8, "name", 4, "D", 1, 0);
    cf_put(db, "users", "user:005", 8, "name", 4, "E", 1, 0);

    cf_iter_t *iter = cf_scan_rows(db, "users", NULL, 0, NULL, 0);
    ASSERT_NE(nullptr, iter);

    std::vector<std::string> keys;
    while (cf_iter_next(iter) == CF_OK) {
        keys.push_back(std::string(cf_iter_row_key(iter),
                                   cf_iter_row_key_len(iter)));
    }
    cf_iter_free(iter);

    EXPECT_EQ(5u, keys.size());
    /* 按行键顺序排列 */
    EXPECT_EQ("user:001", keys[0]);
    EXPECT_EQ("user:002", keys[1]);
    EXPECT_EQ("user:003", keys[2]);
    EXPECT_EQ("user:004", keys[3]);
    EXPECT_EQ("user:005", keys[4]);

    cf_close(db);
}

/* ============================================================
 * 测试 10：行扫描范围
 * ============================================================ */

TEST(CFEngineTest, ScanRowsRange) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    for (int i = 1; i <= 10; i++) {
        char row[16]; snprintf(row, sizeof(row), "row:%03d", i);
        cf_put(db, "data", row, strlen(row), "v", 1, "x", 1, 0);
    }

    /* 范围扫描 row:003 ~ row:007 */
    cf_iter_t *iter = cf_scan_rows(db, "data", "row:003", 7, "row:007", 7);
    ASSERT_NE(nullptr, iter);

    std::vector<std::string> keys;
    while (cf_iter_next(iter) == CF_OK) {
        keys.push_back(std::string(cf_iter_row_key(iter),
                                   cf_iter_row_key_len(iter)));
    }
    cf_iter_free(iter);

    EXPECT_EQ(5u, keys.size());
    EXPECT_EQ("row:003", keys[0]);
    EXPECT_EQ("row:007", keys[4]);

    cf_close(db);
}

/* ============================================================
 * 测试 11：批量操作
 * ============================================================ */

TEST(CFEngineTest, BatchExecute) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* 准备批量插入 */
    std::vector<cf_batch_op_t> ops;
    for (int i = 0; i < 5; i++) {
        cf_batch_op_t op;
        op.type = CF_BATCH_PUT;
        op.cf_name = "metrics";
        char row[16]; snprintf(row, sizeof(row), "m:%d", i);
        op.row_key = strdup(row);
        op.row_key_len = strlen(row);
        char val[16]; snprintf(val, sizeof(val), "v%d", i * 10);
        op.col_name = "value";
        op.col_name_len = 5;
        op.value = strdup(val);
        op.value_len = strlen(val);
        ops.push_back(op);
    }

    std::vector<cf_result_t> results(ops.size());
    EXPECT_EQ(CF_OK, cf_batch_execute(db, ops.data(),
                                      (uint32_t)ops.size(),
                                      results.data()));
    for (auto r : results) {
        EXPECT_EQ(CF_OK, r);
    }

    /* 验证所有行均已插入 */
    for (int i = 0; i < 5; i++) {
        char row[16]; snprintf(row, sizeof(row), "m:%d", i);
        EXPECT_TRUE(cf_row_exists(db, "metrics", row, strlen(row)));
    }

    /* 释放动态分配的字符串 */
    for (auto &op : ops) {
        free((void *)op.row_key);
        free((void *)op.value);
    }

    cf_close(db);
}

/* ============================================================
 * 测试 12：批量混合操作（PUT + DELETE）
 * ============================================================ */

TEST(CFEngineTest, BatchMixedOperations) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* 准备数据 */
    cf_put(db, "logs", "log:1", 5, "msg", 3, "hello", 5, 0);
    cf_put(db, "logs", "log:2", 5, "msg", 3, "world", 5, 0);

    /* 混合批量：PUT 一个，DELETE 一个列 */
    cf_batch_op_t ops[3];
    ops[0].type = CF_BATCH_PUT;
    ops[0].cf_name = "logs";
    ops[0].row_key = "log:3"; ops[0].row_key_len = 5;
    ops[0].col_name = "msg"; ops[0].col_name_len = 3;
    ops[0].value = "new"; ops[0].value_len = 3;

    ops[1].type = CF_BATCH_DELETE_COL;
    ops[1].cf_name = "logs";
    ops[1].row_key = "log:1"; ops[1].row_key_len = 5;
    ops[1].col_name = "msg"; ops[1].col_name_len = 3;
    ops[1].value = nullptr; ops[1].value_len = 0;

    ops[2].type = CF_BATCH_DELETE_ROW;
    ops[2].cf_name = "logs";
    ops[2].row_key = "log:2"; ops[2].row_key_len = 5;
    ops[2].col_name = nullptr; ops[2].col_name_len = 0;
    ops[2].value = nullptr; ops[2].value_len = 0;

    cf_result_t results[3];
    EXPECT_EQ(CF_OK, cf_batch_execute(db, ops, 3, results));
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(CF_OK, results[i]) << "op " << i << " failed";
    }

    /* 验证 */
    void *v; uint32_t l;
    EXPECT_EQ(CF_OK, cf_get(db, "logs", "log:3", 5, "msg", 3, &v, &l));
    free(v);

    EXPECT_EQ(CF_NOT_FOUND, cf_get(db, "logs", "log:1", 5, "msg", 3, &v, &l));
    EXPECT_FALSE(cf_row_exists(db, "logs", "log:2", 5));

    cf_close(db);
}

/* ============================================================
 * 测试 13：列族列表
 * ============================================================ */

TEST(CFEngineTest, ListFamilies) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    cf_create_family(db, "users");
    cf_create_family(db, "products");
    cf_create_family(db, "orders");
    /* 通过 PUT 自动创建 */
    cf_put(db, "events", "evt:1", 5, "type", 4, "click", 5, 0);

    char **names = nullptr;
    uint32_t count = 0;
    EXPECT_EQ(CF_OK, cf_list_families(db, &names, &count));

    EXPECT_EQ(4u, count);

    std::vector<std::string> cf_names;
    for (uint32_t i = 0; i < count; i++) {
        cf_names.push_back(names[i]);
    }
    cf_free_family_list(names, count);

    std::sort(cf_names.begin(), cf_names.end());
    EXPECT_EQ("events", cf_names[0]);
    EXPECT_EQ("orders", cf_names[1]);
    EXPECT_EQ("products", cf_names[2]);
    EXPECT_EQ("users", cf_names[3]);

    cf_close(db);
}

/* ============================================================
 * 测试 14：列族删除（含数据清理）
 * ============================================================ */

TEST(CFEngineTest, DropFamilyWithData) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* 写入多行 */
    for (int i = 0; i < 10; i++) {
        char row[16]; snprintf(row, sizeof(row), "tmp:%d", i);
        cf_put(db, "temporary", row, strlen(row),
               "data", 4, "value", 5, 0);
    }
    EXPECT_TRUE(cf_family_exists(db, "temporary"));

    EXPECT_EQ(CF_OK, cf_drop_family(db, "temporary"));
    EXPECT_FALSE(cf_family_exists(db, "temporary"));

    /* 行也应被清理 */
    EXPECT_FALSE(cf_row_exists(db, "temporary", "tmp:0", 5));

    cf_close(db);
}

/* ============================================================
 * 测试 15：二进制列值
 * ============================================================ */

TEST(CFEngineTest, BinaryColumnValue) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* 含 \x00 的二进制数据 */
    uint8_t binary[256];
    for (int i = 0; i < 256; i++) binary[i] = (uint8_t)i;

    EXPECT_EQ(CF_OK, cf_put(db, "blob", "k", 1, "data", 4,
                            binary, 256, 0));

    void *out = nullptr;
    uint32_t out_len = 0;
    EXPECT_EQ(CF_OK, cf_get(db, "blob", "k", 1, "data", 4,
                            &out, &out_len));
    EXPECT_EQ(256u, out_len);
    EXPECT_EQ(0, memcmp(binary, out, 256));
    free(out);

    cf_close(db);
}

/* ============================================================
 * 测试 16：统计信息
 * ============================================================ */

TEST(CFEngineTest, FamilyStats) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    cf_put(db, "stats_cf", "r1", 2, "c1", 2, "v1", 2, 0);
    cf_put(db, "stats_cf", "r1", 2, "c2", 2, "v2", 2, 0);
    cf_put(db, "stats_cf", "r2", 2, "c1", 2, "v1", 2, 0);
    cf_put(db, "stats_cf", "r3", 2, "c1", 2, "v1", 2, 0);

    cf_family_stats_t stats;
    EXPECT_EQ(CF_OK, cf_family_stats(db, "stats_cf", &stats));
    EXPECT_EQ(3u, stats.num_rows);   /* r1, r2, r3 */
    EXPECT_EQ(4u, stats.num_columns); /* r1.c1, r1.c2, r2.c1, r3.c1 */
    EXPECT_GT(stats.total_size, 0u);

    cf_close(db);
}

/* ============================================================
 * 测试 17：TTL
 * ============================================================ */

TEST(CFEngineTest, TTLColumn) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* TTL = 1 秒 */
    cf_put(db, "session", "s:1", 3, "token", 5, "abc123", 6, 1);

    void *v; uint32_t l;
    EXPECT_EQ(CF_OK, cf_get(db, "session", "s:1", 3, "token", 5, &v, &l));
    free(v);

    /* TTL=0 表示永不过期，仍可读取 */
    cf_put(db, "session", "s:2", 3, "token", 5, "xyz", 3, 0);
    EXPECT_EQ(CF_OK, cf_get(db, "session", "s:2", 3, "token", 5, &v, &l));
    free(v);

    cf_close(db);
}

/* ============================================================
 * 测试 18：行存在性
 * ============================================================ */

TEST(CFEngineTest, RowExistsCheck) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    EXPECT_FALSE(cf_row_exists(db, "any_cf", "missing", 7));

    cf_put(db, "any_cf", "present", 7, "x", 1, "y", 1, 0);
    EXPECT_TRUE(cf_row_exists(db, "any_cf", "present", 7));

    cf_close(db);
}

/* ============================================================
 * 测试 19：NOT_FOUND 错误路径
 * ============================================================ */

TEST(CFEngineTest, NotFoundErrors) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    void *v = nullptr; uint32_t l = 0;
    EXPECT_EQ(CF_NOT_FOUND, cf_get(db, "nope", "k", 1, "c", 1, &v, &l));
    EXPECT_EQ(CF_NOT_FOUND, cf_delete_column(db, "nope", "k", 1, "c", 1));
    EXPECT_EQ(CF_NOT_FOUND, cf_delete_row(db, "nope", "k", 1));

    cf_row_t *row = nullptr;
    EXPECT_EQ(CF_NOT_FOUND, cf_get_row(db, "nope", "k", 1, &row));

    cf_close(db);
}

/* ============================================================
 * 测试 20：无效参数
 * ============================================================ */

TEST(CFEngineTest, InvalidParameters) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    /* 空行键 */
    EXPECT_EQ(CF_INVALID, cf_put(db, "x", "", 0, "c", 1, "v", 1, 0));

    /* 空列名 */
    EXPECT_EQ(CF_INVALID, cf_put(db, "x", "r", 1, "", 0, "v", 1, 0));

    /* NULL db */
    EXPECT_EQ(CF_INVALID, cf_close(nullptr));

    cf_close(db);
}

/* ============================================================
 * 测试 21：多次打开关闭数据库
 * ============================================================ */

TEST(CFEngineTest, ReopenDatabase) {
    cleanup_db();
    /* 第一次会话：写入 */
    {
        cf_db_t *db = cf_open(kTestDbPath);
        ASSERT_NE(nullptr, db);
        cf_put(db, "persistent", "key1", 4, "field", 5, "value", 5, 0);
        cf_close(db);
    }
    /* 第二次会话：应能读到 */
    {
        cf_db_t *db = cf_open(kTestDbPath);
        ASSERT_NE(nullptr, db);
        void *v = nullptr; uint32_t l = 0;
        EXPECT_EQ(CF_OK, cf_get(db, "persistent", "key1", 4,
                                "field", 5, &v, &l));
        EXPECT_EQ(5u, l);
        EXPECT_EQ(0, memcmp("value", v, 5));
        free(v);
        cf_close(db);
    }
}

/* ============================================================
 * 测试 22：宽行（多列）
 * ============================================================ */

TEST(CFEngineTest, WideRowManyColumns) {
    cleanup_db();
    cf_db_t *db = cf_open(kTestDbPath);
    ASSERT_NE(nullptr, db);

    const char *row = "wide:1";
    /* 100 个动态列 */
    for (int i = 0; i < 100; i++) {
        char col[16]; snprintf(col, sizeof(col), "col_%03d", i);
        char val[16]; snprintf(val, sizeof(val), "v_%03d", i);
        EXPECT_EQ(CF_OK, cf_put(db, "wide", row, strlen(row),
                                col, strlen(col),
                                val, (uint32_t)strlen(val), 0));
    }

    cf_row_t *row_data = nullptr;
    EXPECT_EQ(CF_OK, cf_get_row(db, "wide", row, strlen(row), &row_data));
    ASSERT_NE(nullptr, row_data);
    EXPECT_EQ(100u, row_data->num_columns);
    /* 检查列按字典序排列 */
    EXPECT_STREQ("col_000", row_data->columns[0]->name);
    EXPECT_STREQ("col_099", row_data->columns[99]->name);
    cf_row_free(row_data);

    cf_close(db);
}