/**
 * bit_manipulation.c - 位运算技巧演示
 *
 * 用 C11 实现常见的位运算操作：
 * 1. 单独位操作（set/clear/toggle）
 * 2. 位移操作统计
 * 3. 交换奇偶位
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ========== 单独位操作 ========== */

/** 将 value 的第 bit 位设为 1 */
static inline uint32_t bit_set(uint32_t value, int bit)
{
    return value | (1u << bit);
}

/** 将 value 的第 bit 位设为 0 */
static inline uint32_t bit_clear(uint32_t value, int bit)
{
    return value & ~(1u << bit);
}

/** 翻转 value 的第 bit 位 */
static inline uint32_t bit_toggle(uint32_t value, int bit)
{
    return value ^ (1u << bit);
}

/** 读取 value 的第 bit 位 */
static inline bool bit_test(uint32_t value, int bit)
{
    return (value >> bit) & 1u;
}

/* ========== 位移操作统计 ========== */

/** Brian Kernighan 算法：统计二进制中 1 的个数 */
static int popcount(uint32_t value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);  /* 清除最低位的 1 */
        count++;
    }
    return count;
}

/** 判断是否为 2 的幂 */
static inline bool is_power_of_two(uint32_t value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

/** 获取最低位 1 的位置（从 0 开始计数） */
static inline int trailing_zeros(uint32_t value)
{
    if (value == 0) return -1;
    return __builtin_ctz(value);
}

/** 获取最高位 1 的位置 */
static inline int leading_zeros(uint32_t value)
{
    if (value == 0) return -1;
    return 31 - __builtin_clz(value);
}

/* ========== 交换奇偶位 ========== */

/**
 * 交换一个整数的奇数位和偶数位
 * 例如: 二进制 1010 (10) -> 0101 (5)
 */
static uint32_t swap_odd_even_bits(uint32_t value)
{
    /* 0xAAAAAAAA = 10101010101010101010101010101010 (偶数位掩码) */
    /* 0x55555555 = 01010101010101010101010101010101 (奇数位掩码) */
    uint32_t even_bits = value & 0xAAAAAAAA;
    uint32_t odd_bits  = value & 0x55555555;

    even_bits >>= 1;
    odd_bits  <<= 1;

    return even_bits | odd_bits;
}

/* ========== 反转字节序 ========== */

/** 反转 32 位整数的字节序 */
static uint32_t reverse_bytes(uint32_t value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >>  8) |
           ((value & 0x0000FF00) <<  8) |
           ((value & 0x000000FF) << 24);
}

/* ========== 测试驱动 ========== */

static int total_tests  = 0;
static int passed_tests = 0;

#define TEST(cond, fmt, ...) do {                                 \
    total_tests++;                                                \
    if (!(cond)) {                                                \
        printf("  FAIL: " fmt "\n", ##__VA_ARGS__);              \
    } else {                                                      \
        passed_tests++;                                           \
        printf("  PASS\n");                                       \
    }                                                             \
} while (0)

static void test_single_bit_ops(void)
{
    printf("[单独位操作]\n");

    uint32_t v = 0;
    v = bit_set(v, 3);
    TEST(v == 8, "bit_set(0, 3) 期望 8，实际 %u", v);

    v = bit_set(v, 0);
    TEST(v == 9, "bit_set(8, 0) 期望 9，实际 %u", v);

    v = bit_clear(v, 3);
    TEST(v == 1, "bit_clear(9, 3) 期望 1，实际 %u", v);

    v = bit_toggle(v, 1);
    TEST(v == 3, "bit_toggle(1, 1) 期望 3，实际 %u", v);

    TEST(bit_test(v, 0) == true,  "bit_test(3, 0) 期望 true");
    TEST(bit_test(v, 2) == false, "bit_test(3, 2) 期望 false");
}

static void test_shift_stats(void)
{
    printf("[位移操作统计]\n");

    TEST(popcount(0) == 0,         "popcount(0) 期望 0");
    TEST(popcount(7) == 3,         "popcount(7) 期望 3");
    TEST(popcount(0xFF) == 8,      "popcount(0xFF) 期望 8");

    TEST(is_power_of_two(1)  == true,  "is_power_of_two(1) 期望 true");
    TEST(is_power_of_two(16) == true,  "is_power_of_two(16) 期望 true");
    TEST(is_power_of_two(0)  == false, "is_power_of_two(0) 期望 false");
    TEST(is_power_of_two(3)  == false, "is_power_of_two(3) 期望 false");
}

static void test_swap_bits(void)
{
    printf("[交换奇偶位]\n");

    uint32_t r1 = swap_odd_even_bits(0b1010); /* 10 -> 5 */
    TEST(r1 == 5, "swap_odd_even_bits(10) 期望 5，实际 %u", r1);

    uint32_t r2 = swap_odd_even_bits(0b0101); /* 5 -> 10 */
    TEST(r2 == 10, "swap_odd_even_bits(5) 期望 10，实际 %u", r2);

    uint32_t r3 = swap_odd_even_bits(0);
    TEST(r3 == 0, "swap_odd_even_bits(0) 期望 0，实际 %u", r3);
}

static void test_reverse(void)
{
    printf("[反转字节序]\n");

    uint32_t r = reverse_bytes(0x12345678);
    TEST(r == 0x78563412, "reverse_bytes(0x12345678) 期望 0x78563412，实际 0x%08X", r);
}

int main(void)
{
    printf("===== 位运算技巧 =====\n\n");

    test_single_bit_ops();
    putchar('\n');
    test_shift_stats();
    putchar('\n');
    test_swap_bits();
    putchar('\n');
    test_reverse();
    putchar('\n');

    printf("结果: %d / %d 测试通过\n", passed_tests, total_tests);

    return (passed_tests == total_tests) ? 0 : 1;
}
