/*
 * test_stack.cpp - stack 全面测试
 *
 * 覆盖：所有构造 / push / pop / top / emplace / swap / 比较运算符
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_stack() {
    MYSTL_TEST_BEGIN("stack_default_construct");
    {
        stack<int> s;
        EXPECT_TRUE(s.empty());
        EXPECT_EQ(s.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("stack_push_top_pop");
    {
        stack<int> s;
        s.push(1);
        s.push(2);
        s.push(3);
        EXPECT_EQ(s.size(), 3u);
        EXPECT_FALSE(s.empty());
        EXPECT_EQ(s.top(), 3);
        s.pop();
        EXPECT_EQ(s.top(), 2);
        s.pop();
        EXPECT_EQ(s.top(), 1);
        s.pop();
        EXPECT_TRUE(s.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("stack_emplace");
    {
        stack<pair<int, int>> s;
        s.emplace(1, 2);
        s.emplace(3, 4);
        EXPECT_EQ(s.size(), 2u);
        EXPECT_EQ(s.top().first, 3);
        EXPECT_EQ(s.top().second, 4);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("stack_copy");
    {
        stack<int> s1;
        s1.push(10);
        s1.push(20);
        stack<int> s2(s1);
        EXPECT_EQ(s2.size(), 2u);
        EXPECT_EQ(s2.top(), 20);
        s2.pop();
        EXPECT_EQ(s2.top(), 10);
        // 原栈不受影响
        EXPECT_EQ(s1.top(), 20);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("stack_assign");
    {
        stack<int> s1;
        s1.push(1);
        stack<int> s2;
        s2 = s1;
        EXPECT_EQ(s2.top(), 1);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("stack_comparison");
    {
        stack<int> a, b, c;
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

    MYSTL_TEST_BEGIN("stack_container_adapt");
    {
        // 使用 vector 作为底层容器
        stack<int, vector<int>> s;
        s.push(1);
        s.push(2);
        EXPECT_EQ(s.top(), 2);
        s.pop();
        EXPECT_EQ(s.top(), 1);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("stack_swap");
    {
        stack<int> a, b;
        a.push(1); a.push(2);
        b.push(3);
        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(a.top(), 3);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b.top(), 2);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== stack 全面测试 ===\n");
    test_stack();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}