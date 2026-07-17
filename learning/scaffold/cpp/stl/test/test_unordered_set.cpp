/*
 * test_unordered_set.cpp - unordered_set 全面测试
 *
 * 覆盖：构造/插入/查找/擦除/桶接口/迭代器/比较运算符/swap
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

using namespace mystl;

void test_unordered_set() {
    MYSTL_TEST_BEGIN("uset_default_construct");
    {
        unordered_set<int> us;
        EXPECT_TRUE(us.empty());
        EXPECT_EQ(us.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_insert");
    {
        unordered_set<int> us;
        auto r = us.insert(3);
        EXPECT_TRUE(r.second);
        EXPECT_EQ(*r.first, 3);
        r = us.insert(1);
        EXPECT_TRUE(r.second);
        r = us.insert(5);
        EXPECT_TRUE(r.second);
        // 重复插入
        r = us.insert(3);
        EXPECT_FALSE(r.second);
        EXPECT_EQ(us.size(), 3u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_find");
    {
        unordered_set<int> us;
        us.insert(10); us.insert(20); us.insert(30);
        auto it = us.find(20);
        EXPECT_TRUE(it != us.end());
        EXPECT_EQ(*it, 20);
        EXPECT_TRUE(us.find(99) == us.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_erase");
    {
        unordered_set<int> us;
        us.insert(1); us.insert(2); us.insert(3);
        EXPECT_EQ(us.erase(2), 1u);
        EXPECT_EQ(us.size(), 2u);
        EXPECT_EQ(us.erase(99), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_bucket_interface");
    {
        unordered_set<int> us;
        us.insert(1); us.insert(2); us.insert(3);
        EXPECT_TRUE(us.bucket_count() > 0);
        EXPECT_TRUE(us.load_factor() > 0.0f);
        EXPECT_TRUE(us.max_load_factor() > 0.0f);
        EXPECT_TRUE(us.bucket(1) < us.bucket_count());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_iterators");
    {
        unordered_set<int> us;
        us.insert(1); us.insert(2); us.insert(3);
        int count = 0;
        for (auto it = us.begin(); it != us.end(); ++it) ++count;
        EXPECT_EQ(count, 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_clear");
    {
        unordered_set<int> us;
        us.insert(1); us.insert(2);
        us.clear();
        EXPECT_TRUE(us.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_rehash");
    {
        unordered_set<int> us;
        for (int i = 0; i < 100; ++i) us.insert(i);
        EXPECT_EQ(us.size(), 100u);
        // rehash 后仍然能查到所有元素
        for (int i = 0; i < 100; ++i)
            EXPECT_TRUE(us.find(i) != us.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_swap");
    {
        unordered_set<int> a, b;
        a.insert(1); a.insert(2);
        b.insert(3);
        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_TRUE(a.find(3) != a.end());
        EXPECT_EQ(b.size(), 2u);
        EXPECT_TRUE(b.find(1) != b.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("uset_initializer_list");
    {
        unordered_set<int> us = {3, 1, 4, 1, 5};
        EXPECT_EQ(us.size(), 4u);  // 去重
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== unordered_set 全面测试 ===\n");
    test_unordered_set();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}