/**
 * @file test_amapi.cpp
 * @brief IndexAmRoutine 注册表测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/access/amapi.h"
}

namespace {

class AmApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        index_am_init();
    }
};

/* 测试用 AM 回调 */
static int test_ambuild(Relation heap, IndexInfo *indexInfo) { return 0; }
static int test_aminsert(Relation index, Datum *values, bool *isnull) { return 0; }
static int test_amdelete(Relation index, Datum *values, bool *isnull) { return 0; }
static IndexScanDesc test_ambeginscan(Relation index, int nkeys, ScanKey key) { return NULL; }
static void test_amendscan(IndexScanDesc scan) {}
static bool test_amrescan(IndexScanDesc scan, ScanKey key) { return true; }
static void *test_amgetnext(IndexScanDesc scan) { return NULL; }

TEST_F(AmApiTest, RegisterAndGetBtree) {
    IndexAmRoutine btree_am = {
        .am_name = "btree",
        .ambuild = test_ambuild,
        .aminsert = test_aminsert,
        .amdelete = test_amdelete,
        .ambeginscan = test_ambeginscan,
        .amendscan = test_amendscan,
        .amrescan = test_amrescan,
        .amgetnext = test_amgetnext,
    };

    int rc = index_am_register("btree", &btree_am);
    EXPECT_EQ(rc, 0);

    const IndexAmRoutine *got = index_am_get("btree");
    EXPECT_NE(got, nullptr);
    EXPECT_STREQ(got->am_name, "btree");
    EXPECT_EQ(got->ambuild, test_ambuild);
    EXPECT_EQ(got->amgetnext, test_amgetnext);
}

TEST_F(AmApiTest, GetUnknownReturnsNull) {
    const IndexAmRoutine *got = index_am_get("nonexistent");
    EXPECT_EQ(got, nullptr);
}

TEST_F(AmApiTest, RegisterDuplicateFails) {
    IndexAmRoutine am1 = { .am_name = "dup" };
    IndexAmRoutine am2 = { .am_name = "dup" };

    EXPECT_EQ(index_am_register("dup", &am1), 0);
    EXPECT_EQ(index_am_register("dup", &am2), -1); /* 重复注册失败 */
}

TEST_F(AmApiTest, RegisterMultipleAms) {
    IndexAmRoutine btree_am = { .am_name = "btree" };
    IndexAmRoutine hnsw_am = { .am_name = "hnsw" };
    IndexAmRoutine ivf_am = { .am_name = "ivf" };

    EXPECT_EQ(index_am_register("btree", &btree_am), 0);
    EXPECT_EQ(index_am_register("hnsw", &hnsw_am), 0);
    EXPECT_EQ(index_am_register("ivf", &ivf_am), 0);

    EXPECT_NE(index_am_get("btree"), nullptr);
    EXPECT_NE(index_am_get("hnsw"), nullptr);
    EXPECT_NE(index_am_get("ivf"), nullptr);
    EXPECT_EQ(index_am_get("pg_hash"), nullptr);
}

TEST_F(AmApiTest, NullParams) {
    EXPECT_EQ(index_am_register(NULL, NULL), -1);
    EXPECT_EQ(index_am_get(NULL), nullptr);
}

}  /* namespace */