/**
 * @file ts_columnar.h
 * @brief 时序数据列式存储头文件
 *
 * 实现时序数据的列式存储和压缩：
 * 1. Delta 编码压缩
 * 2. RLE (Run-Length Encoding) 压缩
 * 3. Bit-packing 压缩
 * 4. 列式读取优化
 */
#ifndef DB_TS_COLUMNAR_H
#define DB_TS_COLUMNAR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 列式存储块
 * ======================================================================== */

/** 列类型 */
typedef enum ColumnType_e {
    COLUMN_TIMESTAMP = 0,   /**< 时间戳列 */
    COLUMN_VALUE = 1,       /**< 值列 */
    COLUMN_TAG = 2,         /**< 标签列 */
    COLUMN_DELTA = 3        /**< 增量列 */
} ColumnType;

/** 列块状态 */
typedef enum ColumnBlockState_e {
    BLOCK_UNCOMPRESSED = 0,  /**< 未压缩 */
    BLOCK_COMPRESSED = 1,    /**< 已压缩 */
    BLOCK_SPARSE = 2         /**< 稀疏存储 */
} ColumnBlockState;

/** 列块 */
typedef struct ColumnBlock_s {
    ColumnType type;             /**< 列类型 */
    ColumnBlockState state;      /**< 块状态 */

    /* 未压缩数据 */
    void *data;                  /**< 数据指针 */
    uint32_t count;              /**< 数据点数 */
    uint32_t capacity;           /**< 容量 */

    /* 压缩数据 */
    void *compressed_data;       /**< 压缩后数据 */
    size_t compressed_size;      /**< 压缩大小 */

    /* 统计信息 */
    double min_value;
    double max_value;
    double sum_value;

    /* 元数据 */
    int64_t start_time;          /**< 起始时间 */
    int64_t end_time;            /**< 结束时间 */
} ColumnBlock;

/* ========================================================================
 * Delta 编码
 * ======================================================================== */

/**
 * @brief Delta 编码压缩
 *
 * 存储增量而非原始值：
 * - 原始: [100, 105, 110, 115] → 4 × 8 = 32 bytes
 * - Delta: [100, 5, 5, 5] → 8 + 3 × 1 = 11 bytes (假设增量很小)
 */
typedef struct DeltaEncoder_s {
    int64_t first_value;         /**< 第一个值 */
    uint8_t *deltas;             /**< 增量数组 */
    uint32_t count;              /**< 增量数量 */
    uint32_t max_bits;           /**< 最大位宽 */
    size_t compressed_size;      /**< 压缩后大小 */
} DeltaEncoder;

/**
 * @brief 创建 Delta 编码器
 */
DeltaEncoder *delta_encoder_create(void);

/**
 * @brief 添加值到 Delta 编码器
 */
int delta_encoder_add(DeltaEncoder *enc, int64_t value);

/**
 * @brief 完成编码
 */
int delta_encoder_finish(DeltaEncoder *enc);

/**
 * @brief 获取压缩数据
 */
const uint8_t *delta_encoder_get_data(const DeltaEncoder *enc, size_t *out_size);

/**
 * @brief 解码
 */
int delta_encoder_decode(const DeltaEncoder *enc, int64_t *out_values, uint32_t max_count);

/**
 * @brief 释放 Delta 编码器
 */
void delta_encoder_free(DeltaEncoder *enc);

/* ========================================================================
 * RLE 编码
 * ======================================================================== */

/** RLE 运行（连续相同值） */
typedef struct RleRun_s {
    int64_t value;       /**< 运行值 */
    uint32_t count;      /**< 重复次数 */
    uint32_t start_pos;  /**< 起始位置 */
} RleRun;

/** RLE 编码器 */
typedef struct RleEncoder_s {
    RleRun *runs;            /**< 运行数组 */
    uint32_t run_count;      /**< 运行数 */
    uint32_t capacity;       /**< 容量 */

    /* 统计 */
    uint32_t total_count;    /**< 总数据点数 */
    uint32_t unique_count;   /**< 唯一值数 */
} RleEncoder;

/**
 * @brief 创建 RLE 编码器
 */
RleEncoder *rle_encoder_create(void);

/**
 * @brief 添加值到 RLE 编码器
 */
int rle_encoder_add(RleEncoder *enc, int64_t value);

/**
 * @brief 完成编码
 */
int rle_encoder_finish(RleEncoder *enc);

/**
 * @brief 获取压缩数据
 */
const uint8_t *rle_encoder_get_data(const RleEncoder *enc, size_t *out_size);

/**
 * @brief 获取运行数
 */
uint32_t rle_encoder_get_run_count(const RleEncoder *enc);

/**
 * @brief 获取指定运行的统计
 */
const RleRun *rle_encoder_get_run(const RleEncoder *enc, uint32_t run_idx);

/**
 * @brief 解码到范围
 */
int rle_encoder_decode_range(const RleEncoder *enc,
                             uint32_t start, uint32_t end,
                             int64_t *out_values, uint32_t max_count);

/**
 * @brief 释放 RLE 编码器
 */
void rle_encoder_free(RleEncoder *enc);

/* ========================================================================
 * Bit-packing 编码
 * ======================================================================== */

/** Bit-packing 编码器 */
typedef struct BitPackingEncoder_s {
    uint8_t *packed_data;     /**< 打包后的数据 */
    uint32_t bit_width;       /**< 位宽 */
    uint32_t count;           /**< 数据点数 */
    size_t packed_size;       /**< 打包后大小 */
} BitPackingEncoder;

/**
 * @brief 创建 Bit-packing 编码器
 */
BitPackingEncoder *bitpack_encoder_create(uint32_t bit_width);

/**
 * @brief 添加值
 */
int bitpack_encoder_add(BitPackingEncoder *enc, uint64_t value);

/**
 * @brief 添加多个值
 */
int bitpack_encoder_add_batch(BitPackingEncoder *enc,
                              const uint64_t *values,
                              uint32_t count);

/**
 * @brief 完成编码
 */
int bitpack_encoder_finish(BitPackingEncoder *enc);

/**
 * @brief 获取压缩数据
 */
const uint8_t *bitpack_encoder_get_data(const BitPackingEncoder *enc, size_t *out_size);

/**
 * @brief 解码
 */
int bitpack_encoder_decode(const BitPackingEncoder *enc,
                           uint64_t *out_values,
                           uint32_t max_count);

/**
 * @brief 释放 Bit-packing 编码器
 */
void bitpack_encoder_free(BitPackingEncoder *enc);

/* ========================================================================
 * 混合压缩编码
 * ======================================================================== */

/** 压缩类型 */
typedef enum TsCompressionType_e {
    COMPRESS_NONE = 0,       /**< 无压缩 */
    COMPRESS_DELTA = 1,      /**< Delta 编码 */
    COMPRESS_RLE = 2,        /**< RLE 编码 */
    COMPRESS_BITPACK = 3,    /**< Bit-packing */
    COMPRESS_GORILLA = 4,    /**< Gorilla 压缩 */
    COMPRESS_DELTA_RLE = 5,  /**< Delta + RLE */
    COMPRESS_BEST = 6        /**< 自动选择最佳 */
} TsCompressionType;

/** 混合压缩器 */
typedef struct TsColumnarCompressor_s {
    /* 各类型编码器 */
    DeltaEncoder *delta_enc;
    RleEncoder *rle_enc;
    BitPackingEncoder *bitpack_enc;

    /* 当前选择的压缩类型 */
    TsCompressionType selected_type;

    /* 原始数据（用于比较） */
    int64_t *raw_values;
    uint32_t raw_count;

    /* 最佳压缩结果 */
    uint8_t *best_compressed;
    size_t best_compressed_size;
} TsColumnarCompressor;

/**
 * @brief 创建混合压缩器
 */
TsColumnarCompressor *ts_columnar_compressor_create(void);

/**
 * @brief 添加数据点
 */
int ts_columnar_compressor_add(TsColumnarCompressor *comp,
                               int64_t timestamp,
                               double value);

/**
 * @brief 添加多个数据点
 */
int ts_columnar_compressor_add_batch(TsColumnarCompressor *comp,
                                     const int64_t *timestamps,
                                     const double *values,
                                     uint32_t count);

/**
 * @brief 自动选择最佳压缩方式并完成
 */
TsCompressionType ts_columnar_compressor_auto_compress(TsColumnarCompressor *comp);

/**
 * @brief 使用指定压缩方式
 */
int ts_columnar_compressor_compress(TsColumnarCompressor *comp,
                                    TsCompressionType type);

/**
 * @brief 获取压缩数据
 */
const uint8_t *ts_columnar_compressor_get_data(const TsColumnarCompressor *comp,
                                               size_t *out_size,
                                               TsCompressionType *out_type);

/**
 * @brief 获取压缩比
 */
double ts_columnar_compressor_get_ratio(const TsColumnarCompressor *comp);

/**
 * @brief 释放混合压缩器
 */
void ts_columnar_compressor_free(TsColumnarCompressor *comp);

/* ========================================================================
 * 列式存储引擎
 * ======================================================================== */

/** 列式存储页 */
typedef struct ColumnarPage_s {
    /* 列数据 */
    ColumnBlock timestamp_col;  /**< 时间戳列 */
    ColumnBlock value_col;      /**< 值列 */
    ColumnBlock delta_col;      /**< 增量列 */

    /* 标签索引 */
    void *tag_index;            /**< 标签倒排索引 */

    /* 页面元数据 */
    int64_t start_time;
    int64_t end_time;
    uint32_t row_count;
    size_t page_size;
} ColumnarPage;

/**
 * @brief 创建列式存储页
 */
ColumnarPage *columnar_page_create(uint32_t capacity);

/**
 * @brief 添加数据点到页面
 */
int columnar_page_add(ColumnarPage *page, int64_t timestamp, double value);

/**
 * @brief 压缩页面
 */
int columnar_page_compress(ColumnarPage *page, TsCompressionType type);

/**
 * @brief 解压页面
 */
int columnar_page_decompress(ColumnarPage *page);

/**
 * @brief 查询页面（范围）
 */
uint32_t columnar_page_query(const ColumnarPage *page,
                             int64_t start_time, int64_t end_time,
                             void *results, uint32_t max_results);

/**
 * @brief 聚合查询
 */
int columnar_page_aggregate(const ColumnarPage *page,
                            int64_t start_time, int64_t end_time,
                            double *out_sum, double *out_avg,
                            double *out_min, double *out_max,
                            uint32_t *out_count);

/**
 * @brief 释放页面
 */
void columnar_page_free(ColumnarPage *page);

/* ========================================================================
 * 列式读取优化
 * ======================================================================== */

/** 块跳过信息 */
typedef struct BlockSkipInfo_s {
    int64_t start_time;
    int64_t end_time;
    double min_value;
    double max_value;
    bool has_overlap;
} BlockSkipInfo;

/**
 * @brief 创建块跳过信息
 */
BlockSkipInfo *block_skip_info_create(int64_t start_time, int64_t end_time,
                                      double min_value, double max_value);

/**
 * @brief 检查时间范围是否有重叠
 */
bool block_skip_has_overlap(const BlockSkipInfo *info,
                            int64_t start_time, int64_t end_time);

/**
 * @brief 检查值范围是否有重叠
 */
bool block_skip_value_overlap(const BlockSkipInfo *info,
                              double min_value, double max_value);

/**
 * @brief 释放块跳过信息
 */
void block_skip_info_free(BlockSkipInfo *info);

/**
 * @brief 批量跳过检查
 *
 * @param blocks 块信息数组
 * @param count 块数
 * @param start_time 查询起始时间
 * @param end_time 查询结束时间
 * @param min_value 查询最小值
 * @param max_value 查询最大值
 * @param skip_flags 输出跳过标志数组
 * @return 可跳过块数
 */
uint32_t block_skip_batch_check(const BlockSkipInfo **blocks,
                                uint32_t count,
                                int64_t start_time, int64_t end_time,
                                double min_value, double max_value,
                                bool *skip_flags);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_COLUMNAR_H */
