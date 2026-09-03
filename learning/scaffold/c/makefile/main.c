/*
 * makefile demo — 一个稍微真实的小工程，给 Makefile 用
 *
 * 4 个文件：
 *   main.c          入口
 *   mathlib.h       数学函数声明
 *   mathlib.c       数学函数实现（含 PI 常量）
 *   test.c          简单断言测试
 *
 * 编译依赖：
 *   test 用 main 提供的 print_result 工具
 *   mathlib 给 test 算 fibonacci / factorial
 */

#include <stdio.h>
#include <stdlib.h>
#include "mathlib.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    mathlib_init();

    printf("[mathapp] === Demo ===\n");
    printf("pi = %.6f\n", mathlib_pi());
    printf("factorial(5) = %d\n", mathlib_factorial(5));
    printf("fibonacci(10) = %d\n", mathlib_fibonacci(10));
    printf("gcd(48, 18) = %d\n", mathlib_gcd(48, 18));
    printf("[mathapp] OK\n");
    return 0;
}
