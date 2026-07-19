/**
 * @file test_btbuild_empty.cpp
 * @brief btbuild 空表建索引测试（Task 2.19）
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/btreeam.h"
#include "db/rel.h"
}

/* 测试用最小 Relation（只需非 NULL 即可测试 btbuild 的空集合分支） */
typedef struct FakeRelation {
    Oid rd_id;
} FakeRelation;

TEST(BtbuildEmptyTest, NullTuplesReturnsZero) {
    FakeRelation rel;
    int rc = btbuild((Relation)&rel, NULL, 0);
    EXPECT_EQ(rc, 0);  /* ntuples=0 且 tuples=NULL 应返回 0 */
}

TEST(BtbuildEmptyTest, ZeroNtuplesReturnsZero) {
    FakeRelation rel;
    int rc = btbuild((Relation)&rel, NULL, 5);
    EXPECT_EQ(rc, 0);  /* 即使 tuples 非空，ntuples=0 也应返回 0 */
}

TEST(BtbuildEmptyTest, NullRelReturnsMinusOne) {
    int rc = btbuild(NULL, NULL, 0);
    EXPECT_EQ(rc, -1);  /* NULL relation 应返回错误 */
}

TEST(BtbuildEmptyTest, NegativeNtuplesReturnsZero) {
    FakeRelation rel;
    int rc = btbuild((Relation)&rel, NULL, -1);
    EXPECT_EQ(rc, 0);  /* 负数视为空集合 */
}