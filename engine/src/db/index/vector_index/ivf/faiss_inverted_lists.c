/*
 * InvertedLists 抽象层实现: 统一管理 IVF 倒排桶的动态扩容和访问。
 *
 * === 存储模型 ===
 *
 * 采用数组阵列 (Array of Arrays) 结构:
 *   ids[list_no]   → 指向该桶的 int32_t 数组（向量 ID）
 *   codes[list_no]  → 指向该桶的 uint8_t 数组（编码数据，code_size > 0 时有效）
 *   sizes[list_no]  → 当前桶内元素数
 *   caps[list_no]   → 当前桶容量（>= size）
 *
 * 每个桶独立管理容量，避免全局锁和内存碎片。
 *
 * === 扩容策略 ===
 *
 * 初始容量: 32（FAISS_INVERTED_LISTS_INITIAL_CAPACITY）
 * 扩容方式: 2× 倍增直到满足需求
 * 上限: INT32_MAX（拒绝不合理的大容量请求）
 *
 * 倍增策略保证均摊 O(1) 的追加开销（类似 std::vector）。
 *
 * === 墓碑机制 ===
 *
 * 删除元素时将 id 标为 -1（墓碑），不移动后续元素。
 * compact_list 时才物理移除墓碑，将有效元素前移。
 * 这种设计避免了删除操作中的 O(n) 数据搬移。
 */

#include <db/index/vector_index/ivf/inverted_lists.h>
#include <stdlib.h>
#include <string.h>

#define FAISS_INVERTED_LISTS_INITIAL_CAPACITY 32

/*
 * 创建倒排列表: 分配 nlist 个空桶。
 * code_size = 0 时仅管理 ID 数组，不分配 codes 指针数组。
 */
faiss_inverted_lists_t *faiss_inverted_lists_create(size_t nlist, size_t code_size)
{
    if (nlist == 0) {
        return NULL;
    }

    /* calloc 确保指针数组初始为 NULL（各桶在首次 add 时才分配） */
    faiss_inverted_lists_t *il = (faiss_inverted_lists_t *)calloc(1, sizeof(faiss_inverted_lists_t));
    if (!il) {
        return NULL;
    }

    il->nlist = nlist;
    il->code_size = code_size;

    il->ids = (int32_t **)calloc(nlist, sizeof(int32_t *));
    il->sizes = (size_t *)calloc(nlist, sizeof(size_t));
    il->caps = (size_t *)calloc(nlist, sizeof(size_t));

    if (code_size > 0) {
        il->codes = (uint8_t **)calloc(nlist, sizeof(uint8_t *));
    }

    /* 任一分配失败则回滚 */
    if (!il->ids || !il->sizes || !il->caps || (code_size > 0 && !il->codes)) {
        faiss_inverted_lists_drop(il);
        return NULL;
    }

    return il;
}

/* 释放所有桶及其内部数组，最后释放主结构 */
void faiss_inverted_lists_drop(faiss_inverted_lists_t *il)
{
    if (!il) {
        return;
    }

    for (size_t j = 0; j < il->nlist; j++) {
        free(il->ids[j]);
        il->ids[j] = NULL;
        if (il->codes) {
            free(il->codes[j]);
            il->codes[j] = NULL;
        }
    }

    free(il->ids);
    free(il->codes);
    free(il->sizes);
    free(il->caps);
    free(il);
}

/*
 * 确保指定桶容量 >= required_size。
 * 容量不足时直接 realloc 到所需大小（不做倍增，调用方已经做了倍增计算）。
 * 同时扩容 ids 和 codes（若 code_size > 0）。
 */
int faiss_inverted_lists_resize_list(faiss_inverted_lists_t *il, size_t list_no, size_t required_size)
{
    if (!il || list_no >= il->nlist) {
        return -1;
    }

    if (il->caps[list_no] >= required_size) {
        return 0;
    }

    /* 拒绝不合理的大容量 */
    if (required_size > (size_t)INT32_MAX) {
        return -1;
    }

    int32_t *new_ids = (int32_t *)realloc(il->ids[list_no], required_size * sizeof(int32_t));
    if (!new_ids) {
        return -1;
    }
    il->ids[list_no] = new_ids;

    if (il->code_size > 0 && il->codes) {
        uint8_t *new_codes = (uint8_t *)realloc(il->codes[list_no], required_size * il->code_size);
        if (!new_codes) {
            return -1;
        }
        il->codes[list_no] = new_codes;
    }

    il->caps[list_no] = required_size;
    return 0;
}

/*
 * 向指定桶追加一个元素。
 *
 * 容量不足时自动触发 2× 倍增扩容:
 *   - 空桶首次追加: 分配 INITIAL_CAPACITY (32)
 *   - 非空桶扩容: caps × 2 直到满足需求
 *
 * code 为 NULL 时仅写入 ID（非量化模式的正常行为，编码存在单独的 codes 数组中）。
 */
int faiss_inverted_lists_add_entry(faiss_inverted_lists_t *il, size_t list_no, int32_t id, const uint8_t *code)
{
    if (!il || list_no >= il->nlist) {
        return -1;
    }

    size_t required = il->sizes[list_no] + 1;

    if (il->caps[list_no] < required) {
        size_t new_cap = il->caps[list_no] > 0 ? il->caps[list_no] : FAISS_INVERTED_LISTS_INITIAL_CAPACITY;
        while (new_cap < required) {
            new_cap *= 2;
        }
        if (faiss_inverted_lists_resize_list(il, list_no, new_cap) != 0) {
            return -1;
        }
    }

    il->ids[list_no][il->sizes[list_no]] = id;

    if (il->code_size > 0 && code && il->codes && il->codes[list_no]) {
        memcpy(&il->codes[list_no][il->sizes[list_no] * il->code_size], code, il->code_size);
    }

    il->sizes[list_no]++;
    return 0;
}

/* 清空所有桶: size 归零，保留已分配容量（避免下次 add 时重新 malloc） */
void faiss_inverted_lists_reset(faiss_inverted_lists_t *il)
{
    if (!il) {
        return;
    }

    for (size_t j = 0; j < il->nlist; j++) {
        il->sizes[j] = 0;
    }
}

/*
 * 标记墓碑: 将桶内指定偏移的 id 设为 -1。
 * 不移动后续元素，避免 O(n) 数据搬移。
 * 物理删除在 compact_list 中统一执行。
 */
int faiss_inverted_lists_remove_entry(faiss_inverted_lists_t *il, size_t list_no, size_t offset)
{
    if (!il || list_no >= il->nlist) {
        return -1;
    }
    if (offset >= il->sizes[list_no]) {
        return -1;
    }

    il->ids[list_no][offset] = -1;
    return 0;
}

/*
 * 真空压缩: 将桶内所有有效元素（id != -1）前移，覆盖墓碑。
 *
 * 双指针算法:
 *   read:  遍历所有位置
 *   write: 下一个有效元素应写入的位置
 *
 * 当 read != write 时拷贝，否则跳过（元素已在正确位置）。
 * 压缩后 sizes[list_no] 更新为有效元素数。
 * 返回压缩后的桶大小。
 */
size_t faiss_inverted_lists_compact_list(faiss_inverted_lists_t *il, size_t list_no)
{
    if (!il || list_no >= il->nlist) {
        return 0;
    }

    int32_t *ids = il->ids[list_no];
    uint8_t *codes = il->codes ? il->codes[list_no] : NULL;
    size_t size = il->sizes[list_no];
    size_t code_sz = il->code_size;

    size_t write = 0;
    for (size_t read = 0; read < size; read++) {
        if (ids[read] != -1) {
            if (read != write) {
                ids[write] = ids[read];
                if (codes && code_sz > 0) {
                    memcpy(&codes[write * code_sz], &codes[read * code_sz], code_sz);
                }
            }
            write++;
        }
    }

    il->sizes[list_no] = write;
    return write;
}
