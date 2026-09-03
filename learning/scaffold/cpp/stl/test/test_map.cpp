/*
 * test_map.cpp - map 全面测试
 *
 * 覆盖：所有构造 / operator[] / insert / erase / find / at / 迭代器 / swap
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>
#include <cstring>

using namespace mystl;

void test_map() {
    MYSTL_TEST_BEGIN("map_default_construct");
    {
        map<int, int> m;
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(m.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_operator_square");
    {
        map<int, int> m;
        m[1] = 10;
        m[2] = 20;
        m[3] = 30;
        EXPECT_EQ(m.size(), 3u);
        EXPECT_EQ(m[1], 10);
        EXPECT_EQ(m[2], 20);
        EXPECT_EQ(m[3], 30);
        // operator[] 插入默认值
        EXPECT_EQ(m[99], 0);  // 不存在时插入默认值
        EXPECT_EQ(m.size(), 4u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_insert");
    {
        map<int, int> m;
        auto r = m.insert({1, 10});
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first->first, 1);
        EXPECT_EQ(r.first->second, 10);
        // 重复 key
        r = m.insert({1, 20});
        EXPECT_FALSE(r.second);
        EXPECT_EQ(r.first->second, 10);  // 未改变
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_range_construct");
    {
        pair<int, int> arr[] = {{3, 30}, {1, 10}, {2, 20}, {1, 100}};
        map<int, int> m(arr, arr + 4);
        EXPECT_EQ(m.size(), 3u);  // 去重
        EXPECT_EQ(m[1], 10);     // 第一个插入的
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_at");
    {
        map<int, int> m;
        m[1] = 100;
        EXPECT_EQ(m.at(1), 100);
        // at 不存在应该抛异常
        bool threw = false;
        try {
            m.at(999);
        } catch (...) {
            threw = true;
        }
        EXPECT_TRUE(threw);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_erase");
    {
        map<int, int> m;
        m[1] = 10; m[2] = 20; m[3] = 30; m[4] = 40;
        // 按 key
        EXPECT_EQ(m.erase(2), 1u);
        EXPECT_EQ(m.size(), 3u);
        EXPECT_EQ(m.erase(99), 0u);
        // 按迭代器
        m.erase(m.begin());
        EXPECT_EQ(m.size(), 2u);
        // 范围
        m.erase(m.begin(), m.end());
        EXPECT_TRUE(m.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_find");
    {
        map<int, int> m;
        m[1] = 10; m[2] = 20; m[3] = 30;
        auto it = m.find(2);
        EXPECT_TRUE(it != m.end());
        EXPECT_EQ(it->first, 2);
        EXPECT_EQ(it->second, 20);
        EXPECT_TRUE(m.find(99) == m.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_count");
    {
        map<int, int> m;
        m[1] = 10;
        EXPECT_EQ(m.count(1), 1u);
        EXPECT_EQ(m.count(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_lower_upper_bound");
    {
        map<int, int> m;
        m[10] = 100; m[20] = 200; m[30] = 300;
        auto lo = m.lower_bound(20);
        EXPECT_EQ(lo->first, 20);
        auto up = m.upper_bound(20);
        EXPECT_EQ(up->first, 30);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_iterators");
    {
        map<int, int> m;
        m[3] = 30; m[1] = 10; m[2] = 20;
        // 按键有序
        int prev = 0;
        for (auto it = m.begin(); it != m.end(); ++it) {
            EXPECT_TRUE(it->first > prev);
            prev = it->first;
        }
        // 反向
        auto rit = m.rbegin();
        EXPECT_EQ(rit->first, 3);
        EXPECT_EQ((++rit)->first, 2);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_clear");
    {
        map<int, int> m;
        m[1] = 10; m[2] = 20;
        m.clear();
        EXPECT_TRUE(m.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_swap");
    {
        map<int, int> a, b;
        a[1] = 10; a[2] = 20;
        b[3] = 30;
        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(a[3], 30);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b[1], 10);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_comparison");
    {
        map<int, int> a, b, c;
        a[1] = 10; a[2] = 20;
        b[1] = 10; b[2] = 20;
        c[1] = 10; c[3] = 30;
        EXPECT_TRUE(a == b);
        EXPECT_TRUE(a < c);
        EXPECT_TRUE(c > a);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("map_string_key");
    {
        map<const char*, int> m;
        m["hello"] = 1;
        m["world"] = 2;
        EXPECT_EQ(m["hello"], 1);
        EXPECT_EQ(m["world"], 2);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== map 全面测试 ===\n");
    test_map();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}