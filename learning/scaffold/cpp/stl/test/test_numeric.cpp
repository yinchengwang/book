/*
 * test_numeric.cpp - numeric 数值算法库全面测试
 *
 * 覆盖：accumulate/inner_product/partial_sum/adjacent_difference/iota
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

using namespace mystl;

// ============================================================
// accumulate 测试
// ============================================================

void test_accumulate_basic() {
    MYSTL_TEST_BEGIN("accumulate basic");

    int arr[] = {1, 2, 3, 4, 5};
    int sum = mystl::accumulate(arr, arr + 5, 0);
    EXPECT_EQ(sum, 15);

    MYSTL_TEST_END();
}

void test_accumulate_with_init() {
    MYSTL_TEST_BEGIN("accumulate with init");

    int arr[] = {1, 2, 3};
    int sum = mystl::accumulate(arr, arr + 3, 10);
    EXPECT_EQ(sum, 16);  // 10 + 1 + 2 + 3

    MYSTL_TEST_END();
}

void test_accumulate_with_op() {
    MYSTL_TEST_BEGIN("accumulate with operation");

    int arr[] = {1, 2, 3, 4};
    int product = mystl::accumulate(arr, arr + 4, 1, [](int a, int b) { return a * b; });
    EXPECT_EQ(product, 24);  // 1 * 1 * 2 * 3 * 4

    MYSTL_TEST_END();
}

void test_accumulate_empty() {
    MYSTL_TEST_BEGIN("accumulate empty");

    int arr[1];
    int sum = mystl::accumulate(arr, arr, 42);
    EXPECT_EQ(sum, 42);  // 仅返回初始值

    MYSTL_TEST_END();
}

// ============================================================
// inner_product 测试
// ============================================================

void test_inner_product_basic() {
    MYSTL_TEST_BEGIN("inner_product basic");

    int a[] = {1, 2, 3};
    int b[] = {4, 5, 6};
    int dot = mystl::inner_product(a, a + 3, b, 0);
    EXPECT_EQ(dot, 32);  // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32

    MYSTL_TEST_END();
}

void test_inner_product_with_ops() {
    MYSTL_TEST_BEGIN("inner_product with operations");

    int a[] = {1, 2, 3};
    int b[] = {1, 2, 3};
    // 自定义：加法用乘法，乘法用加法
    // (1+1) * (2+2) * (3+3) = 2 * 4 * 6 = 48
    int result = mystl::inner_product(a, a + 3, b, 1,
        [](int x, int y) { return x * y; },  // op1: "加法"
        [](int x, int y) { return x + y; }); // op2: "乘法"
    EXPECT_EQ(result, 48);

    MYSTL_TEST_END();
}

// ============================================================
// partial_sum 测试
// ============================================================

void test_partial_sum_basic() {
    MYSTL_TEST_BEGIN("partial_sum basic");

    int arr[] = {1, 2, 3, 4, 5};
    int result[5];
    mystl::partial_sum(arr, arr + 5, result);

    EXPECT_EQ(result[0], 1);   // 1
    EXPECT_EQ(result[1], 3);   // 1+2
    EXPECT_EQ(result[2], 6);   // 1+2+3
    EXPECT_EQ(result[3], 10);  // 1+2+3+4
    EXPECT_EQ(result[4], 15);  // 1+2+3+4+5

    MYSTL_TEST_END();
}

void test_partial_sum_with_op() {
    MYSTL_TEST_BEGIN("partial_sum with operation");

    int arr[] = {2, 3, 4};
    int result[3];
    // 乘法部分和：2, 2*3=6, 6*4=24
    mystl::partial_sum(arr, arr + 3, result, [](int a, int b) { return a * b; });

    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 6);
    EXPECT_EQ(result[2], 24);

    MYSTL_TEST_END();
}

// ============================================================
// adjacent_difference 测试
// ============================================================

void test_adjacent_difference_basic() {
    MYSTL_TEST_BEGIN("adjacent_difference basic");

    int arr[] = {1, 3, 6, 10, 15};
    int result[5];
    mystl::adjacent_difference(arr, arr + 5, result);

    EXPECT_EQ(result[0], 1);   // 第一个元素不变
    EXPECT_EQ(result[1], 2);   // 3 - 1
    EXPECT_EQ(result[2], 3);   // 6 - 3
    EXPECT_EQ(result[3], 4);   // 10 - 6
    EXPECT_EQ(result[4], 5);   // 15 - 10

    MYSTL_TEST_END();
}

void test_adjacent_difference_with_op() {
    MYSTL_TEST_BEGIN("adjacent_difference with operation");

    int arr[] = {8, 4, 2, 1};
    int result[4];
    // 用除法代替减法
    // result[0] = 8 (第一个元素不变)
    // result[1] = 4 / 8 = 0 (整数除法)
    // result[2] = 2 / 4 = 0
    // result[3] = 1 / 2 = 0
    mystl::adjacent_difference(arr, arr + 4, result, [](int a, int b) { return a / b; });

    EXPECT_EQ(result[0], 8);
    EXPECT_EQ(result[1], 0);  // 4 / 8 = 0 (整数除法)
    EXPECT_EQ(result[2], 0);  // 2 / 4 = 0
    EXPECT_EQ(result[3], 0);  // 1 / 2 = 0

    MYSTL_TEST_END();
}

// ============================================================
// iota 测试
// ============================================================

void test_iota_basic() {
    MYSTL_TEST_BEGIN("iota basic");

    int arr[5];
    mystl::iota(arr, arr + 5, 1);

    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);
    EXPECT_EQ(arr[4], 5);

    MYSTL_TEST_END();
}

void test_iota_from_zero() {
    MYSTL_TEST_BEGIN("iota from zero");

    int arr[4];
    mystl::iota(arr, arr + 4, 0);

    EXPECT_EQ(arr[0], 0);
    EXPECT_EQ(arr[1], 1);
    EXPECT_EQ(arr[2], 2);
    EXPECT_EQ(arr[3], 3);

    MYSTL_TEST_END();
}

void test_iota_negative() {
    MYSTL_TEST_BEGIN("iota negative start");

    int arr[5];
    mystl::iota(arr, arr + 5, -2);

    EXPECT_EQ(arr[0], -2);
    EXPECT_EQ(arr[1], -1);
    EXPECT_EQ(arr[2], 0);
    EXPECT_EQ(arr[3], 1);
    EXPECT_EQ(arr[4], 2);

    MYSTL_TEST_END();
}

// ============================================================
// 主函数
// ============================================================

int main() {
    printf("=== numeric 数值算法库全面测试 ===\n");

    // accumulate
    test_accumulate_basic();
    test_accumulate_with_init();
    test_accumulate_with_op();
    test_accumulate_empty();

    // inner_product
    test_inner_product_basic();
    test_inner_product_with_ops();

    // partial_sum
    test_partial_sum_basic();
    test_partial_sum_with_op();

    // adjacent_difference
    test_adjacent_difference_basic();
    test_adjacent_difference_with_op();

    // iota
    test_iota_basic();
    test_iota_from_zero();
    test_iota_negative();

    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}