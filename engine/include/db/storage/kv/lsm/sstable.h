/**
 * @file sstable.h
 * @brief SSTable 文件格式和读写接口
 */
#ifndef DB_STORAGE_KV_LSM_SSTABLE_H
#define DB_STORAGE_KV_LSM_SSTABLE_H

#include "lsm_tree.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SSTable 文件格式
 * ======================================================================== */

/*
 * SSTable 文件布局：
 * +------------------+
 * | Header (64 bytes) |
 * +------------------+
 * | Data Section      |
 * | (entries)        |
 * +------------------+
 * | Index Section    |
 * | (key + offset)   |
 * +------------------+
 * | Bloom Filter     |
 * | (bits)           |
 * +------------------+
 * | Footer (16 bytes) |
 * +------------------+
 *
 * Header:
 *   magic:      4 bytes (0x53535442 = "SSTB")
 *   version:     4 bytes
 *   min_key:    8 bytes
 *   max_key:    8 bytes
 *   min_seq:    8 bytes
 *   max_seq:    8 bytes
 *   num_entries:4 bytes
 *   index_offset:8 bytes
 *   bloom_offset:8 bytes
 *   reserved:   4 bytes
 *
 * Entry:
 *   key_size:   4 bytes
 *   value_size: 4 bytes
 *   seq:        8 bytes
 *   op:         1 byte (0=PUT, 1=DELETE)
 *   key:        key_size bytes
 *   value:      value_size bytes
 *
 * Index Entry:
 *   key:        variable
 *   offset:     8 bytes
 *
 * Footer:
 *   bloom_size: 4 bytes
 *   checksum:    4 bytes
 *   reserved:   8 bytes
 */

/* ========================================================================
 * SSTable 读写接口
 * ======================================================================== */

/**
 * @brief 写入 SSTable 文件
 *
 * @param path 文件路径
 * @param entries 条目数组
 * @param count 条目数量
 * @param bloom 布隆过滤器（可为 NULL）
 * @param min_key 最小键
 * @param max_key 最大键
 * @return 0 成功
 */
int sstable_write(const char *path,
                 const lsm_entry_t *entries,
                 size_t count,
                 const lsm_bloom_filter_t *bloom,
                 uint64_t min_key,
                 uint64_t max_key);

/**
 * @brief 读取 SSTable 文件
 *
 * @param path 文件路径
 * @param out_entries 输出条目数组（调用者负责释放）
 * @param out_count 输出条目数量
 * @param out_bloom 输出布隆过滤器（调用者负责释放）
 * @return 0 成功
 */
int sstable_read(const char *path,
                 lsm_entry_t **out_entries,
                 size_t *out_count,
                 lsm_bloom_filter_t **out_bloom);

/**
 * @brief 读取 SSTable 元数据
 *
 * @param path 文件路径
 * @param out_meta 输出元数据
 * @return 0 成功
 */
int sstable_read_meta(const char *path,
                     lsm_sstable_meta_t *out_meta);

/**
 * @brief 搜索 SSTable
 *
 * @param path 文件路径
 * @param key 搜索的键
 * @param key_size 键长度
 * @param out_value 输出值（调用者负责释放）
 * @param out_size 输出值大小
 * @return 0 找到，1 未找到，负值错误
 */
int sstable_search(const char *path,
                   const void *key, size_t key_size,
                   void **out_value, size_t *out_size);

/**
 * @brief 使用布隆过滤器快速检查
 *
 * @param path 文件路径
 * @param key 搜索的键
 * @param key_size 键长度
 * @return true 可能存在，false 一定不存在
 */
bool sstable_might_contain(const char *path,
                          const void *key, size_t key_size);

/**
 * @brief 获取 SSTable 文件大小
 *
 * @param path 文件路径
 * @return 文件大小，-1 错误
 */
int64_t sstable_file_size(const char *path);

/**
 * @brief 批量搜索 SSTable
 *
 * @param path 文件路径
 * @param keys 键数组
 * @param key_sizes 键长度数组
 * @param count 键数量
 * @param results 结果数组（需预先分配）
 * @return 找到的数量
 */
size_t sstable_batch_search(const char *path,
                            const void **keys,
                            const size_t *key_sizes,
                            size_t count,
                            void **results,
                            size_t *result_sizes);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_KV_LSM_SSTABLE_H */
