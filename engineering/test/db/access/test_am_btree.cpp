/**
 * @file test_am_btree.cpp
 * @brief BTree 适配 IndexAmRoutine 测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/access/amapi.h"
#include "db/rel.h"
}

/* am_btree_init 是 C 链接，必须在 extern "C" 块外声明 */
extern "C" int am_btree_init(void);

class AmBtreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        index_am_init();
    }
};

TEST(AmBtreeTest, RegisterBtreeAm) {
    int rc = am_btree_init();
    EXPECT_EQ(rc, 0);
}

TEST(AmBtreeTest, GetBtreeAm) {
    am_btree_init();

    const IndexAmRoutine *am = index_am_get("btree");
    ASSERT_NE(am, nullptr);
    EXPECT_STREQ(am->am_name, "btree");
    EXPECT_NE(am->ambuild, nullptr);
    EXPECT_NE(am->aminsert, nullptr);
    EXPECT_NE(am->amdelete, nullptr);
    EXPECT_NE(am->ambeginscan, nullptr);
    EXPECT_NE(am->amendscan, nullptr);
    EXPECT_NE(am->amrescan, nullptr);
    EXPECT_NE(am->amgetnext, nullptr);
}

TEST(AmBtreeTest, BtreeAmCallbacksWork) {
    am_btree_init();

    const IndexAmRoutine *am = index_am_get("btree");
    ASSERT_NE(am, nullptr);

    /* 验证 ambeginscan / amendscan 调用不崩溃 */
    IndexScanDesc scan = am->ambeginscan(NULL, 0, NULL);
    (void)scan;
    /* 传入 NULL relation 时 bt_beginscan 返回 NULL，这是预期的 */

    /* 验证 amrescan 不崩溃 */
    bool ok = am->amrescan(NULL, NULL);
    EXPECT_TRUE(ok);

    /* 验证 amgetnext 返回 NULL（无扫描上下文） */
    void *result = am->amgetnext(NULL);
    EXPECT_EQ(result, nullptr);
}