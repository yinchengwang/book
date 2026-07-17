/*
 * test_priority_queue.cpp - priority_queue 全面测试
 *
 * 覆盖：所有构造 / push / pop / top / emplace / swap
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_priority_queue() {
    MYSTL_TEST_BEGIN("pq_default_construct");
    {
        priority_queue<int> pq;
        EXPECT_TRUE(pq.empty());
        EXPECT_EQ(pq.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pq_push_top_pop");
    {
        priority_queue<int> pq;
        pq.push(3);
        pq.push(1);
        pq.push(4);
        pq.push(1);
        pq.push(5);
        EXPECT_EQ(pq.size(), 5u);
        EXPECT_FALSE(pq.empty());
        EXPECT_EQ(pq.top(), 5);  // 最大堆
        pq.pop();
        EXPECT_EQ(pq.top(), 4);
        pq.pop();
        EXPECT_EQ(pq.top(), 3);
        pq.pop();
        EXPECT_EQ(pq.top(), 1);
        pq.pop();
        EXPECT_EQ(pq.top(), 1);
        pq.pop();
        EXPECT_TRUE(pq.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pq_emplace");
    {
        priority_queue<pair<int, int>> pq;
        pq.emplace(3, 30);
        pq.emplace(1, 10);
        pq.emplace(5, 50);
        EXPECT_EQ(pq.top().first, 5);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pq_greater_min_heap");
    {
        // 最小堆
        priority_queue<int, vector<int>, greater<int>> pq;
        pq.push(3);
        pq.push(1);
        pq.push(4);
        EXPECT_EQ(pq.top(), 1);
        pq.pop();
        EXPECT_EQ(pq.top(), 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pq_range_construct");
    {
        int arr[] = {5, 1, 3, 4, 2};
        priority_queue<int> pq(arr, arr + 5);
        EXPECT_EQ(pq.size(), 5u);
        EXPECT_EQ(pq.top(), 5);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pq_copy");
    {
        priority_queue<int> pq1;
        pq1.push(3); pq1.push(1); pq1.push(4);
        priority_queue<int> pq2(pq1);
        EXPECT_EQ(pq2.size(), 3u);
        EXPECT_EQ(pq2.top(), 4);  // 最大堆 top=4
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pq_swap");
    {
        priority_queue<int> a, b;
        a.push(1); a.push(2); a.push(3);
        b.push(4); b.push(5);
        a.swap(b);
        EXPECT_EQ(a.size(), 2u);
        EXPECT_EQ(a.top(), 5);
        EXPECT_EQ(b.size(), 3u);
        EXPECT_EQ(b.top(), 3);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== priority_queue 全面测试 ===\n");
    test_priority_queue();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}