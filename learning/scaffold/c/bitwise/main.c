/* bitwise scaffold — 位掩码/移位乘除/Brian Kernighan 计数/bitset */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* 辅助: 打印二进制 (8 位) */
static void print_b8(uint8_t x) {
    for (int i = 7; i >= 0; i--)
        printf("%d", (x >> i) & 1);
}

/* 辅助: 打印二进制 (32 位, 按字节分组) */
static void print_b32(uint32_t x) {
    for (int i = 31; i >= 0; i--) {
        printf("%d", (x >> i) & 1);
        if (i % 8 == 0 && i > 0) printf(" ");
    }
}

/* ══════════════════════════════════════════════════════════════
 * 1. 位掩码 — set/clear/toggle/test
 * ══════════════════════════════════════════════════════════════ */
static void demo_bitmask(void) {
    printf("=== 位掩码操作 ===\n");
    uint8_t flags = 0b00000000;
    printf("初始:        "); print_b8(flags); printf("\n");

    /* set bit */
    flags |= (1 << 3);
    printf("set bit 3:   "); print_b8(flags); printf("\n");
    flags |= (1 << 0);
    printf("set bit 0:   "); print_b8(flags); printf("\n");

    /* toggle bit */
    flags ^= (1 << 3);
    printf("toggle bit 3:"); print_b8(flags); printf("\n");
    flags ^= (1 << 3);
    printf("toggle again:"); print_b8(flags); printf("\n");

    /* test bit */
    printf("test bit 0: %s\n", (flags & (1 << 0)) ? "1" : "0");
    printf("test bit 2: %s\n", (flags & (1 << 2)) ? "1" : "0");

    /* clear bit */
    flags &= ~(1 << 0);
    printf("clear bit 0: "); print_b8(flags); printf("\n");
}

/* ══════════════════════════════════════════════════════════════
 * 2. 移位乘法/除法
 * ══════════════════════════════════════════════════════════════ */
static void demo_shift_math(void) {
    printf("\n=== 移位乘法/除法 ===\n");
    int x = 7;
    printf("x = %d\n", x);
    printf("x << 1 = %2d  (= x * 2)\n",  x << 1);
    printf("x << 3 = %2d  (= x * 8)\n",  x << 3);
    printf("x >> 1 = %2d  (= x / 2)\n",  x >> 1);

    /* 负数右移 — 实现定义! */
    int neg = -16;
    printf("neg = %d (-16)\n", neg);
    printf("neg >> 2 = %d (算术右移, 符号扩展)\n", neg >> 2);

    /* 快速取模 (仅对 2^k 有效) */
    for (int i = 0; i < 8; i++)
        printf("  %d & 7 = %d  (%d %% 8 = %d)\n", i, i & 7, i, i % 8);
}

/* ══════════════════════════════════════════════════════════════
 * 3. Brian Kernighan 算法 — 数 1 的个数
 * ══════════════════════════════════════════════════════════════ */
static int popcount_bk(uint32_t n) {
    int cnt = 0;
    while (n) { n &= (n - 1); cnt++; }
    return cnt;
}

static void demo_popcount(void) {
    printf("\n=== Brian Kernighan popcount ===\n");
    uint32_t tests[] = {0, 1, 7, 15, 0xFF, 0xDEADBEEF};
    for (int i = 0; i < 6; i++) {
        uint32_t v = tests[i];
        printf("popcount("); print_b32(v);
        printf(") = %d\n", popcount_bk(v));
    }
    printf("总迭代 = Σ(bit=1) 而非 32 — O(位数) 而非 O(宽度)\n");
}

/* ══════════════════════════════════════════════════════════════
 * 4. Bitset — 用位运算实现集合
 * ══════════════════════════════════════════════════════════════ */
#define BS_CAP 128
typedef struct {
    uint32_t words[BS_CAP / 32];
} BitSet;

static void bs_init(BitSet *bs) {
    for (int i = 0; i < BS_CAP / 32; i++) bs->words[i] = 0;
}

static void bs_set(BitSet *bs, int idx) {
    if (idx >= BS_CAP) return;
    bs->words[idx / 32] |= (1u << (idx % 32));
}

static bool bs_test(BitSet *bs, int idx) {
    if (idx >= BS_CAP) return false;
    return (bs->words[idx / 32] >> (idx % 32)) & 1u;
}

static void bs_clear(BitSet *bs, int idx) {
    if (idx >= BS_CAP) return;
    bs->words[idx / 32] &= ~(1u << (idx % 32));
}

static int bs_count(BitSet *bs) {
    int cnt = 0;
    for (int i = 0; i < BS_CAP / 32; i++)
        cnt += popcount_bk(bs->words[i]);
    return cnt;
}

static void bs_print(BitSet *bs, int n) {
    printf("[bitset] ");
    bool first = true;
    for (int i = 0; i < n; i++) {
        if (bs_test(bs, i)) {
            if (!first) printf(", ");
            printf("%d", i);
            first = false;
        }
    }
    if (first) printf("(空)");
    printf(" | count=%d\n", bs_count(bs));
}

int main(void) {
    /* 位掩码 */
    demo_bitmask();

    /* 移位数学 */
    demo_shift_math();

    /* Brian Kernighan */
    demo_popcount();

    /* Bitset */
    printf("\n=== Bitset 实现 ===\n");
    BitSet bs; bs_init(&bs);
    bs_set(&bs, 3);  bs_set(&bs, 7);  bs_set(&bs, 15);
    bs_set(&bs, 31); bs_set(&bs, 63); bs_set(&bs, 100);
    bs_print(&bs, 110);

    printf("[bitset] test(7) = %s\n", bs_test(&bs, 7) ? "1" : "0");
    printf("[bitset] test(5) = %s\n", bs_test(&bs, 5) ? "1" : "0");

    bs_clear(&bs, 7);
    printf("[bitset] clear(7) → ");
    bs_print(&bs, 110);

    /* 位运算实际应用 */
    printf("\n=== 工程位运算模式 ===\n");
    /* Page header 标志位 (模拟 bufpage.h) */
    uint16_t page_flags = 0;
    page_flags |= (1 << 0);  /* PAGE_HAS_FREE_LINES */
    page_flags |= (1 << 2);  /* PAGE_FULL */
    printf("[page] flags=0x%04x | HAS_FREE=%d FULL=%d\n",
           page_flags,
           (page_flags >> 0) & 1,
           (page_flags >> 2) & 1);

    /* Bitmap 索引: 每 bit 标记一个元组是否可见 */
    uint32_t visibility = 0b10110010;
    printf("[vis]  bitmap="); print_b8((uint8_t)visibility);
    printf(" | visible tuples: ");
    for (int i = 0; i < 8; i++)
        if ((visibility >> i) & 1) printf("%d ", i);
    printf("\n");

    printf("\n=== PASS ===\n");
    return 0;
}
