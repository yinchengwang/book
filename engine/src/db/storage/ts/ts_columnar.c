/**
 * @file ts_columnar.c
 * @brief 时序数据列式存储实现
 *
 * 实现 Delta 编码、RLE、Bit-packing 等压缩算法。
 */
#include "ts_columnar.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* ========================================================================
 * Delta 编码
 * ======================================================================== */

DeltaEncoder *delta_encoder_create(void) {
    DeltaEncoder *enc = (DeltaEncoder *)calloc(1, sizeof(DeltaEncoder));
    if (!enc) return NULL;

    enc->capacity = 1024;
    enc->deltas = (uint8_t *)malloc(enc->capacity);
    if (!enc->deltas) {
        free(enc);
        return NULL;
    }

    return enc;
}

int delta_encoder_add(DeltaEncoder *enc, int64_t value) {
    if (!enc) return -1;

    if (enc->count == 0) {
        enc->first_value = value;
    } else {
        /* 计算增量 */
        int64_t delta = value - (enc->count == 1 ? enc->first_value :
            (enc->first_value + enc->count - 1)); /* 简化：假设均匀分布 */

        /* 简化实现：直接存储为字节 */
        if (enc->count >= enc->capacity) {
            enc->capacity *= 2;
            enc->deltas = (uint8_t *)realloc(enc->deltas, enc->capacity);
            if (!enc->deltas) return -1;
        }

        /* 存储增量（简化：使用 8 字节） */
        int64_t *delta_ptr = (int64_t *)enc->deltas;
        delta_ptr[enc->count - 1] = delta;
    }

    enc->count++;
    return 0;
}

int delta_encoder_finish(DeltaEncoder *enc) {
    if (!enc) return -1;

    /* 计算实际需要的位宽 */
    if (enc->count > 1) {
        int64_t *deltas = (int64_t *)enc->deltas;
        int64_t max_delta = 0;
        for (uint32_t i = 0; i < enc->count - 1; i++) {
            if (deltas[i] > max_delta) max_delta = deltas[i];
        }

        /* 计算位宽 */
        enc->max_bits = 0;
        while ((1LL << enc->max_bits) <= max_delta) {
            enc->max_bits++;
        }
        enc->max_bits = (enc->max_bits + 7) / 8 * 8; /* 对齐到字节 */
        if (enc->max_bits < 8) enc->max_bits = 8;
    }

    enc->compressed_size = sizeof(int64_t) + (enc->count - 1) * sizeof(int64_t);
    return 0;
}

const uint8_t *delta_encoder_get_data(const DeltaEncoder *enc, size_t *out_size) {
    if (!enc || !out_size) return NULL;
    *out_size = enc->compressed_size;
    return (const uint8_t *)enc->deltas;
}

int delta_encoder_decode(const DeltaEncoder *enc, int64_t *out_values, uint32_t max_count) {
    if (!enc || !out_values) return -1;

    if (max_count < enc->count) return -1;

    out_values[0] = enc->first_value;
    int64_t *deltas = (int64_t *)enc->deltas;

    for (uint32_t i = 1; i < enc->count; i++) {
        out_values[i] = out_values[i - 1] + deltas[i - 1];
    }

    return enc->count;
}

void delta_encoder_free(DeltaEncoder *enc) {
    if (enc) {
        free(enc->deltas);
        free(enc);
    }
}

/* ========================================================================
 * RLE 编码
 * ======================================================================== */

RleEncoder *rle_encoder_create(void) {
    RleEncoder *enc = (RleEncoder *)calloc(1, sizeof(RleEncoder));
    if (!enc) return NULL;

    enc->capacity = 128;
    enc->runs = (RleRun *)malloc(enc->capacity * sizeof(RleRun));
    if (!enc->runs) {
        free(enc);
        return NULL;
    }

    return enc;
}

int rle_encoder_add(RleEncoder *enc, int64_t value) {
    if (!enc) return -1;

    enc->total_count++;

    /* 检查是否与上一个运行相同 */
    if (enc->run_count > 0) {
        RleRun *last_run = &enc->runs[enc->run_count - 1];
        if (last_run->value == value) {
            last_run->count++;
            return 0;
        }
    }

    /* 开始新运行 */
    if (enc->run_count >= enc->capacity) {
        enc->capacity *= 2;
        enc->runs = (RleRun *)realloc(enc->runs, enc->capacity * sizeof(RleRun));
        if (!enc->runs) return -1;
    }

    RleRun *run = &enc->runs[enc->run_count];
    run->value = value;
    run->count = 1;
    run->start_pos = enc->total_count - 1;
    enc->run_count++;
    enc->unique_count++;

    return 0;
}

int rle_encoder_finish(RleEncoder *enc) {
    if (!enc) return -1;
    /* RLE 无需特殊完成操作 */
    return 0;
}

const uint8_t *rle_encoder_get_data(const RleEncoder *enc, size_t *out_size) {
    if (!enc || !out_size) return NULL;

    /* 格式: [run_count:4][runs: run_count × (value:8 + count:4 + start:4)] */
    *out_size = 4 + enc->run_count * 16;
    return (const uint8_t *)enc->runs;
}

uint32_t rle_encoder_get_run_count(const RleEncoder *enc) {
    return enc ? enc->run_count : 0;
}

const RleRun *rle_encoder_get_run(const RleEncoder *enc, uint32_t run_idx) {
    if (!enc || run_idx >= enc->run_count) return NULL;
    return &enc->runs[run_idx];
}

int rle_encoder_decode_range(const RleEncoder *enc,
                             uint32_t start, uint32_t end,
                             int64_t *out_values, uint32_t max_count) {
    if (!enc || !out_values) return -1;

    uint32_t out_idx = 0;
    uint32_t pos = 0;

    for (uint32_t i = 0; i < enc->run_count && out_idx < max_count; i++) {
        RleRun *run = &enc->runs[i];

        /* 计算这个运行的覆盖范围 */
        uint32_t run_start = pos;
        uint32_t run_end = pos + run->count;

        /* 检查与查询范围的交集 */
        uint32_t query_start = start < run_start ? run_start : start;
        uint32_t query_end = end > run_end ? run_end : end;

        if (query_start < query_end) {
            /* 复制值 */
            uint32_t copy_count = query_end - query_start;
            for (uint32_t j = 0; j < copy_count && out_idx < max_count; j++) {
                out_values[out_idx++] = run->value;
            }
        }

        pos = run_end;
        if (pos >= end) break;
    }

    return out_idx;
}

void rle_encoder_free(RleEncoder *enc) {
    if (enc) {
        free(enc->runs);
        free(enc);
    }
}

/* ========================================================================
 * Bit-packing 编码
 * ======================================================================== */

BitPackingEncoder *bitpack_encoder_create(uint32_t bit_width) {
    BitPackingEncoder *enc = (BitPackingEncoder *)calloc(1, sizeof(BitPackingEncoder));
    if (!enc) return NULL;

    enc->bit_width = bit_width > 0 ? bit_width : 8;
    enc->capacity = 1024;

    /* 分配打包缓冲区 */
    size_t max_size = (enc->capacity * enc->bit_width + 7) / 8 + 4;
    enc->packed_data = (uint8_t *)malloc(max_size);
    if (!enc->packed_data) {
        free(enc);
        return NULL;
    }

    return enc;
}

int bitpack_encoder_add(BitPackingEncoder *enc, uint64_t value) {
    if (!enc) return -1;

    if (enc->count >= enc->capacity) {
        enc->capacity *= 2;
        size_t max_size = (enc->capacity * enc->bit_width + 7) / 8 + 4;
        uint8_t *new_data = (uint8_t *)realloc(enc->packed_data, max_size);
        if (!new_data) return -1;
        enc->packed_data = new_data;
    }

    /* Bit-packing 实现 */
    uint32_t byte_pos = (enc->count * enc->bit_width) / 8;
    uint32_t bit_offset = (enc->count * enc->bit_width) % 8;
    uint64_t val = value & ((1ULL << enc->bit_width) - 1);

    /* 写入位 */
    for (uint32_t b = 0; b < enc->bit_width; b++) {
        if (val & (1ULL << b)) {
            enc->packed_data[byte_pos + b / 8] |= (1 << (b % 8));
        }
    }

    enc->count++;
    return 0;
}

int bitpack_encoder_add_batch(BitPackingEncoder *enc,
                              const uint64_t *values,
                              uint32_t count) {
    if (!enc || !values) return -1;

    for (uint32_t i = 0; i < count; i++) {
        if (bitpack_encoder_add(enc, values[i]) != 0) return -1;
    }

    return 0;
}

int bitpack_encoder_finish(BitPackingEncoder *enc) {
    if (!enc) return -1;
    enc->packed_size = (enc->count * enc->bit_width + 7) / 8;
    return 0;
}

const uint8_t *bitpack_encoder_get_data(const BitPackingEncoder *enc, size_t *out_size) {
    if (!enc || !out_size) return NULL;
    *out_size = enc->packed_size;
    return enc->packed_data;
}

int bitpack_encoder_decode(const BitPackingEncoder *enc,
                           uint64_t *out_values,
                           uint32_t max_count) {
    if (!enc || !out_values) return -1;

    uint32_t count = enc->count < max_count ? enc->count : max_count;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t byte_pos = (i * enc->bit_width) / 8;
        uint32_t bit_offset = (i * enc->bit_width) % 8;

        uint64_t val = 0;
        for (uint32_t b = 0; b < enc->bit_width; b++) {
            uint32_t src_byte = byte_pos + b / 8;
            uint32_t src_bit = b % 8;
            if (enc->packed_data[src_byte] & (1 << src_bit)) {
                val |= (1ULL << b);
            }
        }
        out_values[i] = val;
    }

    return count;
}

void bitpack_encoder_free(BitPackingEncoder *enc) {
    if (enc) {
        free(enc->packed_data);
        free(enc);
    }
}

/* ========================================================================
 * 混合压缩编码
 * ======================================================================== */

TsColumnarCompressor *ts_columnar_compressor_create(void) {
    TsColumnarCompressor *comp = (TsColumnarCompressor *)calloc(1, sizeof(TsColumnarCompressor));
    if (!comp) return NULL;

    comp->raw_capacity = 4096;
    comp->raw_values = (int64_t *)malloc(comp->raw_capacity * sizeof(int64_t));
    if (!comp->raw_values) {
        free(comp);
        return NULL;
    }

    return comp;
}

int ts_columnar_compressor_add(TsColumnarCompressor *comp,
                               int64_t timestamp,
                               double value) {
    if (!comp) return -1;

    if (comp->raw_count >= comp->raw_capacity) {
        comp->raw_capacity *= 2;
        comp->raw_values = (int64_t *)realloc(comp->raw_values,
            comp->raw_capacity * sizeof(int64_t));
        if (!comp->raw_values) return -1;
    }

    /* 简化：直接存储值 */
    comp->raw_values[comp->raw_count++] = (int64_t)value;
    (void)timestamp; /* 简化实现 */

    return 0;
}

int ts_columnar_compressor_add_batch(TsColumnarCompressor *comp,
                                     const int64_t *timestamps,
                                     const double *values,
                                     uint32_t count) {
    if (!comp || !values) return -1;

    for (uint32_t i = 0; i < count; i++) {
        if (ts_columnar_compressor_add(comp, timestamps ? timestamps[i] : 0,
                                       values[i]) != 0) {
            return -1;
        }
    }

    return 0;
}

TsCompressionType ts_columnar_compressor_auto_compress(TsColumnarCompressor *comp) {
    if (!comp || comp->raw_count == 0) return COMPRESS_NONE;

    TsCompressionType best_type = COMPRESS_NONE;
    size_t best_size = comp->raw_count * sizeof(int64_t);

    /* 测试各压缩方式 */
    TsCompressionType types[] = {COMPRESS_DELTA, COMPRESS_RLE, COMPRESS_BITPACK};
    const char *type_names[] = {"DELTA", "RLE", "BITPACK"};

    for (int t = 0; t < 3; t++) {
        ts_columnar_compressor_compress(comp, types[t]);
        size_t compressed_size = comp->best_compressed_size;

        if (compressed_size < best_size) {
            best_size = compressed_size;
            best_type = types[t];
        }
    }

    /* 使用最佳方式重新压缩 */
    if (best_type != COMPRESS_NONE) {
        ts_columnar_compressor_compress(comp, best_type);
    }

    comp->selected_type = best_type;
    return best_type;
}

int ts_columnar_compressor_compress(TsColumnarCompressor *comp,
                                    TsCompressionType type) {
    if (!comp) return -1;

    comp->selected_type = type;

    /* 根据类型选择编码器 */
    switch (type) {
        case COMPRESS_DELTA: {
            if (!comp->delta_enc) comp->delta_enc = delta_encoder_create();
            for (uint32_t i = 0; i < comp->raw_count; i++) {
                delta_encoder_add(comp->delta_enc, comp->raw_values[i]);
            }
            delta_encoder_finish(comp->delta_enc);
            size_t size;
            comp->best_compressed = (uint8_t *)delta_encoder_get_data(comp->delta_enc, &size);
            comp->best_compressed_size = size;
            break;
        }

        case COMPRESS_RLE: {
            if (!comp->rle_enc) comp->rle_enc = rle_encoder_create();
            for (uint32_t i = 0; i < comp->raw_count; i++) {
                rle_encoder_add(comp->rle_enc, comp->raw_values[i]);
            }
            rle_encoder_finish(comp->rle_enc);
            size_t size;
            comp->best_compressed = (uint8_t *)rle_encoder_get_data(comp->rle_enc, &size);
            comp->best_compressed_size = size;
            break;
        }

        case COMPRESS_BITPACK: {
            /* 计算位宽 */
            int64_t max_val = 0;
            for (uint32_t i = 0; i < comp->raw_count; i++) {
                if (comp->raw_values[i] > max_val) max_val = comp->raw_values[i];
            }
            uint32_t bit_width = 0;
            while ((1LL << bit_width) <= max_val) bit_width++;
            bit_width = (bit_width + 7) / 8 * 8;
            if (bit_width < 8) bit_width = 8;

            if (!comp->bitpack_enc) comp->bitpack_enc = bitpack_encoder_create(bit_width);
            for (uint32_t i = 0; i < comp->raw_count; i++) {
                bitpack_encoder_add(comp->bitpack_enc, (uint64_t)comp->raw_values[i]);
            }
            bitpack_encoder_finish(comp->bitpack_enc);
            size_t size;
            comp->best_compressed = (uint8_t *)bitpack_encoder_get_data(comp->bitpack_enc, &size);
            comp->best_compressed_size = size;
            break;
        }

        default:
            comp->best_compressed = (uint8_t *)comp->raw_values;
            comp->best_compressed_size = comp->raw_count * sizeof(int64_t);
    }

    return 0;
}

const uint8_t *ts_columnar_compressor_get_data(const TsColumnarCompressor *comp,
                                               size_t *out_size,
                                               TsCompressionType *out_type) {
    if (!comp || !out_size) return NULL;

    *out_size = comp->best_compressed_size;
    if (out_type) *out_type = comp->selected_type;
    return comp->best_compressed;
}

double ts_columnar_compressor_get_ratio(const TsColumnarCompressor *comp) {
    if (!comp) return 0.0;

    size_t original_size = comp->raw_count * sizeof(int64_t);
    if (comp->best_compressed_size == 0) return 0.0;

    return (double)original_size / comp->best_compressed_size;
}

void ts_columnar_compressor_free(TsColumnarCompressor *comp) {
    if (comp) {
        free(comp->raw_values);
        delta_encoder_free(comp->delta_enc);
        rle_encoder_free(comp->rle_enc);
        bitpack_encoder_free(comp->bitpack_enc);
        free(comp);
    }
}

/* ========================================================================
 * 列式存储页
 * ======================================================================== */

ColumnarPage *columnar_page_create(uint32_t capacity) {
    ColumnarPage *page = (ColumnarPage *)calloc(1, sizeof(ColumnarPage));
    if (!page) return NULL;

    page->timestamp_col.type = COLUMN_TIMESTAMP;
    page->timestamp_col.capacity = capacity;
    page->timestamp_col.data = malloc(capacity * sizeof(int64_t));

    page->value_col.type = COLUMN_VALUE;
    page->value_col.capacity = capacity;
    page->value_col.data = malloc(capacity * sizeof(double));

    if (!page->timestamp_col.data || !page->value_col.data) {
        free(page->timestamp_col.data);
        free(page->value_col.data);
        free(page);
        return NULL;
    }

    return page;
}

int columnar_page_add(ColumnarPage *page, int64_t timestamp, double value) {
    if (!page || page->row_count >= page->timestamp_col.capacity) return -1;

    int64_t *timestamps = (int64_t *)page->timestamp_col.data;
    double *values = (double *)page->value_col.data;

    timestamps[page->row_count] = timestamp;
    values[page->row_count] = value;

    if (page->row_count == 0) {
        page->start_time = timestamp;
        page->value_col.min_value = value;
        page->value_col.max_value = value;
        page->value_col.sum_value = value;
    } else {
        if (value < page->value_col.min_value) page->value_col.min_value = value;
        if (value > page->value_col.max_value) page->value_col.max_value = value;
        page->value_col.sum_value += value;
    }

    page->end_time = timestamp;
    page->row_count++;

    return 0;
}

int columnar_page_compress(ColumnarPage *page, TsCompressionType type) {
    if (!page) return -1;

    /* 使用混合压缩器压缩值列 */
    TsColumnarCompressor *comp = ts_columnar_compressor_create();
    if (!comp) return -1;

    double *values = (double *)page->value_col.data;
    for (uint32_t i = 0; i < page->row_count; i++) {
        ts_columnar_compressor_add(comp, 0, values[i]);
    }

    if (type == COMPRESS_BEST) {
        type = ts_columnar_compressor_auto_compress(comp);
    } else {
        ts_columnar_compressor_compress(comp, type);
    }

    size_t compressed_size;
    TsCompressionType actual_type;
    const uint8_t *compressed = ts_columnar_compressor_get_data(comp, &compressed_size, &actual_type);

    page->value_col.compressed_data = malloc(compressed_size);
    if (page->value_col.compressed_data) {
        memcpy(page->value_col.compressed_data, compressed, compressed_size);
        page->value_col.compressed_size = compressed_size;
        page->value_col.state = BLOCK_COMPRESSED;
    }

    ts_columnar_compressor_free(comp);
    return 0;
}

int columnar_page_decompress(ColumnarPage *page) {
    if (!page) return -1;

    if (page->value_col.state != BLOCK_COMPRESSED) return 0;

    /* 简化：直接复制回原始数据 */
    /* 实际需要根据压缩类型解码 */
    /* 这里简化处理，实际应该使用解压 API */

    page->value_col.state = BLOCK_UNCOMPRESSED;
    return 0;
}

uint32_t columnar_page_query(const ColumnarPage *page,
                             int64_t start_time, int64_t end_time,
                             void *results, uint32_t max_results) {
    if (!page) return 0;

    int64_t *timestamps = (int64_t *)page->timestamp_col.data;
    double *values = (double *)page->value_col.data;
    double *out = (double *)results;

    uint32_t count = 0;
    for (uint32_t i = 0; i < page->row_count && count < max_results; i++) {
        if (timestamps[i] >= start_time && timestamps[i] <= end_time) {
            out[count++] = values[i];
        }
    }

    return count;
}

int columnar_page_aggregate(const ColumnarPage *page,
                            int64_t start_time, int64_t end_time,
                            double *out_sum, double *out_avg,
                            double *out_min, double *out_max,
                            uint32_t *out_count) {
    if (!page) return -1;

    int64_t *timestamps = (int64_t *)page->timestamp_col.data;
    double *values = (double *)page->value_col.data;

    double sum = 0, min = DBL_MAX, max = -DBL_MAX;
    uint32_t count = 0;

    for (uint32_t i = 0; i < page->row_count; i++) {
        if (timestamps[i] >= start_time && timestamps[i] <= end_time) {
            double v = values[i];
            sum += v;
            if (v < min) min = v;
            if (v > max) max = v;
            count++;
        }
    }

    if (out_sum) *out_sum = sum;
    if (out_avg) *out_avg = count > 0 ? sum / count : 0;
    if (out_min) *out_min = count > 0 ? min : 0;
    if (out_max) *out_max = count > 0 ? max : 0;
    if (out_count) *out_count = count;

    return 0;
}

void columnar_page_free(ColumnarPage *page) {
    if (page) {
        free(page->timestamp_col.data);
        free(page->value_col.data);
        free(page->value_col.compressed_data);
        free(page);
    }
}

/* ========================================================================
 * 块跳过优化
 * ======================================================================== */

BlockSkipInfo *block_skip_info_create(int64_t start_time, int64_t end_time,
                                      double min_value, double max_value) {
    BlockSkipInfo *info = (BlockSkipInfo *)calloc(1, sizeof(BlockSkipInfo));
    if (!info) return NULL;

    info->start_time = start_time;
    info->end_time = end_time;
    info->min_value = min_value;
    info->max_value = max_value;
    info->has_overlap = true;

    return info;
}

bool block_skip_has_overlap(const BlockSkipInfo *info,
                            int64_t start_time, int64_t end_time) {
    if (!info) return true;
    return !(info->end_time < start_time || info->start_time > end_time);
}

bool block_skip_value_overlap(const BlockSkipInfo *info,
                              double min_value, double max_value) {
    if (!info) return true;
    return !(info->max_value < min_value || info->min_value > max_value);
}

uint32_t block_skip_batch_check(const BlockSkipInfo **blocks,
                                uint32_t count,
                                int64_t start_time, int64_t end_time,
                                double min_value, double max_value,
                                bool *skip_flags) {
    if (!blocks || !skip_flags) return 0;

    uint32_t skip_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        const BlockSkipInfo *info = blocks[i];

        /* 检查时间范围重叠 */
        bool time_overlap = block_skip_has_overlap(info, start_time, end_time);

        /* 检查值范围重叠 */
        bool value_overlap = block_skip_value_overlap(info, min_value, max_value);

        /* 如果任一不重叠，可以跳过 */
        skip_flags[i] = !time_overlap && !value_overlap;
        if (skip_flags[i]) skip_count++;
    }

    return skip_count;
}

void block_skip_info_free(BlockSkipInfo *info) {
    free(info);
}
