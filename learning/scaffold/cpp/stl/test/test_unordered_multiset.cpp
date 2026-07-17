/*
 * test_unordered_multiset.cpp - unordered_multiset 全面测试
 *
 * 覆盖：构造/插入/查找/count/擦除/桶接口/迭代器/clear/swap
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

using namespace mystl;

void test_unordered_multiset() {
    MYSTL_TEST_BEGIN("ums_default_construct");
    {
        unordered_multiset<int> ums;
        EXPECT_TRUE(ums.empty());
        EXPECT_EQ(ums.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_insert");
    {
        unordered_multiset<int> ums;
        auto it = ums.insert(3);
        EXPECT_EQ(*it, 3);

        it = ums.insert(1);
        EXPECT_EQ(*it, 1);

        it = ums.insert(5);
        EXPECT_EQ(*it, 5);

        // 重复插入允许
        it = ums.insert(3);
        EXPECT_EQ(*it, 3);
        EXPECT_EQ(ums.size(), 4u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_find");
    {
        unordered_multiset<int> ums;
        ums.insert(10); ums.insert(20); ums.insert(30); ums.insert(20);

        auto it = ums.find(20);
        EXPECT_TRUE(it != ums.end());
        EXPECT_EQ(*it, 20);

        EXPECT_TRUE(ums.find(99) == ums.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_count");
    {
        unordered_multiset<int> ums;
        ums.insert(1); ums.insert(2); ums.insert(1); ums.insert(3); ums.insert(1);

        EXPECT_EQ(ums.count(1), 3u);
        EXPECT_EQ(ums.count(2), 1u);
        EXPECT_EQ(ums.count(3), 1u);
        EXPECT_EQ(ums.count(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_erase");
    {
        unordered_multiset<int> ums;
        ums.insert(1); ums.insert(2); ums.insert(1); ums.insert(3);

        EXPECT_EQ(ums.erase(1), 2u);  // 擦除所有值为 1 的元素
        EXPECT_EQ(ums.size(), 2u);
        EXPECT_EQ(ums.erase(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_bucket_interface");
    {
        unordered_multiset<int> ums;
        ums.insert(1); ums.insert(2); ums.insert(3);

        EXPECT_TRUE(ums.bucket_count() > 0);
        EXPECT_TRUE(ums.load_factor() > 0.0f);
        EXPECT_TRUE(ums.max_load_factor() > 0.0f);
        EXPECT_TRUE(ums.bucket(1) < ums.bucket_count());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_iterators");
    {
        unordered_multiset<int> ums;
        ums.insert(1); ums.insert(2); ums.insert(3);

        int count = 0;
        for (auto it = ums.begin(); it != ums.end(); ++it) ++count;
        EXPECT_EQ(count, 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_clear");
    {
        unordered_multiset<int> ums;
        ums.insert(1); ums.insert(2);
        ums.clear();
        EXPECT_TRUE(ums.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_rehash");
    {
        unordered_multiset<int> ums;
        for (int i = 0; i < 100; ++i) ums.insert(i);
        EXPECT_EQ(ums.size(), 100u);

        // rehash 后仍然能查到所有元素
        for (int i = 0; i < 100; ++i)
            EXPECT_TRUE(ums.find(i) != ums.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_swap");
    {
        unordered_multiset<int> a, b;
        a.insert(1); a.insert(2);
        b.insert(3);

        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_TRUE(a.find(3) != a.end());
        EXPECT_EQ(b.size(), 2u);
        EXPECT_TRUE(b.find(1) != b.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ums_initializer_list");
    {
        unordered_multiset<int> ums = {3, 1, 4, 1, 5};
        EXPECT_EQ(ums.size(), 5u);  // 允许重复
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== unordered_multiset 全面测试 ===\n");
    test_unordered_multiset();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}