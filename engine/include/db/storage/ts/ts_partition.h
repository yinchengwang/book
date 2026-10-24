/**
 * @file ts_partition.h
 * @brief 时序数据库分区接口
 *
 * Phase12 - 实现时序分区，追赶 TimescaleDB 水平。
 */
#ifndef DB_STORAGE_TS_PARTITION_H
#define DB_STORAGE_TS_PARTITION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 分区类型 */
typedef enum {
    PARTITION_TIME = 0,      /**< 时间分区 */
    PARTITION_SPACE = 1,     /**< 空间分区（标签）*/
    PARTITION_HASH = 2        /**< 哈希分区 */
} partition_type_t;

/** 分区信息 */
typedef struct {
    uint64_t partition_id;
    int64_t time_start;     /**< 时间范围起始 */
    int64_t time_end;       /**< 时间范围结束 */
    char *tag_value;        /**< 标签值（空间分区用）*/
    uint64_t num_chunks;     /**< chunk 数量 */
    size_t size_bytes;       /**< 分区大小 */
} ts_partition_info_t;

/** 时序分区管理器不透明类型 */
typedef struct ts_partition_manager ts_partition_manager_t;

/** 时序分区配置 */
typedef struct {
    partition_type_t type;
    int64_t interval_ms;      /**< 时间间隔（毫秒）*/
    size_t chunk_size_bytes;   /**< chunk 大小 */
    int32_t num_partitions;  /**< 分区数（空间分区）*/
} ts_partition_config_t;

/**
 * @brief 创建分区管理器
 */
ts_partition_manager_t *ts_partition_manager_create(const ts_partition_config_t *config);

/**
 * @brief 销毁分区管理器
 */
void ts_partition_manager_destroy(ts_partition_manager_t *mgr);

/**
 * @brief 获取数据应写入的分区
 */
const ts_partition_info_t *ts_partition_get(const ts_partition_manager_t *mgr, int64_t timestamp);

/**
 * @brief 获取分区信息
 */
const ts_partition_info_t *ts_partition_get_info(const ts_partition_manager_t *mgr, uint64_t partition_id);

/**
 * @brief 获取所有分区
 */
ts_partition_info_t *ts_partition_get_all(const ts_partition_manager_t *mgr, size_t *out_count);

/**
 * @brief 释放分区信息数组
 */
void ts_partition_info_free(ts_partition_info_t *info, size_t count);

/**
 * @brief 创建新分区
 */
int ts_partition_create(ts_partition_manager_t *mgr, int64_t start, int64_t end);

/**
 * @brief 删除旧分区
 */
int ts_partition_drop(ts_partition_manager_t *mgr, uint64_t partition_id);

/**
 * @brief 触发分区内重压缩
 */
int ts_partition_compress(ts_partition_manager_t *mgr, uint64_t partition_id);

/**
 * @brief 获取分区数量
 */
uint32_t ts_partition_count(const ts_partition_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TS_PARTITION_H */
