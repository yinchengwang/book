/*
 * bitmap.c —— 位图实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   struct ds_bitmap {
 *       uint64_t *bits;   // 位图存储（以 64 位为一组）
 *       size_t    n_bits; // 支持的位数
 *       size_t    n_words; // 需要的 64 位字数量 = (n_bits + 63) / 64
 *   };
 *
 * 位索引分解（以 pos 为例）：
 *   word_index = pos / 64  （所在的 64 位字）
 *   bit_offset = pos % 64  （在该字中的位偏移）
 *
 * 位操作宏：
 *   SET:    bits[word] |=  (1ULL << offset)
 *   CLEAR:  bits[word] &= ~(1ULL << offset)
 *   TEST:   (bits[word] >> offset) & 1
 *
 * 示例（pos=130）：
 *   word_index = 130 / 64 = 2
 *   bit_offset = 130 % 64 = 2
 *   → 操作 bits[2] 的第 2 位
 */

#include <ds/bitmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BITS_PER_WORD 64u
#define BITMASK(offset) (1ULL << (offset))

struct ds_bitmap {
    uint64_t *bits;
    size_t    n_bits;
    size_t    n_words;
};

ds_bitmap_t *ds_bitmap_create(size_t max_value)
{
    ds_bitmap_t *bitmap;

    bitmap = (ds_bitmap_t *)calloc(1u, sizeof(ds_bitmap_t));
    if (!bitmap) return NULL;

    bitmap->n_bits = max_value + 1u; /* [0, max_value] 共 max_value+1 个数 */
    bitmap->n_words = (bitmap->n_bits + BITS_PER_WORD - 1u) / BITS_PER_WORD;

    bitmap->bits = (uint64_t *)calloc(bitmap->n_words, sizeof(uint64_t));
    if (!bitmap->bits) {
        free(bitmap);
        return NULL;
    }

    return bitmap;
}

void ds_bitmap_destroy(ds_bitmap_t *bitmap)
{
    if (!bitmap) return;
    free(bitmap->bits);
    free(bitmap);
}

int ds_bitmap_set(ds_bitmap_t *bitmap, size_t pos)
{
    size_t   word;
    unsigned offset;

    if (!bitmap || pos >= bitmap->n_bits) return -1;

    word = pos / BITS_PER_WORD;
    offset = (unsigned)(pos % BITS_PER_WORD);
    bitmap->bits[word] |= BITMASK(offset);
    return 0;
}

int ds_bitmap_clear(ds_bitmap_t *bitmap, size_t pos)
{
    size_t   word;
    unsigned offset;

    if (!bitmap || pos >= bitmap->n_bits) return -1;

    word = pos / BITS_PER_WORD;
    offset = (unsigned)(pos % BITS_PER_WORD);
    bitmap->bits[word] &= ~BITMASK(offset);
    return 0;
}

bool ds_bitmap_test(const ds_bitmap_t *bitmap, size_t pos)
{
    size_t   word;
    unsigned offset;

    if (!bitmap || pos >= bitmap->n_bits) return false;

    word = pos / BITS_PER_WORD;
    offset = (unsigned)(pos % BITS_PER_WORD);
    return (bitmap->bits[word] & BITMASK(offset)) != 0;
}

void ds_bitmap_clear_all(ds_bitmap_t *bitmap)
{
    if (!bitmap) return;
    memset(bitmap->bits, 0, bitmap->n_words * sizeof(uint64_t));
}

/*
 * 统计 1 的个数（popcount）。
 *
 * 使用内置 __builtin_popcountll（GCC/Clang）或手动实现。
 * 手动实现用"分治累加法"，又称 SWAR（SIMD Within A Register）：
 *   1. 每 2 位一组统计
 *   2. 每 4 位一组累加
 *   3. 每 8 位一组累加
 *   4. 最终汇总到 64 位
 *
 * 对于 MSVC，可使用 __popcnt64 内建函数。
 */
static unsigned popcount_u64(uint64_t x)
{
    /* 标准 SWAR popcount 算法 */
    x = x - ((x >> 1u) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2u) & 0x3333333333333333ULL);
    x = (x + (x >> 4u)) & 0x0F0F0F0F0F0F0F0FULL;
    return (unsigned)((x * 0x0101010101010101ULL) >> 56u);
}

size_t ds_bitmap_popcount(const ds_bitmap_t *bitmap)
{
    size_t total;

    if (!bitmap) return 0u;

    total = 0u;
    for (size_t i = 0u; i < bitmap->n_words; ++i) {
        total += popcount_u64(bitmap->bits[i]);
    }
    return total;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

void ds_bitmap_demo(void)
{
    printf("========== 位图演示 ==========\n");

    ds_bitmap_t *bm = ds_bitmap_create(100);
    if (!bm) { printf("创建失败\n"); return; }

    printf("\n【设置位 3, 7, 42, 99】\n");
    ds_bitmap_set(bm, 3);
    ds_bitmap_set(bm, 7);
    ds_bitmap_set(bm, 42);
    ds_bitmap_set(bm, 99);

    printf("测试位:\n");
    printf("  bit[3]: %s\n", ds_bitmap_test(bm, 3) ? "1" : "0");
    printf("  bit[7]: %s\n", ds_bitmap_test(bm, 7) ? "1" : "0");
    printf("  bit[5]: %s\n", ds_bitmap_test(bm, 5) ? "1" : "0");
    printf("  bit[42]: %s\n", ds_bitmap_test(bm, 42) ? "1" : "0");
    printf("  bit[99]: %s\n", ds_bitmap_test(bm, 99) ? "1" : "0");
    printf("总数（popcount）: %zu\n", ds_bitmap_popcount(bm));

    printf("\n【清除 bit[7]】\n");
    ds_bitmap_clear(bm, 7);
    printf("  bit[7]: %s\n", ds_bitmap_test(bm, 7) ? "1" : "0");
    printf("总数: %zu\n", ds_bitmap_popcount(bm));

    /* 布隆过滤器概念演示 */
    printf("\n【布隆过滤器概念】\n");
    printf("假设要判断 \"hello\" 是否在一个大集合中：\n");
    printf("  1. hash1(\"hello\")=3  → set bit[3]\n");
    printf("  2. hash2(\"hello\")=42 → set bit[42]\n");
    printf("  3. hash3(\"hello\")=99 → set bit[99]\n");
    printf("查询时，检查 bit[3], bit[42], bit[99] 是否都为 1：\n");
    printf("  都是 1 → \"hello\" 可能存在（有误判可能）\n");
    printf("  任一为 0 → \"hello\" 一定不存在\n");

    ds_bitmap_destroy(bm);
    printf("========== 位图演示结束 ==========\n");
}
