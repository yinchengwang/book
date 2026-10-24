/**
 * @file stream_engine.c
 * @brief 流式存储引擎实现
 *
 * 提供简单的内存队列式流式存储，支持：
 * - 分区管理
 * - 生产者写入
 * - 消费者订阅
 * - 偏移量追踪
 */
#include "stream_engine.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

/* 前向声明 */
static int stream_produce_to_partition(void *stream, const void *data, size_t len, int partition);

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ========================================================================
 * 静态变量
 * ======================================================================== */

static char g_data_dir[512] = {0};
static bool g_initialized = false;
static int32_t g_next_partition = 0;  /* 轮询分区分配 */

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int64_t get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    int64_t ts = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ts / 10000;  // 转换为毫秒
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static int init_mutex(void *mutex) {
#ifdef _WIN32
    InitializeCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_init((pthread_mutex_t *)mutex, NULL);
#endif
    return 0;
}

static int lock_mutex(void *mutex) {
#ifdef _WIN32
    EnterCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_lock((pthread_mutex_t *)mutex);
#endif
    return 0;
}

static int unlock_mutex(void *mutex) {
#ifdef _WIN32
    LeaveCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
#endif
    return 0;
}

static int destroy_mutex(void *mutex) {
#ifdef _WIN32
    DeleteCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
#endif
    return 0;
}

static stream_partition_t *create_partition(int32_t id) {
    stream_partition_t *part = (stream_partition_t *)calloc(1, sizeof(stream_partition_t));
    if (!part) return NULL;
    part->id = id;
    part->first_offset = 0;
    part->last_offset = -1;
    part->record_count = 0;
    init_mutex(&part->mutex);
    return part;
}

static void destroy_partition(stream_partition_t *part) {
    if (!part) return;
    lock_mutex(&part->mutex);
    // 释放所有消息
    stream_record_t *record = part->head;
    while (record) {
        stream_record_t *next = record->next;
        free(record->data);
        free(record);
        record = next;
    }
    unlock_mutex(&part->mutex);
    destroy_mutex(&part->mutex);
    free(part);
}

static int64_t get_next_offset(stream_partition_t *part) {
    return part->last_offset + 1;
}

/* ========================================================================
 * 偏移索引管理
 * ======================================================================== */

/**
 * 为分区构建偏移索引，加速 stream_consume 的定位
 */
static int build_offset_index(stream_partition_t *part) {
    if (!part || !part->head) return 0;

    /* 计算索引范围 */
    int64_t min_off = part->head->offset;
    int64_t max_off = part->last_offset;
    int64_t index_count = max_off - min_off + 1;

    /* 限制索引大小：最多索引 10000 条记录 */
    if (index_count > 10000) {
        index_count = 10000;
    }

    stream_record_t **index = (stream_record_t **)calloc(index_count, sizeof(stream_record_t *));
    if (!index) return -1;

    stream_record_t *r = part->head;
    int64_t base = min_off;
    while (r) {
        int64_t off = r->offset - base;
        if (off >= 0 && off < index_count) {
            index[off] = r;
        }
        r = r->next;
    }

    part->index_base = base;
    part->index_size = index_count;
    part->offset_index = index;
    return 0;
}

/**
 * 根据偏移快速查找记录（使用索引或二分）
 */
static stream_record_t *find_record_by_offset(stream_partition_t *part, int64_t target_offset) {
    if (!part || target_offset < 0) return NULL;

    /* 无索引时退化为线性扫描 */
    if (!part->offset_index) {
        stream_record_t *r = part->head;
        while (r) {
            if (r->offset >= target_offset) {
                if (r->offset == target_offset) return r;
                if (r->offset > target_offset && !r->prev) return r;
            }
            r = r->next;
        }
        return NULL;
    }

    /* 使用索引查找 */
    if (target_offset < part->index_base) return NULL;
    int64_t idx = target_offset - part->index_base;
    if (idx >= part->index_size) {
        /* 超出索引范围，找最后一条 */
        return part->tail;
    }
    return part->offset_index[idx];
}

/* ========================================================================
 * 消费者偏移持久化
 * ======================================================================== */

/* 消费者偏移存储路径 */
static int get_consumer_offset_path(const char *stream_name, int32_t partition, char *buf, size_t bufsize) {
    snprintf(buf, bufsize, "%s/%s.consumer.p%d.offset", g_data_dir, stream_name, partition);
    return 0;
}

static int64_t load_consumer_offset(const char *stream_name, int32_t partition) {
    char path[512];
    get_consumer_offset_path(stream_name, partition, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;  /* 无文件则返回 -1，表示从头开始 */
    int64_t offset = -1;
    fread(&offset, sizeof(offset), 1, f);
    fclose(f);
    return offset;
}

static int save_consumer_offset(const char *stream_name, int32_t partition, int64_t offset) {
    char path[512];
    get_consumer_offset_path(stream_name, partition, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(&offset, sizeof(offset), 1, f);
    fclose(f);
    return 0;
}

/* ========================================================================
 * 生命周期
 * ======================================================================== */

int stream_engine_init(const char *data_dir) {
    if (g_initialized) {
        return 0;
    }
    if (data_dir) {
        strncpy(g_data_dir, data_dir, sizeof(g_data_dir) - 1);
    } else {
        strcpy(g_data_dir, "./data/stream");
    }
    g_initialized = true;
    LOG_INFO("Stream engine initialized, data_dir: %s", g_data_dir);
    return 0;
}

int stream_engine_shutdown(void) {
    g_initialized = false;
    memset(g_data_dir, 0, sizeof(g_data_dir));
    LOG_INFO("Stream engine shutdown");
    return 0;
}

/* ========================================================================
 * 流操作
 * ======================================================================== */

int stream_create(const char *name, const stream_config_t *config) {
    if (!g_initialized || !name || !config) {
        return -1;
    }

    stream_handle_t *handle = (stream_handle_t *)calloc(1, sizeof(stream_handle_t));
    if (!handle) return -1;

    strncpy(handle->name, name, sizeof(handle->name) - 1);
    memcpy(&handle->config, config, sizeof(stream_config_t));
    handle->num_partitions = config->partition_count > 0 ? config->partition_count : 1;
    handle->partitions = (stream_partition_t *)calloc(handle->num_partitions, sizeof(stream_partition_t));

    if (!handle->partitions) {
        free(handle);
        return -1;
    }

    for (int i = 0; i < handle->num_partitions; i++) {
        stream_partition_t *part = create_partition(i);
        if (!part) {
            for (int j = 0; j < i; j++) {
                destroy_partition(&handle->partitions[j]);
            }
            free(handle->partitions);
            free(handle);
            return -1;
        }
        handle->partitions[i] = *part;
        free(part);
    }

    // 将 handle 存入全局表（简化实现，使用文件名存储）
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.handle", g_data_dir, name);

    /* 持久化元数据（与 stream_open 读取格式一致） */
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(handle->name, sizeof(handle->name), 1, f);
        fwrite(&handle->config, sizeof(handle->config), 1, f);
        fwrite(&handle->num_partitions, sizeof(handle->num_partitions), 1, f);
        fclose(f);
    }

    free(handle);
    LOG_INFO("Stream created: %s, partitions: %d", name, handle->num_partitions);
    return 0;
}

void *stream_open(const char *name) {
    if (!g_initialized || !name) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.handle", g_data_dir, name);

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* 只读取元数据（避免读取悬空指针） */
    char saved_name[256] = {0};
    stream_config_t saved_config = {0};
    int32_t saved_num_partitions = 0;

    fread(saved_name, sizeof(saved_name), 1, f);
    fread(&saved_config, sizeof(saved_config), 1, f);
    fread(&saved_num_partitions, sizeof(saved_num_partitions), 1, f);
    fclose(f);

    /* 重建 handle */
    stream_handle_t *handle = (stream_handle_t *)calloc(1, sizeof(stream_handle_t));
    if (!handle) return NULL;

    strncpy(handle->name, name, sizeof(handle->name) - 1);
    memcpy(&handle->config, &saved_config, sizeof(stream_config_t));
    handle->num_partitions = saved_num_partitions > 0 ? saved_num_partitions : 1;

    /* 重建分区数组 */
    handle->partitions = (stream_partition_t *)calloc(handle->num_partitions, sizeof(stream_partition_t));
    if (!handle->partitions) {
        free(handle);
        return NULL;
    }

    for (int i = 0; i < handle->num_partitions; i++) {
        stream_partition_t *part = create_partition(i);
        if (!part) {
            for (int j = 0; j < i; j++) {
                destroy_partition(&handle->partitions[j]);
            }
            free(handle->partitions);
            free(handle);
            return NULL;
        }
        handle->partitions[i] = *part;
        free(part);

        /* 从持久化文件加载记录 */
        char record_path[512];
        snprintf(record_path, sizeof(record_path), "%s/%s.records.p%d", g_data_dir, name, i);
        FILE *rf = fopen(record_path, "rb");
        if (rf) {
            stream_record_t *prev = NULL;
            while (1) {
                int64_t off = 0, ts = 0;
                int32_t datalen = 0;
                if (fread(&off, sizeof(off), 1, rf) != 1) break;
                if (fread(&ts, sizeof(ts), 1, rf) != 1) break;
                if (fread(&datalen, sizeof(datalen), 1, rf) != 1) break;

                stream_record_t *rec = (stream_record_t *)malloc(sizeof(stream_record_t));
                if (!rec) break;
                rec->offset = off;
                rec->timestamp = ts;
                rec->partition = i;
                rec->len = datalen;
                rec->data = malloc(datalen);
                if (!rec->data) {
                    free(rec);
                    break;
                }
                if (fread(rec->data, datalen, 1, rf) != 1) {
                    free(rec->data);
                    free(rec);
                    break;
                }

                rec->next = NULL;
                if (prev) {
                    prev->next = rec;
                    rec->prev = prev;
                    handle->partitions[i].tail = rec;
                } else {
                    handle->partitions[i].head = handle->partitions[i].tail = rec;
                    rec->prev = NULL;
                }
                handle->partitions[i].last_offset = off;
                handle->partitions[i].record_count++;
                handle->total_records++;
                prev = rec;
            }
            fclose(rf);
            build_offset_index(&handle->partitions[i]);
        }
    }

    return handle;
}

int stream_close(void *stream) {
    if (stream) {
        stream_handle_t *handle = (stream_handle_t *)stream;
        if (handle->partitions) {
            for (int i = 0; i < handle->num_partitions; i++) {
                // 简化：仅销毁 mutex
            }
            free(handle->partitions);
        }
        free(handle);
    }
    return 0;
}

int stream_drop(const char *name) {
    if (!name) return -1;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.handle", g_data_dir, name);
    remove(path);
    LOG_INFO("Stream dropped: %s", name);
    return 0;
}

bool stream_exists(const char *name) {
    if (!name) return false;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.handle", g_data_dir, name);
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

/* ========================================================================
 * 生产者操作
 * ======================================================================== */

int stream_produce(void *stream, const void *data, size_t len) {
    return stream_produce_to_partition(stream, data, len, 0);
}

static int stream_produce_to_partition(void *stream, const void *data, size_t len, int partition) {
    if (!stream || !data || len == 0) return -1;

    stream_handle_t *handle = (stream_handle_t *)stream;
    if (partition < 0 || partition >= handle->num_partitions) {
        partition = 0;
    }

    stream_partition_t *part = &handle->partitions[partition];
    lock_mutex(&part->mutex);

    // 创建新记录
    stream_record_t *record = (stream_record_t *)malloc(sizeof(stream_record_t));
    if (!record) {
        unlock_mutex(&part->mutex);
        return -1;
    }

    record->data = malloc(len);
    if (!record->data) {
        free(record);
        unlock_mutex(&part->mutex);
        return -1;
    }

    memcpy(record->data, data, len);
    record->len = len;
    record->offset = get_next_offset(part);
    record->partition = partition;
    record->timestamp = get_timestamp_ms();
    record->next = NULL;

    // 添加到链表
    if (part->tail) {
        part->tail->next = record;
        record->prev = part->tail;
        part->tail = record;
    } else {
        part->head = part->tail = record;
        record->prev = NULL;
    }

    part->last_offset = record->offset;
    part->record_count++;
    handle->total_records++;

    /* 持久化记录到分区文件 */
    char record_path[512];
    snprintf(record_path, sizeof(record_path), "%s/%s.records.p%d",
             g_data_dir, handle->name, partition);
    FILE *rf = fopen(record_path, "ab");
    if (rf) {
        fwrite(&record->offset, sizeof(record->offset), 1, rf);
        fwrite(&record->timestamp, sizeof(record->timestamp), 1, rf);
        int32_t datalen = (int32_t)record->len;
        fwrite(&datalen, sizeof(datalen), 1, rf);
        fwrite(record->data, record->len, 1, rf);
        fclose(rf);
    }

    /* 更新偏移索引 */
    if (part->offset_index) {
        int64_t idx = record->offset - part->index_base;
        if (idx >= 0 && idx < part->index_size) {
            part->offset_index[idx] = record;
        }
    }

    unlock_mutex(&part->mutex);
    return 0;
}

int64_t stream_get_offset(void *stream) {
    if (!stream) return -1;
    stream_handle_t *handle = (stream_handle_t *)stream;
    return handle->total_records;
}

int64_t stream_get_lag(void *stream, int partition) {
    if (!stream) return -1;
    stream_handle_t *handle = (stream_handle_t *)stream;
    if (partition < 0 || partition >= handle->num_partitions) {
        partition = 0;
    }
    stream_partition_t *part = &handle->partitions[partition];
    return part->last_offset - part->record_count + 1;
}

/* ========================================================================
 * 消费者操作
 * ======================================================================== */

stream_consumer_t *stream_subscribe(void *stream, int64_t start_offset) {
    if (!stream) return NULL;

    stream_handle_t *handle = (stream_handle_t *)stream;

    /* 轮询分配分区 */
    int assigned_partition = g_next_partition % handle->num_partitions;
    g_next_partition++;

    stream_consumer_impl_t *impl = (stream_consumer_impl_t *)calloc(1, sizeof(stream_consumer_impl_t));
    if (!impl) return NULL;

    impl->partition = &handle->partitions[assigned_partition];
    strncpy(impl->stream_name, handle->name, sizeof(impl->stream_name) - 1);

    /* 尝试从持久化存储恢复偏移量 */
    if (start_offset < 0) {
        int64_t saved = load_consumer_offset(handle->name, assigned_partition);
        impl->current_offset = (saved >= 0) ? saved : 0;
    } else {
        impl->current_offset = start_offset;
    }

    impl->state = CONSUMER_STATE_RUNNING;
#ifdef _WIN32
    InitializeCriticalSection(&impl->mutex);
#else
    pthread_mutex_init(&impl->mutex, NULL);
#endif

    stream_consumer_t *consumer = (stream_consumer_t *)calloc(1, sizeof(stream_consumer_t));
    if (!consumer) {
        free(impl);
        return NULL;
    }
    consumer->current_offset = impl->current_offset;
    consumer->state = impl->state;
    consumer->internal = impl;
    consumer->partition = impl->partition;

    return consumer;
}

int stream_consume(stream_consumer_t *consumer, void *out_data, size_t *out_len, size_t max_len) {
    if (!consumer || !out_data || !out_len) return -1;

    stream_consumer_impl_t *impl = (stream_consumer_impl_t *)consumer->internal;
#ifdef _WIN32
    EnterCriticalSection(&impl->mutex);
#else
    pthread_mutex_lock(&impl->mutex);
#endif

    stream_partition_t *part = impl->partition;

    /* 使用偏移索引快速定位记录 */
    stream_record_t *record = find_record_by_offset(part, impl->current_offset);

    /* 索引未命中或无索引时，退化为线性扫描找下一条有效记录 */
    if (!record || record->offset < impl->current_offset) {
        record = part->head;
        while (record) {
            if (record->offset >= impl->current_offset) break;
            record = record->next;
        }
    }

    if (!record) {
#ifdef _WIN32
        LeaveCriticalSection(&impl->mutex);
#else
        pthread_mutex_unlock(&impl->mutex);
#endif
        *out_len = 0;
        return -1;  /* 无新消息 */
    }

    size_t copy_len = record->len < max_len ? record->len : max_len;
    memcpy(out_data, record->data, copy_len);
    *out_len = copy_len;
    impl->current_offset = record->offset + 1;
    consumer->current_offset = impl->current_offset;

    /* 持久化消费者偏移 */
    save_consumer_offset(impl->stream_name, part->id, impl->current_offset);

#ifdef _WIN32
    LeaveCriticalSection(&impl->mutex);
#else
    pthread_mutex_unlock(&impl->mutex);
#endif
    return 0;
}

int stream_commit_offset(stream_consumer_t *consumer, int64_t offset) {
    if (!consumer) return -1;
    stream_consumer_impl_t *impl = (stream_consumer_impl_t *)consumer->internal;
#ifdef _WIN32
    EnterCriticalSection(&impl->mutex);
#else
    pthread_mutex_lock(&impl->mutex);
#endif
    impl->current_offset = offset;
    consumer->current_offset = offset;

    /* 持久化偏移量 */
    save_consumer_offset(impl->stream_name, impl->partition->id, offset);

#ifdef _WIN32
    LeaveCriticalSection(&impl->mutex);
#else
    pthread_mutex_unlock(&impl->mutex);
#endif
    return 0;
}

int stream_consumer_close(stream_consumer_t *consumer) {
    if (!consumer) return 0;
    stream_consumer_impl_t *impl = (stream_consumer_impl_t *)consumer->internal;
    if (impl) {
#ifdef _WIN32
        DeleteCriticalSection(&impl->mutex);
#else
        pthread_mutex_destroy(&impl->mutex);
#endif
        free(impl);
    }
    free(consumer);
    return 0;
}

/* ========================================================================
 * 窗口操作（预留）
 * ======================================================================== */

int stream_window_create(const char *stream_name, const char *window_def) {
    LOG_INFO("Window creation not implemented yet: %s, def: %s", stream_name, window_def);
    return 0;
}

int stream_window_agg(const char *window_name, const char *agg_func, const char *column, void *out, size_t *out_len) {
    LOG_INFO("Window aggregation not implemented yet");
    return -1;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

int stream_get_stream_stats(const char *name, storage_stats_t *stats) {
    if (!stats) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    stats->num_objects = 1;  // 简化
    return 0;
}

int stream_get_partition_count(const char *name, int *count) {
    if (!count) return -1;
    *count = 1;  // 简化
    return 0;
}

/* ========================================================================
 * storage_ops_t 适配层
 * ======================================================================== */

static int stream_table_create(const char *name, const storage_schema_t *schema) {
    stream_config_t config = {0};
    config.name = name;
    config.partition_count = 1;
    config.retention_ms = 86400000;  // 1天
    return stream_create(name, &config);
}

static void *stream_table_open(const char *name, AccessMode mode) {
    (void)mode;
    return stream_open(name);
}

static int stream_table_close(void *rel) {
    return stream_close(rel);
}

static int stream_table_drop(const char *name) {
    return stream_drop(name);
}

static int stream_tuple_insert(void *rel, const void *data, size_t len) {
    return stream_produce(rel, data, len);
}

static int stream_get_stats(const char *name, storage_stats_t *stats) {
    return stream_get_stream_stats(name, stats);
}

/* ========================================================================
 * 引擎 ops 表
 * ======================================================================== */

static storage_ops_t g_stream_storage_ops = {
    .name = "stream_engine",
    .model = MODEL_STREAM,
    .init = stream_engine_init,
    .shutdown = stream_engine_shutdown,
    .table_create = stream_table_create,
    .table_open = stream_table_open,
    .table_close = stream_table_close,
    .table_drop = stream_table_drop,
    .tuple_insert = stream_tuple_insert,
    .get_stats = stream_get_stats,
};

const storage_ops_t *stream_engine_get_ops(void) {
    return &g_stream_storage_ops;
}
