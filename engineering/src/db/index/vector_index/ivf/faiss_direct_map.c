/*
 * DirectMap 实现: 向量 ID → (list_no, offset) 数组映射。
 *
 * === 设计意图 ===
 *
 * 在倒排索引中，向量存储在桶内。要删除某个向量，需要知道它在哪个桶的
 * 哪个位置。没有 DirectMap 时需要遍历所有桶搜索该 ID，O(N) 代价。
 *
 * DirectMap 提供 O(1) 的 ID → 位置查询:
 *   direct_map[id] = (list_no << 32) | offset
 *     - 高 32 位: list_no (桶编号)
 *     - 低 32 位: offset   (桶内偏移)
 *
 * === 编码格式 ===
 *
 * 使用 int64_t 打包两个 int32_t:
 *   entry = ((int64_t)list_no << 32) | (offset & 0xFFFFFFFF)
 *
 * -1 表示"不存在"或"已删除"。
 * 数组按需扩容: 当 id >= size 时，扩容到 id * 2 + 1。
 *
 * === Type 字段 ===
 *
 * type = 0: 无映射（不启用 DirectMap）
 * type = 1: 数组映射（按 id 直接索引）
 *
 * 当前只实现了 type=1，type=0 预留给不需要删除/重建功能的场景。
 */

#include <db/index/vector_index/ivf/direct_map.h>
#include <stdlib.h>
#include <string.h>

/*
 * 初始化 DirectMap。
 * type=1 时分配 capacity 大小的数组，所有槽初始值为 -1。
 * capacity=0 允许延迟分配（后续 add 时按需扩容）。
 */
int faiss_direct_map_init(faiss_direct_map_t *dm, int32_t type, int32_t capacity)
{
    if (!dm) {
        return -1;
    }
    dm->type = type;
    dm->size = 0;
    dm->array = NULL;

    if (type == 0) {
        return 0;
    }

    if (capacity > 0) {
        dm->array = (int64_t *)malloc((size_t)capacity * sizeof(int64_t));
        if (!dm->array) {
            return -1;
        }
        dm->size = capacity;

        /* -1 表示槽位为空（不存在该 id 的映射） */
        for (int32_t i = 0; i < capacity; i++) {
            dm->array[i] = -1;
        }
    }

    return 0;
}

/* 释放数组并重置字段 */
void faiss_direct_map_drop(faiss_direct_map_t *dm)
{
    if (!dm) {
        return;
    }
    free(dm->array);
    dm->array = NULL;
    dm->type = 0;
    dm->size = 0;
}

/*
 * 添加或更新映射。
 *
 * id 超出数组范围时自动扩容至 id * 2 + 1。
 * 扩容策略: new_size = id * 2 + 1 确保了至少翻倍，均摊 O(1)。
 * 新增的槽填充为 -1。
 *
 * 编码: entry = (list_no << 32) | (offset & 0xFFFFFFFF)
 */
void faiss_direct_map_add(faiss_direct_map_t *dm, int32_t id, int64_t list_no, int64_t offset)
{
    if (!dm || dm->type == 0 || id < 0) {
        return;
    }

    if (id >= dm->size) {
        int32_t new_size = id * 2 + 1;
        int64_t *new_array = (int64_t *)realloc(dm->array, (size_t)new_size * sizeof(int64_t));
        if (!new_array) {
            return;
        }
        for (int32_t i = dm->size; i < new_size; i++) {
            new_array[i] = -1;
        }
        dm->array = new_array;
        dm->size = new_size;
    }

    dm->array[id] = (list_no << 32) | (offset & 0xFFFFFFFF);
}

/*
 * 查询映射。
 * 返回编码后的 entry，不存在则返回 -1。
 * 调用方需要解码: list_no = entry >> 32, offset = entry & 0xFFFFFFFF
 */
int64_t faiss_direct_map_get(const faiss_direct_map_t *dm, int32_t id)
{
    if (!dm || dm->type == 0 || id < 0 || id >= dm->size) {
        return -1;
    }
    return dm->array[id];
}

/* 将指定 id 的映射重置为 -1（标记为不存在） */
void faiss_direct_map_remove(faiss_direct_map_t *dm, int32_t id)
{
    if (!dm || dm->type == 0 || id < 0 || id >= dm->size) {
        return;
    }
    dm->array[id] = -1;
}

/*
 * 清空所有映射（不释放数组，保留容量）。
 * 配合 faiss_inverted_lists_reset 使用，实现快速重置。
 */
void faiss_direct_map_clear(faiss_direct_map_t *dm)
{
    if (!dm || dm->type == 0) {
        return;
    }
    for (int32_t i = 0; i < dm->size; i++) {
        dm->array[i] = -1;
    }
}
