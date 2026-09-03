/*
 * test_multiset.cpp - multiset 全面测试
 *
 * 覆盖：insert / count / erase / find / equal_range / 迭代器
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_multiset() {
    MYSTL_TEST_BEGIN("multiset_insert");
    {
        multiset<int> ms;
        ms.insert(3);
        ms.insert(1);
        ms.insert(3);
        ms.insert(5);
        ms.insert(3);
        EXPECT_EQ(ms.size(), 5u);
        EXPECT_EQ(ms.count(3), 3u);
        EXPECT_EQ(ms.count(1), 1u);
        EXPECT_EQ(ms.count(5), 1u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_range_construct");
    {
        int arr[] = {1, 1, 2, 2, 3, 3, 3};
        multiset<int> ms(arr, arr + 7);
        EXPECT_EQ(ms.size(), 7u);
        EXPECT_EQ(ms.count(1), 2u);
        EXPECT_EQ(ms.count(3), 3u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_erase");
    {
        multiset<int> ms;
        ms.insert(1); ms.insert(1); ms.insert(2); ms.insert(3);
        // 按 key 删除（删除所有匹配）
        EXPECT_EQ(ms.erase(1), 2u);
        EXPECT_EQ(ms.size(), 2u);
        // 按迭代器删除（删除单个）
        ms.erase(ms.begin());
        EXPECT_EQ(ms.size(), 1u);
        // 范围删除
        ms.erase(ms.begin(), ms.end());
        EXPECT_TRUE(ms.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_find");
    {
        multiset<int> ms;
        ms.insert(1); ms.insert(1); ms.insert(2);
        auto it = ms.find(1);
        EXPECT_TRUE(it != ms.end());
        EXPECT_EQ(*it, 1);
        EXPECT_TRUE(ms.find(99) == ms.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_equal_range");
    {
        multiset<int> ms;
        ms.insert(1); ms.insert(1); ms.insert(2); ms.insert(1); ms.insert(3);
        auto r = ms.equal_range(1);
        int count = 0;
        for (auto it = r.first; it != r.second; ++it) {
            EXPECT_EQ(*it, 1);
            ++count;
        }
        EXPECT_EQ(count, 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_iterators");
    {
        multiset<int> ms;
        ms.insert(3); ms.insert(1); ms.insert(3); ms.insert(2);
        // 有序
        int prev = 0;
        for (auto it = ms.begin(); it != ms.end(); ++it) {
            EXPECT_TRUE(*it >= prev);
            prev = *it;
        }
        // 反向
        auto it = ms.end();
        --it;
        EXPECT_EQ(*it, 3);
        ++it;
        EXPECT_TRUE(it == ms.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_clear");
    {
        multiset<int> ms;
        ms.insert(1); ms.insert(1);
        ms.clear();
        EXPECT_TRUE(ms.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multiset_swap");
    {
        multiset<int> a, b;
        a.insert(1); a.insert(1);
        b.insert(2);
        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(a.count(2), 1u);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(b.count(1), 2u);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== multiset 全面测试 ===\n");
    test_multiset();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}