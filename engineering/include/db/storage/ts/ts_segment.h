/**
 * @file ts_segment.h
 * @brief 时序数据分段索引
 *
 * 将时序数据按时间范围分段存储，每段使用压缩块。
 * 支持高效的时间范围查询和 TTL 清理。
 */
#ifndef DB_TS_SEGMENT_H
#define DB_TS_SEGMENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 分段索引魔数 */
#define TS_SEGMENT_MAGIC 0x54534547  /* "TSEG" */

/** 分段索引版本 */
#define TS_SEGMENT_VERSION 1

/** 默认每段数据点数 */
#define TS_SEGMENT_DEFAULT_SIZE 4096

/** 时间粒度（在此定义，供 ts_engine.h 使用） */
#ifndef TS_GRANULARITY_DEFINED
#define TS_GRANULARITY_DEFINED
typedef enum {
    TS_GRANULARITY_SECOND = 0,
    TS_GRANULARITY_MINUTE = 1,
    TS_GRANULARITY_HOUR = 2,
    TS_GRANULARITY_DAY = 3,
} ts_granularity_t;
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 段元数据
 */
typedef struct ts_segment_meta_s {
    int64_t start_ts;          /**< 段起始时间戳 */
    int64_t end_ts;            /**< 段结束时间戳 */
    uint64_t file_offset;      /**< 数据文件偏移 */
    uint32_t point_count;      /**< 段内数据点数 */
    uint32_t compressed_size;  /**< 压缩后大小 */
    double min_value;          /**< 段内最小值 */
    double max_value;          /**< 段内最大值 */
} ts_segment_meta_t;

/**
 * @brief 查询结果
 */
typedef struct ts_segment_result_s {
    int64_t timestamp;         /**< 时间戳 */
    double value;              /**< 值 */
} ts_segment_result_t;

/**
 * @brief 时间范围查询结果集
 */
typedef struct ts_segment_results_s {
    ts_segment_result_t *results;  /**< 结果数组 */
    uint32_t count;                /**< 结果数量 */
    uint32_t capacity;             /**< 容量 */
} ts_segment_results_t;

/**
 * @brief 分段索引
 */
typedef struct ts_segment_index_s {
    char data_dir[512];             /**< 数据目录 */
    uint32_t seg_size;              /**< 每段数据点数 */
    ts_granularity_t granularity;   /**< 时间粒度 */

    /* 段元数据 */
    ts_segment_meta_t *segments;    /**< 段元数据数组 */
    uint32_t seg_count;             /**< 段数 */
    uint32_t seg_capacity;          /**< 段容量 */

    /* 当前写入段 */
    ts_segment_meta_t current_seg;  /**< 当前段元数据 */
    void *compress_buffer;          /**< 压缩缓冲区 */
    uint32_t buffer_count;          /**< 缓冲区数据点数 */

    /* 统计 */
    uint64_t total_points;          /**< 总数据点数 */
    uint64_t query_count;           /**< 查询次数 */
} ts_segment_index_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建分段索引
 *
 * @param data_dir 数据目录
 * @param seg_size 每段数据点数
 * @param granularity 时间粒度
 * @return 分段索引句柄，失败返回 NULL
 */
ts_segment_index_t *ts_segment_index_create(const char *data_dir,
                                            uint32_t seg_size,
                                            ts_granularity_t granularity);

/**
 * @brief 打开已存在的分段索引
 *
 * @param data_dir 数据目录
 * @return 分段索引句柄，失败返回 NULL
 */
ts_segment_index_t *ts_segment_index_open(const char *data_dir);

/**
 * @brief 保存分段索引
 *
 * @param index 分段索引句柄
 * @return 0 成功，-1 失败
 */
int ts_segment_index_save(ts_segment_index_t *index);

/**
 * @brief 关闭并释放分段索引
 *
 * @param index 分段索引句柄
 */
void ts_segment_index_destroy(ts_segment_index_t *index);

/* ========================================================================
 * 数据操作 API
 * ======================================================================== */

/**
 * @brief 追加数据点
 *
 * @param index 分段索引句柄
 * @param timestamp 时间戳（毫秒）
 * @param value 值
 * @return 0 成功，-1 失败
 */
int ts_segment_append(ts_segment_index_t *index, int64_t timestamp, double value);

/**
 * @brief 批量追加数据点
 *
 * @param index 分段索引句柄
 * @param timestamps 时间戳数组
 * @param values 值数组
 * @param count 数据点数
 * @return 成功追加的数量
 */
uint32_t ts_segment_append_batch(ts_segment_index_t *index,
                                 const int64_t *timestamps,
                                 const double *values,
                                 uint32_t count);

/**
 * @brief 时间范围查询
 *
 * @param index 分段索引句柄
 * @param start_ts 起始时间戳
 * @param end_ts 结束时间戳
 * @param results 查询结果（调用者负责释放）
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t ts_segment_query(const ts_segment_index_t *index,
                          int64_t start_ts, int64_t end_ts,
                          ts_segment_results_t *results,
                          uint32_t max_results);

/**
 * @brief 释放查询结果
 *
 * @param results 查询结果
 */
void ts_segment_free_results(ts_segment_results_t *results);

/* ========================================================================
 * 段管理 API
 * ======================================================================== */

/**
 * @brief 获取段数
 *
 * @param index 分段索引句柄
 * @return 段数
 */
uint32_t ts_segment_count(const ts_segment_index_t *index);

/**
 * @brief 获取段元数据
 *
 * @param index 分段索引句柄
 * @param seg_idx 段索引
 * @return 段元数据，失败返回 NULL
 */
const ts_segment_meta_t *ts_segment_get_meta(const ts_segment_index_t *index,
                                              uint32_t seg_idx);

/**
 * @brief 刷新当前段（压缩并持久化）
 *
 * @param index 分段索引句柄
 * @return 0 成功，-1 失败
 */
int ts_segment_flush_current(ts_segment_index_t *index);

/* ========================================================================
 * TTL 清理 API
 * ======================================================================== */

/**
 * @brief 删除过期数据
 *
 * @param index 分段索引句柄
 * @param older_than_ms 删除此时间戳之前的记录
 * @return 删除的段数
 */
uint32_t ts_segment_expire(ts_segment_index_t *index, int64_t older_than_ms);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 将时间戳对齐到粒度边界
 *
 * @param timestamp 时间戳
 * @param granularity 时间粒度
 * @return 对齐后的时间戳
 */
int64_t ts_segment_align_timestamp(int64_t timestamp, ts_granularity_t granularity);

/**
 * @brief 获取粒度对应的毫秒数
 *
 * @param granularity 时间粒度
 * @return 毫秒数
 */
int64_t ts_segment_granularity_to_ms(ts_granularity_t granularity);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_SEGMENT_H */
