/*
 * test_queue.cpp - queue 全面测试
 *
 * 覆盖：所有构造 / push / pop / front / back / emplace / swap / 比较运算符
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_queue() {
    MYSTL_TEST_BEGIN("queue_default_construct");
    {
        queue<int> q;
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(q.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("queue_push_front_back_pop");
    {
        queue<int> q;
        q.push(1);
        q.push(2);
        q.push(3);
        EXPECT_EQ(q.size(), 3u);
        EXPECT_FALSE(q.empty());
        EXPECT_EQ(q.front(), 1);
        EXPECT_EQ(q.back(), 3);
        q.pop();
        EXPECT_EQ(q.front(), 2);
        EXPECT_EQ(q.back(), 3);
        q.pop();
        EXPECT_EQ(q.front(), 3);
        q.pop();
        EXPECT_TRUE(q.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("queue_emplace");
    {
        queue<pair<int, int>> q;
        q.emplace(1, 2);
        q.emplace(3, 4);
        EXPECT_EQ(q.size(), 2u);
        EXPECT_EQ(q.front().first, 1);
        EXPECT_EQ(q.back().second, 4);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("queue_copy");
    {
        queue<int> q1;
        q1.push(10);
        q1.push(20);
        queue<int> q2(q1);
        EXPECT_EQ(q2.size(), 2u);
        EXPECT_EQ(q2.front(), 10);
        EXPECT_EQ(q2.back(), 20);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("queue_assign");
    {
        queue<int> q1;
        q1.push(1);
        queue<int> q2;
        q2 = q1;
        EXPECT_EQ(q2.front(), 1);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("queue_comparison");
    {
        queue<int> a, b, c;
        a.push(1); a.push(2); a.push(3);
        b.push(1); b.push(2); b.push(3);
        c.push(1); c.push(2); c.push(4);
        EXPECT_TRUE(a == b);
        EXPECT_FALSE(a != b);
        EXPECT_TRUE(a < c);
        EXPECT_TRUE(a <= b);
        EXPECT_TRUE(c > a);
        EXPECT_TRUE(c >= b);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("queue_swap");
    {
        queue<int> a, b;
        a.push(1); a.push(2);
        b.push(3);
        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(a.front(), 3);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b.front(), 1);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== queue 全面测试 ===\n");
    test_queue();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}