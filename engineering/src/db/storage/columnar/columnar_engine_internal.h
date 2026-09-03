/**
 * @file columnar_engine_internal.h
 * @brief 列式存储引擎内部头文件
 */
#ifndef DB_STORAGE_COLUMNAR_ENGINE_INTERNAL_H
#define DB_STORAGE_COLUMNAR_ENGINE_INTERNAL_H

#include "db/columnar_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 列数据块 */
typedef struct column_chunk_s {
    char name[64];               /**< 列名 */
    void *data;                  /**< 原始数据（运行时）*/
    size_t capacity;             /**< 容量 */
    size_t count;                /**< 当前行数 */
    size_t element_size;         /**< 元素大小 */
    size_t file_offset;          /**< 文件偏移量（序列化时使用）*/
    size_t compressed_size;      /**< 压缩后大小（0表示未压缩）*/
    struct column_chunk_s *next; /**< 下一块 */
} column_chunk_t;

/** 列描述符 */
typedef struct column_desc_s {
    char name[64];               /**< 列名 */
    int32_t type_oid;            /**< 类型 OID */
    column_chunk_t *chunks;      /**< 数据块链表 */
    size_t total_count;          /**< 总行数 */
} column_desc_t;

/** 列存表内部结构 */
typedef struct columnar_table_internal_s {
    char name[256];
    column_desc_t *columns;      /**< 列数组 */
    int32_t num_columns;         /**< 列数 */
    int64_t row_count;           /**< 行数 */
    char data_dir[512];
} columnar_table_internal_t;

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static column_chunk_t *create_chunk(const char *name, size_t element_size) {
    column_chunk_t *chunk = (column_chunk_t *)calloc(1, sizeof(column_chunk_t));
    if (!chunk) return NULL;
    strncpy(chunk->name, name, sizeof(chunk->name) - 1);
    chunk->element_size = element_size;
    chunk->capacity = 1024;  // 初始容量
    chunk->data = malloc(chunk->capacity * element_size);
    if (!chunk->data) {
        free(chunk);
        return NULL;
    }
    return chunk;
}

static void destroy_chunk(column_chunk_t *chunk) {
    if (!chunk) return;
    column_chunk_t *current = chunk;
    while (current) {
        column_chunk_t *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
}

static int append_to_chunk(column_chunk_t *chunk, const void *value) {
    if (chunk->count >= chunk->capacity) {
        /* 尝试扩展当前块 */
        size_t new_capacity = chunk->capacity * 2;
        void *new_data = realloc(chunk->data, new_capacity * chunk->element_size);
        if (!new_data) return -1;
        chunk->data = new_data;
        chunk->capacity = new_capacity;
    }
    memcpy((char *)chunk->data + chunk->count * chunk->element_size, value, chunk->element_size);
    chunk->count++;
    return 0;
}

/* ========================================================================
 * 压缩/解压缩（简单的Delta+RLE压缩）
 * ======================================================================== */

/**
 * @brief 压缩数据块
 * @param chunk 要压缩的数据块
 * @return 0成功，-1失败
 */
static int columnar_compress_chunk(column_chunk_t *chunk) {
    if (!chunk || !chunk->data || chunk->count == 0) return -1;
    if (chunk->compressed_size > 0) return 0; /* 已压缩 */

    size_t raw_size = chunk->count * chunk->element_size;

    /* 简单Delta+RLE压缩 */
    size_t max_compressed = raw_size + 256;
    void *compressed = malloc(max_compressed);
    if (!compressed) return -1;

    unsigned char *out = (unsigned char *)compressed;
    size_t pos = 0;

    if (chunk->element_size == 8) {
        uint64_t prev = 0;
        bool has_prev = false;
        size_t rle_count = 0;

        for (size_t i = 0; i < chunk->count; i++) {
            uint64_t val = ((uint64_t *)chunk->data)[i];

            if (has_prev && val == prev) {
                rle_count++;
            } else {
                if (rle_count > 0) {
                    if (pos + 17 > max_compressed) { free(compressed); return -1; }
                    out[pos++] = 0x01;
                    memcpy(&out[pos], &prev, sizeof(prev)); pos += sizeof(prev);
                    memcpy(&out[pos], &rle_count, sizeof(rle_count)); pos += sizeof(rle_count);
                }
                prev = val;
                rle_count = 1;
                has_prev = true;
            }
        }
        if (rle_count > 0) {
            if (pos + 17 > max_compressed) { free(compressed); return -1; }
            out[pos++] = 0x01;
            memcpy(&out[pos], &prev, sizeof(prev)); pos += sizeof(prev);
            memcpy(&out[pos], &rle_count, sizeof(rle_count)); pos += sizeof(rle_count);
        }
    } else {
        for (size_t i = 0; i < chunk->count; i++) {
            if (pos + chunk->element_size + 1 > max_compressed) { free(compressed); return -1; }
            out[pos++] = 0x00;
            memcpy(&out[pos], (char *)chunk->data + i * chunk->element_size, chunk->element_size);
            pos += chunk->element_size;
        }
    }

    free(chunk->data);
    chunk->data = compressed;
    chunk->compressed_size = pos;
    chunk->capacity = pos;
    return 0;
}

/**
 * @brief 解压数据块
 * @param chunk 要解压的数据块
 * @return 0成功，-1失败
 */
static int columnar_decompress_chunk(column_chunk_t *chunk) {
    if (!chunk || chunk->compressed_size == 0) return 0;
    if (chunk->element_size == 0) return -1;

    size_t raw_size = chunk->count * chunk->element_size;
    void *decompressed = malloc(raw_size);
    if (!decompressed) return -1;

    unsigned char *in = (unsigned char *)chunk->data;
    size_t pos = 0;
    size_t out_idx = 0;

    while (pos < chunk->compressed_size && out_idx < chunk->count) {
        unsigned char marker = in[pos++];

        if (marker == 0x01) {
            uint64_t val;
            size_t count;
            memcpy(&val, &in[pos], sizeof(val)); pos += sizeof(val);
            memcpy(&count, &in[pos], sizeof(count)); pos += sizeof(count);
            for (size_t i = 0; i < count && out_idx < chunk->count; i++) {
                ((uint64_t *)decompressed)[out_idx++] = val;
            }
        } else {
            memcpy((char *)decompressed + out_idx * chunk->element_size, &in[pos], chunk->element_size);
            pos += chunk->element_size;
            out_idx++;
        }
    }

    free(chunk->data);
    chunk->data = decompressed;
    chunk->compressed_size = 0;
    chunk->capacity = raw_size;
    return 0;
}

#endif /* DB_STORAGE_COLUMNAR_ENGINE_INTERNAL_H */
