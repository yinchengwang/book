/*
 * test_unordered_multimap.cpp - unordered_multimap 全面测试
 *
 * 覆盖：构造/插入/查找/count/擦除/桶接口/迭代器/clear/swap
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

using namespace mystl;

void test_unordered_multimap() {
    MYSTL_TEST_BEGIN("umm_default_construct");
    {
        unordered_multimap<int, int> umm;
        EXPECT_TRUE(umm.empty());
        EXPECT_EQ(umm.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_insert");
    {
        unordered_multimap<int, int> umm;
        auto it = umm.insert({3, 30});
        EXPECT_EQ(it->first, 3);
        EXPECT_EQ(it->second, 30);

        it = umm.insert({1, 10});
        EXPECT_EQ(it->first, 1);

        it = umm.insert({5, 50});
        EXPECT_EQ(it->first, 5);

        // 重复 key 允许
        it = umm.insert({3, 300});
        EXPECT_EQ(it->first, 3);
        EXPECT_EQ(it->second, 300);
        EXPECT_EQ(umm.size(), 4u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_find");
    {
        unordered_multimap<int, int> umm;
        umm.insert({10, 100}); umm.insert({20, 200}); umm.insert({20, 300});

        auto it = umm.find(20);
        EXPECT_TRUE(it != umm.end());
        EXPECT_EQ(it->first, 20);
        // find 返回第一个匹配的元素
        EXPECT_TRUE(it->second == 200 || it->second == 300);

        EXPECT_TRUE(umm.find(99) == umm.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_count");
    {
        unordered_multimap<int, int> umm;
        umm.insert({1, 10}); umm.insert({2, 20}); umm.insert({1, 100}); umm.insert({3, 30}); umm.insert({1, 1000});

        EXPECT_EQ(umm.count(1), 3u);
        EXPECT_EQ(umm.count(2), 1u);
        EXPECT_EQ(umm.count(3), 1u);
        EXPECT_EQ(umm.count(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_erase");
    {
        unordered_multimap<int, int> umm;
        umm.insert({1, 10}); umm.insert({2, 20}); umm.insert({1, 100}); umm.insert({3, 30});

        EXPECT_EQ(umm.erase(1), 2u);  // 擦除所有 key=1 的元素
        EXPECT_EQ(umm.size(), 2u);
        EXPECT_EQ(umm.erase(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_bucket_interface");
    {
        unordered_multimap<int, int> umm;
        umm.insert({1, 10}); umm.insert({2, 20}); umm.insert({3, 30});

        EXPECT_TRUE(umm.bucket_count() > 0);
        EXPECT_TRUE(umm.load_factor() > 0.0f);
        EXPECT_TRUE(umm.max_load_factor() > 0.0f);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_iterators");
    {
        unordered_multimap<int, int> umm;
        umm.insert({1, 10}); umm.insert({2, 20}); umm.insert({3, 30});

        int count = 0;
        for (auto it = umm.begin(); it != umm.end(); ++it) {
            ++count;
            EXPECT_TRUE(it->first >= 1 && it->first <= 3);
        }
        EXPECT_EQ(count, 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_clear");
    {
        unordered_multimap<int, int> umm;
        umm.insert({1, 10}); umm.insert({2, 20});
        umm.clear();
        EXPECT_TRUE(umm.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umm_swap");
    {
        unordered_multimap<int, int> a, b;
        a.insert({1, 10}); a.insert({2, 20});
        b.insert({3, 30});

        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_TRUE(a.find(3) != a.end());
        EXPECT_EQ(b.size(), 2u);
        EXPECT_TRUE(b.find(1) != b.end());
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== unordered_multimap 全面测试 ===\n");
    test_unordered_multimap();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}
