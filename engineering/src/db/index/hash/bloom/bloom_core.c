/*
 * bloom_core.c — Bloom Filter 核心实现
 *
 * ============================================================
 * 原理
 * ============================================================
 *
 * Bloom Filter 使用 k 个哈希函数将元素映射到位数组的 k 个位置。
 * - 添加元素：将这 k 个位置设为 1
 * - 查询元素：检查这 k 个位置是否都为 1
 *
 * 位数组大小计算（最优参数）：
 *   m = -n * ln(p) / (ln(2)^2)
 *   其中 n = 预期元素数，p = 假阳性率
 *
 * 哈希函数数量计算：
 *   k = (m/n) * ln(2)
 *
 * 使用双哈希法生成 k 个哈希值：
 *   h(i, key) = h1(key) + i * h2(key) mod m
 *   其中 h1 和 h2 是两个独立的哈希函数
 */

#include <db/index/hash/bloom.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 默认假阳性率 1% */
#define BLOOM_DEFAULT_FP_RATE 0.01

/* 位数组对齐 */
#define BLOOM_ALIGN_BITS 8

/* -----------------------------------------------------------------------
 * 内部数据结构
 * ----------------------------------------------------------------------- */

struct bloom_filter {
    uint8_t *bits;        /* 位数组 */
    size_t   size;        /* 位数组大小（位数） */
    size_t   byte_size;   /* 位数组字节大小 */
    size_t   item_count;  /* 已添加元素数量 */
    size_t   hash_count;  /* 哈希函数数量 */
};

/* -----------------------------------------------------------------------
 * 哈希函数实现
 * ----------------------------------------------------------------------- */

/*
 * fnv_1a_hash — FNV-1a 32 位哈希函数
 *
 * FNV-1a 是一种快速、分布均匀的哈希函数：
 *   offset_basis = 2166136261  (0x811c9dc5)
 *   prime        = 16777619    (0x01000193)
 */
static uint32_t fnv_1a_hash(const void *key, size_t keylen)
{
    const uint8_t *data = (const uint8_t *)key;
    uint32_t       hash = 2166136261u;
    size_t         i;

    for (i = 0; i < keylen; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

/*
 * double_hash — 双哈希法生成 k 个哈希值
 *
 * 使用两个基础哈希函数生成 k 个哈希位置：
 *   h(i) = h1 + i * h2 mod m
 *
 * 这种方法避免了计算多个独立哈希函数的开销。
 *
 * 参数:
 *   filter  - Bloom Filter
 *   key     - 键
 *   keylen  - 键长度
 *   indices - 输出数组，至少需要 hash_count 个元素
 */
static void double_hash(const bloom_filter_t *filter,
                        const void *key, size_t keylen,
                        size_t *indices)
{
    const uint8_t *key_bytes = (const uint8_t *)key;
    uint32_t h1 = fnv_1a_hash(key, keylen);
    uint32_t h2 = fnv_1a_hash(key_bytes + keylen / 2, keylen / 2 + keylen % 2);

    size_t i;
    for (i = 0; i < filter->hash_count; i++) {
        indices[i] = (size_t)((h1 + (uint64_t)i * h2) % filter->size);
    }
}

/* -----------------------------------------------------------------------
 * 位操作辅助函数
 * ----------------------------------------------------------------------- */

/*
 * 设置指定位为 1
 */
static inline void set_bit(uint8_t *bits, size_t pos)
{
    bits[pos / 8] |= (uint8_t)(1 << (pos % 8));
}

/*
 * 获取指定位的值
 */
static inline int get_bit(const uint8_t *bits, size_t pos)
{
    return (bits[pos / 8] >> (pos % 8)) & 1;
}

/* -----------------------------------------------------------------------
 * 参数计算
 * ----------------------------------------------------------------------- */

/*
 * 计算最优的位数组大小
 *
 * 公式: m = -n * ln(p) / (ln(2)^2)
 */
static size_t optimal_size(size_t n, double p)
{
    if (p <= 0 || p >= 1) {
        p = BLOOM_DEFAULT_FP_RATE;
    }
    /*
     * 使用公式 m = -n * ln(p) / (ln(2)^2) 计算最优大小
     * ln(2)^2 ≈ 0.48045
     */
    double m = -((double)n * log(p)) / 0.4804530139182014;
    /* 向上取整并对齐到 8 的倍数 */
    size_t size = (size_t)ceil(m);
    size = (size + 7) & ~((size_t)7);
    return size > 0 ? size : 8;
}

/*
 * 计算最优的哈希函数数量
 *
 * 公式: k = (m/n) * ln(2)
 */
static size_t optimal_hash_count(size_t n, size_t m)
{
    if (n == 0) return 1;
    double k = ((double)m / (double)n) * 0.6931471805599453;  /* ln(2) */
    size_t count = (size_t)ceil(k);
    return count > 0 ? count : 1;
}

/* -----------------------------------------------------------------------
 * 公开 API 实现
 * ----------------------------------------------------------------------- */

bloom_filter_t *bloom_create(const bloom_config_t *config)
{
    bloom_filter_t *filter;
    size_t          expected_items;
    double          fp_rate;

    /* 处理配置参数 */
    if (config) {
        expected_items = config->expected_items > 0 ? config->expected_items : 1000;
        fp_rate = config->false_positive_rate;
        if (fp_rate <= 0 || fp_rate >= 1) {
            fp_rate = BLOOM_DEFAULT_FP_RATE;
        }
    } else {
        expected_items = 1000;
        fp_rate = BLOOM_DEFAULT_FP_RATE;
    }

    /* 分配 Filter 结构 */
    filter = (bloom_filter_t *)malloc(sizeof(bloom_filter_t));
    if (!filter) {
        return NULL;
    }

    /* 计算最优参数 */
    filter->size = optimal_size(expected_items, fp_rate);
    filter->hash_count = optimal_hash_count(expected_items, filter->size);

    /* 分配位数组 */
    filter->byte_size = (filter->size + 7) / 8;
    filter->bits = (uint8_t *)calloc(filter->byte_size, sizeof(uint8_t));
    if (!filter->bits) {
        free(filter);
        return NULL;
    }

    filter->item_count = 0;
    return filter;
}

void bloom_destroy(bloom_filter_t *filter)
{
    if (!filter) return;
    free(filter->bits);
    free(filter);
}

int bloom_add(bloom_filter_t *filter, const void *key, size_t keylen)
{
    size_t *indices;
    size_t  i;

    if (!filter || !key || keylen == 0) {
        return -1;
    }

    /* 分配临时数组存储哈希位置 */
    indices = (size_t *)malloc(filter->hash_count * sizeof(size_t));
    if (!indices) {
        return -1;
    }

    /* 计算 k 个哈希位置并设置位 */
    double_hash(filter, key, keylen, indices);
    for (i = 0; i < filter->hash_count; i++) {
        set_bit(filter->bits, indices[i]);
    }

    filter->item_count++;
    free(indices);
    return 0;
}

bool bloom_query(const bloom_filter_t *filter, const void *key, size_t keylen)
{
    size_t *indices;
    size_t  i;
    int     result = 1;

    if (!filter || !key || keylen == 0) {
        return false;
    }

    /* 分配临时数组 */
    indices = (size_t *)malloc(filter->hash_count * sizeof(size_t));
    if (!indices) {
        return false;
    }

    /* 计算 k 个哈希位置并检查 */
    double_hash(filter, key, keylen, indices);
    for (i = 0; i < filter->hash_count; i++) {
        if (!get_bit(filter->bits, indices[i])) {
            result = 0;
            break;
        }
    }

    free(indices);
    return result != 0;
}

size_t bloom_get_size(const bloom_filter_t *filter)
{
    return filter ? filter->size : 0;
}

size_t bloom_get_hash_count(const bloom_filter_t *filter)
{
    return filter ? filter->hash_count : 0;
}

size_t bloom_get_item_count(const bloom_filter_t *filter)
{
    return filter ? filter->item_count : 0;
}

size_t bloom_get_memory_usage(const bloom_filter_t *filter)
{
    return filter ? filter->byte_size : 0;
}