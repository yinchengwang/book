/*
 * test_unordered_map.cpp - unordered_map 全面测试
 *
 * 覆盖：构造/插入/operator[]/查找/擦除/桶接口/迭代器/比较运算符/swap
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

using namespace mystl;

void test_unordered_map() {
    MYSTL_TEST_BEGIN("umap_default_construct");
    {
        unordered_map<int, int> um;
        EXPECT_TRUE(um.empty());
        EXPECT_EQ(um.size(), 0u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_insert");
    {
        unordered_map<int, int> um;
        auto r = um.insert({3, 30});
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first->first, 3);
        EXPECT_EQ(r.first->second, 30);

        r = um.insert({1, 10});
        EXPECT_TRUE(r.second);

        r = um.insert({5, 50});
        EXPECT_TRUE(r.second);

        // 重复插入
        r = um.insert({3, 300});
        EXPECT_FALSE(r.second);  // key 已存在，插入失败
        EXPECT_EQ(r.first->second, 30);  // 值未被修改
        EXPECT_EQ(um.size(), 3u);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_operator_bracket");
    {
        unordered_map<int, int> um;

        // 写入
        um[1] = 10;
        um[2] = 20;
        um[3] = 30;
        EXPECT_EQ(um.size(), 3u);

        // 读取
        EXPECT_EQ(um[1], 10);
        EXPECT_EQ(um[2], 20);
        EXPECT_EQ(um[3], 30);

        // 不存在的 key 会创建默认值
        int val = um[99];
        EXPECT_EQ(val, 0);  // int 默认值
        EXPECT_EQ(um.size(), 4u);

        // 修改已存在的值
        um[1] = 100;
        EXPECT_EQ(um[1], 100);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_find");
    {
        unordered_map<int, int> um;
        um[10] = 100; um[20] = 200; um[30] = 300;

        auto it = um.find(20);
        EXPECT_TRUE(it != um.end());
        EXPECT_EQ(it->first, 20);
        EXPECT_EQ(it->second, 200);

        EXPECT_TRUE(um.find(99) == um.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_erase");
    {
        unordered_map<int, int> um;
        um[1] = 10; um[2] = 20; um[3] = 30;

        EXPECT_EQ(um.erase(2), 1u);
        EXPECT_EQ(um.size(), 2u);
        EXPECT_TRUE(um.find(2) == um.end());

        EXPECT_EQ(um.erase(99), 0u);  // 不存在的 key
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_bucket_interface");
    {
        unordered_map<int, int> um;
        um[1] = 10; um[2] = 20; um[3] = 30;

        EXPECT_TRUE(um.bucket_count() > 0);
        EXPECT_TRUE(um.load_factor() > 0.0f);
        EXPECT_TRUE(um.max_load_factor() > 0.0f);
        EXPECT_TRUE(um.bucket(2) < um.bucket_count());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_iterators");
    {
        unordered_map<int, int> um;
        um[1] = 10; um[2] = 20; um[3] = 30;

        int count = 0;
        for (auto it = um.begin(); it != um.end(); ++it) {
            ++count;
            EXPECT_TRUE(it->first >= 1 && it->first <= 3);
        }
        EXPECT_EQ(count, 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_clear");
    {
        unordered_map<int, int> um;
        um[1] = 10; um[2] = 20;
        um.clear();
        EXPECT_TRUE(um.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_rehash");
    {
        unordered_map<int, int> um;
        for (int i = 0; i < 100; ++i) um[i] = i * 10;
        EXPECT_EQ(um.size(), 100u);

        // rehash 后仍然能查到所有元素
        for (int i = 0; i < 100; ++i) {
            auto it = um.find(i);
            EXPECT_TRUE(it != um.end());
            EXPECT_EQ(it->second, i * 10);
        }
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_swap");
    {
        unordered_map<int, int> a, b;
        a[1] = 10; a[2] = 20;
        b[3] = 30;

        a.swap(b);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_TRUE(a.find(3) != a.end());
        EXPECT_EQ(b.size(), 2u);
        EXPECT_TRUE(b.find(1) != b.end());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("umap_initializer_list");
    {
        unordered_map<int, int> um = {{3, 30}, {1, 10}, {4, 40}, {1, 100}};
        EXPECT_EQ(um.size(), 3u);  // 去重
        EXPECT_EQ(um[1], 10);  // 第一个值
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== unordered_map 全面测试 ===\n");
    test_unordered_map();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}
