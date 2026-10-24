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

/* ========================================================================
 * C2-4 T1：增量编码器（delta-of-delta + XOR）骨架
 *
 * 完整实现：每个点写入时即时编码，不保留原始 16B/点。
 * 简化版：当前仅记录增量（dod、xor_value）到 per-block 数组，
 * flush 时由原 ts_compress_flush 编码为位流。完整位流编码待续。
 * ======================================================================== */

int ts_compress_add_immediate(ts_compressor_t *comp, int64_t timestamp, double value) {
    if (!comp || !comp->current_block) return -22;  /* DBERR_CORRUPT 之类的负值 */

    ts_compress_block_t *block = comp->current_block;

    /* 满块路径：T2 触发显式 flush + retry */
    if (block->state == TS_COMPRESS_BLOCK_FULL) {
        if (ts_compress_flush(comp) != 0) {
            return -28;  /* DBERR_FULL 标识 */
        }
        /* flush 后 comp 仍持有同一 current_block，但 state 应为 COMPRESSED/EMPTY */
        /* 注：完整实现应支持"flush 后开新块"，此处 best-effort 续写 */
        if (block->state == TS_COMPRESS_BLOCK_FULL) {
            return -28;  /* flush 未释放空间 */
        }
    }

    /* 沿用 ts_compress_add 的写入路径（已经是 C2-4 之前就位），
     * 即 T1 的核心：调用方使用本函数时表示期望热路径即时压缩。
     * 当前实现复用 add —— 完整 delta-of-delta 位流编码待后续变更展开。
     */
    return ts_compress_add(comp, timestamp, value);
}

int ts_compress_force_flush_add(ts_compressor_t *comp, int64_t timestamp, double value) {
    if (!comp) return -22;
    /* 强制 flush 一次后 retry insert */
    ts_compress_flush(comp);
    return ts_compress_add(comp, timestamp, value);
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

    /* 获取上一个压缩块的数据（时间戳和值数据合并） */
    if (comp->total_compressed_size == 0) return NULL;

    /* 上一个块的数据存储在压缩后的缓冲区中 */
    /* 注意：这里返回的是最后一个已压缩块的数据 */
    /* 实际实现中，应该有一个块链表来跟踪所有压缩后的块 */
    /* 简化实现：返回总压缩大小，但数据需要从块链表中获取 */

    /* 由于当前实现中 flush 后会创建新块，
     * 我们需要一个机制来跟踪历史块。
     * 简化处理：返回 NULL，调用者应使用 ts_compress_get_data 获取当前块数据 */
    *out_size = 0;
    return NULL;
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

    /* 估算点数：通过遍历时间戳压缩数据计算 */
    if (out_num_points) {
        /* 压缩格式：每个点的时间戳压缩为 1 bit 控制位 + (14 或 32) bits 数据 */
        /* 第一个点用 64 bits，后续点平均约 15 bits */
        /* 总大小（字节）* 8 / 平均每点位数 */
        /* 简化估算：假设每点约 16 bits（1 + 14 + 1 填充） */
        size_t total_bits = 0;
        size_t temp_offset = 0;

        /* 跳过首个时间戳（64 bits） */
        temp_offset = 64;

        /* 计算剩余位数 */
        while (temp_offset < 64 * 1024) {  /* 最大 64KB 数据 */
            int control = read_bits(compressed_data, &temp_offset, 1);
            if (control == 0) {
                temp_offset += TIMESTAMP_DELTA_BITS;  /* 14 bits */
            } else {
                temp_offset += 32;  /* 32 bits */
            }
            total_bits = temp_offset;
        }

        /* 估算点数：首点 64 bits + 后续每点约 15 bits */
        uint32_t estimated = 1;
        if (total_bits > 64) {
            estimated = 1 + (uint32_t)((total_bits - 64) / 15);
        }
        *out_num_points = estimated;
    }

    return 0;
}

/* ========================================================================
 * Gorilla XOR 浮点压缩实现
 *
 * 核心算法：
 * 1. 第一个值存储原始 32-bit
 * 2. 后续值与前一个值 XOR
 * 3. XOR 结果的前导零(L)和尾部零(T)已知后，只需存储中间有效位
 * 编码格式：
 *   - 0x00: 与前值相同（1 bit）
 *   - 0x01 + meaningful bits: 有效位数未减少
 *   - 0x10: 前值 L 位后有效，存储 L 和尾部位
 *   - 0x11 + L bits + T bits: 需要存储 L 和 T
 * ======================================================================== */

/* 缓冲区初始大小 */
#define GORILLA_INIT_BUF_SIZE 4096

/* 单个 float 的位数 */
#define FLOAT_BITS 32

/**
 * @brief 位写入：向缓冲区写入指定数量的位
 */
static void gorilla_write_bits(gorilla_encoder_t *enc, uint64_t value, int num_bits) {
    for (int i = 0; i < num_bits; i++) {
        int bit = (value >> i) & 1;
        if (bit) {
            enc->buffer[enc->byte_pos] |= (1 << enc->bit_pos);
        }
        enc->bit_pos++;
        if (enc->bit_pos == BITS_PER_BYTE) {
            enc->bit_pos = 0;
            enc->byte_pos++;
        }
    }
}

/**
 * @brief 位读取：从缓冲区读取指定数量的位
 */
static uint64_t gorilla_read_bits(gorilla_decoder_t *dec, int num_bits) {
    uint64_t value = 0;
    for (int i = 0; i < num_bits; i++) {
        if (dec->byte_pos >= dec->buffer_size) break;
        int bit = (dec->buffer[dec->byte_pos] >> dec->bit_pos) & 1;
        value |= ((uint64_t)bit << i);
        dec->bit_pos++;
        if (dec->bit_pos == BITS_PER_BYTE) {
            dec->bit_pos = 0;
            dec->byte_pos++;
        }
    }
    return value;
}

int gorilla_encoder_init(gorilla_encoder_t *enc) {
    if (!enc) return -1;
    enc->buffer = (uint8_t *)calloc(GORILLA_INIT_BUF_SIZE, 1);
    if (!enc->buffer) return -1;
    enc->byte_pos = 0;
    enc->bit_pos = 0;
    enc->prev_value = 0.0f;
    enc->has_prev = 0;
    return 0;
}

void gorilla_encoder_destroy(gorilla_encoder_t *enc) {
    if (enc && enc->buffer) {
        free(enc->buffer);
        enc->buffer = NULL;
    }
}

int gorilla_decoder_init(gorilla_decoder_t *dec, const uint8_t *data, size_t size) {
    if (!dec || !data || size == 0) return -1;
    dec->buffer = data;
    dec->buffer_size = size;
    dec->byte_pos = 0;
    dec->bit_pos = 0;
    dec->prev_value = 0.0f;
    dec->has_prev = 0;
    return 0;
}

void gorilla_decoder_destroy(gorilla_decoder_t *dec) {
    if (dec) {
        dec->buffer = NULL;
        dec->buffer_size = 0;
    }
}

int gorilla_encode(gorilla_encoder_t *enc, float value) {
    if (!enc) return -1;

    /* 确保缓冲区足够大 */
    size_t needed = enc->byte_pos + 16;
    size_t current_size = GORILLA_INIT_BUF_SIZE;
    while (current_size < needed) current_size *= 2;
    if (current_size > GORILLA_INIT_BUF_SIZE) {
        uint8_t *new_buf = (uint8_t *)realloc(enc->buffer, current_size);
        if (!new_buf) return -1;
        enc->buffer = new_buf;
        memset(enc->buffer + (current_size / 2), 0, current_size / 2);
    }

    uint32_t bits;
    memcpy(&bits, &value, sizeof(float));

    if (!enc->has_prev) {
        /* 第一个值：直接存储 32-bit */
        gorilla_write_bits(enc, bits, FLOAT_BITS);
        enc->prev_value = value;
        enc->has_prev = 1;
        return 0;
    }

    /* 计算 XOR */
    uint32_t prev_bits;
    memcpy(&prev_bits, &enc->prev_value, sizeof(float));
    uint32_t xor_val = bits ^ prev_bits;

    if (xor_val == 0) {
        /* 与前值相同：存储 1 bit (0) */
        gorilla_write_bits(enc, 0, 1);
    } else {
        /* 有变化：存储 1 bit (1) */
        gorilla_write_bits(enc, 1, 1);

        /* 计算前导零和尾部零 */
        int leading_zeros = 0;
        int trailing_zeros = 0;
        uint32_t temp = xor_val;

        /* 前导零 */
        for (int i = FLOAT_BITS - 1; i >= 0; i--) {
            if ((temp >> i) & 1) break;
            leading_zeros++;
        }

        /* 尾部零 */
        temp = xor_val;
        while ((temp & 1) == 0 && trailing_zeros < FLOAT_BITS) {
            temp >>= 1;
            trailing_zeros++;
        }

        int significant_bits = FLOAT_BITS - leading_zeros - trailing_zeros;

        /* 存储有意义位变化标志（简化：总是1） */
        gorilla_write_bits(enc, 1, 1);

        /* 存储前导零数量（5 bits，最多 32） */
        gorilla_write_bits(enc, leading_zeros, 5);

        /* 存储尾部零数量（5 bits，最多 32） */
        gorilla_write_bits(enc, trailing_zeros, 5);

        /* 存储有效位 */
        gorilla_write_bits(enc, xor_val >> trailing_zeros, significant_bits);
    }

    enc->prev_value = value;
    return 0;
}

int gorilla_decode(gorilla_decoder_t *dec, float *value) {
    if (!dec || !value) return -1;

    if (!dec->has_prev) {
        /* 读取第一个值（32-bit） */
        uint32_t bits = (uint32_t)gorilla_read_bits(dec, FLOAT_BITS);
        float v;
        memcpy(&v, &bits, sizeof(float));
        *value = v;
        dec->prev_value = v;
        dec->has_prev = 1;
        return 0;
    }

    /* 读取控制位 */
    int has_change = (int)gorilla_read_bits(dec, 1);

    if (has_change == 0) {
        /* 与前值相同 */
        *value = dec->prev_value;
        return 0;
    }

    /* 读取有意义位变化标志 */
    int meaningful = (int)gorilla_read_bits(dec, 1);
    (void)meaningful;

    /* 读取前导零数量（5 bits） */
    int leading_zeros = (int)gorilla_read_bits(dec, 5);

    /* 读取尾部零数量（5 bits） */
    int trailing_zeros = (int)gorilla_read_bits(dec, 5);

    /* 计算有效位数 */
    int significant_bits = FLOAT_BITS - leading_zeros - trailing_zeros;
    if (significant_bits <= 0) significant_bits = 1;  /* 防止负数 */

    /* 读取有效位 */
    uint32_t xor_val = (uint32_t)gorilla_read_bits(dec, significant_bits);
    xor_val <<= trailing_zeros;

    /* 重建值 */
    uint32_t prev_bits;
    memcpy(&prev_bits, &dec->prev_value, sizeof(float));
    uint32_t result = prev_bits ^ xor_val;

    float v;
    memcpy(&v, &result, sizeof(float));
    *value = v;
    dec->prev_value = v;

    return 0;
}

const uint8_t *gorilla_encoder_get_data(const gorilla_encoder_t *enc, size_t *out_size) {
    if (!enc || !out_size) return NULL;
    /* 输出实际使用的字节数 */
    size_t total_bits = enc->byte_pos * BITS_PER_BYTE + enc->bit_pos;
    *out_size = (total_bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
    return enc->buffer;
}
