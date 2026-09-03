/*
 * direct_map.h
 *
 * DirectMap 数据结构 —— 向量 ID 到倒排桶位置的映射表。
 * 对齐 FAISS DirectMap 的语义：Type 1 使用数组按 id 直接索引。
 *
 * direct_map[id] = (list_no << 32) | offset
 *   高 32 位: list_no (桶编号)
 *   低 32 位: offset   (桶内偏移)
 */

#ifndef FAISS_DIRECT_MAP_H
#define FAISS_DIRECT_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faiss_direct_map {
    int32_t type;       /* 0=无映射, 1=数组映射 */
    int64_t *array;     /* type=1 时: array[id] = (list_no << 32) | offset */
    int32_t size;       /* 数组容量 */
} faiss_direct_map_t;

/**
 * 初始化 DirectMap。
 * type=1 时分配 capacity 大小的数组，所有槽初始值为 -1。
 * type=0 时不分配。
 * 成功返回 0，失败返回 -1。
 */
int faiss_direct_map_init(faiss_direct_map_t *dm, int32_t type, int32_t capacity);

/**
 * 释放 DirectMap 占用的内存并重置字段。
 */
void faiss_direct_map_drop(faiss_direct_map_t *dm);

/**
 * 添加或更新映射。
 * id 超出数组范围时自动扩容至 id * 2。
 */
void faiss_direct_map_add(faiss_direct_map_t *dm, int32_t id, int64_t list_no, int64_t offset);

/**
 * 查询映射。返回 (list_no << 32 | offset)，不存在则返回 -1。
 */
int64_t faiss_direct_map_get(const faiss_direct_map_t *dm, int32_t id);

/**
 * 将指定 id 的映射重置为 -1（标记为不存在）。
 */
void faiss_direct_map_remove(faiss_direct_map_t *dm, int32_t id);

/**
 * 将所有映射槽重置为 -1。
 */
void faiss_direct_map_clear(faiss_direct_map_t *dm);

#ifdef __cplusplus
}
#endif

#endif /* FAISS_DIRECT_MAP_H */
