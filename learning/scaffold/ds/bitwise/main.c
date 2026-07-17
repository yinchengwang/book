/**
 * @file main.c
 * @brief 数据结构位运算学习卡片 - 演示位操作与位图
 *
 * 涵盖内容：
 * - 基础位运算：AND, OR, XOR, NOT, 左移, 右移
 * - 位操作技巧：设置位、清除位、翻转位、检查位
 * - 位图（Bitmap）：用于集合成员检测
 * - 布隆过滤器原理：多哈希函数概率过滤
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 基础位运算演示 ==================== */

/**
 * 打印二进制表示
 * @param value 要打印的值
 * @param bits  位数
 */
static void print_binary(unsigned int value, int bits)
{
    printf("  二进制: ");
    for (int i = bits - 1; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
        if (i % 4 == 0 && i > 0) {
            printf(" ");
        }
    }
    printf(" (0x%02X)\n", value);
}

/**
 * 基础位运算演示
 */
static void demo_basic_bitwise(void)
{
    printf("[bitwise] 基础位运算演示\n");

    unsigned char a = 0b10110010;  // 178
    unsigned char b = 0b01101100;  // 108

    printf("  a = ");
    print_binary(a, 8);
    printf("  b = ");
    print_binary(b, 8);

    /* 按位与 AND: 两位都为1时结果为1 */
    unsigned char result_and = a & b;
    printf("  a & b = ");
    print_binary(result_and, 8);
    printf("  用途: 提取特定位、清除特定位\n");

    /* 按位或 OR: 任一位为1时结果为1 */
    unsigned char result_or = a | b;
    printf("  a | b = ");
    print_binary(result_or, 8);
    printf("  用途: 设置特定位\n");

    /* 按位异或 XOR: 两位不同时结果为1 */
    unsigned char result_xor = a ^ b;
    printf("  a ^ b = ");
    print_binary(result_xor, 8);
    printf("  用途: 加密、交换两数（不借助临时变量）\n");

    /* 按位取反 NOT: 0变1, 1变0 */
    unsigned char result_not = ~a;
    printf("  ~a   = ");
    print_binary(result_not, 8);
    printf("  注意: 有符号数高位扩展\n");

    /* 左移: 高位丢弃，低位补0 */
    unsigned char result_shl = a << 2;
    printf("  a << 2 = ");
    print_binary(result_shl, 8);
    printf("  用途: 乘以2^n\n");

    /* 右移: 低位丢弃，高位补0（逻辑右移）或补符号位（算术右移） */
    unsigned char result_shr = a >> 2;
    printf("  a >> 2 = ");
    print_binary(result_shr, 8);
    printf("  用途: 除以2^n\n");

    printf("\n");
}

/* ==================== 位操作技巧演示 ==================== */

/**
 * 设置第 n 位为1
 */
static unsigned int set_bit(unsigned int value, int n)
{
    return value | (1U << n);
}

/**
 * 清除第 n 位（置0）
 */
static unsigned int clear_bit(unsigned int value, int n)
{
    return value & ~(1U << n);
}

/**
 * 翻转第 n 位
 */
static unsigned int toggle_bit(unsigned int value, int n)
{
    return value ^ (1U << n);
}

/**
 * 检查第 n 位是否为1
 */
static int check_bit(unsigned int value, int n)
{
    return (value >> n) & 1U;
}

/**
 * 提取连续的位域
 * @param value  源值
 * @param start  起始位（从0开始）
 * @param len    位数
 */
static unsigned int extract_bits(unsigned int value, int start, int len)
{
    unsigned int mask = ((1U << len) - 1);
    return (value >> start) & mask;
}

/**
 * 位操作技巧演示
 */
static void demo_bit_tricks(void)
{
    printf("[bitwise] 位操作技巧演示\n");

    unsigned int flags = 0;

    /* 设置位: 将特定位置1 */
    flags = set_bit(flags, 0);  // 设置第0位
    flags = set_bit(flags, 3);  // 设置第3位
    printf("  设置位0和位3后: 0x%02X\n", flags);

    /* 检查位: 查看特定位是否设置 */
    printf("  检查位0: %d, 检查位1: %d\n", check_bit(flags, 0), check_bit(flags, 1));

    /* 清除位: 将特定位置0 */
    flags = clear_bit(flags, 0);
    printf("  清除位0后: 0x%02X\n", flags);

    /* 翻转位: 0变1, 1变0 */
    flags = set_bit(flags, 5);
    flags = toggle_bit(flags, 5);  // 变回0
    printf("  翻转位5两次后: 0x%02X\n", flags);

    /* 提取位域: 提取连续的几位 */
    unsigned int value = 0b1111000011110000;  // 0xF0F0
    unsigned int field = extract_bits(value, 8, 4);  // 提取高4位
    printf("  提取 0x%04X 的 [11:8] 位域: 0x%X\n", value, field);

    printf("\n");
}

/* ==================== 位图（Bitmap）演示 ==================== */

/**
 * 位图结构
 * 使用位图表示整数集合，节省空间
 */
typedef struct {
    unsigned int *bits;  /* 位数组 */
    size_t num_bits;     /* 总位数 */
    size_t num_words;    /* 字数 */
} Bitmap;

/**
 * 创建位图
 */
static Bitmap *bitmap_create(size_t num_bits)
{
    Bitmap *bm = (Bitmap *)malloc(sizeof(Bitmap));
    if (!bm) return NULL;

    bm->num_bits = num_bits;
    bm->num_words = (num_bits + 31) / 32;
    bm->bits = (unsigned int *)calloc(bm->num_words, sizeof(unsigned int));

    if (!bm->bits) {
        free(bm);
        return NULL;
    }
    return bm;
}

/**
 * 释放位图
 */
static void bitmap_free(Bitmap *bm)
{
    if (bm) {
        free(bm->bits);
        free(bm);
    }
}

/**
 * 设置位
 */
static void bitmap_set(Bitmap *bm, size_t pos)
{
    if (pos >= bm->num_bits) return;
    bm->bits[pos / 32] |= (1U << (pos % 32));
}

/**
 * 清除位
 */
static void bitmap_clear(Bitmap *bm, size_t pos)
{
    if (pos >= bm->num_bits) return;
    bm->bits[pos / 32] &= ~(1U << (pos % 32));
}

/**
 * 检查位
 */
static int bitmap_test(const Bitmap *bm, size_t pos)
{
    if (pos >= bm->num_bits) return 0;
    return (bm->bits[pos / 32] >> (pos % 32)) & 1U;
}

/**
 * 位图演示 - 集合成员检测
 */
static void demo_bitmap(void)
{
    printf("[bitwise] 位图（Bitmap）演示\n");

    /* 创建100位的位图 */
    Bitmap *bm = bitmap_create(100);
    if (!bm) {
        printf("  位图创建失败\n");
        return;
    }

    /* 添加元素到集合: 0, 5, 10, 25, 33, 50, 75 */
    int members[] = {0, 5, 10, 25, 33, 50, 75};
    int num_members = sizeof(members) / sizeof(members[0]);

    printf("  添加集合元素: ");
    for (int i = 0; i < num_members; i++) {
        bitmap_set(bm, members[i]);
        printf("%d ", members[i]);
    }
    printf("\n");

    /* 查询成员资格 */
    printf("  成员检测:\n");
    printf("    bitmap_test(5)   = %d (应为1)\n", bitmap_test(bm, 5));
    printf("    bitmap_test(10)  = %d (应为1)\n", bitmap_test(bm, 10));
    printf("    bitmap_test(99) = %d (应为0)\n", bitmap_test(bm, 99));
    printf("    bitmap_test(33)  = %d (应为1)\n", bitmap_test(bm, 33));
    printf("    bitmap_test(11)  = %d (应为0)\n", bitmap_test(bm, 11));

    /* 空间对比 */
    printf("  空间对比: %zu位数组用 %zu 字节 (vs %zu 字节 bool数组)\n",
           bm->num_bits, bm->num_words * 4, bm->num_bits);

    bitmap_free(bm);
    printf("\n");
}

/* ==================== 布隆过滤器原理演示 ==================== */

/**
 * 简化的哈希函数
 */
static unsigned int simple_hash(int value, int seed)
{
    return (unsigned int)((value * 31 + seed) % 100);
}

/**
 * 布隆过滤器结构（简化版）
 */
typedef struct {
    Bitmap *bitmap;       /* 位图 */
    int num_hashes;      /* 哈希函数数量 */
} BloomFilter;

/**
 * 创建布隆过滤器
 */
static BloomFilter *bloom_create(int num_hashes)
{
    BloomFilter *bf = (BloomFilter *)malloc(sizeof(BloomFilter));
    if (!bf) return NULL;

    bf->bitmap = bitmap_create(200);  /* 200位的位图 */
    bf->num_hashes = num_hashes;

    if (!bf->bitmap) {
        free(bf);
        return NULL;
    }
    return bf;
}

/**
 * 添加元素到布隆过滤器
 */
static void bloom_add(BloomFilter *bf, int value)
{
    for (int i = 0; i < bf->num_hashes; i++) {
        unsigned int pos = simple_hash(value, i * 17);
        bitmap_set(bf->bitmap, pos % bf->bitmap->num_bits);
    }
}

/**
 * 检查元素是否可能在集合中
 * 注意：可能存在假阳性（false positive），但不会有假阴性
 */
static int bloom_might_contain(const BloomFilter *bf, int value)
{
    for (int i = 0; i < bf->num_hashes; i++) {
        unsigned int pos = simple_hash(value, i * 17);
        if (!bitmap_test(bf->bitmap, pos % bf->bitmap->num_bits)) {
            return 0;  /* 一定不在集合中 */
        }
    }
    return 1;  /* 可能在集合中 */
}

/**
 * 释放布隆过滤器
 */
static void bloom_free(BloomFilter *bf)
{
    if (bf) {
        bitmap_free(bf->bitmap);
        free(bf);
    }
}

/**
 * 布隆过滤器演示
 */
static void demo_bloom_filter(void)
{
    printf("[bitwise] 布隆过滤器（Bloom Filter）原理演示\n");

    /* 创建3个哈希函数的布隆过滤器 */
    BloomFilter *bf = bloom_create(3);

    /* 添加一些值 */
    int added[] = {10, 20, 30, 50, 100};
    printf("  添加元素: ");
    for (size_t i = 0; i < sizeof(added) / sizeof(added[0]); i++) {
        bloom_add(bf, added[i]);
        printf("%d ", added[i]);
    }
    printf("\n");

    /* 检测 */
    printf("  成员检测:\n");
    printf("    检测 10: %d (已添加，期望1)\n", bloom_might_contain(bf, 10));
    printf("    检测 20: %d (已添加，期望1)\n", bloom_might_contain(bf, 20));
    printf("    检测 99: %d (未添加，期望0，但可能有假阳性)\n", bloom_might_contain(bf, 99));
    printf("    检测 77: %d (未添加，期望0，但可能有假阳性)\n", bloom_might_contain(bf, 77));

    printf("  特点：不存在假阴性，但存在假阳性\n");
    printf("  应用：缓存穿透防护、去重、IP黑名单等\n");

    bloom_free(bf);
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 位运算 ===\n\n");

    /* 基础位运算演示 */
    demo_basic_bitwise();

    /* 位操作技巧演示 */
    demo_bit_tricks();

    /* 位图演示 */
    demo_bitmap();

    /* 布隆过滤器原理演示 */
    demo_bloom_filter();

    printf("=== PASS ===\n");
    return 0;
}
