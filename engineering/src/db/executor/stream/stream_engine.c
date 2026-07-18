/**
 * @file stream_engine.c
 * @brief 流引擎实现
 *
 * 实现内存环形缓冲区流引擎，支持基本的数据插入和查询。
 */
#include "db/stream_engine.h"
#include <stdlib.h>
#include <string.h>

/* 全局流表 */
static stream_engine_db_t *g_streams[STREAM_MAX_STREAMS];
static int g_stream_count = 0;
static char g_data_dir[512] = {0};
static int g_initialized = 0;

/* ========================================================================
 * 流引擎生命周期
 * ======================================================================== */

int stream_engine_init(const char *data_dir)
{
    if (g_initialized) {
        return 0;
    }

    if (data_dir) {
        strncpy(g_data_dir, data_dir, sizeof(g_data_dir) - 1);
    }

    memset(g_streams, 0, sizeof(g_streams));
    g_stream_count = 0;
    g_initialized = 1;

    return 0;
}

int stream_engine_shutdown(void)
{
    if (!g_initialized) {
        return 0;
    }

    /* 关闭所有流 */
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (g_streams[i] != NULL) {
            if (g_streams[i]->buffer != NULL) {
                free(g_streams[i]->buffer);
            }
            free(g_streams[i]);
            g_streams[i] = NULL;
        }
    }

    g_stream_count = 0;
    g_initialized = 0;

    return 0;
}

int stream_engine_create(const char *name, const storage_schema_t *schema)
{
    (void)schema; /* 暂不使用 schema */

    if (!g_initialized || name == NULL) {
        return -1;
    }

    /* 检查是否已存在 */
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (g_streams[i] != NULL && strcmp(g_streams[i]->name, name) == 0) {
            return -1; /* 已存在 */
        }
    }

    /* 查找空闲槽位 */
    int slot = -1;
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (g_streams[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return -1; /* 槽位已满 */
    }

    /* 创建流 */
    stream_engine_db_t *db = (stream_engine_db_t *)calloc(1, sizeof(stream_engine_db_t));
    if (db == NULL) {
        return -1;
    }

    strncpy(db->name, name, sizeof(db->name) - 1);
    if (g_data_dir[0]) {
        strncpy(db->data_dir, g_data_dir, sizeof(db->data_dir) - 1);
    }
    db->mode = ACCESS_MODE_READ_WRITE;
    db->buffer_size = STREAM_DEFAULT_BUFFER_SIZE;
    db->write_pos = 0;
    db->read_pos = 0;
    db->count = 0;
    db->min_watermark = 0;

    /* 分配环形缓冲区 */
    db->buffer = (stream_record_t *)calloc(db->buffer_size, sizeof(stream_record_t));
    if (db->buffer == NULL) {
        free(db);
        return -1;
    }

    g_streams[slot] = db;
    g_stream_count++;

    return 0;
}

void *stream_engine_open(const char *name, AccessMode mode)
{
    (void)mode; /* 暂时忽略模式 */

    if (!g_initialized || name == NULL) {
        return NULL;
    }

    /* 查找流 */
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (g_streams[i] != NULL && strcmp(g_streams[i]->name, name) == 0) {
            return g_streams[i];
        }
    }

    return NULL;
}

int stream_engine_close(void *rel)
{
    (void)rel; /* 内存引擎，无需关闭操作 */
    return 0;
}

int stream_engine_drop(const char *name)
{
    if (!g_initialized || name == NULL) {
        return -1;
    }

    /* 查找并删除流 */
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (g_streams[i] != NULL && strcmp(g_streams[i]->name, name) == 0) {
            if (g_streams[i]->buffer != NULL) {
                free(g_streams[i]->buffer);
            }
            free(g_streams[i]);
            g_streams[i] = NULL;
            g_stream_count--;
            return 0;
        }
    }

    return -1; /* 未找到 */
}

/* ========================================================================
 * 数据操作
 * ======================================================================== */

int stream_engine_insert(void *rel, const void *data, size_t len)
{
    stream_engine_db_t *db = (stream_engine_db_t *)rel;

    if (db == NULL || data == NULL || len < sizeof(stream_record_t)) {
        return -1;
    }

    /* 检查缓冲区是否已满 */
    if (db->count >= db->buffer_size) {
        /* 覆盖最旧的记录 */
        db->read_pos = (db->read_pos + 1) % db->buffer_size;
        db->count--;
    }

    /* 写入记录 */
    stream_record_t *record = (stream_record_t *)data;
    db->buffer[db->write_pos] = *record;
    db->write_pos = (db->write_pos + 1) % db->buffer_size;
    db->count++;

    /* 更新水位线 */
    if (db->min_watermark == 0 || record->timestamp < db->min_watermark) {
        db->min_watermark = record->timestamp;
    }

    return 0;
}

int stream_engine_query(void *rel, int64_t watermark,
                        int64_t *out_timestamps, double *out_values,
                        char (*out_tags)[64], uint32_t *out_count,
                        uint32_t max_results)
{
    stream_engine_db_t *db = (stream_engine_db_t *)rel;

    if (db == NULL || out_count == NULL) {
        return -1;
    }

    uint32_t count = 0;

    /* 遍历缓冲区 */
    if (db->count > 0) {
        uint64_t start = db->read_pos;
        uint64_t end = db->write_pos;

        /* 处理环形缓冲区 */
        if (end <= start) {
            end += db->buffer_size;
        }

        for (uint64_t i = start; i < end && count < max_results; i++) {
            uint64_t idx = i % db->buffer_size;
            stream_record_t *record = &db->buffer[idx];

            /* 按水位线过滤 */
            if (record->timestamp > watermark) {
                if (out_timestamps != NULL) {
                    out_timestamps[count] = record->timestamp;
                }
                if (out_values != NULL) {
                    out_values[count] = record->value;
                }
                if (out_tags != NULL) {
                    strncpy(out_tags[count], record->tag, 63);
                    out_tags[count][63] = '\0';
                }
                count++;
            }
        }
    }

    *out_count = count;
    return 0;
}

int stream_engine_query_records(void *rel, int64_t watermark,
                                stream_record_t *out_records,
                                uint32_t *out_count,
                                uint32_t max_results)
{
    stream_engine_db_t *db = (stream_engine_db_t *)rel;

    if (db == NULL || out_count == NULL) {
        return -1;
    }

    uint32_t count = 0;

    /* 遍历缓冲区 */
    if (db->count > 0) {
        uint64_t start = db->read_pos;
        uint64_t end = db->write_pos;

        /* 处理环形缓冲区 */
        if (end <= start) {
            end += db->buffer_size;
        }

        for (uint64_t i = start; i < end && count < max_results; i++) {
            uint64_t idx = i % db->buffer_size;
            stream_record_t *record = &db->buffer[idx];

            /* 按水位线过滤 */
            if (record->timestamp > watermark) {
                if (out_records != NULL) {
                    out_records[count] = *record;
                }
                count++;
            }
        }
    }

    *out_count = count;
    return 0;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

int stream_engine_stats(const char *name, storage_stats_t *stats)
{
    if (!g_initialized || name == NULL || stats == NULL) {
        return -1;
    }

    /* 查找流 */
    stream_engine_db_t *db = NULL;
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (g_streams[i] != NULL && strcmp(g_streams[i]->name, name) == 0) {
            db = g_streams[i];
            break;
        }
    }

    if (db == NULL) {
        return -1;
    }

    memset(stats, 0, sizeof(storage_stats_t));
    stats->total_size = db->buffer_size * sizeof(stream_record_t);
    stats->used_size = db->count * sizeof(stream_record_t);
    stats->num_objects = db->count;
    stats->num_pages = 1; /* 内存引擎，单页 */
    stats->cache_hit_rate = 1.0; /* 内存引擎，100% 命中 */

    return 0;
}
