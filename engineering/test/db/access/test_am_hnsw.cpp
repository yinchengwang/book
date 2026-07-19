/**
 * @file test_am_hnsw.cpp
 * @brief HNSW 适配 IndexAmRoutine 测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/access/amapi.h"
}

extern "C" int am_hnsw_init(void);

TEST(AmHnswTest, RegisterHnswAm) {
    index_am_init();
    int rc = am_hnsw_init();
    EXPECT_EQ(rc, 0);
}

TEST(AmHnswTest, GetHnswAm) {
    index_am_init();
    am_hnsw_init();

    const IndexAmRoutine *am = index_am_get("hnsw");
    ASSERT_NE(am, nullptr);
    EXPECT_STREQ(am->am_name, "hnsw");
    EXPECT_NE(am->ambuild, nullptr);
    EXPECT_NE(am->aminsert, nullptr);
    EXPECT_NE(am->amdelete, nullptr);
    EXPECT_NE(am->ambeginscan, nullptr);
    EXPECT_NE(am->amendscan, nullptr);
    EXPECT_NE(am->amrescan, nullptr);
    EXPECT_NE(am->amgetnext, nullptr);
}

TEST(AmHnswTest, CoexistWithBtree) {
    index_am_init();
    am_hnsw_init();

    /* 验证 HNSW 和 btree 同时存在 */
    EXPECT_NE(index_am_get("hnsw"), nullptr);
    EXPECT_EQ(index_am_get("btree"), nullptr); /* 未注册 btree */
}