/**
 * @file columnar_store.h
 * @brief 通用列式存储头文件
 *
 * 实现 Parquet 风格列存格式、压缩、MinMax 索引、延迟物化等
 */
#ifndef DB_COLUMNAR_STORE_H
#define DB_COLUMNAR_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 列式存储格式
 * ======================================================================== */

/** 列类型 */
typedef enum ColumnType_e {
    COLUMN_INT8 = 0,
    COLUMN_INT16,
    COLUMN_INT32,
    COLUMN_INT64,
    COLUMN_UINT8,
    COLUMN_UINT16,
    COLUMN_UINT32,
    COLUMN_UINT64,
    COLUMN_FLOAT,
    COLUMN_DOUBLE,
    COLUMN_STRING,
    COLUMN_BINARY,
    COLUMN_BOOL,
    COLUMN_TIMESTAMP,
} ColumnType;

/** 列块元数据 */
typedef struct ColumnChunkMeta_s {
    char column_name[64];
    ColumnType type;
    int64_t num_values;
    int64_t num_nulls;
    int64_t data_offset;
    int64_t data_size;
    int64_t index_offset;
    int64_t uncompressed_size;
    int64_t compressed_size;
    void *statistics;
} ColumnChunkMeta;

/** 行组元数据 */
typedef struct RowGroupMeta_s {
    int64_t num_rows;
    int64_t total_size;
    ColumnChunkMeta *columns;
    size_t num_columns;
} RowGroupMeta;

/** 文件页脚 */
typedef struct ColumnarFooter_s {
    RowGroupMeta *row_groups;
    size_t num_row_groups;
    int64_t total_rows;
    int version;
    char *schema;
} ColumnarFooter;

/* ========================================================================
 * 压缩方法
 * ======================================================================== */

/** 压缩类型 */
typedef enum CompressionType_e {
    COMPRESSION_NONE = 0,
    COMPRESSION_SNAPPY,
    COMPRESSION_GZIP,
    COMPRESSION_ZSTD,
    COMPRESSION_LZ4,
} CompressionType;

/**
 * @brief 压缩数据
 * @param input 输入数据
 * @param input_size 输入大小
 * @param output_size 输出大小
 * @param type 压缩类型
 * @return 压缩后数据（调用者负责释放）
 */
void *columnar_compress(const void *input, size_t input_size,
                       size_t *output_size, CompressionType type);

/**
 * @brief 解压数据
 */
void *columnar_decompress(const void *input, size_t input_size,
                        size_t uncompressed_size, CompressionType type);

/* ========================================================================
 * Dictionary 编码
 * ======================================================================== */

/** 字典编码器 */
typedef struct DictEncoder_s {
    char **dictionary;
    size_t num_entries;
    size_t capacity;
    int32_t *indices;
    size_t num_indices;
} DictEncoder;

/**
 * @brief 创建字典编码器
 */
DictEncoder *dict_encoder_create(void);

/**
 * @brief 添加值到字典
 */
int dict_encoder_add(DictEncoder *enc, const char *value);

/**
 * @brief 编码值到索引
 */
int32_t dict_encoder_encode(DictEncoder *enc, const char *value);

/**
 * @brief 获取字典
 */
char **dict_encoder_get_dictionary(DictEncoder *enc, size_t *out_size);

/**
 * @brief 获取索引数组
 */
int32_t *dict_encoder_get_indices(DictEncoder *enc, size_t *out_count);

/**
 * @brief 销毁字典编码器
 */
void dict_encoder_destroy(DictEncoder *enc);

/* ========================================================================
 * RLE 编码
 * ======================================================================== */

/** RLE 编码器 */
typedef struct RleEncoder_s {
    void *buffer;
    size_t buffer_size;
    size_t capacity;
    int bit_width;
} RleEncoder;

/**
 * @brief 创建 RLE 编码器
 */
RleEncoder *rle_encoder_create(int bit_width);

/**
 * @brief 添加值
 */
int rle_encoder_add(RleEncoder *enc, int64_t value, int count);

/**
 * @brief 获取编码数据
 */
void *rle_encoder_get_data(RleEncoder *enc, size_t *out_size);

/**
 * @brief 销毁 RLE 编码器
 */
void rle_encoder_destroy(RleEncoder *enc);

/* ========================================================================
 * Bit-packing 编码
 * ======================================================================== */

/**
 * @brief Bit-packing 编码
 * @param values 输入值
 * @param num_values 值数量
 * @param bit_width 位宽
 * @param output_size 输出大小
 * @return 编码后数据
 */
void *bitpack_encode(const int64_t *values, size_t num_values,
                   int bit_width, size_t *output_size);

/**
 * @brief Bit-packing 解码
 */
int64_t *bitpack_decode(const void *data, size_t num_values,
                       int bit_width);

/**
 * @brief 解码到目标数组
 */
void bitpack_decode_to(const void *data, size_t num_values,
                     int bit_width, int64_t *output);

/* ========================================================================
 * MinMax 索引
 * ======================================================================== */

/** MinMax 统计 */
typedef struct MinMaxIndex_s {
    char column_name[64];
    union {
        int64_t min_int;
        double min_double;
        char *min_string;
    } min;
    union {
        int64_t max_int;
        double max_double;
        char *max_string;
    } max;
    bool has_min_max;
    int64_t num_nulls;
    int64_t num_values;
} MinMaxIndex;

/**
 * @brief 创建 MinMax 索引
 */
MinMaxIndex *minmax_index_create(const char *column_name, ColumnType type);

/**
 * @brief 添加值到索引
 */
void minmax_index_add(MinMaxIndex *idx, const void *value);

/**
 * @brief 检查值是否可能在范围内
 */
bool minmax_index_might_contain(const MinMaxIndex *idx, const void *value,
                               CompareOp op);

/**
 * @brief 获取索引大小（用于序列化）
 */
size_t minmax_index_size(const MinMaxIndex *idx);

/**
 * @brief 序列化索引
 */
void *minmax_index_serialize(const MinMaxIndex *idx, size_t *out_size);

/**
 * @brief 反序列化索引
 */
MinMaxIndex *minmax_index_deserialize(const void *data, size_t size);

/**
 * @brief 销毁索引
 */
void minmax_index_destroy(MinMaxIndex *idx);

/* ========================================================================
 * 列式存储读写
 * ======================================================================== */

/** 列式存储 */
typedef struct ColumnarStore_s {
    char *file_path;
    ColumnarFooter *footer;
    CompressionType compression;
    void *file_handle;
} ColumnarStore;

/**
 * @brief 创建列式存储
 */
ColumnarStore *columnar_store_create(const char *file_path,
                                  CompressionType compression);

/**
 * @brief 打开列式存储
 */
ColumnarStore *columnar_store_open(const char *file_path);

/**
 * @brief 关闭列式存储
 */
void columnar_store_close(ColumnarStore *store);

/**
 * @brief 写入行组
 */
int columnar_store_write_row_group(ColumnarStore *store,
                                void **column_data,
                                size_t *column_sizes,
                                ColumnType *column_types,
                                size_t num_columns,
                                size_t num_rows);

/**
 * @brief 读取行组
 */
int columnar_store_read_row_group(ColumnarStore *store,
                               size_t row_group_idx,
                               void **column_data,
                               size_t *column_sizes);

/**
 * @brief 读取列范围
 */
int columnar_store_read_column_range(ColumnarStore *store,
                                  size_t row_group_idx,
                                  size_t column_idx,
                                  size_t start_row,
                                  size_t num_rows,
                                  void *output);

/**
 * @brief 获取元数据
 */
const ColumnarFooter *columnar_store_get_footer(const ColumnarStore *store);

/**
 * @brief 获取行组数量
 */
size_t columnar_store_get_num_row_groups(const ColumnarStore *store);

/**
 * @brief 获取总行数
 */
int64_t columnar_store_get_total_rows(const ColumnarStore *store);

/* ========================================================================
 * 延迟物化
 * ======================================================================== */

/** 延迟物化上下文 */
typedef struct LateMaterializeCtx_s {
    ColumnarStore *store;
    size_t row_group_idx;
    size_t num_rows;
    size_t num_columns;
    void **projected_columns;
    size_t *column_sizes;
    MinMaxIndex **indexes;
} LateMaterializeCtx;

/**
 * @brief 创建延迟物化上下文
 */
LateMaterializeCtx *late_materialize_create(ColumnarStore *store,
                                          size_t row_group_idx,
                                          size_t num_rows);

/**
 * @brief 添加投影列
 */
int late_materialize_add_column(LateMaterializeCtx *ctx,
                              size_t column_idx,
                              size_t start_row,
                              size_t num_rows);

/**
 * @brief 执行延迟物化
 */
void **late_materialize_execute(LateMaterializeCtx *ctx, size_t *out_num_columns);

/**
 * @brief 销毁延迟物化上下文
 */
void late_materialize_destroy(LateMaterializeCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_COLUMNAR_STORE_H */
