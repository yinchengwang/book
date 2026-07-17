/**
 * @file ts_compress.h
 * @brief 时序数据 Gorilla 压缩算法
 *
 * 实现 Facebook Gorilla 压缩算法，用于高效压缩时序数据。
 * 该算法可以在保持高压缩率的同时支持随机访问。
 */
#ifndef DB_STORAGE_TS_COMPRESS_H
#define DB_STORAGE_TS_COMPRESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 压缩块大小（每个块存储的点数） */
#define TS_COMPRESS_BLOCK_SIZE 4096

/** 最大压缩位宽 */
#define TS_COMPRESS_MAX_BITS 64

/* ========================================================================
 * 压缩块结构
 * ======================================================================== */

/**
 * @brief 时序记录
 */
typedef struct ts_record_s {
    int64_t timestamp;  /**< 时间戳（毫秒） */
    double value;         /**< 值 */
} ts_record_t;

/**
 * @brief 压缩块状态
 */
typedef enum {
    TS_COMPRESS_BLOCK_EMPTY = 0,     /**< 空块 */
    TS_COMPRESS_BLOCK_WRITING = 1,    /**< 正在写入 */
    TS_COMPRESS_BLOCK_FULL = 2,      /**< 已满 */
    TS_COMPRESS_BLOCK_COMPRESSED = 3 /**< 已压缩 */
} ts_compress_block_state_t;

/**
 * @brief 压缩块
 */
typedef struct ts_compress_block_s {
    /* 元数据 */
    int64_t first_timestamp;         /**< 首个时间戳 */
    int64_t last_timestamp;          /**< 最后一个时间戳 */
    uint32_t num_points;             /**< 当前点数 */
    ts_compress_block_state_t state; /**< 块状态 */

    /* 原始数据（写入时使用） */
    ts_record_t *records;            /**< 原始记录数组 */

    /* 压缩后数据 */
    uint8_t *compressed_ts;         /**< 压缩后的时间戳数据 */
    size_t ts_bits_used;             /**< 时间戳实际使用位数 */

    uint8_t *compressed_values;      /**< 压缩后的值数据 */
    size_t value_bits_used;          /**< 值实际使用位数 */
    size_t compressed_size;          /**< 总压缩大小 */

    /* 统计信息 */
    double min_val;
    double max_val;
    double sum_val;
} ts_compress_block_t;

/**
 * @brief 压缩器
 */
typedef struct ts_compressor_s {
    ts_compress_block_t *current_block;  /**< 当前写入块 */
    int64_t last_timestamp;               /**< 上一个时间戳 */
    double last_value;                    /**< 上一个值 */

    /* 统计 */
    uint64_t total_points;
    size_t total_original_size;
    size_t total_compressed_size;
} ts_compressor_t;

/* ========================================================================
 * 压缩 API
 * ======================================================================== */

/**
 * @brief 创建压缩器
 * @return 压缩器指针，失败返回 NULL
 */
ts_compressor_t *ts_compressor_create(void);

/**
 * @brief 释放压缩器
 * @param comp 压缩器
 */
void ts_compressor_free(ts_compressor_t *comp);

/**
 * @brief 添加数据点到压缩器
 * @param comp 压缩器
 * @param timestamp 时间戳（毫秒）
 * @param value 值
 * @return 0 成功，-1 失败
 */
int ts_compress_add(ts_compressor_t *comp, int64_t timestamp, double value);

/**
 * @brief 刷新当前块（压缩并准备输出）
 * @param comp 压缩器
 * @return 0 成功，-1 失败
 */
int ts_compress_flush(ts_compressor_t *comp);

/**
 * @brief 获取当前块的压缩数据
 * @param comp 压缩器
 * @param out_size 输出压缩数据大小
 * @return 压缩数据指针，失败返回 NULL
 */
const uint8_t *ts_compress_get_data(const ts_compressor_t *comp, size_t *out_size);

/**
 * @brief 获取压缩统计信息
 * @param comp 压缩器
 * @param total_points 输出总点数
 * @param compression_ratio 输出压缩比
 */
void ts_compress_get_stats(const ts_compressor_t *comp,
                          uint64_t *total_points,
                          double *compression_ratio);

/* ========================================================================
 * 解压 API
 * ======================================================================== */

/**
 * @brief 解压块
 * @param compressed_data 压缩数据
 * @param size 压缩数据大小
 * @param out_records 输出记录数组（调用者分配）
 * @param max_records 最大记录数
 * @return 实际解压的记录数，失败返回 -1
 */
int ts_decompress(const uint8_t *compressed_data, size_t size,
                  ts_record_t *out_records, int max_records);

/**
 * @brief 解压块到指定范围
 * @param compressed_data 压缩数据
 * @param size 压缩数据大小
 * @param start_time 起始时间
 * @param end_time 结束时间
 * @param out_records 输出记录数组（调用者分配）
 * @param max_records 最大记录数
 * @return 实际解压的记录数，失败返回 -1
 */
int ts_decompress_range(const uint8_t *compressed_data, size_t size,
                       int64_t start_time, int64_t end_time,
                       ts_record_t *out_records, int max_records);

/**
 * @brief 获取压缩块信息
 * @param compressed_data 压缩数据
 * @param out_first_timestamp 输出首个时间戳
 * @param out_num_points 输出点数
 * @return 0 成功，-1 失败
 */
int ts_compress_get_info(const uint8_t *compressed_data,
                         int64_t *out_first_timestamp,
                         uint32_t *out_num_points);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TS_COMPRESS_H */
