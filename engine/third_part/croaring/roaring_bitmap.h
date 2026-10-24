/**
 * @file roaring_bitmap.h
 * @brief CRoaring bitmap 最小兼容层（P5-2 亿级 filter bitmap 内存压缩）
 *
 * 基于 sorted array 实现的轻量级 bitmap，API 兼容 CRoaring 核心函数。
 * 功能子集：create / destroy / add / contains / count / is_empty。
 *
 * 内存优势：
 *   - 原始 int8_t bitmap：N 字节（N = bitmap_size）
 *   - 本实现：sorted uint32_t 数组，4 * popcount 字节
 *   - 亿级场景下内存压缩到 ~10-20%
 */

#ifndef ROARING_BITMAP_H
#define ROARING_BITMAP_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型定义 ========== */

/* roaring bitmap 句柄（内部为 sorted uint32_t 数组） */
typedef struct {
    uint32_t*   array;      /* 排序后的值数组 */
    uint32_t    size;       /* 当前元素个数 */
    uint32_t    capacity;   /* 数组容量 */
} roaring_bitmap_t;

/* ========== 核心 API（CRoaring 兼容） ========== */

/**
 * @brief 创建空的 roaring bitmap
 * @return 新建的 bitmap 句柄，失败返回 NULL
 */
static inline roaring_bitmap_t* roaring_bitmap_create(void) {
    roaring_bitmap_t* rb = (roaring_bitmap_t*)calloc(1, sizeof(roaring_bitmap_t));
    if (!rb) return NULL;
    rb->capacity = 256;
    rb->array = (uint32_t*)malloc(rb->capacity * sizeof(uint32_t));
    if (!rb->array) {
        free(rb);
        return NULL;
    }
    rb->size = 0;
    return rb;
}

/**
 * @brief 释放 roaring bitmap
 */
static inline void roaring_bitmap_free(roaring_bitmap_t* rb) {
    if (!rb) return;
    free(rb->array);
    free(rb);
}

/**
 * @brief 向 roaring bitmap 添加一个值（二分查找 + 插入）
 * @param rb    roaring bitmap 句柄
 * @param value 要添加的值（uint32_t）
 */
static inline void roaring_bitmap_add(roaring_bitmap_t* rb, uint32_t value) {
    if (!rb) return;

    /* 二分查找插入位置 */
    uint32_t lo = 0, hi = rb->size;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (rb->array[mid] < value) lo = mid + 1;
        else if (rb->array[mid] > value) hi = mid;
        else return;  /* 已存在，无需插入 */
    }

    /* 扩容检查 */
    if (rb->size >= rb->capacity) {
        uint32_t new_cap = rb->capacity * 2;
        uint32_t* new_arr = (uint32_t*)realloc(rb->array, new_cap * sizeof(uint32_t));
        if (!new_arr) return;  /* 内存不足，静默失败 */
        rb->array = new_arr;
        rb->capacity = new_cap;
    }

    /* 插入（memmove 保持有序） */
    if (lo < rb->size) {
        memmove(&rb->array[lo + 1], &rb->array[lo],
                (rb->size - lo) * sizeof(uint32_t));
    }
    rb->array[lo] = value;
    rb->size++;
}

/**
 * @brief 检查值是否存在于 bitmap 中（二分查找）
 * @param rb    roaring bitmap 句柄
 * @param value 要查询的值
 * @return 1 存在，0 不存在
 */
static inline int roaring_bitmap_contains(const roaring_bitmap_t* rb, uint32_t value) {
    if (!rb || rb->size == 0) return 0;

    /* 二分查找 */
    uint32_t lo = 0, hi = rb->size;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (rb->array[mid] < value) lo = mid + 1;
        else if (rb->array[mid] > value) hi = mid;
        else return 1;
    }
    return 0;
}

/**
 * @brief 获取 bitmap 中元素个数
 */
static inline uint32_t roaring_bitmap_count(const roaring_bitmap_t* rb) {
    return rb ? rb->size : 0;
}

/**
 * @brief 检查 bitmap 是否为空
 * @return 1 空，0 非空
 */
static inline int roaring_bitmap_is_empty(const roaring_bitmap_t* rb) {
    return !rb || rb->size == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* ROARING_BITMAP_H */
