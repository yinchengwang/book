/*
 * xor_filter_core.c — Xor Filter 核心实现
 *
 * ============================================================
 * 原理
 * ============================================================
 *
 * Xor Filter 使用三个数组 A0, A1, A2，每个大小约为 1.23n 位。
 *
 * 每个元素 x 有：
 * - 三个位置：h0(x), h1(x), h2(x)（通过哈希函数计算）
 * - 一个指纹：fp(x)
 *
 * 查询时满足：
 *   A0[h0(x)] ^ A1[h1(x)] ^ A2[h2(x)] = fp(x)
 *
 * 构造过程使用贪婪算法：
 * 1. 收集所有元素的指纹
 * 2. 找出只在一个数组中出现的位置（确定分配）
 * 3. 递归处理剩余元素
 * 4. 如果有循环，则构造失败
 */

#include <db/index/hash/xor_filter.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* 最大构造迭代次数 */
#define XOR_MAX_CONSTRUCT_ITERATIONS 100

/* -----------------------------------------------------------------------
 * 内部数据结构
 * ----------------------------------------------------------------------- */

struct xor_filter {
    uint32_t *arrays[XOR_FILTER_ARRAY_COUNT];  /* 三个数组 */
    size_t    size;         /* 每个数组的大小 */
    size_t    item_count;   /* 已添加元素数量 */
    bool      built;        /* 是否已构造完成 */

    /* 临时存储：添加的元素及其指纹 */
    uint32_t *fingerprints;  /* 元素指纹数组 */
    uint32_t *h0_positions;  /* 位置 h0 */
    uint32_t *h1_positions; /* 位置 h1 */
    uint32_t *h2_positions; /* 位置 h2 */
};

/* -----------------------------------------------------------------------
 * 哈希函数
 * ----------------------------------------------------------------------- */

/*
 * fnv_1a_hash — FNV-1a 哈希函数
 *
 * 返回 64 位哈希值。
 */
static uint64_t fnv_1a_hash(const void *key, size_t keylen, uint32_t seed)
{
    const uint8_t *data = (const uint8_t *)key;
    uint64_t       hash = 14695981039346656037ull;  /* FNV offset basis */
    size_t         i;

    /* 加入种子 */
    hash ^= (uint64_t)seed;
    hash *= 1099511628211ull;  /* FNV prime */

    for (i = 0; i < keylen; i++) {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/*
 * get_positions — 计算元素在三个数组中的位置
 */
static void get_positions(const xor_filter_t *filter,
                         const void *key, size_t keylen,
                         uint32_t *h0, uint32_t *h1, uint32_t *h2)
{
    uint64_t hash0 = fnv_1a_hash(key, keylen, 0);
    uint64_t hash1 = fnv_1a_hash(key, keylen, 1);
    uint64_t hash2 = fnv_1a_hash(key, keylen, 2);

    *h0 = (uint32_t)(hash0 % filter->size);
    *h1 = (uint32_t)(hash1 % filter->size);
    *h2 = (uint32_t)(hash2 % filter->size);
}

/*
 * get_fingerprint — 计算元素的指纹
 */
static uint32_t get_fingerprint(const void *key, size_t keylen)
{
    uint64_t hash = fnv_1a_hash(key, keylen, 3);
    /* 取低 24 位作为指纹 */
    return (uint32_t)(hash & 0xFFFFFF);
}

/* -----------------------------------------------------------------------
 * 构造算法
 * ----------------------------------------------------------------------- */

/*
 * 内部元素结构
 */
typedef struct {
    uint32_t fingerprint;
    uint32_t h0, h1, h2;
    bool     assigned;
} xor_element_t;

/*
 * compute_size — 计算数组大小
 *
 * 公式: size = 1.23 * n，向上取整到 2 的幂次
 */
static size_t compute_size(size_t n)
{
    size_t base = (size_t)((double)n * 1.23);
    /* 向上取整到 8 的倍数 */
    size_t size = (base + 7) & ~((size_t)7);
    /* 确保最小值 */
    return size > 16 ? size : 16;
}

/*
 * construct_filter — 构造 Xor Filter
 *
 * 使用简单的贪婪算法：
 * 1. 找出只在一个位置出现的指纹
 * 2. 分配该指纹到那个位置
 * 3. 从其他元素的候选位置中移除该指纹
 * 4. 重复直到所有元素都分配完成
 */
static int construct_filter(xor_filter_t *filter)
{
    size_t n = filter->item_count;
    size_t size = filter->size;

    if (n == 0) {
        return 0;
    }

    /* 分配内部元素数组 */
    xor_element_t *elements = (xor_element_t *)calloc(n, sizeof(xor_element_t));
    if (!elements) {
        return -1;
    }

    /* 初始化元素 */
    for (size_t i = 0; i < n; i++) {
        elements[i].fingerprint = filter->fingerprints[i];
        elements[i].h0 = filter->h0_positions[i];
        elements[i].h1 = filter->h1_positions[i];
        elements[i].h2 = filter->h2_positions[i];
        elements[i].assigned = false;
    }

    /* 分配位置计数器（跟踪每个位置被多少未分配元素使用） */
    size_t *counter0 = (size_t *)calloc(size, sizeof(size_t));
    size_t *counter1 = (size_t *)calloc(size, sizeof(size_t));
    size_t *counter2 = (size_t *)calloc(size, sizeof(size_t));

    if (!counter0 || !counter1 || !counter2) {
        free(counter0);
        free(counter1);
        free(counter2);
        free(elements);
        return -1;
    }

    /* 初始化计数器 */
    for (size_t i = 0; i < n; i++) {
        counter0[elements[i].h0]++;
        counter1[elements[i].h1]++;
        counter2[elements[i].h2]++;
    }

    /* 初始化数组为 0 */
    for (size_t i = 0; i < size; i++) {
        filter->arrays[0][i] = 0;
        filter->arrays[1][i] = 0;
        filter->arrays[2][i] = 0;
    }

    /* 贪婪构造 */
    size_t assigned_count = 0;
    int iteration = 0;
    bool changed = true;

    while (assigned_count < n && changed && iteration < XOR_MAX_CONSTRUCT_ITERATIONS) {
        changed = false;
        iteration++;

        for (size_t i = 0; i < n; i++) {
            if (elements[i].assigned) {
                continue;
            }

            /* 找出只有一个候选位置可用的元素 */
            uint32_t single_pos = 0;
            int array_idx = -1;

            if (counter0[elements[i].h0] == 1) {
                single_pos = elements[i].h0;
                array_idx = 0;
            } else if (counter1[elements[i].h1] == 1) {
                single_pos = elements[i].h1;
                array_idx = 1;
            } else if (counter2[elements[i].h2] == 1) {
                single_pos = elements[i].h2;
                array_idx = 2;
            }

            if (array_idx >= 0) {
                /* 分配此元素 */
                elements[i].assigned = true;
                assigned_count++;
                changed = true;

                /* 将指纹 XOR 到对应位置 */
                filter->arrays[array_idx][single_pos] ^= elements[i].fingerprint;

                /* 从其他元素中移除此指纹的影响 */
                for (size_t j = 0; j < n; j++) {
                    if (elements[j].assigned) {
                        continue;
                    }

                    if (elements[j].h0 == single_pos) {
                        counter0[single_pos]--;
                    }
                    if (elements[j].h1 == single_pos) {
                        counter1[single_pos]--;
                    }
                    if (elements[j].h2 == single_pos) {
                        counter2[single_pos]--;
                    }
                }
            }
        }
    }

    /* 检查是否所有元素都分配完成 */
    int result = 0;
    if (assigned_count < n) {
        /* 构造失败，返回错误 */
        result = -1;
    }

    free(counter0);
    free(counter1);
    free(counter2);
    free(elements);

    return result;
}

/* -----------------------------------------------------------------------
 * 公开 API 实现
 * ----------------------------------------------------------------------- */

xor_filter_t *xor_filter_create(const xor_filter_config_t *config)
{
    xor_filter_t *filter;
    size_t        expected_items;
    size_t        size;

    /* 处理配置参数 */
    if (config && config->expected_items > 0) {
        expected_items = config->expected_items;
    } else {
        expected_items = 1000;
    }

    /* 计算数组大小 */
    size = compute_size(expected_items);

    /* 分配 Filter 结构 */
    filter = (xor_filter_t *)malloc(sizeof(xor_filter_t));
    if (!filter) {
        return NULL;
    }

    /* 分配三个数组 */
    for (int i = 0; i < XOR_FILTER_ARRAY_COUNT; i++) {
        filter->arrays[i] = (uint32_t *)calloc(size, sizeof(uint32_t));
        if (!filter->arrays[i]) {
            for (int j = 0; j < i; j++) {
                free(filter->arrays[j]);
            }
            free(filter);
            return NULL;
        }
    }

    /* 分配临时存储 */
    filter->fingerprints = (uint32_t *)malloc(expected_items * sizeof(uint32_t));
    filter->h0_positions = (uint32_t *)malloc(expected_items * sizeof(uint32_t));
    filter->h1_positions = (uint32_t *)malloc(expected_items * sizeof(uint32_t));
    filter->h2_positions = (uint32_t *)malloc(expected_items * sizeof(uint32_t));

    if (!filter->fingerprints || !filter->h0_positions ||
        !filter->h1_positions || !filter->h2_positions) {
        free(filter->fingerprints);
        free(filter->h0_positions);
        free(filter->h1_positions);
        free(filter->h2_positions);
        for (int i = 0; i < XOR_FILTER_ARRAY_COUNT; i++) {
            free(filter->arrays[i]);
        }
        free(filter);
        return NULL;
    }

    filter->size = size;
    filter->item_count = 0;
    filter->built = false;

    return filter;
}

void xor_filter_destroy(xor_filter_t *filter)
{
    if (!filter) return;

    for (int i = 0; i < XOR_FILTER_ARRAY_COUNT; i++) {
        free(filter->arrays[i]);
    }
    free(filter->fingerprints);
    free(filter->h0_positions);
    free(filter->h1_positions);
    free(filter->h2_positions);
    free(filter);
}

int xor_filter_add(xor_filter_t *filter, const void *key, size_t keylen)
{
    uint32_t h0, h1, h2;
    uint32_t fp;

    if (!filter || !key || keylen == 0) {
        return -1;
    }

    if (filter->built) {
        /* 构造完成后不能再添加 */
        return -1;
    }

    /* 计算位置和指纹 */
    get_positions(filter, key, keylen, &h0, &h1, &h2);
    fp = get_fingerprint(key, keylen);

    /* 检查是否已存在相同的元素（基于哈希位置） */
    for (size_t i = 0; i < filter->item_count; i++) {
        if (filter->h0_positions[i] == h0 &&
            filter->h1_positions[i] == h1 &&
            filter->h2_positions[i] == h2) {
            return 0;  /* 已存在，返回成功但不重复添加 */
        }
    }

    /* 存储到临时数组 */
    size_t idx = filter->item_count;
    filter->fingerprints[idx] = fp;
    filter->h0_positions[idx] = h0;
    filter->h1_positions[idx] = h1;
    filter->h2_positions[idx] = h2;
    filter->item_count++;

    return 0;
}

bool xor_filter_query(const xor_filter_t *filter, const void *key, size_t keylen)
{
    uint32_t h0, h1, h2;
    uint32_t fp;
    uint32_t xored;

    if (!filter || !key || keylen == 0) {
        return false;
    }

    if (!filter->built) {
        return false;
    }

    /* 计算位置和指纹 */
    get_positions(filter, key, keylen, &h0, &h1, &h2);
    fp = get_fingerprint(key, keylen);

    /* XOR 解码 */
    xored = filter->arrays[0][h0] ^ filter->arrays[1][h1] ^ filter->arrays[2][h2];

    return xored == fp;
}

int xor_filter_build(xor_filter_t *filter)
{
    if (!filter) {
        return -1;
    }

    if (filter->built) {
        return 0;
    }

    if (filter->item_count == 0) {
        filter->built = true;
        return 0;
    }

    /* 执行构造算法 */
    if (construct_filter(filter) != 0) {
        return -1;
    }

    filter->built = true;

    /* 释放临时存储 */
    free(filter->fingerprints);
    free(filter->h0_positions);
    free(filter->h1_positions);
    free(filter->h2_positions);
    filter->fingerprints = NULL;
    filter->h0_positions = NULL;
    filter->h1_positions = NULL;
    filter->h2_positions = NULL;

    return 0;
}

bool xor_filter_is_built(const xor_filter_t *filter)
{
    return filter ? filter->built : false;
}

size_t xor_filter_get_size(const xor_filter_t *filter)
{
    return filter ? filter->size : 0;
}

size_t xor_filter_get_item_count(const xor_filter_t *filter)
{
    return filter ? filter->item_count : 0;
}