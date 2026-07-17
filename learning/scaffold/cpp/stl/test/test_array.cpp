/*
 * test_array.cpp - array 容器测试
 */

#include "mystl_test.h"
#include "mystl/mystl.h"
#include <stdexcept>

void test_default_construct() {
    MYSTL_TEST_BEGIN("array default construct");

    mystl::array<int, 5> arr;
    // 默认构造：元素值初始化为 0
    EXPECT_EQ(arr.size(), 5u);
    EXPECT_EQ(arr.max_size(), 5u);
    EXPECT_FALSE(arr.empty());

    MYSTL_TEST_END();
}

void test_aggregate_init() {
    MYSTL_TEST_BEGIN("array aggregate init");

    mystl::array<int, 5> arr = {1, 2, 3, 4, 5};
    EXPECT_EQ(arr.size(), 5u);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[4], 5);

    // 部分初始化，剩余元素值初始化
    mystl::array<int, 5> arr2 = {10, 20};
    EXPECT_EQ(arr2[0], 10);
    EXPECT_EQ(arr2[1], 20);
    EXPECT_EQ(arr2[2], 0);  // 值初始化

    MYSTL_TEST_END();
}

void test_at_bounds_check() {
    MYSTL_TEST_BEGIN("array at with bounds check");

    mystl::array<int, 5> arr = {1, 2, 3, 4, 5};

    // 正常访问
    EXPECT_EQ(arr.at(0), 1);
    EXPECT_EQ(arr.at(4), 5);

    // const 版本
    const mystl::array<int, 5>& carr = arr;
    EXPECT_EQ(carr.at(2), 3);

    // 越界访问抛出异常
    bool threw = false;
    try {
        arr.at(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    // 越界访问（负数索引，转为大正数）
    threw = false;
    try {
        arr.at(static_cast<size_t>(-1));
    } catch (const std::out_of_range&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    MYSTL_TEST_END();
}

void test_operator_bracket() {
    MYSTL_TEST_BEGIN("array operator[]");

    mystl::array<int, 5> arr = {10, 20, 30, 40, 50};

    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[2], 30);
    EXPECT_EQ(arr[4], 50);

    // 修改元素
    arr[1] = 25;
    EXPECT_EQ(arr[1], 25);

    // const 版本
    const mystl::array<int, 5>& carr = arr;
    EXPECT_EQ(carr[1], 25);

    MYSTL_TEST_END();
}

void test_front_back() {
    MYSTL_TEST_BEGIN("array front/back");

    mystl::array<int, 5> arr = {100, 200, 300, 400, 500};

    EXPECT_EQ(arr.front(), 100);
    EXPECT_EQ(arr.back(), 500);

    // 修改
    arr.front() = 111;
    arr.back() = 555;
    EXPECT_EQ(arr.front(), 111);
    EXPECT_EQ(arr.back(), 555);

    // const 版本
    const mystl::array<int, 5>& carr = arr;
    EXPECT_EQ(carr.front(), 111);
    EXPECT_EQ(carr.back(), 555);

    MYSTL_TEST_END();
}

void test_data() {
    MYSTL_TEST_BEGIN("array data");

    mystl::array<int, 5> arr = {1, 2, 3, 4, 5};

    int* ptr = arr.data();
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[4], 5);

    // const 版本
    const mystl::array<int, 5>& carr = arr;
    const int* cptr = carr.data();
    EXPECT_TRUE(cptr != nullptr);
    EXPECT_EQ(cptr[2], 3);

    MYSTL_TEST_END();
}

void test_size_empty() {
    MYSTL_TEST_BEGIN("array size/empty");

    mystl::array<int, 5> arr5;
    EXPECT_EQ(arr5.size(), 5u);
    EXPECT_EQ(arr5.max_size(), 5u);
    EXPECT_FALSE(arr5.empty());

    // N=0 的情况
    mystl::array<int, 0> arr0;
    EXPECT_EQ(arr0.size(), 0u);
    EXPECT_TRUE(arr0.empty());

    MYSTL_TEST_END();
}

void test_iterators() {
    MYSTL_TEST_BEGIN("array iterators");

    mystl::array<int, 5> arr = {1, 2, 3, 4, 5};

    // begin/end
    auto it = arr.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);

    int sum = 0;
    for (auto i = arr.begin(); i != arr.end(); ++i) {
        sum += *i;
    }
    EXPECT_EQ(sum, 15);

    // cbegin/cend
    sum = 0;
    for (auto i = arr.cbegin(); i != arr.cend(); ++i) {
        sum += *i;
    }
    EXPECT_EQ(sum, 15);

    // rbegin/rend
    mystl::array<int, 5>::reverse_iterator rit = arr.rbegin();
    EXPECT_EQ(*rit, 5);
    ++rit;
    EXPECT_EQ(*rit, 4);

    sum = 0;
    for (auto i = arr.rbegin(); i != arr.rend(); ++i) {
        sum += *i;
    }
    EXPECT_EQ(sum, 15);

    // crbegin/crend
    sum = 0;
    for (auto i = arr.crbegin(); i != arr.crend(); ++i) {
        sum += *i;
    }
    EXPECT_EQ(sum, 15);

    MYSTL_TEST_END();
}

void test_const_iterators() {
    MYSTL_TEST_BEGIN("array const iterators");

    const mystl::array<int, 5> arr = {10, 20, 30, 40, 50};

    // begin/end on const
    auto it = arr.begin();
    EXPECT_EQ(*it, 10);

    // cbegin/cend
    int sum = 0;
    for (auto i = arr.cbegin(); i != arr.cend(); ++i) {
        sum += *i;
    }
    EXPECT_EQ(sum, 150);

    MYSTL_TEST_END();
}

void test_fill() {
    MYSTL_TEST_BEGIN("array fill");

    mystl::array<int, 5> arr;
    arr.fill(42);

    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], 42);
    }

    MYSTL_TEST_END();
}

void test_swap() {
    MYSTL_TEST_BEGIN("array swap");

    mystl::array<int, 5> a = {1, 2, 3, 4, 5};
    mystl::array<int, 5> b = {10, 20, 30, 40, 50};

    a.swap(b);

    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[4], 50);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[4], 5);

    // 非成员 swap
    mystl::swap(a, b);
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(b[0], 10);

    MYSTL_TEST_END();
}

void test_comparison() {
    MYSTL_TEST_BEGIN("array comparison");

    mystl::array<int, 5> a = {1, 2, 3, 4, 5};
    mystl::array<int, 5> b = {1, 2, 3, 4, 5};
    mystl::array<int, 5> c = {1, 2, 3, 4, 6};
    mystl::array<int, 5> d = {1, 2, 3, 4, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);

    // 字典序比较
    EXPECT_TRUE(a < c);   // 5 < 6
    EXPECT_FALSE(a < d);  // 5 > 4
    EXPECT_TRUE(d < a);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(c > a);

    MYSTL_TEST_END();
}

void test_zero_size_array() {
    MYSTL_TEST_BEGIN("array zero size");

    mystl::array<int, 0> arr;
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0u);
    EXPECT_EQ(arr.begin(), arr.end());
    EXPECT_EQ(arr.data(), nullptr);

    MYSTL_TEST_END();
}

int main() {
    test_default_construct();
    test_aggregate_init();
    test_at_bounds_check();
    test_operator_bracket();
    test_front_back();
    test_data();
    test_size_empty();
    test_iterators();
    test_const_iterators();
    test_fill();
    test_swap();
    test_comparison();
    test_zero_size_array();

    MYSTL_TEST_SUMMARY();

    return mystl_test::stats().failed > 0 ? 1 : 0;
}
