/*
 * test_vector.cpp - vector 容器测试
 */

#include "mystl_test.h"
#include "mystl/mystl.h"
#include <stdexcept>

void test_default_construct() {
    MYSTL_TEST_BEGIN("vector default construct");

    mystl::vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);

    MYSTL_TEST_END();
}

void test_size_construct() {
    MYSTL_TEST_BEGIN("vector size construct");

    mystl::vector<int> v(5);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_TRUE(v.capacity() >= 5u);

    // 值初始化为 0
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 0);
    }

    MYSTL_TEST_END();
}

void test_value_size_construct() {
    MYSTL_TEST_BEGIN("vector value+size construct");

    mystl::vector<int> v(5, 42);
    EXPECT_EQ(v.size(), 5u);

    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 42);
    }

    MYSTL_TEST_END();
}

void test_initializer_list() {
    MYSTL_TEST_BEGIN("vector initializer list");

    mystl::vector<int> v = {1, 2, 3, 4, 5};
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[4], 5);

    MYSTL_TEST_END();
}

void test_range_construct() {
    MYSTL_TEST_BEGIN("vector range construct");

    int arr[] = {10, 20, 30, 40, 50};
    mystl::vector<int> v(arr, arr + 5);

    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[4], 50);

    // 从 array 构建
    mystl::array<int, 3> a = {1, 2, 3};
    mystl::vector<int> v2(a.begin(), a.end());
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[1], 2);

    MYSTL_TEST_END();
}

void test_copy_construct() {
    MYSTL_TEST_BEGIN("vector copy construct");

    mystl::vector<int> v1 = {1, 2, 3, 4, 5};
    mystl::vector<int> v2(v1);

    EXPECT_EQ(v2.size(), v1.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        EXPECT_EQ(v2[i], v1[i]);
    }

    // 修改 v1 不影响 v2
    v1[0] = 100;
    EXPECT_EQ(v2[0], 1);

    MYSTL_TEST_END();
}

void test_move_construct() {
    MYSTL_TEST_BEGIN("vector move construct");

    mystl::vector<int> v1 = {1, 2, 3, 4, 5};
    size_t old_size = v1.size();

    mystl::vector<int> v2(mystl::move(v1));

    EXPECT_EQ(v2.size(), old_size);
    EXPECT_EQ(v2[0], 1);
    EXPECT_EQ(v1.size(), 0u);
    EXPECT_TRUE(v1.empty());

    MYSTL_TEST_END();
}

void test_copy_assign() {
    MYSTL_TEST_BEGIN("vector copy assign");

    mystl::vector<int> v1 = {1, 2, 3};
    mystl::vector<int> v2;

    v2 = v1;
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[0], 1);

    // 自赋值
    v1 = v1;
    EXPECT_EQ(v1.size(), 3u);

    MYSTL_TEST_END();
}

void test_move_assign() {
    MYSTL_TEST_BEGIN("vector move assign");

    mystl::vector<int> v1 = {1, 2, 3, 4, 5};
    mystl::vector<int> v2;

    v2 = mystl::move(v1);

    EXPECT_EQ(v2.size(), 5u);
    EXPECT_EQ(v1.size(), 0u);

    MYSTL_TEST_END();
}

void test_push_back() {
    MYSTL_TEST_BEGIN("vector push_back");

    mystl::vector<int> v;

    for (int i = 0; i < 10; ++i) {
        v.push_back(i);
    }

    EXPECT_EQ(v.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(v[i], i);
    }

    // 移动版本
    int x = 100;
    v.push_back(mystl::move(x));
    EXPECT_EQ(v.back(), 100);

    MYSTL_TEST_END();
}

void test_pop_back() {
    MYSTL_TEST_BEGIN("vector pop_back");

    mystl::vector<int> v = {1, 2, 3, 4, 5};

    v.pop_back();
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v.back(), 4);

    v.pop_back();
    v.pop_back();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v.back(), 2);

    MYSTL_TEST_END();
}

void test_element_access() {
    MYSTL_TEST_BEGIN("vector element access");

    mystl::vector<int> v = {10, 20, 30, 40, 50};

    // operator[]
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[4], 50);
    v[2] = 35;
    EXPECT_EQ(v[2], 35);

    // at()
    EXPECT_EQ(v.at(0), 10);
    EXPECT_EQ(v.at(4), 50);

    bool threw = false;
    try {
        v.at(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    // front/back
    EXPECT_EQ(v.front(), 10);
    EXPECT_EQ(v.back(), 50);
    v.front() = 15;
    v.back() = 55;
    EXPECT_EQ(v.front(), 15);
    EXPECT_EQ(v.back(), 55);

    // data()
    int* ptr = v.data();
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr[0], 15);

    // const 版本
    const mystl::vector<int>& cv = v;
    EXPECT_EQ(cv.front(), 15);
    EXPECT_EQ(cv.back(), 55);
    EXPECT_EQ(cv.at(2), 35);

    MYSTL_TEST_END();
}

void test_capacity() {
    MYSTL_TEST_BEGIN("vector capacity");

    mystl::vector<int> v;
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);
    EXPECT_TRUE(v.empty());

    v.reserve(100);
    EXPECT_TRUE(v.capacity() >= 100u);
    EXPECT_EQ(v.size(), 0u);

    v.push_back(1);
    v.push_back(2);
    EXPECT_EQ(v.size(), 2u);

    v.shrink_to_fit();
    EXPECT_TRUE(v.capacity() >= v.size());

    // max_size
    EXPECT_TRUE(v.max_size() > 0);

    MYSTL_TEST_END();
}

void test_resize() {
    MYSTL_TEST_BEGIN("vector resize");

    mystl::vector<int> v = {1, 2, 3};

    // 扩展
    v.resize(5, 42);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[3], 42);
    EXPECT_EQ(v[4], 42);

    // 收缩
    v.resize(2);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);

    // 默认值初始化
    v.resize(4);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[2], 0);  // 值初始化
    EXPECT_EQ(v[3], 0);

    MYSTL_TEST_END();
}

void test_insert_single() {
    MYSTL_TEST_BEGIN("vector insert single");

    mystl::vector<int> v = {1, 2, 3, 4, 5};

    // 中间插入
    auto it = v.insert(v.begin() + 2, 100);
    EXPECT_EQ(v.size(), 6u);
    EXPECT_EQ(*it, 100);
    EXPECT_EQ(v[2], 100);
    EXPECT_EQ(v[3], 3);

    // 头部插入
    it = v.insert(v.begin(), 0);
    EXPECT_EQ(v.size(), 7u);
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(v[0], 0);

    // 尾部插入
    it = v.insert(v.end(), 200);
    EXPECT_EQ(v.size(), 8u);
    EXPECT_EQ(v.back(), 200);

    MYSTL_TEST_END();
}

void test_insert_multiple() {
    MYSTL_TEST_BEGIN("vector insert multiple");

    mystl::vector<int> v = {1, 2, 3};

    v.insert(v.begin() + 1, 3, 42);
    EXPECT_EQ(v.size(), 6u);
    EXPECT_EQ(v[1], 42);
    EXPECT_EQ(v[2], 42);
    EXPECT_EQ(v[3], 42);
    EXPECT_EQ(v[4], 2);

    MYSTL_TEST_END();
}

void test_insert_range() {
    MYSTL_TEST_BEGIN("vector insert range");

    mystl::vector<int> v = {1, 5};
    int arr[] = {2, 3, 4};

    mystl::vector<int>::iterator pos = v.begin() + 1;
    v.insert(pos, arr, arr + 3);
    EXPECT_EQ(v.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(v[i], i + 1);
    }

    MYSTL_TEST_END();
}

void test_erase_single() {
    MYSTL_TEST_BEGIN("vector erase single");

    mystl::vector<int> v = {1, 2, 3, 4, 5};

    // 中间删除
    auto it = v.erase(v.begin() + 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(v[2], 4);

    // 头部删除
    it = v.erase(v.begin());
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(*it, 2);

    // 尾部删除
    it = v.erase(v.end() - 1);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(it, v.end());

    MYSTL_TEST_END();
}

void test_erase_range() {
    MYSTL_TEST_BEGIN("vector erase range");

    mystl::vector<int> v = {1, 2, 3, 4, 5, 6, 7};

    auto it = v.erase(v.begin() + 2, v.begin() + 5);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 6);
    EXPECT_EQ(v[3], 7);

    MYSTL_TEST_END();
}

void test_clear() {
    MYSTL_TEST_BEGIN("vector clear");

    mystl::vector<int> v = {1, 2, 3, 4, 5};
    v.clear();

    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);

    MYSTL_TEST_END();
}

void test_iterators() {
    MYSTL_TEST_BEGIN("vector iterators");

    mystl::vector<int> v = {1, 2, 3, 4, 5};

    // 正向迭代器
    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // cbegin/cend
    sum = 0;
    for (auto it = v.cbegin(); it != v.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // 反向迭代器
    sum = 0;
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // crbegin/crend
    sum = 0;
    for (auto it = v.crbegin(); it != v.crend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    MYSTL_TEST_END();
}

void test_swap() {
    MYSTL_TEST_BEGIN("vector swap");

    mystl::vector<int> a = {1, 2, 3};
    mystl::vector<int> b = {10, 20, 30, 40, 50};

    a.swap(b);

    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 1);

    // 非成员 swap
    mystl::swap(a, b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(b.size(), 5u);

    MYSTL_TEST_END();
}

void test_comparison() {
    MYSTL_TEST_BEGIN("vector comparison");

    mystl::vector<int> a = {1, 2, 3};
    mystl::vector<int> b = {1, 2, 3};
    mystl::vector<int> c = {1, 2, 3, 4};
    mystl::vector<int> d = {1, 2, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);

    // 字典序
    EXPECT_TRUE(a < c);   // 短
    EXPECT_TRUE(a < d);   // 3 < 4
    EXPECT_FALSE(d < a);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(d > a);

    MYSTL_TEST_END();
}

void test_emplace_back() {
    MYSTL_TEST_BEGIN("vector emplace_back");

    mystl::vector<int> v;
    v.emplace_back(42);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v.back(), 42);

    // 连续添加
    for (int i = 0; i < 10; ++i) {
        v.emplace_back(i);
    }
    EXPECT_EQ(v.size(), 11u);

    MYSTL_TEST_END();
}

void test_iterator_invalidation() {
    MYSTL_TEST_BEGIN("vector iterator invalidation");

    mystl::vector<int> v = {1, 2, 3};
    int* old_data = v.data();

    // push_back 可能导致扩容，旧指针失效
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }

    // 验证数据正确
    EXPECT_EQ(v.size(), 103u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);

    // data() 指针可能改变（扩容后）
    // 注意：不强制验证改变，因为小规模可能不扩容

    MYSTL_TEST_END();
}

void test_empty_container() {
    MYSTL_TEST_BEGIN("vector empty container");

    mystl::vector<int> v;

    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.begin(), v.end());

    // 对空容器操作
    v.clear();
    EXPECT_TRUE(v.empty());

    v.resize(0);
    EXPECT_TRUE(v.empty());

    MYSTL_TEST_END();
}

void test_single_element() {
    MYSTL_TEST_BEGIN("vector single element");

    mystl::vector<int> v;
    v.push_back(42);

    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v.front(), 42);
    EXPECT_EQ(v.back(), 42);
    EXPECT_EQ(v[0], 42);
    EXPECT_EQ(v.at(0), 42);

    v.pop_back();
    EXPECT_TRUE(v.empty());

    MYSTL_TEST_END();
}

int main() {
    test_default_construct();
    test_size_construct();
    test_value_size_construct();
    test_initializer_list();
    test_range_construct();
    test_copy_construct();
    test_move_construct();
    test_copy_assign();
    test_move_assign();
    test_push_back();
    test_pop_back();
    test_element_access();
    test_capacity();
    test_resize();
    test_insert_single();
    test_insert_multiple();
    test_insert_range();
    test_erase_single();
    test_erase_range();
    test_clear();
    test_iterators();
    test_swap();
    test_comparison();
    test_emplace_back();
    test_iterator_invalidation();
    test_empty_container();
    test_single_element();

    MYSTL_TEST_SUMMARY();

    return mystl_test::stats().failed > 0 ? 1 : 0;
}
