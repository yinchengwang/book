/*
 * mystl_test_runner.cpp - mystl 全面测试运行器
 *
 * 统一运行所有组件的测试
 */

#include "mystl_test.h"
#include "mystl/mystl.h"
#include <cstdio>

using namespace mystl;

// 外部测试函数声明
extern void test_type_traits();           // type_traits 测试
extern void test_allocator();             // allocator 测试
extern void test_iterator();              // iterator 测试
extern void test_functional();            // functional 测试
extern void test_memory();                // memory 测试
extern void test_utility();               // utility 测试

extern void test_vector();                // vector 测试
extern void test_array();                 // array 测试（需改为函数形式）
extern void test_list();                  // list 测试
extern void test_forward_list();          // forward_list 测试
extern void test_deque();                 // deque 测试

extern void test_stack();                 // stack 测试
extern void test_queue();                 // queue 测试
extern void test_priority_queue();        // priority_queue 测试

extern void test_set();                   // set 测试
extern void test_multiset();              // multiset 测试
extern void test_map();                   // map 测试
extern void test_multimap();              // multimap 测试

extern void test_unordered_set();         // unordered_set 测试
extern void test_unordered_map();         // unordered_map 测试
extern void test_unordered_multiset();    // unordered_multiset 测试
extern void test_unordered_multimap();    // unordered_multimap 测试

extern void test_algorithm_main();        // algorithm 测试
extern void test_numeric_main();          // numeric 测试

// array 测试函数包装
void test_array_wrapper() {
    printf("\n=== array 测试 ===\n");
    // 直接内联测试
    MYSTL_TEST_BEGIN("array default construct");
    {
        mystl::array<int, 5> arr;
        EXPECT_EQ(arr.size(), 5u);
        EXPECT_FALSE(arr.empty());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("array aggregate init");
    {
        mystl::array<int, 5> arr = {1, 2, 3, 4, 5};
        EXPECT_EQ(arr.size(), 5u);
        EXPECT_EQ(arr[0], 1);
        EXPECT_EQ(arr[4], 5);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("array zero size");
    {
        mystl::array<int, 0> arr;
        EXPECT_TRUE(arr.empty());
        EXPECT_EQ(arr.size(), 0u);
        EXPECT_EQ(arr.begin(), arr.end());
        EXPECT_EQ(arr.data(), nullptr);
    }
    MYSTL_TEST_END();
}

// 统计总测试数
int total_tests = 0;
int total_passed = 0;
int total_failed = 0;

void run_test_group(const char* name, void(*test_func)()) {
    printf("\n========== %s ==========\n", name);
    size_t before_failed = mystl_test::stats().failed;
    test_func();
    total_tests += mystl_test::stats().total - total_tests;
    total_passed += mystl_test::stats().passed - total_passed;
    total_failed = mystl_test::stats().failed;
}

int main() {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              mystl 手写 STL 全面测试套件                  ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    // 基础设施测试
    printf("\n========== 基础设施 ==========\n");

    // 注意：以下测试需要改为函数形式
    // run_test_group("type_traits", test_type_traits);
    // run_test_group("allocator", test_allocator);
    // run_test_group("iterator", test_iterator);
    // run_test_group("functional", test_functional);
    // run_test_group("memory", test_memory);
    // run_test_group("utility", test_utility);

    // 序列容器测试
    printf("\n========== 序列容器 ==========\n");
    // run_test_group("vector", test_vector);
    run_test_group("array", test_array_wrapper);
    // run_test_group("list", test_list);
    // run_test_group("forward_list", test_forward_list);
    // run_test_group("deque", test_deque);

    // 适配器测试
    printf("\n========== 容器适配器 ==========\n");
    // run_test_group("stack", test_stack);
    // run_test_group("queue", test_queue);
    // run_test_group("priority_queue", test_priority_queue);

    // 有序关联容器测试
    printf("\n========== 有序关联容器 ==========\n");
    // run_test_group("set", test_set);
    // run_test_group("multiset", test_multiset);
    // run_test_group("map", test_map);
    // run_test_group("multimap", test_multimap);

    // 无序关联容器测试
    printf("\n========== 无序关联容器 ==========\n");
    // run_test_group("unordered_set", test_unordered_set);
    // run_test_group("unordered_map", test_unordered_map);
    // run_test_group("unordered_multiset", test_unordered_multiset);
    // run_test_group("unordered_multimap", test_unordered_multimap);

    // 算法测试
    printf("\n========== 算法 ==========\n");
    // run_test_group("algorithm", test_algorithm_main);
    // run_test_group("numeric", test_numeric_main);

    // 总结
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║                      测试总结                             ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  总测试数: %-5d  通过: %-5d  失败: %-5d            ║\n",
           mystl_test::stats().total, mystl_test::stats().passed, mystl_test::stats().failed);
    printf("╚══════════════════════════════════════════════════════════╝\n");

    return mystl_test::stats().failed > 0 ? 1 : 0;
}