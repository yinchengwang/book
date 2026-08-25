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
    void *data;                  /**< 原始数据 */
    size_t capacity;             /**< 容量 */
    size_t count;                /**< 当前行数 */
    size_t element_size;         /**< 元素大小 */
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
    free(chunk->data);
    while (chunk->next) {
        column_chunk_t *next = chunk->next;
        free(chunk->data);
        free(chunk);
        chunk = next;
    }
    free(chunk);
}

static int append_to_chunk(column_chunk_t *chunk, const void *value) {
    if (chunk->count >= chunk->capacity) {
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

#endif /* DB_STORAGE_COLUMNAR_ENGINE_INTERNAL_H */
