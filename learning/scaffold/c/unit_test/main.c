/*
 * unit_test scaffold — 演示入口
 *
 * 本文件是被测试的业务代码的简单调用入口。
 * 真正的单元测试在 test.c 中。
 *
 * 复现：
 *   # 运行 demo
 *   gcc -Wall -Wextra -O2 -std=c11 -o utest_demo main.c mathlib.c
 *   ./utest_demo
 *
 *   # 运行测试
 *   gcc -Wall -Wextra -O2 -std=c11 -o test_runner test.c mathlib.c
 *   ./test_runner
 */

#include <stdio.h>
#include "mathlib.h"

int main(void) {
    printf("=== 单元测试演示（demo 模式） ===\n");
    printf("factorial(5)=%d, is_prime(7)=%d, gcd(48,18)=%d, power(2,10)=%d\n",
           factorial(5), is_prime(7), gcd(48, 18), power(2, 10));
    printf("factorial(0)=%d (boundary), is_prime(1)=%d (non-prime)\n",
           factorial(0), is_prime(1));
    printf("\n运行 'make test' 或手动编译 test.c 来执行单元测试。\n");
    printf("=== PASS ===\n");
    return 0;
}
