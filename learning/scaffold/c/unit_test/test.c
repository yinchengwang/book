/*
 * test.c — 手工单元测试（零依赖，零框架）
 *
 * 用极简的 TEST / ASSERT_EQ / ASSERT_TRUE 宏模拟 xUnit 测试框架。
 * 每个 TEST(name) 定义一个测试用例，ASSERT_EQ 检查预期值。
 *
 * 编译与运行：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test_runner test.c mathlib.c
 *   ./test_runner
 */

#include <stdio.h>
#include "mathlib.h"

/* ======= 手工测试框架（20 行宏） ======= */

static int test_fails = 0;
static int test_total = 0;

#define TEST(name) \
    static void test_##name(void)

#define RUN_TEST(name) do {                                    \
    test_total++;                                              \
    printf("[test] %-25s ... ", #name);                        \
    int _prev_fails = test_fails;                              \
    test_##name();                                             \
    if (test_fails == _prev_fails) printf("PASS\n");           \
} while(0)

#define ASSERT_EQ(a, b) do {                                   \
    int _a = (a), _b = (b);                                    \
    if (_a != _b) {                                            \
        printf("FAIL\n  %s:%d: ASSERT_EQ(%s=%d, %s=%d)\n",     \
               __FILE__, __LINE__, #a, _a, #b, _b);            \
        test_fails++;                                          \
        return;                                                \
    }                                                          \
} while(0)

#define ASSERT_TRUE(cond) ASSERT_EQ(cond, 1)

/* ======= 测试用例 ======= */

/* --- factorial --- */
TEST(factorial_zero)  { ASSERT_EQ(factorial(0), 1); }
TEST(factorial_one)   { ASSERT_EQ(factorial(1), 1); }
TEST(factorial_five)  { ASSERT_EQ(factorial(5), 120); }
TEST(factorial_neg)   { ASSERT_EQ(factorial(-1), -1); }

/* --- is_prime --- */
TEST(prime_two)       { ASSERT_EQ(is_prime(2), 1); }
TEST(prime_seven)     { ASSERT_EQ(is_prime(7), 1); }
TEST(prime_one)       { ASSERT_EQ(is_prime(1), 0); }
TEST(prime_four)      { ASSERT_EQ(is_prime(4), 0); }
TEST(prime_large)     { ASSERT_EQ(is_prime(97), 1); }

/* --- gcd --- */
TEST(gcd_same)        { ASSERT_EQ(gcd(48, 48), 48); }
TEST(gcd_coprime)     { ASSERT_EQ(gcd(17, 13), 1); }
TEST(gcd_typical)     { ASSERT_EQ(gcd(48, 18), 6); }
TEST(gcd_zero)        { ASSERT_EQ(gcd(0, 7), 7); }
TEST(gcd_negative)    { ASSERT_EQ(gcd(-48, 18), 6); }

/* --- power --- */
TEST(power_zero_exp)  { ASSERT_EQ(power(5, 0), 1); }
TEST(power_one)       { ASSERT_EQ(power(2, 1), 2); }
TEST(power_typical)   { ASSERT_EQ(power(2, 10), 1024); }
TEST(power_neg_exp)   { ASSERT_EQ(power(2, -1), -1); }

/* ======= 运行所有测试 ======= */

int main(void) {
    printf("=== 单元测试：mathlib ===\n\n");

    RUN_TEST(factorial_zero);
    RUN_TEST(factorial_one);
    RUN_TEST(factorial_five);
    RUN_TEST(factorial_neg);

    RUN_TEST(prime_two);
    RUN_TEST(prime_seven);
    RUN_TEST(prime_one);
    RUN_TEST(prime_four);
    RUN_TEST(prime_large);

    RUN_TEST(gcd_same);
    RUN_TEST(gcd_coprime);
    RUN_TEST(gcd_typical);
    RUN_TEST(gcd_zero);
    RUN_TEST(gcd_negative);

    RUN_TEST(power_zero_exp);
    RUN_TEST(power_one);
    RUN_TEST(power_typical);
    RUN_TEST(power_neg_exp);

    printf("\n=== result: %d/%d passed, %d failed ===\n",
           test_total - test_fails, test_total, test_fails);
    return test_fails ? 1 : 0;
}
