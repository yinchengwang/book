/**
 * @file cf_sstable.h
 * @brief 列族存储 SSTable 接口
 *
 * Phase12 - 实现列族存储，追赶 Cassandra 水平。
 */
#ifndef DB_STORAGE_CF_SSTABLE_H
#define DB_STORAGE_CF_SSTABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 列族 SSTable 配置 */
typedef struct {
    size_t max_file_size;      /**< 最大文件大小 */
    size_t block_size;         /**< 块大小 */
    int compression;            /**< 压缩类型 0=无 1=snappy 2=lz4 */
    bool bloom_filter;         /**< 启用布隆过滤器 */
} cf_sstable_config_t;

/** SSTable 元数据 */
typedef struct {
    uint64_t file_id;
    char path[512];
    uint64_t min_key;
    uint64_t max_key;
    size_t file_size;
    uint64_t min_timestamp;
    uint64_t max_timestamp;
    uint32_t num_entries;
    bool being_compacted;
} cf_sstable_meta_t;

/** SSTable 不透明类型 */
typedef struct cf_sstable cf_sstable_t;

/**
 * @brief 创建 SSTable
 */
cf_sstable_t *cf_sstable_create(const char *dir, const cf_sstable_config_t *config);

/**
 * @brief 打开 SSTable
 */
cf_sstable_t *cf_sstable_open(const char *path, const cf_sstable_config_t *config);

/**
 * @brief 关闭 SSTable
 */
void cf_sstable_close(cf_sstable_t *sst);

/**
 * @brief 写入键值对
 */
int cf_sstable_put(cf_sstable_t *sst, uint64_t key, const void *value, size_t value_len);

/**
 * @brief 读取值
 */
int cf_sstable_get(cf_sstable_t *sst, uint64_t key, void **out_value, size_t *out_len);

/**
 * @brief 范围扫描
 */
int cf_sstable_range(cf_sstable_t *sst, uint64_t start_key, uint64_t end_key,
                    int (*callback)(uint64_t key, const void *value, size_t len, void *ctx));

/**
 * @brief 获取元数据
 */
const cf_sstable_meta_t *cf_sstable_get_meta(const cf_sstable_t *sst);

/**
 * @brief 合并 SSTable
 */
cf_sstable_t *cf_sstable_compact(cf_sstable_t **inputs, size_t num_inputs, const char *output_dir);

/**
 * @brief 获取文件大小
 */
size_t cf_sstable_file_size(const cf_sstable_t *sst);

/**
 * @brief 获取条目数
 */
uint32_t cf_sstable_num_entries(const cf_sstable_t *sst);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_CF_SSTABLE_H */
