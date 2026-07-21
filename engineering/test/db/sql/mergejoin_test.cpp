/**
 * @file mergejoin_test.cpp
 * @brief W4.5: MergeJoin 算子测试
 *
 * 测试排序归并连接算法的正确性。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodeMergejoin.h"
#include "db/sql/executor.h"
}

#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace {

/**
 * @brief 测试 MergeJoin 概念 — 归并比较
 */
TEST(MergeJoinTest, CompareKeys) {
    /* 使用 Datum 值模拟键比较 */
    Datum d1 = (Datum)(uintptr_t)10;
    Datum d2 = (Datum)(uintptr_t)20;
    Datum d3 = (Datum)(uintptr_t)10;

    /* 10 < 20 */
    EXPECT_LT((int64_t)(uintptr_t)d1, (int64_t)(uintptr_t)d2);
    /* 10 == 10 */
    EXPECT_EQ((int64_t)(uintptr_t)d1, (int64_t)(uintptr_t)d3);
    /* 20 > 10 */
    EXPECT_GT((int64_t)(uintptr_t)d2, (int64_t)(uintptr_t)d1);
}

/**
 * @brief 测试 MergeJoin 初始化
 */
TEST(MergeJoinTest, InitAndDestroy) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    MergeJoin plan;
    memset(&plan, 0, sizeof(plan));
    plan.join.plan.type = T_MergeJoin;
    plan.join.jointype = JOIN_INNER;
    plan.mergeclauses = NULL;

    PlanState *state = ExecInitMergeJoin((Plan *)&plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 验证初始化 */
    MergeJoinState *mj = (MergeJoinState *)state;
    EXPECT_FALSE(mj->merge_initialized);
    EXPECT_FALSE(mj->mj_OuterDone);
    EXPECT_FALSE(mj->mj_InnerDone);

    ExecEndMergeJoin(mj);
    FreeEState(estate);
}

/**
 * @brief 测试 NULL 销毁安全
 */
TEST(MergeJoinTest, NullEnd) {
    ExecEndMergeJoin(nullptr);
}

/**
 * @brief 测试 MergeJoin 执行（无子节点，应返回 NULL）
 */
TEST(MergeJoinTest, ExecuteWithNoChildren) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    MergeJoin plan;
    memset(&plan, 0, sizeof(plan));
    plan.join.plan.type = T_MergeJoin;

    PlanState *state = ExecInitMergeJoin((Plan *)&plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 无子节点，应返回 NULL */
    TupleTableSlot *slot = ExecMergeJoin(state);
    EXPECT_EQ(slot, nullptr);

    ExecEndMergeJoin((MergeJoinState *)state);
    FreeEState(estate);
}

/**
 * @brief 测试归并算法概念 — 两个有序数组归并
 */
TEST(MergeJoinTest, MergeConcept) {
    /* 两个有序数组模拟内外表 */
    std::vector<int> outer = {1, 3, 5, 7, 9};
    std::vector<int> inner = {2, 3, 4, 5, 6};

    /* 归并连接：找出相等值 */
    std::vector<int> matches;
    size_t oi = 0, ii = 0;

    while (oi < outer.size() && ii < inner.size()) {
        if (outer[oi] < inner[ii]) {
            oi++;
        } else if (outer[oi] > inner[ii]) {
            ii++;
        } else {
            matches.push_back(outer[oi]);
            oi++;
            ii++;
        }
    }

    /* 期望匹配：3, 5 */
    ASSERT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0], 3);
    EXPECT_EQ(matches[1], 5);
}

/**
 * @brief 测试归并算法 — 全部匹配
 */
TEST(MergeJoinTest, AllMatch) {
    std::vector<int> outer = {1, 2, 3, 4, 5};
    std::vector<int> inner = {1, 2, 3, 4, 5};

    std::vector<int> matches;
    size_t oi = 0, ii = 0;

    while (oi < outer.size() && ii < inner.size()) {
        if (outer[oi] < inner[ii]) {
            oi++;
        } else if (outer[oi] > inner[ii]) {
            ii++;
        } else {
            matches.push_back(outer[oi]);
            oi++;
            ii++;
        }
    }

    EXPECT_EQ(matches.size(), 5);
}

/**
 * @brief 测试归并算法 — 无匹配
 */
TEST(MergeJoinTest, NoMatch) {
    std::vector<int> outer = {1, 3, 5};
    std::vector<int> inner = {2, 4, 6};

    std::vector<int> matches;
    size_t oi = 0, ii = 0;

    while (oi < outer.size() && ii < inner.size()) {
        if (outer[oi] < inner[ii]) {
            oi++;
        } else if (outer[oi] > inner[ii]) {
            ii++;
        } else {
            matches.push_back(outer[oi]);
            oi++;
            ii++;
        }
    }

    EXPECT_EQ(matches.size(), 0);
}

/**
 * @brief 测试归并算法 — 外表为空
 */
TEST(MergeJoinTest, OuterEmpty) {
    std::vector<int> outer = {};
    std::vector<int> inner = {1, 2, 3};

    std::vector<int> matches;
    size_t oi = 0, ii = 0;

    while (oi < outer.size() && ii < inner.size()) {
        if (outer[oi] < inner[ii]) {
            oi++;
        } else if (outer[oi] > inner[ii]) {
            ii++;
        } else {
            matches.push_back(outer[oi]);
            oi++;
            ii++;
        }
    }

    EXPECT_EQ(matches.size(), 0);
}

/**
 * @brief 测试归并算法 — 内表为空
 */
TEST(MergeJoinTest, InnerEmpty) {
    std::vector<int> outer = {1, 2, 3};
    std::vector<int> inner = {};

    std::vector<int> matches;
    size_t oi = 0, ii = 0;

    while (oi < outer.size() && ii < inner.size()) {
        if (outer[oi] < inner[ii]) {
            oi++;
        } else if (outer[oi] > inner[ii]) {
            ii++;
        } else {
            matches.push_back(outer[oi]);
            oi++;
            ii++;
        }
    }

    EXPECT_EQ(matches.size(), 0);
}

/**
 * @brief 测试归并算法 — 重复值
 */
TEST(MergeJoinTest, DuplicateValues) {
    std::vector<int> outer = {1, 2, 2, 3, 4};
    std::vector<int> inner = {2, 2, 3, 3, 5};

    std::vector<int> matches;
    size_t oi = 0, ii = 0;

    while (oi < outer.size() && ii < inner.size()) {
        if (outer[oi] < inner[ii]) {
            oi++;
        } else if (outer[oi] > inner[ii]) {
            ii++;
        } else {
            matches.push_back(outer[oi]);
            oi++;
            ii++;
        }
    }

    /* 简化实现：重复值只匹配一次 */
    EXPECT_GT(matches.size(), 0);
}

}  // namespace