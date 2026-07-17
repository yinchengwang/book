/**
 * @file main.c
 * @brief 递归思想 - 递归基础 + 回溯模板
 * @author yinchengwang
 * @version 1.0
 * @date 2026-07-12
 * @note C11 标准实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// 第一部分：阶乘（递归 + 记忆化）
// =============================================================================

/**
 * @brief 普通递归实现阶乘
 * @param n 非负整数
 * @return n!
 */
long long factorial_recursive(int n) {
    if (n <= 1) return 1;
    return (long long)n * factorial_recursive(n - 1);
}

/**
 * @brief 带记忆化的阶乘（避免重复计算）
 */
static long long memo[21] = {0};  // 记忆化数组，最多计算 20!

long long factorial_memoized(int n) {
    if (n <= 1) return 1;
    if (memo[n] != 0) return memo[n];  // 命中缓存
    memo[n] = (long long)n * factorial_memoized(n - 1);
    return memo[n];
}

// =============================================================================
// 第二部分：斐波那契数列（多种解法对比）
// =============================================================================

/**
 * @brief 普通递归（指数级时间复杂度 O(2^n)，极其低效）
 */
long long fib_recursive(int n) {
    if (n <= 1) return n;
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

/**
 * @brief 记忆化递归（自顶向下 DP，时间 O(n)，空间 O(n)）
 */
static long long fib_memo[91] = {0};  // fib(90) 会超出 64 位

long long fib_memoized(int n) {
    if (n <= 1) return n;
    if (fib_memo[n] != 0) return fib_memo[n];
    fib_memo[n] = fib_memoized(n - 1) + fib_memoized(n - 2);
    return fib_memo[n];
}

/**
 * @brief 迭代版本（自底向上 DP，时间 O(n)，空间 O(1)）
 */
long long fib_iterative(int n) {
    if (n <= 1) return n;
    long long a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        long long c = a + b;
        a = b;
        b = c;
    }
    return b;
}

// =============================================================================
// 第三部分：回溯模板 - 子集生成
// =============================================================================

#define MAX_N 20

/**
 * @brief 回溯生成子集的核心模板
 * @param nums 输入数组
 * @param nums_size 数组大小
 * @param index 当前处理的位置
 * @param track 当前已选元素
 * @param track_size 当前已选元素数量
 */
void backtrack_subsets(int* nums, int nums_size, int index, int* track, int track_size) {
    // 收集结果（每个节点都是一种子集）
    printf("{");
    for (int i = 0; i < track_size; i++) {
        printf("%d", track[i]);
        if (i < track_size - 1) printf(", ");
    }
    printf("}\n");

    // 回溯模板核心：遍历选择列表
    for (int i = index; i < nums_size; i++) {
        // 做选择
        track[track_size] = nums[i];
        // 进入下一层决策树
        backtrack_subsets(nums, nums_size, i + 1, track, track_size + 1);
        // 撤销选择（回溯）
    }
}

/**
 * @brief 生成所有子集
 */
void generate_subsets(int* nums, int nums_size) {
    int track[MAX_N];
    backtrack_subsets(nums, nums_size, 0, track, 0);
}

// =============================================================================
// 主函数：测试所有功能
// =============================================================================

int main(void) {
    printf("=== 递归思想练习 ===\n\n");

    // 测试阶乘
    printf("--- 阶乘测试 ---\n");
    printf("factorial(10) 递归   = %lld\n", factorial_recursive(10));
    printf("factorial(10) 记忆化 = %lld\n", factorial_memoized(10));
    printf("factorial(20) 记忆化 = %lld\n", factorial_memoized(20));

    // 测试斐波那契
    printf("\n--- 斐波那契数列测试 ---\n");
    printf("n=10 递归   = %lld (不推荐，重复计算太多)\n", fib_recursive(10));
    printf("n=10 记忆化 = %lld\n", fib_memoized(10));
    printf("n=10 迭代   = %lld\n", fib_iterative(10));
    printf("n=40 记忆化 = %lld\n", fib_memoized(40));
    printf("n=40 迭代   = %lld\n", fib_iterative(40));

    // 测试子集生成（回溯模板）
    printf("\n--- 子集生成测试（回溯模板） ---\n");
    int nums[] = {1, 2, 3};
    int nums_size = sizeof(nums) / sizeof(nums[0]);
    printf("数组 [1, 2, 3] 的所有子集:\n");
    generate_subsets(nums, nums_size);

    return 0;
}
