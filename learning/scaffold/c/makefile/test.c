/*
 * test.c — 简单断言宏驱动的测试
 *
 * 用 TEST 宏断言，每失败一次会 set 非零 rc。
 * `make test` 应当返回 0。
 */

#include <stdio.h>
#include "mathlib.h"

static int s_failures = 0;
int run_test_body(void);  /* forward decl */

#define TEST(expr, expected, label)                                          \
    do {                                                                     \
        int actual = (expr);                                                 \
        if (actual != (expected)) {                                          \
            fprintf(stderr, "FAIL: %s (%s): got %d, expected %d\n",          \
                    #expr, label, actual, (expected));                       \
            s_failures++;                                                    \
        } else {                                                             \
            printf("ok: %s == %d\n", #expr, (expected));                     \
        }                                                                    \
    } while (0)

/* entry point for standalone test binary */
int main(void) {
    return run_test_body();
}

/* reuse-named function so main() can call back without conflict */
int run_test_body(void) {
    s_failures = 0;
    mathlib_init();
    TEST(mathlib_factorial(0),  1,   "0! = 1");
    TEST(mathlib_factorial(5),  120, "5! = 120");
    TEST(mathlib_fibonacci(0),  0,   "fib(0) = 0");
    TEST(mathlib_fibonacci(10), 55,  "fib(10) = 55");
    TEST(mathlib_gcd(48, 18),   6,   "gcd");
    TEST((int)mathlib_pi(),     3,   "pi approx = 3");

    if (s_failures > 0) {
        fprintf(stderr, "test failed: %d\n", s_failures);
        return 1;
    }
    printf("test passed: all 6\n");
    return 0;
}
