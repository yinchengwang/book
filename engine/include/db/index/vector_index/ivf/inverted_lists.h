/*
 * inverted_lists.h
 *
 * InvertedLists 抽象层 —— 统一管理 IVF 倒排桶的 IDs 和 Codes 存储。
 * 对齐 FAISS InvertedLists / ArrayInvertedLists 的语义。
 */

#ifndef FAISS_INVERTED_LISTS_H
#define FAISS_INVERTED_LISTS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faiss_inverted_lists {
    size_t nlist;          /* 倒排桶数量 */
    size_t code_size;      /* 单向量编码大小 (0 表示不存编码) */
    int32_t **ids;         /* 每个桶的向量 ID 数组 */
    uint8_t **codes;       /* 每个桶的编码数组 (code_size > 0 时有效) */
    size_t *sizes;         /* 每个桶当前元素数 */
    size_t *caps;          /* 每个桶当前容量 */
} faiss_inverted_lists_t;

/**
 * 分配 nlist 个空桶。
 * code_size = 0 时仅管理 ID 数组，不分配编码存储。
 */
faiss_inverted_lists_t *faiss_inverted_lists_create(size_t nlist, size_t code_size);

/**
 * 释放所有桶及其内部数组占用的内存。
 */
void faiss_inverted_lists_drop(faiss_inverted_lists_t *il);

/**
 * 返回指定桶的当前元素数。
 */
static inline size_t faiss_inverted_lists_list_size(const faiss_inverted_lists_t *il, size_t list_no)
{
    return il->sizes[list_no];
}

/**
 * 返回指定桶的 ID 数组只读指针。
 */
static inline const int32_t *faiss_inverted_lists_get_ids(const faiss_inverted_lists_t *il, size_t list_no)
{
    return il->ids[list_no];
}

/**
 * 返回指定桶的编码数组只读指针。
 * 若 code_size 为 0 则始终返回 NULL。
 */
static inline const uint8_t *faiss_inverted_lists_get_codes(const faiss_inverted_lists_t *il, size_t list_no)
{
    if (il->code_size == 0) return NULL;
    return il->codes[list_no];
}

/**
 * 向指定桶追加一个元素。
 * 容量不足时自动 2× 扩容（初始容量 32）。
 * 成功返回 0，失败返回 -1。
 */
int faiss_inverted_lists_add_entry(faiss_inverted_lists_t *il, size_t list_no, int32_t id, const uint8_t *code);

/**
 * 确保桶容量至少为 required_size。
 * 成功返回 0，失败返回 -1。
 */
int faiss_inverted_lists_resize_list(faiss_inverted_lists_t *il, size_t list_no, size_t required_size);

/**
 * 清空所有桶（size 归零，保留已分配容量）。
 */
void faiss_inverted_lists_reset(faiss_inverted_lists_t *il);

/**
 * 将桶内指定偏移位置的元素标记为墓碑（id = -1）。
 * 成功返回 0，失败返回 -1。
 */
int faiss_inverted_lists_remove_entry(faiss_inverted_lists_t *il, size_t list_no, size_t offset);

/**
 * 真空清理：将桶内所有墓碑后的有效元素前移，释放尾部空间。
 * 返回压缩后的桶大小。
 */
size_t faiss_inverted_lists_compact_list(faiss_inverted_lists_t *il, size_t list_no);

#ifdef __cplusplus
}
#endif

#endif /* FAISS_INVERTED_LISTS_H */
