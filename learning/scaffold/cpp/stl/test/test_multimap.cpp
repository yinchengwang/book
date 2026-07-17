/*
 * test_multimap.cpp - multimap 全面测试
 *
 * 覆盖：insert / count / erase / find / equal_range / 迭代器
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_multimap() {
    MYSTL_TEST_BEGIN("multimap_insert");
    {
        multimap<int, int> mm;
        mm.insert({1, 10});
        mm.insert({1, 20});
        mm.insert({1, 30});
        mm.insert({2, 200});
        EXPECT_EQ(mm.size(), 4u);
        EXPECT_EQ(mm.count(1), 3u);
        EXPECT_EQ(mm.count(2), 1u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_range_construct");
    {
        pair<int, int> arr[] = {{1, 10}, {1, 20}, {2, 20}, {1, 30}};
        multimap<int, int> mm(arr, arr + 4);
        EXPECT_EQ(mm.size(), 4u);
        EXPECT_EQ(mm.count(1), 3u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_erase_key");
    {
        multimap<int, int> mm;
        mm.insert({1, 10}); mm.insert({1, 20}); mm.insert({2, 30});
        EXPECT_EQ(mm.erase(1), 2u);
        EXPECT_EQ(mm.size(), 1u);
        EXPECT_EQ(mm.erase(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_erase_iterator");
    {
        multimap<int, int> mm;
        mm.insert({1, 10}); mm.insert({1, 20}); mm.insert({2, 30});
        mm.erase(mm.begin());
        EXPECT_EQ(mm.size(), 2u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_find");
    {
        multimap<int, int> mm;
        mm.insert({1, 10}); mm.insert({1, 20}); mm.insert({2, 30});
        auto it = mm.find(2);
        EXPECT_TRUE(it != mm.end());
        EXPECT_EQ(it->second, 30);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_equal_range");
    {
        multimap<int, int> mm;
        mm.insert({1, 10}); mm.insert({1, 20}); mm.insert({1, 30}); mm.insert({2, 40});
        auto r = mm.equal_range(1);
        int count = 0;
        int sum = 0;
        for (auto it = r.first; it != r.second; ++it) {
            EXPECT_EQ(it->first, 1);
            sum += it->second;
            ++count;
        }
        EXPECT_EQ(count, 3);
        EXPECT_EQ(sum, 10 + 20 + 30);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_iterators");
    {
        multimap<int, int> mm;
        mm.insert({2, 20}); mm.insert({1, 10}); mm.insert({1, 5});
        // 按键有序
        int prev_key = 0;
        for (auto it = mm.begin(); it != mm.end(); ++it) {
            EXPECT_TRUE(it->first >= prev_key);
            prev_key = it->first;
        }
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("multimap_clear");
    {
        multimap<int, int> mm;
        mm.insert({1, 1}); mm.insert({1, 2});
        mm.clear();
        EXPECT_TRUE(mm.empty());
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== multimap 全面测试 ===\n");
    test_multimap();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}