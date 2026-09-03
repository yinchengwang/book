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
 * @brief 立即压缩写入：C2-4 T1 增量编码器
 *
 * 与 ts_compress_add 的区别：边写入边编码（delta-of-delta + XOR），
 * 内存中不保留原始 16B/点，热路径即可享受压缩收益。
 *
 * @param comp 压缩器
 * @param timestamp 时间戳
 * @param value 值
 * @return 0 成功，DBERR_FULL（满且无法 flush） / DBERR_CORRUPT（编码异常）
 */
int ts_compress_add_immediate(ts_compressor_t *comp, int64_t timestamp, double value);

/**
 * @brief 满块路径（T2）：强制 flush + 重试插入
 * @return 0 成功，DBERR_FULL（flush 后仍满）
 */
int ts_compress_force_flush_add(ts_compressor_t *comp, int64_t timestamp, double value);

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

/* ========================================================================
 * Gorilla XOR 编码 API
 * ======================================================================== */

/**
 * @brief Gorilla 编码器
 *
 * 使用 Facebook Gorilla 算法对浮点数序列进行压缩。
 * 核心原理：相邻值 XOR 后产生大量前导/尾部零，
 * 只存储有效位数以获得高压缩率。
 */
typedef struct {
    uint8_t *buffer;   /**< 输出缓冲区 */
    size_t byte_pos;   /**< 当前字节位置 */
    size_t bit_pos;    /**< 当前位位置（0-7） */
    float prev_value;  /**< 上一个值 */
    int has_prev;      /**< 是否已存储第一个值 */
} gorilla_encoder_t;

/**
 * @brief Gorilla 解码器
 */
typedef struct {
    const uint8_t *buffer; /**< 输入缓冲区 */
    size_t buffer_size;    /**< 缓冲区大小 */
    size_t byte_pos;       /**< 当前字节位置 */
    size_t bit_pos;        /**< 当前位位置（0-7） */
    float prev_value;      /**< 上一个解码的值 */
    int has_prev;          /**< 是否已解码第一个值 */
} gorilla_decoder_t;

/**
 * @brief 初始化 Gorilla 编码器
 * @param enc 编码器
 * @return 0 成功
 */
int gorilla_encoder_init(gorilla_encoder_t *enc);

/**
 * @brief 销毁 Gorilla 编码器
 * @param enc 编码器
 */
void gorilla_encoder_destroy(gorilla_encoder_t *enc);

/**
 * @brief 初始化 Gorilla 解码器
 * @param dec 解码器
 * @param data 输入数据
 * @param size 数据大小
 * @return 0 成功
 */
int gorilla_decoder_init(gorilla_decoder_t *dec, const uint8_t *data, size_t size);

/**
 * @brief 销毁 Gorilla 解码器
 * @param dec 解码器
 */
void gorilla_decoder_destroy(gorilla_decoder_t *dec);

/**
 * @brief 编码一个浮点值
 * @param enc 编码器
 * @param value 要编码的值
 * @return 0 成功
 */
int gorilla_encode(gorilla_encoder_t *enc, float value);

/**
 * @brief 解码一个浮点值
 * @param dec 解码器
 * @param value 输出解码的值
 * @return 0 成功，-1 已结束
 */
int gorilla_decode(gorilla_decoder_t *dec, float *value);

/**
 * @brief 获取编码器输出缓冲区
 * @param enc 编码器
 * @param out_size 输出大小
 * @return 缓冲区指针
 */
const uint8_t *gorilla_encoder_get_data(const gorilla_encoder_t *enc, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TS_COMPRESS_H */
