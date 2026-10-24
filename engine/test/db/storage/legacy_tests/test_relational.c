/**
 * @file test_relational.c
 * @brief 关系存储模态追赶测试
 *
 * 测试 TupleDesc、ScanKey、Relation 基础操作、Table 元数据
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 头文件 */
#include "db/storage/rel/rel.h"
#include "db/storage/rel/table.h"

/* ========================================================================
 * TupleDesc 测试
 * ======================================================================== */

class TupleDescTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TupleDescTest, CreateDestroy) {
    TupleDesc tdesc = CreateTupleDesc(3);
    ASSERT_NE(tdesc, nullptr);
    EXPECT_EQ(tdesc->natts, 3);
    EXPECT_NE(tdesc->attrs, nullptr);
    FreeTupleDesc(tdesc);
}

TEST_F(TupleDescTest, CreateInvalid) {
    EXPECT_EQ(CreateTupleDesc(0), nullptr);
    EXPECT_EQ(CreateTupleDesc(-1), nullptr);
}

TEST_F(TupleDescTest, Natts) {
    TupleDesc tdesc = CreateTupleDesc(5);
    ASSERT_NE(tdesc, nullptr);
    EXPECT_EQ(TupleDescNatts(tdesc), 5);
    FreeTupleDesc(tdesc);
}

TEST_F(TupleDescTest, NattsNull) {
    EXPECT_EQ(TupleDescNatts(nullptr), 0);
}

TEST_F(TupleDescTest, AttrAccess) {
    TupleDesc tdesc = CreateTupleDesc(3);
    ASSERT_NE(tdesc, nullptr);

    /* attnum 从 1 开始 */
    void *attr1 = TupleDescAttr(tdesc, 1);
    ASSERT_NE(attr1, nullptr);
    void *attr2 = TupleDescAttr(tdesc, 2);
    ASSERT_NE(attr2, nullptr);
    void *attr3 = TupleDescAttr(tdesc, 3);
    ASSERT_NE(attr3, nullptr);

    /* 越界 */
    EXPECT_EQ(TupleDescAttr(tdesc, 0), nullptr);
    EXPECT_EQ(TupleDescAttr(tdesc, 4), nullptr);

    FreeTupleDesc(tdesc);
}

TEST_F(TupleDescTest, AttrNull) {
    EXPECT_EQ(TupleDescAttr(nullptr, 1), nullptr);
}

TEST_F(TupleDescTest, Copy) {
    TupleDesc src = CreateTupleDesc(2);
    ASSERT_NE(src, nullptr);

    /* 设置列名 */
    strncpy(src->attrs[0].attname, "id", NAMEDATALEN - 1);
    src->attrs[0].atttypid = 1;
    src->attrs[0].attlen = 4;

    strncpy(src->attrs[1].attname, "name", NAMEDATALEN - 1);
    src->attrs[1].atttypid = 2;
    src->attrs[1].attlen = 256;

    src->tdtypeid = 100;
    src->tdtypmod = 5;

    TupleDesc copy = CreateTupleDescCopy(src);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->natts, 2);
    EXPECT_EQ(copy->tdtypeid, 100);
    EXPECT_EQ(copy->tdtypmod, 5);

    /* 验证列数据被复制 */
    EXPECT_EQ(copy->attrs[0].atttypid, 1u);
    EXPECT_EQ(copy->attrs[0].attlen, 4);
    EXPECT_STREQ(copy->attrs[0].attname, "id");

    EXPECT_EQ(copy->attrs[1].atttypid, 2u);
    EXPECT_EQ(copy->attrs[1].attlen, 256);
    EXPECT_STREQ(copy->attrs[1].attname, "name");

    FreeTupleDesc(src);
    FreeTupleDesc(copy);
}

TEST_F(TupleDescTest, CopyNull) {
    EXPECT_EQ(CreateTupleDescCopy(nullptr), nullptr);
}

TEST_F(TupleDescTest, FreeNull) {
    /* 不崩溃即可 */
    FreeTupleDesc(nullptr);
}

/* ========================================================================
 * ScanKey 测试
 * ======================================================================== */

class ScanKeyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScanKeyTest, Init) {
    ScanKeyData key;
    int val = 42;
    ScanKeyInit(&key, 1, SCAN_KEY_EQ, &val);

    EXPECT_EQ(key.sk_attno, 1);
    EXPECT_EQ(key.sk_op, SCAN_KEY_EQ);
    EXPECT_EQ(key.sk_argument, &val);
    EXPECT_EQ(key.sk_arglen, 0u);
}

TEST_F(ScanKeyTest, InitWithInfo) {
    ScanKeyData key;
    char data[] = "hello";
    ScanKeyInitWithInfo(&key, 2, SCAN_KEY_LT, sizeof(data), data);

    EXPECT_EQ(key.sk_attno, 2);
    EXPECT_EQ(key.sk_op, SCAN_KEY_LT);
    EXPECT_EQ(key.sk_argument, data);
    EXPECT_EQ(key.sk_arglen, sizeof(data));
}

TEST_F(ScanKeyTest, InitNull) {
    /* 不崩溃即可 */
    ScanKeyInit(nullptr, 1, SCAN_KEY_EQ, nullptr);
    ScanKeyInitWithInfo(nullptr, 1, SCAN_KEY_EQ, 0, nullptr);
}

TEST_F(ScanKeyTest, AllOps) {
    ScanKeyData key;
    int val = 0;

    ScanKeyOp ops[] = {
        SCAN_KEY_EQ, SCAN_KEY_LT, SCAN_KEY_LE,
        SCAN_KEY_GT, SCAN_KEY_GE, SCAN_KEY_NE,
        SCAN_KEY_SEARCH, SCAN_KEY_NSEARCH
    };

    for (int i = 0; i < 8; i++) {
        ScanKeyInit(&key, 1, ops[i], &val);
        EXPECT_EQ(key.sk_op, ops[i]);
    }
}

/* ========================================================================
 * Relation 基础操作测试
 * ======================================================================== */

class RelationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RelationTest, OpenNode) {
    Relation rel = relation_opennode(100, REL_OPEN_READONLY);
    ASSERT_NE(rel, nullptr);
    EXPECT_EQ(rel->rd_relfilenode, 100u);
    EXPECT_EQ(rel->rd_refcnt, 1);
    relation_close(rel, REL_OPEN_READONLY);
}

TEST_F(RelationTest, OpenNodeZero) {
    EXPECT_EQ(relation_opennode(0, REL_OPEN_READONLY), nullptr);
}

TEST_F(RelationTest, CloseNull) {
    /* 不崩溃即可 */
    relation_close(nullptr, REL_OPEN_READONLY);
}

TEST_F(RelationTest, CloseRefCount) {
    Relation rel = relation_opennode(200, REL_OPEN_READWRITE);
    ASSERT_NE(rel, nullptr);
    EXPECT_EQ(rel->rd_refcnt, 1);
    EXPECT_EQ(rel->rd_lockmode, 1);

    relation_close(rel, REL_OPEN_READWRITE);
    /* 关闭后 refcnt=0，再关闭不崩溃 */
    relation_close(rel, REL_OPEN_READWRITE);
}

TEST_F(RelationTest, Getters) {
    Relation rel = relation_opennode(300, REL_OPEN_READONLY);
    ASSERT_NE(rel, nullptr);

    EXPECT_EQ(relation_getnblocks(rel), 0u);
    EXPECT_EQ(relation_getfilenode(rel), 300u);
    EXPECT_EQ(relation_getam(rel), AM_HEAP);

    relation_close(rel, REL_OPEN_READONLY);
}

TEST_F(RelationTest, GettersNull) {
    EXPECT_EQ(relation_getdesc(nullptr), nullptr);
    EXPECT_EQ(relation_getnatts(nullptr), 0);
    EXPECT_EQ(relation_getnblocks(nullptr), 0u);
    EXPECT_EQ(relation_getfilenode(nullptr), 0u);
    EXPECT_EQ(relation_getam(nullptr), AM_HEAP);
}

TEST_F(RelationTest, OpenInvalidOid) {
    EXPECT_EQ(relation_open(0, REL_OPEN_READONLY), nullptr);
}

TEST_F(RelationTest, CreateAndDrop) {
    TupleDesc tdesc = CreateTupleDesc(2);
    ASSERT_NE(tdesc, nullptr);

    int ret = relation_create(999, tdesc, RELKIND_RELATION, AM_HEAP);
    EXPECT_EQ(ret, 0);

    ret = relation_drop(999);
    EXPECT_EQ(ret, 0);

    FreeTupleDesc(tdesc);
}

TEST_F(RelationTest, CreateInvalidOid) {
    TupleDesc tdesc = CreateTupleDesc(1);
    EXPECT_EQ(relation_create(0, tdesc, RELKIND_RELATION, AM_HEAP), -1);
    FreeTupleDesc(tdesc);
}

TEST_F(RelationTest, CreateNullDesc) {
    EXPECT_EQ(relation_create(999, nullptr, RELKIND_RELATION, AM_HEAP), -1);
}

TEST_F(RelationTest, DropInvalidOid) {
    EXPECT_EQ(relation_drop(0), -1);
}

TEST_F(RelationTest, RelKind) {
    EXPECT_EQ(RELKIND_RELATION, 'r');
    EXPECT_EQ(RELKIND_INDEX, 'i');
    EXPECT_EQ(RELKIND_SEQUENCE, 'S');
    EXPECT_EQ(RELKIND_VIEW, 'v');
}

TEST_F(RelationTest, AccessMethodType) {
    EXPECT_EQ(AM_HEAP, 0);
    EXPECT_EQ(AM_BTREE, 1);
    EXPECT_EQ(AM_HASH, 2);
}

TEST_F(RelationTest, RelOpenMode) {
    EXPECT_EQ(RELMODE_READ, 0);
    EXPECT_EQ(RELMODE_WRITE, 1);
}

TEST_F(RelationTest, ScanDirection) {
    EXPECT_EQ(ForwardScanDirection, 0);
    EXPECT_EQ(BackwardScanDirection, 1);
}

/* ========================================================================
 * Table 元数据测试
 *
 * 注意：table_create 需要 kv_t*，但元数据访问函数不依赖 KV
 * ======================================================================== */

class TableMetaTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建列定义 */
        columns[0].name = strdup("id");
        columns[0].type = SQL_TYPE_INT;
        columns[0].length = 0;
        columns[0].not_null = true;
        columns[0].primary_key = true;

        columns[1].name = strdup("name");
        columns[1].type = SQL_TYPE_VARCHAR;
        columns[1].length = 64;
        columns[1].not_null = false;
        columns[1].primary_key = false;

        /* 创建表（传入假的 kv_t*） */
        table = table_create((kv_t *)0x1, "users", columns, 2);
    }

    void TearDown() override {
        table_close(table);
        free(columns[0].name);
        free(columns[1].name);
    }

    table_column_t columns[2];
    table_t *table;
};

TEST_F(TableMetaTest, Create) {
    ASSERT_NE(table, nullptr);
}

TEST_F(TableMetaTest, CreateInvalid) {
    EXPECT_EQ(table_create(nullptr, "t", columns, 2), nullptr);
    EXPECT_EQ(table_create((kv_t *)0x1, nullptr, columns, 2), nullptr);
    EXPECT_EQ(table_create((kv_t *)0x1, "t", nullptr, 2), nullptr);
    EXPECT_EQ(table_create((kv_t *)0x1, "t", columns, 0), nullptr);
}

TEST_F(TableMetaTest, GetMeta) {
    const table_meta_t *meta = table_get_meta(table);
    ASSERT_NE(meta, nullptr);
    EXPECT_STREQ(meta->name, "users");
    EXPECT_EQ(meta->num_columns, 2u);
}

TEST_F(TableMetaTest, GetMetaNull) {
    EXPECT_EQ(table_get_meta(nullptr), nullptr);
}

TEST_F(TableMetaTest, NumColumns) {
    EXPECT_EQ(table_num_columns(table), 2u);
}

TEST_F(TableMetaTest, NumColumnsNull) {
    EXPECT_EQ(table_num_columns(nullptr), 0u);
}

TEST_F(TableMetaTest, GetColumn) {
    const table_column_t *col0 = table_get_column(table, 0);
    ASSERT_NE(col0, nullptr);
    EXPECT_STREQ(col0->name, "id");
    EXPECT_EQ(col0->type, SQL_TYPE_INT);
    EXPECT_TRUE(col0->not_null);
    EXPECT_TRUE(col0->primary_key);

    const table_column_t *col1 = table_get_column(table, 1);
    ASSERT_NE(col1, nullptr);
    EXPECT_STREQ(col1->name, "name");
    EXPECT_EQ(col1->type, SQL_TYPE_VARCHAR);
    EXPECT_EQ(col1->length, 64u);
    EXPECT_FALSE(col1->not_null);
    EXPECT_FALSE(col1->primary_key);
}

TEST_F(TableMetaTest, GetColumnOutOfBounds) {
    EXPECT_EQ(table_get_column(table, 2), nullptr);
    EXPECT_EQ(table_get_column(table, 999), nullptr);
}

TEST_F(TableMetaTest, GetColumnNull) {
    EXPECT_EQ(table_get_column(nullptr, 0), nullptr);
}

TEST_F(TableMetaTest, RowSize) {
    size_t size = table_row_size(table);
    /* null_mask(4) + id(4) + name(64) = 72 */
    EXPECT_EQ(size, 72u);
}

TEST_F(TableMetaTest, RowSizeNull) {
    EXPECT_EQ(table_row_size(nullptr), 0u);
}

TEST_F(TableMetaTest, CloseNull) {
    /* 不崩溃即可 */
    table_close(nullptr);
}

TEST_F(TableMetaTest, DropNull) {
    EXPECT_EQ(table_drop(nullptr), -1);
}

/* ========================================================================
 * Table Meta Set 测试
 *
 * 使用 table_create 创建表，然后测试 meta set 函数
 * ======================================================================== */

class TableMetaSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        table_column_t cols[1];
        cols[0].name = strdup("col0");
        cols[0].type = SQL_TYPE_INT;
        cols[0].length = 0;
        cols[0].not_null = false;
        cols[0].primary_key = false;

        table = table_create((kv_t *)0x1, "meta_set_test", cols, 1);
        free(cols[0].name);
    }

    void TearDown() override {
        table_close(table);
    }

    table_t *table;
};

TEST_F(TableMetaSetTest, SetNumColumns) {
    table_meta_set_num_columns(table, 3);
    const table_meta_t *meta = table_get_meta(table);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->num_columns, 3u);
    EXPECT_NE(meta->columns, nullptr);
}

TEST_F(TableMetaSetTest, SetRowSize) {
    table_meta_set_row_size(table, 128);
    const table_meta_t *meta = table_get_meta(table);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->row_size, 128u);
}

TEST_F(TableMetaSetTest, SetColumn) {
    table_meta_set_num_columns(table, 1);
    table_meta_set_column(table, 0, "col1", SQL_TYPE_INT, 0, true, false);

    const table_column_t *col = table_get_column(table, 0);
    ASSERT_NE(col, nullptr);
    EXPECT_STREQ(col->name, "col1");
    EXPECT_EQ(col->type, SQL_TYPE_INT);
    EXPECT_TRUE(col->not_null);
    EXPECT_FALSE(col->primary_key);
}

TEST_F(TableMetaSetTest, SetColumnNull) {
    /* 不崩溃即可 */
    table_meta_set_column(nullptr, 0, "x", SQL_TYPE_INT, 0, false, false);
    table_meta_set_column(table, 0, nullptr, SQL_TYPE_INT, 0, false, false);
    table_meta_set_num_columns(table, 1);
    table_meta_set_column(table, 99, "x", SQL_TYPE_INT, 0, false, false);
}

TEST_F(TableMetaSetTest, SetColumnFromStored) {
    table_meta_set_num_columns(table, 1);

    table_column_t stored;
    stored.name = strdup("from_stored");
    stored.type = SQL_TYPE_VARCHAR;
    stored.length = 100;
    stored.not_null = false;
    stored.primary_key = true;
    stored.offset = 8;

    table_meta_set_column_from_stored(table, 0, &stored);

    const table_column_t *col = table_get_column(table, 0);
    ASSERT_NE(col, nullptr);
    EXPECT_STREQ(col->name, "from_stored");
    EXPECT_EQ(col->type, SQL_TYPE_VARCHAR);
    EXPECT_EQ(col->length, 100u);
    EXPECT_TRUE(col->primary_key);
    EXPECT_EQ(col->offset, 8u);

    free(stored.name);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
