/**
 * @file ts_compress.c
 * @brief 时序数据 Gorilla 压缩算法实现
 */
#include "db/storage/ts/ts_compress.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 常量
 * ======================================================================== */

#define BITS_PER_BYTE 8
#define TIMESTAMP_DELTA_BITS 14   /* 2^14 = 16384ms ~ 16秒 */
#define VALUE_MANTISSA_BITS 14    /* 14位尾数 */
#define VALUE_EXP_BITS 6          /* 6位指数 */

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 计算存储值所需的位数
 */
static int count_bits_needed(uint64_t value) {
    if (value == 0) return 1;
    int bits = 0;
    while (value > 0) {
        bits++;
        value >>= 1;
    }
    return bits;
}

/**
 * @brief 计算 XOR 后的有效位数
 */
static int xor_bits_needed(uint64_t a, uint64_t b) {
    uint64_t xor = a ^ b;
    return count_bits_needed(xor);
}

/**
 * @brief 位操作辅助
 */
static void write_bits(uint8_t *buffer, size_t *bit_offset, uint64_t value, int num_bits) {
    size_t byte_offset = *bit_offset / BITS_PER_BYTE;
    size_t bit_offset_in_byte = *bit_offset % BITS_PER_BYTE;

    for (int i = 0; i < num_bits; i++) {
        int bit = (value >> i) & 1;
        if (bit) {
            buffer[byte_offset] |= (1 << bit_offset_in_byte);
        }
        bit_offset_in_byte++;
        if (bit_offset_in_byte == BITS_PER_BYTE) {
            bit_offset_in_byte = 0;
            byte_offset++;
        }
    }
    *bit_offset += num_bits;
}

static uint64_t read_bits(const uint8_t *buffer, size_t *bit_offset, int num_bits) {
    uint64_t value = 0;
    size_t byte_offset = *bit_offset / BITS_PER_BYTE;
    size_t bit_offset_in_byte = *bit_offset % BITS_PER_BYTE;

    for (int i = 0; i < num_bits; i++) {
        int bit = (buffer[byte_offset] >> bit_offset_in_byte) & 1;
        value |= ((uint64_t)bit << i);
        bit_offset_in_byte++;
        if (bit_offset_in_byte == BITS_PER_BYTE) {
            bit_offset_in_byte = 0;
            byte_offset++;
        }
    }
    *bit_offset += num_bits;
    return value;
}

/* ========================================================================
 * 压缩器实现
 * ======================================================================== */

ts_compressor_t *ts_compressor_create(void) {
    ts_compressor_t *comp = (ts_compressor_t *)calloc(1, sizeof(ts_compressor_t));
    if (!comp) return NULL;

    comp->current_block = (ts_compress_block_t *)calloc(1, sizeof(ts_compress_block_t));
    if (!comp->current_block) {
        free(comp);
        return NULL;
    }

    comp->current_block->records = (ts_record_t *)calloc(TS_COMPRESS_BLOCK_SIZE, sizeof(ts_record_t));
    if (!comp->current_block->records) {
        free(comp->current_block);
        free(comp);
        return NULL;
    }

    comp->current_block->state = TS_COMPRESS_BLOCK_EMPTY;
    comp->last_timestamp = 0;
    comp->last_value = 0.0;

    return comp;
}

void ts_compressor_free(ts_compressor_t *comp) {
    if (!comp) return;

    if (comp->current_block) {
        free(comp->current_block->records);
        free(comp->current_block->compressed_ts);
        free(comp->current_block->compressed_values);
        free(comp->current_block);
    }
    free(comp);
}

int ts_compress_add(ts_compressor_t *comp, int64_t timestamp, double value) {
    if (!comp || !comp->current_block) return -1;

    ts_compress_block_t *block = comp->current_block;

    /* 如果块已满，先压缩 */
    if (block->state == TS_COMPRESS_BLOCK_FULL) {
        ts_compress_flush(comp);
        block = comp->current_block;
    }

    /* 初始化首个时间戳 */
    if (block->state == TS_COMPRESS_BLOCK_EMPTY) {
        block->first_timestamp = timestamp;
        block->last_timestamp = timestamp;
        block->state = TS_COMPRESS_BLOCK_WRITING;
    }

    /* 存储原始数据 */
    uint32_t idx = block->num_points;
    if (idx >= TS_COMPRESS_BLOCK_SIZE) {
        return -1;
    }

    block->records[idx].timestamp = timestamp;
    block->records[idx].value = value;
    block->num_points++;
    block->last_timestamp = timestamp;

    /* 更新统计 */
    if (block->num_points == 1) {
        block->min_val = block->max_val = value;
    } else {
        if (value < block->min_val) block->min_val = value;
        if (value > block->max_val) block->max_val = value;
    }
    block->sum_val += value;
    comp->total_points++;
    comp->total_original_size += sizeof(ts_record_t);

    /* 检查是否需要压缩 */
    if (block->num_points >= TS_COMPRESS_BLOCK_SIZE) {
        block->state = TS_COMPRESS_BLOCK_FULL;
    }

    comp->last_timestamp = timestamp;
    comp->last_value = value;

    return 0;
}

int ts_compress_flush(ts_compressor_t *comp) {
    if (!comp || !comp->current_block) return -1;

    ts_compress_block_t *block = comp->current_block;
    if (block->num_points == 0) return 0;

    /* 分配压缩缓冲区（每点约 1-2 字节） */
    size_t max_compressed_size = block->num_points * 4 + 64;
    block->compressed_ts = (uint8_t *)calloc(max_compressed_size, 1);
    block->compressed_values = (uint8_t *)calloc(max_compressed_size, 1);

    if (!block->compressed_ts || !block->compressed_values) {
        printf("[DEBUG] ts_compress_flush: malloc failed, num_points=%u, size=%zu\n",
               block->num_points, max_compressed_size);
        free(block->compressed_ts);
        free(block->compressed_values);
        block->compressed_ts = NULL;
        block->compressed_values = NULL;
        return -1;
    }

    /* 简化的压缩：
     * - 时间戳：存储首个时间戳 + delta-of-delta
     * - 值：存储 XOR + 游程编码
     */

    size_t ts_bit_offset = 0;
    size_t value_bit_offset = 0;

    /* 写入首个时间戳（64位） */
    write_bits(block->compressed_ts, &ts_bit_offset, (uint64_t)block->first_timestamp, 64);

    /* 写入首个值（64位双精度） */
    uint64_t value_bits;
    memcpy(&value_bits, &block->records[0].value, sizeof(double));
    write_bits(block->compressed_values, &value_bit_offset, value_bits, 64);

    int64_t prev_timestamp = block->first_timestamp;
    double prev_value = block->records[0].value;

    /* 压缩后续数据 */
    for (uint32_t i = 1; i < block->num_points; i++) {
        /* 时间戳压缩 */
        int64_t delta = block->records[i].timestamp - prev_timestamp;
        int64_t delta_of_delta = delta - (prev_timestamp - (i > 1 ? block->records[i-1].timestamp : prev_timestamp));

        /* 简化：存储 delta（后续可优化为 delta-of-delta） */
        if (delta >= 0 && delta < (1 << TIMESTAMP_DELTA_BITS)) {
            write_bits(block->compressed_ts, &ts_bit_offset, 0, 1);  /* control bit: 0 = 小delta */
            write_bits(block->compressed_ts, &ts_bit_offset, (uint64_t)delta, TIMESTAMP_DELTA_BITS);
        } else {
            write_bits(block->compressed_ts, &ts_bit_offset, 1, 1);  /* control bit: 1 = 大delta */
            write_bits(block->compressed_ts, &ts_bit_offset, (uint64_t)delta, 32);
        }

        /* 值压缩 */
        uint64_t current_value_bits;
        memcpy(&current_value_bits, &block->records[i].value, sizeof(double));
        uint64_t xor = current_value_bits ^ *(uint64_t *)&prev_value;

        if (xor == 0) {
            write_bits(block->compressed_values, &value_bit_offset, 0, 1);  /* 0 = 无变化 */
        } else {
            int significant_bits = count_bits_needed(xor);
            write_bits(block->compressed_values, &value_bit_offset, 1, 1);  /* 1 = 有变化 */

            if (significant_bits <= 32) {
                write_bits(block->compressed_values, &value_bit_offset, 0, 1);  /* 0 = 32位 */
                write_bits(block->compressed_values, &value_bit_offset, xor, 32);
            } else {
                write_bits(block->compressed_values, &value_bit_offset, 1, 1);  /* 1 = 64位 */
                write_bits(block->compressed_values, &value_bit_offset, xor, 64);
            }
        }

        prev_timestamp = block->records[i].timestamp;
        prev_value = block->records[i].value;
    }

    block->ts_bits_used = ts_bit_offset;
    block->value_bits_used = value_bit_offset;
    block->compressed_size = (ts_bit_offset + value_bit_offset + 7) / 8;

    comp->total_compressed_size += block->compressed_size;
    block->state = TS_COMPRESS_BLOCK_COMPRESSED;

    /* 释放原始数据 */
    free(block->records);
    block->records = NULL;

    /* 创建新的空块 */
    comp->current_block = (ts_compress_block_t *)calloc(1, sizeof(ts_compress_block_t));
    if (!comp->current_block) return -1;

    comp->current_block->records = (ts_record_t *)calloc(TS_COMPRESS_BLOCK_SIZE, sizeof(ts_record_t));
    if (!comp->current_block->records) {
        free(comp->current_block);
        comp->current_block = NULL;
        return -1;
    }
    comp->current_block->state = TS_COMPRESS_BLOCK_EMPTY;

    return 0;
}

const uint8_t *ts_compress_get_data(const ts_compressor_t *comp, size_t *out_size) {
    if (!comp || !comp->current_block || out_size == NULL) return NULL;

    *out_size = comp->current_block->compressed_size;
    return comp->current_block->compressed_ts;
}

/**
 * @brief 获取上一个压缩块的数据（在 flush 后调用）
 * @param comp 压缩器
 * @param out_size 输出压缩数据大小
 * @return 压缩数据指针，失败返回 NULL
 */
const uint8_t *ts_compress_get_last_block_data(const ts_compressor_t *comp, size_t *out_size) {
    if (!comp || out_size == NULL) return NULL;
    /* 上一个块在压缩后被替换，可以通过 total_compressed_size 判断是否有数据 */
    if (comp->total_compressed_size == 0) return NULL;
    /* 返回值压缩数据（时间戳压缩数据需要单独处理） */
    *out_size = comp->total_compressed_size;
    return (const uint8_t *)"1";  /* 占位，实际需要合并两个缓冲区 */
}

void ts_compress_get_stats(const ts_compressor_t *comp,
                          uint64_t *total_points,
                          double *compression_ratio) {
    if (!comp) return;

    if (total_points) *total_points = comp->total_points;
    if (compression_ratio && comp->total_original_size > 0) {
        *compression_ratio = (double)comp->total_original_size / comp->total_compressed_size;
    }
}

/* ========================================================================
 * 解压实现
 * ======================================================================== */

int ts_decompress(const uint8_t *compressed_data, size_t size,
                  ts_record_t *out_records, int max_records) {
    if (!compressed_data || !out_records || max_records <= 0) return -1;

    size_t ts_bit_offset = 0;
    size_t value_bit_offset = 0;

    /* 读取首个时间戳 */
    uint64_t first_ts = read_bits(compressed_data, &ts_bit_offset, 64);
    out_records[0].timestamp = (int64_t)first_ts;

    /* 读取首个值 */
    uint64_t first_value_bits = read_bits(compressed_data, &value_bit_offset, 64);
    memcpy(&out_records[0].value, &first_value_bits, sizeof(double));

    int num_decoded = 1;
    int64_t prev_timestamp = out_records[0].timestamp;
    double prev_value = out_records[0].value;

    while (num_decoded < max_records && ts_bit_offset < size * 8) {
        /* 读取时间戳 delta */
        int control = read_bits(compressed_data, &ts_bit_offset, 1);
        int64_t delta;
        if (control == 0) {
            delta = (int64_t)read_bits(compressed_data, &ts_bit_offset, TIMESTAMP_DELTA_BITS);
        } else {
            delta = (int64_t)read_bits(compressed_data, &ts_bit_offset, 32);
        }
        out_records[num_decoded].timestamp = prev_timestamp + delta;

        /* 读取值 */
        int has_change = read_bits(compressed_data, &value_bit_offset, 1);
        uint64_t value_bits;
        if (has_change == 0) {
            value_bits = *(uint64_t *)&prev_value;
        } else {
            int bits_32 = read_bits(compressed_data, &value_bit_offset, 1);
            if (bits_32 == 0) {
                value_bits = read_bits(compressed_data, &value_bit_offset, 32);
            } else {
                value_bits = read_bits(compressed_data, &value_bit_offset, 64);
            }
        }
        memcpy(&out_records[num_decoded].value, &value_bits, sizeof(double));

        prev_timestamp = out_records[num_decoded].timestamp;
        prev_value = out_records[num_decoded].value;
        num_decoded++;
    }

    return num_decoded;
}

int ts_decompress_range(const uint8_t *compressed_data, size_t size,
                       int64_t start_time, int64_t end_time,
                       ts_record_t *out_records, int max_records) {
    /* 简化实现：先解压所有，然后过滤 */
    int total = ts_decompress(compressed_data, size, out_records, max_records);
    if (total < 0) return total;

    int count = 0;
    for (int i = 0; i < total && count < max_records; i++) {
        if (out_records[i].timestamp >= start_time && out_records[i].timestamp <= end_time) {
            out_records[count] = out_records[i];
            count++;
        }
    }
    return count;
}

int ts_compress_get_info(const uint8_t *compressed_data,
                         int64_t *out_first_timestamp,
                         uint32_t *out_num_points) {
    if (!compressed_data) return -1;

    size_t bit_offset = 0;

    /* 读取首个时间戳 */
    uint64_t first_ts = read_bits(compressed_data, &bit_offset, 64);
    if (out_first_timestamp) *out_first_timestamp = (int64_t)first_ts;

    /* 估算点数（简化实现） */
    if (out_num_points) {
        size_t remaining_bits = 0;  /* 需要更好的方式计算 */
        *out_num_points = 0;  /* 暂时设为0，调用者需要使用 ts_decompress */
    }

    return 0;
}
