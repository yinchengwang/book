/**
 * @file ts_engine.c
 * @brief 时序存储引擎实现
 */
#include "db/storage/ts/ts_engine.h"
#include "db/storage/ts/ts_compress.h"
#include "db/storage/ts/ts_retention.h"
#include "db/storage/ts/ts_segment.h"
#include "db/core/log.h"
#include "db/mm_pool.h"
#include "db/lock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

#define TS_ENGINE_NAME "ts_engine"
#define TS_DATA_PREFIX "ts_"

typedef struct ts_engine_global_s {
    char data_dir[512];
    bool initialized;
} ts_engine_global_t;

static ts_engine_global_t g_ts_engine = {
    .data_dir = {0},
    .initialized = false
};

typedef struct ts_header_s {
    char name[256];
    int64_t start_time;
    int64_t end_time;
    uint64_t num_points;
} ts_header_t;

/* ts_record_t 已从 ts_compress.h 中引入 */

/* 粒度到毫秒的转换 */
static int64_t ts_granularity_to_ms_impl(ts_granularity_t granularity) {
    switch (granularity) {
        case TS_GRANULARITY_SECOND:  return 1000LL;
        case TS_GRANULARITY_MINUTE:   return 60 * 1000LL;
        case TS_GRANULARITY_HOUR:     return 60 * 60 * 1000LL;
        case TS_GRANULARITY_DAY:      return 24 * 60 * 60 * 1000LL;
        default:                   return 1000LL;
    }
}

/* 时间戳对齐到指定粒度（毫秒） */
static int64_t ts_align_timestamp_impl(int64_t timestamp, ts_granularity_t granularity) {
    int64_t ms = ts_granularity_to_ms_impl(granularity);
    return timestamp - (timestamp % ms);
}

static int get_meta_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s.meta",
             g_ts_engine.data_dir, TS_DATA_PREFIX, name);
    return 0;
}

static int get_data_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s.tsd",
             g_ts_engine.data_dir, TS_DATA_PREFIX, name);
    return 0;
}

static int ts_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (name == NULL) return -1;

    /* 创建数据目录（兼容 Windows/macOS/Linux） */
#ifdef _WIN32
    if (mkdir(g_ts_engine.data_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(g_ts_engine.data_dir, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_ERROR("创建时序数据目录失败: %s", g_ts_engine.data_dir);
        return -1;
    }

    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "rb");
    if (fp) {
        fclose(fp);
        return 0;
    }

    ts_header_t header = { .name = {0}, .start_time = 0, .end_time = 0, .num_points = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }
    return 0;
}

static void *ts_engine_table_open(const char *name, AccessMode mode) {
    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return NULL;

    ts_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    ts_engine_db_t *db = (ts_engine_db_t *)calloc(1, sizeof(ts_engine_db_t));
    if (db == NULL) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    db->mode = mode;
    db->start_time = header.start_time;
    db->end_time = header.end_time;
    db->num_points = header.num_points;

    /* 初始化内存池（使用全局时序池） */
    db->mem_pool = g_ts_pool;
    db->use_mem_pool = (g_ts_pool != NULL);

    /* 初始化锁（默认禁用） */
    db->lockmgr = NULL;
    db->rwlock = NULL;
    db->use_lock = false;

    /* 初始化分段索引 */
    db->use_segment_index = true;
    char segment_dir[512];
    snprintf(segment_dir, sizeof(segment_dir), "%s/%s%s/segments",
             g_ts_engine.data_dir, TS_DATA_PREFIX, name);
    db->segment_index = ts_segment_index_open(segment_dir);
    if (db->segment_index == NULL) {
        db->segment_index = ts_segment_index_create(segment_dir,
            TS_SEGMENT_DEFAULT_SIZE, TS_GRANULARITY_HOUR);
        if (db->segment_index == NULL) {
            LOG_WARN("分段索引创建失败");
            db->use_segment_index = false;
        }
    }

    /* 初始化保留策略字段 */
    ts_retention_policy_t policy;
    if (ts_retention_load_policy(g_ts_engine.data_dir, name, &policy) == 0) {
        db->raw_retention_days = policy.raw_retention_days;
        db->compressed_retention_days = policy.compressed_retention_days;
        db->compaction_interval_hours = policy.compaction_interval_hours;
    } else {
        ts_retention_policy_t default_policy = ts_retention_default_policy();
        db->raw_retention_days = default_policy.raw_retention_days;
        db->compressed_retention_days = default_policy.compressed_retention_days;
        db->compaction_interval_hours = default_policy.compaction_interval_hours;
    }
    db->last_compaction_time = 0;
    db->expired_points = 0;
    db->deleted_points = 0;

    return db;
}

static int ts_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    /* 保存分段索引 */
    if (db->use_segment_index && db->segment_index != NULL) {
        ts_segment_index_save(db->segment_index);
        ts_segment_index_destroy(db->segment_index);
        db->segment_index = NULL;
    }

    char meta_path[512];
    get_meta_path(db->name, meta_path, sizeof(meta_path));

    ts_header_t header = {
        .start_time = db->start_time,
        .end_time = db->end_time,
        .num_points = db->num_points
    };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 保存保留策略 */
    ts_retention_policy_t policy = {
        .raw_retention_days = db->raw_retention_days,
        .compressed_retention_days = db->compressed_retention_days,
        .compaction_interval_hours = db->compaction_interval_hours
    };
    ts_retention_policy_save(g_ts_engine.data_dir, db->name, &policy);

    /* 释放读写锁 */
    if (db->rwlock != NULL) {
        free(db->rwlock);
        db->rwlock = NULL;
    }

    free(db);
    return 0;
}

static int ts_engine_table_drop(const char *name) {
    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));
    return remove(meta_path) == 0 ? 0 : -1;
}

static int ts_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;
    if (len < sizeof(int64_t) + sizeof(double)) return -1;

    int64_t timestamp;
    double value;
    memcpy(&timestamp, data, sizeof(int64_t));
    memcpy(&value, (const uint8_t *)data + sizeof(int64_t), sizeof(double));

    /* 优先使用分段索引存储 */
    if (db->use_segment_index && db->segment_index != NULL) {
        if (ts_segment_append(db->segment_index, timestamp, value) == 0) {
            if (db->num_points == 0 || timestamp < db->start_time) db->start_time = timestamp;
            if (db->num_points == 0 || timestamp > db->end_time) db->end_time = timestamp;
            db->num_points++;
            return 0;
        }
        LOG_WARN("分段索引插入失败，回退到文件存储");
    }

    /* 回退到文件存储 */
    char data_path[512];
    get_data_path(db->name, data_path, sizeof(data_path));

    FILE *fp = fopen(data_path, "ab");
    if (fp == NULL) {
        fp = fopen(data_path, "wb");
        if (fp == NULL) return -1;
    }

    ts_record_t record = { .timestamp = timestamp, .value = value };
    fwrite(&record, sizeof(record), 1, fp);
    fclose(fp);

    if (db->num_points == 0 || timestamp < db->start_time) db->start_time = timestamp;
    if (db->num_points == 0 || timestamp > db->end_time) db->end_time = timestamp;
    db->num_points++;

    return 0;
}

static scan_desc_t *ts_engine_scan_begin(void *rel,
                                         const scan_key_t *keys, int nkeys,
                                         ScanDirection direction) {
    (void)keys;
    (void)nkeys;
    (void)direction;

    if (rel == NULL) return NULL;
    return NULL;
}

static int ts_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

static int ts_engine_scan_end(scan_desc_t *scan) {
    if (scan) free(scan);
    return 0;
}

static int ts_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    if (name == NULL) return 0;

    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return -1;

    ts_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    stats->num_objects = header.num_points;
    return 0;
}

static const storage_ops_t g_ts_engine_ops = {
    .name = TS_ENGINE_NAME,
    .model = MODEL_TIMESERIES,
    .init = ts_engine_init,
    .shutdown = ts_engine_shutdown,
    .table_create = ts_engine_table_create,
    .table_open = ts_engine_table_open,
    .table_close = ts_engine_table_close,
    .table_drop = ts_engine_table_drop,
    .tuple_insert = ts_engine_tuple_insert,
    .tuple_update = NULL,
    .tuple_delete = NULL,
    .scan_begin = ts_engine_scan_begin,
    .scan_next = ts_engine_scan_next,
    .scan_end = ts_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = ts_engine_get_stats,
};

int ts_engine_init(const char *data_dir) {
    if (g_ts_engine.initialized) return 0;
    if (data_dir == NULL) return -1;
    strncpy(g_ts_engine.data_dir, data_dir, sizeof(g_ts_engine.data_dir) - 1);
    g_ts_engine.initialized = true;

    /* Task 2.14: 注册到 storage_ops_t 全局表 */
    storage_register_engine(MODEL_TIMESERIES, &g_ts_engine_ops);
    return 0;
}

int ts_engine_shutdown(void) {
    g_ts_engine.initialized = false;
    return 0;
}

const storage_ops_t *ts_engine_get_ops(void) {
    return &g_ts_engine_ops;
}

int ts_engine_create(const char *name, const storage_schema_t *schema) {
    return ts_engine_table_create(name, schema);
}

void *ts_engine_open(const char *name, AccessMode mode) {
    return ts_engine_table_open(name, mode);
}

int ts_engine_close(void *rel) {
    return ts_engine_table_close(rel);
}

int ts_engine_drop(const char *name) {
    return ts_engine_table_drop(name);
}

int ts_engine_insert(void *rel, const void *data, size_t len) {
    return ts_engine_tuple_insert(rel, data, len);
}

int ts_engine_query(void *rel,
                    int64_t start_time, int64_t end_time,
                    ts_granularity_t granularity,
                    ts_aggregate_func_t func,
                    ts_query_results_t *results) {
    if (rel == NULL || results == NULL) {
        return -1;
    }

    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    /* 初始化结果结构 */
    results->results = calloc(1024, sizeof(ts_aggregate_result_t));
    if (results->results == NULL) {
        return -1;
    }
    results->count = 0;
    results->capacity = 1024;

    char data_path[512];
    get_data_path(db->name, data_path, sizeof(data_path));

    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) {
        return 0;  /* 文件不存在时返回空结果 */
    }

    int64_t bucket_ms = ts_granularity_to_ms(granularity);

    /* 读取所有数据点 */
    ts_record_t record;
    while (fread(&record, sizeof(record), 1, fp) == 1) {
        /* 时间范围过滤 */
        if (start_time > 0 && record.timestamp < start_time) continue;
        if (end_time > 0 && record.timestamp > end_time) continue;

        /* 计算时间桶 */
        int64_t bucket_time = ts_align_timestamp(record.timestamp, granularity);

        /* 在结果数组中查找或创建对应的桶 */
        bool found = false;
        for (uint32_t i = 0; i < results->count; i++) {
            if (results->results[i].timestamp == bucket_time) {
                found = true;
                results->results[i].count++;
                switch (func) {
                    case AGG_SUM:
                        results->results[i].sum += record.value;
                        break;
                    case AGG_AVG:
                        results->results[i].sum += record.value;
                        break;
                    case AGG_MIN:
                        if (results->results[i].count == 1) {
                            results->results[i].min = record.value;
                        } else if (record.value < results->results[i].min) {
                            results->results[i].min = record.value;
                        }
                        break;
                    case AGG_MAX:
                        if (results->results[i].count == 1) {
                            results->results[i].max = record.value;
                        } else if (record.value > results->results[i].max) {
                            results->results[i].max = record.value;
                        }
                        break;
                    case AGG_COUNT:
                        /* count 已在上面递增 */
                        break;
                }
                break;
            }
        }

        /* 未找到，创建新桶 */
        if (!found) {
            if (results->count >= results->capacity) {
                uint32_t new_cap = results->capacity * 2;
                ts_aggregate_result_t *new_results = realloc(
                    results->results, new_cap * sizeof(ts_aggregate_result_t));
                if (new_results == NULL) {
                    fclose(fp);
                    return -1;
                }
                results->results = new_results;
                results->capacity = new_cap;
            }

            uint32_t idx = results->count++;
            results->results[idx].timestamp = bucket_time;
            results->results[idx].count = 1;
            results->results[idx].sum = record.value;
            results->results[idx].min = record.value;
            results->results[idx].max = record.value;
        }
    }

    fclose(fp);

    /* 计算 AVG */
    if (func == AGG_AVG) {
        for (uint32_t i = 0; i < results->count; i++) {
            if (results->results[i].count > 0) {
                results->results[i].avg = results->results[i].sum / results->results[i].count;
            }
        }
    }

    /* 按时间排序结果 */
    for (uint32_t i = 0; i < results->count - 1; i++) {
        for (uint32_t j = i + 1; j < results->count; j++) {
            if (results->results[i].timestamp > results->results[j].timestamp) {
                ts_aggregate_result_t tmp = results->results[i];
                results->results[i] = results->results[j];
                results->results[j] = tmp;
            }
        }
    }

    return 0;
}

void ts_engine_free_results(ts_query_results_t *results) {
    if (results && results->results) {
        free(results->results);
        results->results = NULL;
        results->count = 0;
    }
}

int ts_engine_stats(const char *name, storage_stats_t *stats) {
    return ts_engine_get_stats(name, stats);
}

int64_t ts_granularity_to_ms(ts_granularity_t granularity) {
    return ts_granularity_to_ms_impl(granularity);
}

int64_t ts_align_timestamp(int64_t timestamp, ts_granularity_t granularity) {
    return ts_align_timestamp_impl(timestamp, granularity);
}

/* ========================================================================
 * 保留策略 API
 * ======================================================================== */

int ts_engine_set_retention(void *rel, const ts_retention_policy_t *policy) {
    if (rel == NULL || policy == NULL) return -1;
    if (policy->raw_retention_days < -1 || policy->compressed_retention_days < -1) return -1;
    if (policy->compaction_interval_hours <= 0) return -1;

    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    db->raw_retention_days = policy->raw_retention_days;
    db->compressed_retention_days = policy->compressed_retention_days;
    db->compaction_interval_hours = policy->compaction_interval_hours;

    /* 持久化到文件 */
    ts_retention_policy_save(g_ts_engine.data_dir, db->name, policy);

    return 0;
}

int ts_engine_get_retention(void *rel, ts_retention_policy_t *policy) {
    if (rel == NULL || policy == NULL) return -1;

    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    policy->raw_retention_days = db->raw_retention_days;
    policy->compressed_retention_days = db->compressed_retention_days;
    policy->compaction_interval_hours = db->compaction_interval_hours;

    return 0;
}

int ts_engine_compact(void *rel) {
    if (rel == NULL) return -1;

    ts_engine_db_t *db = (ts_engine_db_t *)rel;
    int64_t now = (int64_t)time(NULL) * 1000;

    /* 检查是否到达压缩间隔 */
    if (db->last_compaction_time > 0) {
        int64_t interval_ms = ts_hours_to_ms(db->compaction_interval_hours);
        if (now - db->last_compaction_time < interval_ms) {
            return 0;  /* 未到达压缩时间 */
        }
    }

    /* 执行原始数据过期删除 */
    if (db->raw_retention_days > 0) {
        int64_t expire_time = now - ts_days_to_ms(db->raw_retention_days);
        ts_engine_expire(rel, expire_time);
    }

    /* 更新压缩时间 */
    db->last_compaction_time = now;

    LOG_INFO("时序引擎 [%s] 压缩清理完成", db->name);

    return 0;
}

int ts_engine_expire(void *rel, int64_t older_than_ms) {
    if (rel == NULL) return -1;

    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    char data_path[512];
    get_data_path(db->name, data_path, sizeof(data_path));

    /* 读取所有数据，过滤过期数据 */
    FILE *fp_in = fopen(data_path, "rb");
    if (fp_in == NULL) {
        return 0;  /* 文件不存在 */
    }

    /* 创建临时文件 */
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "%s.temp", data_path);
    FILE *fp_out = fopen(temp_path, "wb");
    if (fp_out == NULL) {
        fclose(fp_in);
        return -1;
    }

    ts_record_t record;
    int64_t deleted = 0;
    int64_t kept_start_time = 0;
    bool has_kept = false;

    while (fread(&record, sizeof(record), 1, fp_in) == 1) {
        if (record.timestamp >= older_than_ms) {
            fwrite(&record, sizeof(record), 1, fp_out);
            if (!has_kept || record.timestamp < kept_start_time) {
                kept_start_time = record.timestamp;
                has_kept = true;
            }
        } else {
            deleted++;
        }
    }

    fclose(fp_in);
    fclose(fp_out);

    /* 替换原文件 */
    if (remove(data_path) != 0) {
        remove(temp_path);
        return -1;
    }
    if (rename(temp_path, data_path) != 0) {
        return -1;
    }

    /* 更新统计信息 */
    db->num_points -= deleted;
    db->expired_points += deleted;
    db->deleted_points += deleted;

    if (has_kept) {
        db->start_time = kept_start_time;
    }

    LOG_INFO("时序引擎 [%s] 过期删除完成，删除 %ld 条记录", db->name, deleted);

    return (int)deleted;
}

int ts_engine_retention_stats(void *rel, uint64_t *total_points,
                               uint64_t *expired_points, uint64_t *deleted_points) {
    if (rel == NULL) return -1;

    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (total_points) *total_points = db->num_points;
    if (expired_points) *expired_points = db->expired_points;
    if (deleted_points) *deleted_points = db->deleted_points;

    return 0;
}

/* ========================================================================
 * 分段索引 API 实现
 * ======================================================================== */

int ts_engine_enable_segment_index(void *rel, uint32_t seg_size, int granularity) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (db->segment_index != NULL) {
        ts_segment_index_destroy(db->segment_index);
    }

    char segment_dir[512];
    snprintf(segment_dir, sizeof(segment_dir), "%s/%s%s/segments",
             g_ts_engine.data_dir, TS_DATA_PREFIX, db->name);

    ts_granularity_t gran = (ts_granularity_t)granularity;
    if (seg_size == 0) seg_size = TS_SEGMENT_DEFAULT_SIZE;

    db->segment_index = ts_segment_index_create(segment_dir, seg_size, gran);
    if (db->segment_index == NULL) {
        LOG_ERROR("创建分段索引失败");
        return -1;
    }

    db->use_segment_index = true;
    LOG_INFO("分段索引已启用: seg_size=%u, granularity=%d", seg_size, granularity);
    return 0;
}

int ts_engine_segment_stats(void *rel, uint32_t *seg_count, uint64_t *total_points) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (db->segment_index == NULL) {
        if (seg_count) *seg_count = 0;
        if (total_points) *total_points = 0;
        return -1;
    }

    if (seg_count) *seg_count = ts_segment_count(db->segment_index);
    if (total_points) {
        *total_points = ((ts_segment_index_t *)db->segment_index)->total_points;
    }

    return 0;
}

uint32_t ts_engine_segment_query(void *rel, int64_t start_time, int64_t end_time,
                                  void *results, uint32_t max_results) {
    if (rel == NULL || results == NULL || max_results == 0) return 0;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (!db->use_segment_index || db->segment_index == NULL) {
        return 0;
    }

    ts_segment_results_t *seg_results = (ts_segment_results_t *)results;
    return ts_segment_query(db->segment_index, start_time, end_time,
                           seg_results, max_results);
}

/* ========================================================================
 * 内存池管理 API 实现
 * ======================================================================== */

int ts_engine_enable_mem_pool(void *rel, bool use_pool) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (use_pool && g_ts_pool != NULL) {
        db->mem_pool = g_ts_pool;
        db->use_mem_pool = true;
        LOG_INFO("时序引擎内存池已启用");
    } else {
        db->mem_pool = NULL;
        db->use_mem_pool = false;
        LOG_INFO("时序引擎内存池已禁用");
    }
    return 0;
}

int ts_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats) {
    if (rel == NULL || stats == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (db->mem_pool != NULL) {
        return mm_pool_get_stats(db->mem_pool, stats);
    }

    memset(stats, 0, sizeof(mm_pool_stats_t));
    return 0;
}

/* ========================================================================
 * 并发锁控制 API 实现
 * ======================================================================== */

/* 全局锁管理器用于时序引擎 */
static lock_manager_t *g_ts_lockmgr = NULL;

/* 简单的读写锁实现（基于自旋锁） */
typedef struct {
    volatile int readers;
    volatile int writers_waiting;
    volatile int writer_active;
} ts_rwlock_t;

static void ts_rwlock_init(ts_rwlock_t *rwlock) {
    rwlock->readers = 0;
    rwlock->writers_waiting = 0;
    rwlock->writer_active = 0;
}

static void ts_rwlock_read_lock(ts_rwlock_t *rwlock) {
    while (1) {
        while (rwlock->writer_active) { /* 等待写锁 */; }
        __sync_fetch_and_add(&rwlock->readers, 1);
        if (!rwlock->writer_active) return;
        __sync_fetch_and_sub(&rwlock->readers, 1);
    }
}

static void ts_rwlock_read_unlock(ts_rwlock_t *rwlock) {
    __sync_fetch_and_sub(&rwlock->readers, 1);
}

static void ts_rwlock_write_lock(ts_rwlock_t *rwlock) {
    __sync_fetch_and_add(&rwlock->writers_waiting, 1);
    while (1) {
        while (rwlock->readers > 0) { /* 等待读锁 */; }
        if (__sync_bool_compare_and_swap(&rwlock->writer_active, 0, 1)) {
            __sync_fetch_and_sub(&rwlock->writers_waiting, 1);
            return;
        }
    }
}

static void ts_rwlock_write_unlock(ts_rwlock_t *rwlock) {
    __sync_fetch_and_and(&rwlock->writer_active, 0);
}

int ts_engine_enable_lock(void *rel, bool use_lock) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;

    if (use_lock) {
        if (g_ts_lockmgr == NULL) {
            g_ts_lockmgr = lock_mgr_create();
            if (g_ts_lockmgr == NULL) {
                LOG_ERROR("创建时序引擎锁管理器失败");
                return -1;
            }
        }
        if (db->rwlock == NULL) {
            db->rwlock = calloc(1, sizeof(ts_rwlock_t));
            if (db->rwlock == NULL) return -1;
            ts_rwlock_init((ts_rwlock_t *)db->rwlock);
        }
        db->lockmgr = g_ts_lockmgr;
        db->use_lock = true;
        LOG_INFO("时序引擎并发锁已启用");
    } else {
        db->use_lock = false;
        db->lockmgr = NULL;
        if (db->rwlock != NULL) {
            free(db->rwlock);
            db->rwlock = NULL;
        }
        LOG_INFO("时序引擎并发锁已禁用");
    }
    return 0;
}

int ts_engine_read_lock(void *rel) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        ts_rwlock_read_lock((ts_rwlock_t *)db->rwlock);
    }
    return 0;
}

void ts_engine_read_unlock(void *rel) {
    if (rel == NULL) return;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        ts_rwlock_read_unlock((ts_rwlock_t *)db->rwlock);
    }
}

int ts_engine_write_lock(void *rel, uint32_t timeout_ms) {
    if (rel == NULL) return -1;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        ts_rwlock_write_lock((ts_rwlock_t *)db->rwlock);
    }
    (void)timeout_ms;
    return 0;
}

void ts_engine_write_unlock(void *rel) {
    if (rel == NULL) return;
    ts_engine_db_t *db = (ts_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        ts_rwlock_write_unlock((ts_rwlock_t *)db->rwlock);
    }
}
