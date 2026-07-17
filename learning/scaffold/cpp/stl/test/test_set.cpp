/*
 * test_set.cpp - set 全面测试
 *
 * 覆盖：所有构造 / insert / erase / find / count / lower_bound / upper_bound
 *      / equal_range / 迭代器 / 比较运算符 / swap
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_set() {
    MYSTL_TEST_BEGIN("set_default_construct");
    {
        set<int> s;
        EXPECT_TRUE(s.empty());
        EXPECT_EQ(s.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_insert_unique");
    {
        set<int> s;
        auto r = s.insert(3);
        EXPECT_TRUE(r.second);
        EXPECT_EQ(*r.first, 3);
        r = s.insert(1);
        EXPECT_TRUE(r.second);
        r = s.insert(5);
        EXPECT_TRUE(r.second);
        // 重复插入
        r = s.insert(3);
        EXPECT_FALSE(r.second);
        EXPECT_EQ(s.size(), 3u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_range_construct");
    {
        int arr[] = {3, 1, 4, 1, 5, 9};
        set<int> s(arr, arr + 6);
        EXPECT_EQ(s.size(), 5u);  // 去重
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_copy_construct");
    {
        set<int> s1;
        s1.insert(1); s1.insert(2); s1.insert(3);
        set<int> s2(s1);
        EXPECT_EQ(s2.size(), 3u);
        EXPECT_TRUE(s2.find(2) != s2.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_erase");
    {
        set<int> s;
        s.insert(1); s.insert(2); s.insert(3); s.insert(4); s.insert(5);
        // 按 key 删除
        EXPECT_EQ(s.erase(3), 1u);
        EXPECT_EQ(s.size(), 4u);
        EXPECT_EQ(s.erase(10), 0u);  // 不存在
        // 按迭代器删除
        s.erase(s.begin());
        EXPECT_EQ(s.size(), 3u);
        // 范围删除
        s.erase(s.begin(), s.end());
        EXPECT_TRUE(s.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_find");
    {
        set<int> s;
        s.insert(10); s.insert(20); s.insert(30);
        auto it = s.find(20);
        EXPECT_TRUE(it != s.end());
        EXPECT_EQ(*it, 20);
        EXPECT_TRUE(s.find(99) == s.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_count");
    {
        set<int> s;
        s.insert(1); s.insert(2); s.insert(3);
        EXPECT_EQ(s.count(1), 1u);
        EXPECT_EQ(s.count(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_lower_upper_bound");
    {
        set<int> s;
        s.insert(10); s.insert(20); s.insert(30); s.insert(40);
        auto lo = s.lower_bound(20);
        EXPECT_EQ(*lo, 20);
        auto up = s.upper_bound(20);
        EXPECT_EQ(*up, 30);
        lo = s.lower_bound(25);
        EXPECT_EQ(*lo, 30);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_equal_range");
    {
        set<int> s;
        s.insert(10); s.insert(20); s.insert(30);
        auto r = s.equal_range(20);
        EXPECT_EQ(*r.first, 20);
        EXPECT_EQ(*r.second, 30);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_iterators");
    {
        set<int> s;
        s.insert(3); s.insert(1); s.insert(4); s.insert(1); s.insert(5);
        // 有序遍历
        int prev = 0;
        int count = 0;
        for (auto it = s.begin(); it != s.end(); ++it) {
            EXPECT_TRUE(*it > prev);
            prev = *it;
            ++count;
        }
        EXPECT_EQ(count, 4);  // 1,3,4,5
        // 反向迭代
        count = 0;
        prev = 100;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            if (count > 0) EXPECT_TRUE(*it < prev);
            prev = *it;
            ++count;
        }
        EXPECT_EQ(count, 4);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_clear");
    {
        set<int> s;
        s.insert(1); s.insert(2);
        s.clear();
        EXPECT_TRUE(s.empty());
        EXPECT_EQ(s.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_swap");
    {
        set<int> a, b;
        a.insert(1); a.insert(2);
        b.insert(3);
        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_TRUE(a.find(3) != a.end());
        EXPECT_EQ(b.size(), 2u);
        EXPECT_TRUE(b.find(1) != b.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("set_comparison");
    {
        set<int> a, b, c;
        a.insert(1); a.insert(2);
        b.insert(1); b.insert(2);
        c.insert(1); c.insert(3);
        EXPECT_TRUE(a == b);
        EXPECT_FALSE(a != b);
        EXPECT_TRUE(a < c);
        EXPECT_TRUE(c > a);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== set 全面测试 ===\n");
    test_set();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}