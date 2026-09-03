/*
 * test_deque.cpp - deque 容器测试
 */

#include "mystl_test.h"
#include "mystl/mystl.h"
#include <stdexcept>

void test_default_construct() {
    MYSTL_TEST_BEGIN("deque default construct");

    mystl::deque<int> d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);

    MYSTL_TEST_END();
}

void test_size_value_construct() {
    MYSTL_TEST_BEGIN("deque size/value construct");

    mystl::deque<int> d(5, 42);
    EXPECT_EQ(d.size(), 5u);

    for (size_t i = 0; i < d.size(); ++i) {
        EXPECT_EQ(d[i], 42);
    }

    // 仅 size 构造
    mystl::deque<int> d2(3);
    EXPECT_EQ(d2.size(), 3u);
    for (size_t i = 0; i < d2.size(); ++i) {
        EXPECT_EQ(d2[i], 0);
    }

    MYSTL_TEST_END();
}

void test_initializer_list() {
    MYSTL_TEST_BEGIN("deque initializer list");

    mystl::deque<int> d = {1, 2, 3, 4, 5};
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[4], 5);

    MYSTL_TEST_END();
}

void test_range_construct() {
    MYSTL_TEST_BEGIN("deque range construct");

    int arr[] = {10, 20, 30, 40, 50};
    mystl::deque<int> d(arr, arr + 5);

    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[4], 50);

    MYSTL_TEST_END();
}

void test_push_front_back() {
    MYSTL_TEST_BEGIN("deque push_front/push_back");

    mystl::deque<int> d;

    d.push_back(1);
    d.push_back(2);
    d.push_front(0);

    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d.front(), 0);
    EXPECT_EQ(d.back(), 2);

    d.push_front(-1);
    EXPECT_EQ(d.front(), -1);

    d.push_back(3);
    EXPECT_EQ(d.back(), 3);

    // 移动版本
    int x = 100;
    d.push_front(mystl::move(x));
    EXPECT_EQ(d.front(), 100);

    MYSTL_TEST_END();
}

void test_pop_front_back() {
    MYSTL_TEST_BEGIN("deque pop_front/pop_back");

    mystl::deque<int> d = {1, 2, 3, 4, 5};

    d.pop_front();
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d.front(), 2);

    d.pop_back();
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d.back(), 4);

    d.pop_front();
    d.pop_back();
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(d.front(), 3);
    EXPECT_EQ(d.back(), 3);

    MYSTL_TEST_END();
}

void test_operator_bracket() {
    MYSTL_TEST_BEGIN("deque operator[]");

    mystl::deque<int> d = {10, 20, 30, 40, 50};

    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[4], 50);

    d[2] = 35;
    EXPECT_EQ(d[2], 35);

    // const 版本
    const mystl::deque<int>& cd = d;
    EXPECT_EQ(cd[1], 20);

    MYSTL_TEST_END();
}

void test_at_bounds_check() {
    MYSTL_TEST_BEGIN("deque at with bounds check");

    mystl::deque<int> d = {1, 2, 3, 4, 5};

    EXPECT_EQ(d.at(0), 1);
    EXPECT_EQ(d.at(4), 5);

    bool threw = false;
    try {
        d.at(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    // const 版本
    const mystl::deque<int>& cd = d;
    EXPECT_EQ(cd.at(2), 3);

    MYSTL_TEST_END();
}

void test_front_back() {
    MYSTL_TEST_BEGIN("deque front/back");

    mystl::deque<int> d = {100, 200, 300, 400, 500};

    EXPECT_EQ(d.front(), 100);
    EXPECT_EQ(d.back(), 500);

    d.front() = 110;
    d.back() = 510;
    EXPECT_EQ(d.front(), 110);
    EXPECT_EQ(d.back(), 510);

    // const 版本
    const mystl::deque<int>& cd = d;
    EXPECT_EQ(cd.front(), 110);
    EXPECT_EQ(cd.back(), 510);

    MYSTL_TEST_END();
}

void test_size_empty_max() {
    MYSTL_TEST_BEGIN("deque size/empty/max_size");

    mystl::deque<int> d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(d.max_size() > 0);

    d.push_back(1);
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 1u);

    MYSTL_TEST_END();
}

void test_resize() {
    MYSTL_TEST_BEGIN("deque resize");

    mystl::deque<int> d = {1, 2, 3};

    d.resize(5, 42);
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[3], 42);
    EXPECT_EQ(d[4], 42);

    d.resize(2);
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);

    d.resize(4);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[2], 0);
    EXPECT_EQ(d[3], 0);

    MYSTL_TEST_END();
}

void test_insert_single() {
    MYSTL_TEST_BEGIN("deque insert single");

    mystl::deque<int> d = {1, 2, 3, 4, 5};

    auto it = d.insert(d.begin(), 0);
    EXPECT_EQ(d.size(), 6u);
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(d[0], 0);

    it = d.insert(d.end(), 6);
    EXPECT_EQ(d.size(), 7u);
    EXPECT_EQ(d.back(), 6);

    it = d.insert(d.begin() + 3, 100);
    EXPECT_EQ(d.size(), 8u);
    EXPECT_EQ(d[3], 100);

    MYSTL_TEST_END();
}

void test_erase_single() {
    MYSTL_TEST_BEGIN("deque erase single");

    mystl::deque<int> d = {1, 2, 3, 4, 5};

    auto it = d.erase(d.begin());
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(*it, 2);

    it = d.erase(d.begin() + 2);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(*it, 5);

    MYSTL_TEST_END();
}

void test_erase_range() {
    MYSTL_TEST_BEGIN("deque erase range");

    mystl::deque<int> d = {1, 2, 3, 4, 5, 6, 7};

    auto it = d.erase(d.begin() + 2, d.begin() + 5);
    EXPECT_EQ(d.size(), 4u);

    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 6);
    EXPECT_EQ(d[3], 7);

    MYSTL_TEST_END();
}

void test_clear() {
    MYSTL_TEST_BEGIN("deque clear");

    mystl::deque<int> d = {1, 2, 3, 4, 5};
    d.clear();

    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);

    MYSTL_TEST_END();
}

void test_iterators() {
    MYSTL_TEST_BEGIN("deque iterators");

    mystl::deque<int> d = {1, 2, 3, 4, 5};

    // 正向迭代
    int sum = 0;
    for (auto it = d.begin(); it != d.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // cbegin/cend
    sum = 0;
    for (auto it = d.cbegin(); it != d.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // 反向迭代
    sum = 0;
    for (auto it = d.rbegin(); it != d.rend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    MYSTL_TEST_END();
}

void test_swap() {
    MYSTL_TEST_BEGIN("deque swap");

    mystl::deque<int> a = {1, 2, 3};
    mystl::deque<int> b = {10, 20, 30, 40};

    a.swap(b);

    EXPECT_EQ(a.size(), 4u);
    EXPECT_EQ(a.front(), 10);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b.front(), 1);

    // 非成员 swap
    mystl::swap(a, b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(b.size(), 4u);

    MYSTL_TEST_END();
}

void test_comparison() {
    MYSTL_TEST_BEGIN("deque comparison");

    mystl::deque<int> a = {1, 2, 3};
    mystl::deque<int> b = {1, 2, 3};
    mystl::deque<int> c = {1, 2, 3, 4};
    mystl::deque<int> d = {1, 2, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);

    EXPECT_TRUE(a < c);
    EXPECT_TRUE(a < d);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(c > a);

    MYSTL_TEST_END();
}

void test_copy_move() {
    MYSTL_TEST_BEGIN("deque copy/move");

    mystl::deque<int> d1 = {1, 2, 3};

    // 拷贝构造
    mystl::deque<int> d2(d1);
    EXPECT_EQ(d2.size(), 3u);
    EXPECT_EQ(d1.size(), 3u);

    // 移动构造
    mystl::deque<int> d3(mystl::move(d1));
    EXPECT_EQ(d3.size(), 3u);
    EXPECT_TRUE(d1.empty());

    // 拷贝赋值
    mystl::deque<int> d4;
    d4 = d2;
    EXPECT_EQ(d4.size(), 3u);

    // 移动赋值
    mystl::deque<int> d5;
    d5 = mystl::move(d3);
    EXPECT_EQ(d5.size(), 3u);
    EXPECT_TRUE(d3.empty());

    MYSTL_TEST_END();
}

void test_assign() {
    MYSTL_TEST_BEGIN("deque assign");

    mystl::deque<int> d;

    d.assign(5, 42);
    EXPECT_EQ(d.size(), 5u);
    for (size_t i = 0; i < d.size(); ++i) {
        EXPECT_EQ(d[i], 42);
    }

    int arr[] = {1, 2, 3, 4};
    d.assign(arr, arr + 4);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d.front(), 1);

    MYSTL_TEST_END();
}

void test_empty_single() {
    MYSTL_TEST_BEGIN("deque empty/single");

    // 空容器
    mystl::deque<int> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.begin(), empty.end());

    // 单元素
    mystl::deque<int> single = {42};
    EXPECT_EQ(single.size(), 1u);
    EXPECT_EQ(single.front(), 42);
    EXPECT_EQ(single.back(), 42);

    MYSTL_TEST_END();
}

void test_multi_block() {
    MYSTL_TEST_BEGIN("deque multi block (>16 elements)");

    // deque block size for int = max(16, 512/sizeof(int)) = 128 (on most systems)
    // 测试跨多个 block 的场景，添加足够多的元素
    mystl::deque<int> d;

    // 添加 200 个元素，跨越多个 block
    for (int i = 0; i < 200; ++i) {
        d.push_back(i);
    }

    EXPECT_EQ(d.size(), 200u);

    // 验证所有元素
    for (int i = 0; i < 200; ++i) {
        EXPECT_EQ(d[i], i);
    }

    // 从两端删除
    for (int i = 0; i < 50; ++i) {
        d.pop_front();
        d.pop_back();
    }

    EXPECT_EQ(d.size(), 100u);
    EXPECT_EQ(d.front(), 50);
    EXPECT_EQ(d.back(), 149);

    MYSTL_TEST_END();
}

void test_emplace_front_back() {
    MYSTL_TEST_BEGIN("deque emplace_front/emplace_back");

    mystl::deque<int> d;

    d.emplace_back(42);
    EXPECT_EQ(d.back(), 42);

    d.emplace_front(10);
    EXPECT_EQ(d.front(), 10);

    d.emplace_back(100);
    d.emplace_front(0);

    EXPECT_EQ(d.front(), 0);
    EXPECT_EQ(d.back(), 100);

    MYSTL_TEST_END();
}

int main() {
    test_default_construct();
    test_size_value_construct();
    test_initializer_list();
    test_range_construct();
    test_push_front_back();
    test_pop_front_back();
    test_operator_bracket();
    test_at_bounds_check();
    test_front_back();
    test_size_empty_max();
    test_resize();
    test_insert_single();
    test_erase_single();
    test_erase_range();
    test_clear();
    test_iterators();
    test_swap();
    test_comparison();
    test_copy_move();
    test_assign();
    test_empty_single();
    test_multi_block();
    test_emplace_front_back();

    MYSTL_TEST_SUMMARY();

    return mystl_test::stats().failed > 0 ? 1 : 0;
}