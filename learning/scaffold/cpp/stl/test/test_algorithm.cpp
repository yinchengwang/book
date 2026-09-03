/*
 * test_algorithm.cpp - algorithm 算法库全面测试
 *
 * 覆盖：查找/计数/复制/填充/变换/替换/删除/去重/排序/二分搜索/极值/堆操作
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

using namespace mystl;

// ============================================================
// 非修改序列操作
// ============================================================

void test_find() {
    MYSTL_TEST_BEGIN("find");

    int arr[] = {1, 2, 3, 4, 5};
    auto it = mystl::find(arr, arr + 5, 3);
    EXPECT_TRUE(it != arr + 5);
    EXPECT_EQ(*it, 3);

    it = mystl::find(arr, arr + 5, 99);
    EXPECT_TRUE(it == arr + 5);

    MYSTL_TEST_END();
}

void test_find_if() {
    MYSTL_TEST_BEGIN("find_if");

    int arr[] = {1, 2, 3, 4, 5};
    auto it = mystl::find_if(arr, arr + 5, [](int x) { return x > 3; });
    EXPECT_TRUE(it != arr + 5);
    EXPECT_EQ(*it, 4);

    MYSTL_TEST_END();
}

void test_count() {
    MYSTL_TEST_BEGIN("count");

    int arr[] = {1, 2, 3, 2, 2, 4};
    EXPECT_EQ(mystl::count(arr, arr + 6, 2), 3);
    EXPECT_EQ(mystl::count(arr, arr + 6, 5), 0);

    MYSTL_TEST_END();
}

void test_count_if() {
    MYSTL_TEST_BEGIN("count_if");

    int arr[] = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(mystl::count_if(arr, arr + 6, [](int x) { return x % 2 == 0; }), 3);

    MYSTL_TEST_END();
}

void test_equal() {
    MYSTL_TEST_BEGIN("equal");

    int a[] = {1, 2, 3};
    int b[] = {1, 2, 3};
    int c[] = {1, 2, 4};

    EXPECT_TRUE(mystl::equal(a, a + 3, b));
    EXPECT_FALSE(mystl::equal(a, a + 3, c));

    MYSTL_TEST_END();
}

// ============================================================
// 修改序列操作
// ============================================================

void test_copy() {
    MYSTL_TEST_BEGIN("copy");

    int src[] = {1, 2, 3, 4, 5};
    int dst[5];
    mystl::copy(src, src + 5, dst);

    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(dst[i], src[i]);

    MYSTL_TEST_END();
}

void test_fill() {
    MYSTL_TEST_BEGIN("fill");

    int arr[5];
    mystl::fill(arr, arr + 5, 42);

    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(arr[i], 42);

    MYSTL_TEST_END();
}

void test_transform() {
    MYSTL_TEST_BEGIN("transform");

    int src[] = {1, 2, 3, 4, 5};
    int dst[5];
    mystl::transform(src, src + 5, dst, [](int x) { return x * 2; });

    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(dst[i], (i + 1) * 2);

    MYSTL_TEST_END();
}

void test_replace() {
    MYSTL_TEST_BEGIN("replace");

    int arr[] = {1, 2, 3, 2, 5};
    mystl::replace(arr, arr + 5, 2, 99);

    EXPECT_EQ(arr[1], 99);
    EXPECT_EQ(arr[3], 99);

    MYSTL_TEST_END();
}

void test_reverse() {
    MYSTL_TEST_BEGIN("reverse");

    int arr[] = {1, 2, 3, 4, 5};
    mystl::reverse(arr, arr + 5);

    EXPECT_EQ(arr[0], 5);
    EXPECT_EQ(arr[1], 4);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 2);
    EXPECT_EQ(arr[4], 1);

    MYSTL_TEST_END();
}

void test_remove() {
    MYSTL_TEST_BEGIN("remove");

    int arr[] = {1, 2, 3, 2, 4, 2, 5};
    int* new_end = mystl::remove(arr, arr + 7, 2);

    // 移除后有效元素：1, 3, 4, 5
    EXPECT_EQ(new_end - arr, 4);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 3);
    EXPECT_EQ(arr[2], 4);
    EXPECT_EQ(arr[3], 5);

    MYSTL_TEST_END();
}

void test_unique() {
    MYSTL_TEST_BEGIN("unique");

    int arr[] = {1, 1, 2, 2, 2, 3, 3, 4};
    int* new_end = mystl::unique(arr, arr + 8);

    EXPECT_EQ(new_end - arr, 4);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);

    MYSTL_TEST_END();
}

// ============================================================
// 排序操作
// ============================================================

void test_is_sorted() {
    MYSTL_TEST_BEGIN("is_sorted");

    int sorted[] = {1, 2, 3, 4, 5};
    int unsorted[] = {3, 1, 4, 1, 5};

    EXPECT_TRUE(mystl::is_sorted(sorted, sorted + 5));
    EXPECT_FALSE(mystl::is_sorted(unsorted, unsorted + 5));

    MYSTL_TEST_END();
}

void test_sort() {
    MYSTL_TEST_BEGIN("sort");

    int arr[] = {5, 2, 8, 1, 9, 3, 7, 4, 6};
    mystl::sort(arr, arr + 9);

    EXPECT_TRUE(mystl::is_sorted(arr, arr + 9));
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[8], 9);

    MYSTL_TEST_END();
}

void test_sort_with_compare() {
    MYSTL_TEST_BEGIN("sort with compare");

    int arr[] = {5, 2, 8, 1, 9};
    mystl::sort(arr, arr + 5, [](int a, int b) { return a > b; });

    EXPECT_EQ(arr[0], 9);
    EXPECT_EQ(arr[4], 1);

    MYSTL_TEST_END();
}

// ============================================================
// 二分搜索
// ============================================================

void test_lower_bound() {
    MYSTL_TEST_BEGIN("lower_bound");

    int arr[] = {1, 2, 2, 2, 3, 4, 5};
    auto it = mystl::lower_bound(arr, arr + 7, 2);
    EXPECT_EQ(it - arr, 1);  // 第一个 >= 2 的位置

    it = mystl::lower_bound(arr, arr + 7, 6);
    EXPECT_TRUE(it == arr + 7);  // 未找到

    MYSTL_TEST_END();
}

void test_upper_bound() {
    MYSTL_TEST_BEGIN("upper_bound");

    int arr[] = {1, 2, 2, 2, 3, 4, 5};
    auto it = mystl::upper_bound(arr, arr + 7, 2);
    EXPECT_EQ(it - arr, 4);  // 第一个 > 2 的位置

    MYSTL_TEST_END();
}

void test_binary_search() {
    MYSTL_TEST_BEGIN("binary_search");

    int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    EXPECT_TRUE(mystl::binary_search(arr, arr + 9, 5));
    EXPECT_TRUE(mystl::binary_search(arr, arr + 9, 1));
    EXPECT_TRUE(mystl::binary_search(arr, arr + 9, 9));
    EXPECT_FALSE(mystl::binary_search(arr, arr + 9, 0));
    EXPECT_FALSE(mystl::binary_search(arr, arr + 9, 10));

    MYSTL_TEST_END();
}

// ============================================================
// 极值操作
// ============================================================

void test_min_max() {
    MYSTL_TEST_BEGIN("min/max");

    EXPECT_EQ(mystl::min(3, 5), 3);
    EXPECT_EQ(mystl::max(3, 5), 5);

    auto p = mystl::minmax(5, 3);
    EXPECT_EQ(p.first, 3);
    EXPECT_EQ(p.second, 5);

    MYSTL_TEST_END();
}

void test_minmax_element() {
    MYSTL_TEST_BEGIN("min/max_element");

    int arr[] = {5, 2, 8, 1, 9, 3};

    auto min_it = mystl::min_element(arr, arr + 6);
    EXPECT_EQ(*min_it, 1);
    EXPECT_EQ(min_it - arr, 3);

    auto max_it = mystl::max_element(arr, arr + 6);
    EXPECT_EQ(*max_it, 9);
    EXPECT_EQ(max_it - arr, 4);

    MYSTL_TEST_END();
}

// ============================================================
// 堆操作
// ============================================================

void test_heap() {
    MYSTL_TEST_BEGIN("heap operations");

    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6};

    // make_heap
    mystl::make_heap(arr, arr + 8);
    EXPECT_TRUE(mystl::is_heap(arr, arr + 8));

    // push_heap
    arr[8] = 10;
    mystl::push_heap(arr, arr + 9);
    EXPECT_TRUE(mystl::is_heap(arr, arr + 9));

    // sort_heap
    mystl::sort_heap(arr, arr + 9);
    EXPECT_TRUE(mystl::is_sorted(arr, arr + 9));

    MYSTL_TEST_END();
}

// ============================================================
// 字典序比较
// ============================================================

void test_lexicographical_compare() {
    MYSTL_TEST_BEGIN("lexicographical_compare");

    int a[] = {1, 2, 3};
    int b[] = {1, 2, 4};
    int c[] = {1, 2, 3, 0};

    EXPECT_TRUE(mystl::lexicographical_compare(a, a + 3, b, b + 3));
    EXPECT_FALSE(mystl::lexicographical_compare(b, b + 3, a, a + 3));
    EXPECT_TRUE(mystl::lexicographical_compare(a, a + 3, c, c + 4));

    MYSTL_TEST_END();
}

// ============================================================
// 主函数
// ============================================================

int main() {
    printf("=== algorithm 算法库全面测试 ===\n");

    // 非修改序列操作
    test_find();
    test_find_if();
    test_count();
    test_count_if();
    test_equal();

    // 修改序列操作
    test_copy();
    test_fill();
    test_transform();
    test_replace();
    test_reverse();
    test_remove();
    test_unique();

    // 排序操作
    test_is_sorted();
    test_sort();
    test_sort_with_compare();

    // 二分搜索
    test_lower_bound();
    test_upper_bound();
    test_binary_search();

    // 极值操作
    test_min_max();
    test_minmax_element();

    // 堆操作
    test_heap();

    // 字典序比较
    test_lexicographical_compare();

    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}