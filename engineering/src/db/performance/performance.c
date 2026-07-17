/*
 * performance.c - 性能优化实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <db/performance/performance.h>

/* ─────────────────────────────────────────────────────────────────
 * SIMD 检测
 * ───────────────────────────────────────────────────────────────── */

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #define SIMD_TYPE "SSE/AVX"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
    #define SIMD_TYPE "NEON"
#else
    #define SIMD_TYPE "NONE"
#endif

bool simd_is_supported(void)
{
    #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)
    return true;
    #else
    return false;
    #endif
}

const char *simd_get_type(void)
{
    return SIMD_TYPE;
}

/* ─────────────────────────────────────────────────────────────────
 * 向量化操作（简化实现）
 * ───────────────────────────────────────────────────────────────── */

void simd_compare_int(const int32_t *a, const int32_t *b, uint8_t *result, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        result[i] = (a[i] == b[i]) ? 1 : 0;
    }
}

int64_t simd_sum_int64(const int64_t *data, size_t count)
{
    int64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum;
}

double simd_sum_double(const double *data, size_t count)
{
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum;
}

double simd_avg_double(const double *data, size_t count)
{
    if (count == 0) return 0.0;
    return simd_sum_double(data, count) / count;
}

/* ─────────────────────────────────────────────────────────────────
 * 并行执行器结构
 * ───────────────────────────────────────────────────────────────── */

struct parallel_executor {
    parallel_config_t config;
    int actual_workers;
};

parallel_executor_t *parallel_executor_create(const parallel_config_t *config)
{
    parallel_executor_t *executor = (parallel_executor_t *)calloc(1,
        sizeof(parallel_executor_t));
    if (executor == NULL) return NULL;

    if (config) {
        executor->config = *config;
    } else {
        executor->config.num_workers = get_cpu_count();
        executor->config.chunk_size = 1024;
        executor->config.auto_tuning = true;
    }

    if (executor->config.num_workers <= 0) {
        executor->config.num_workers = 1;
    }

    executor->actual_workers = executor->config.num_workers;
    return executor;
}

void parallel_executor_destroy(parallel_executor_t *executor)
{
    free(executor);
}

int parallel_executor_workers(const parallel_executor_t *executor)
{
    return executor ? executor->actual_workers : 0;
}

int parallel_execute(parallel_executor_t *executor,
                     parallel_task_t *tasks, int num_tasks)
{
    if (executor == NULL || tasks == NULL || num_tasks <= 0) return -1;

    /* TODO: 实现真正的并行执行
     * 目前简化实现：串行执行
     */
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].func) {
            tasks[i].func(tasks[i].arg, 0);
        }
    }

    return 0;
}

int parallel_map_reduce(parallel_executor_t *executor,
                       void *data, size_t count,
                       void *(*map_func)(void *, int, int),
                       void *(*reduce_func)(void *, void *),
                       void *result)
{
    (void)executor;

    if (data == NULL || map_func == NULL || reduce_func == NULL || result == NULL) {
        return -1;
    }

    /* 简化实现：单线程 MapReduce */
    void *map_result = map_func(data, 0, (int)count);
    memcpy(result, map_result, sizeof(void *));
    free(map_result);

    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 批量操作（简化实现）
 * ───────────────────────────────────────────────────────────────── */

int batch_insert(const char *table, void **rows, size_t count,
                 batch_insert_callback_t callback, void *arg)
{
    (void)table;

    if (rows == NULL || callback == NULL) return -1;

    int inserted = 0;
    for (size_t i = 0; i < count; i++) {
        if (callback(rows[i], arg) == 0) {
            inserted++;
        }
    }
    return inserted;
}

int batch_update(const char *table, void **conditions, void **updates, size_t count,
                batch_update_callback_t callback, void *arg)
{
    (void)table;

    if (conditions == NULL || updates == NULL || callback == NULL) return -1;

    int updated = 0;
    for (size_t i = 0; i < count; i++) {
        if (callback(conditions[i], updates[i], arg) == 0) {
            updated++;
        }
    }
    return updated;
}

/* ─────────────────────────────────────────────────────────────────
 * 查询缓存结构
 * ───────────────────────────────────────────────────────────────── */

typedef struct cache_entry {
    char *key;
    void *value;
    size_t value_size;
    uint64_t expires_at;
    struct cache_entry *next;
} cache_entry_t;

struct query_cache {
    cache_entry_t **buckets;
    size_t num_buckets;
    size_t max_entries;
    size_t current_entries;
    int ttl_seconds;
    query_cache_stats_t stats;
};

static unsigned int cache_hash(const char *key)
{
    unsigned int h = 2166136261U;
    while (*key) {
        h ^= *key++;
        h *= 16777619U;
    }
    return h;
}

query_cache_t *query_cache_create(size_t max_entries, int ttl_seconds)
{
    query_cache_t *cache = (query_cache_t *)calloc(1, sizeof(query_cache_t));
    if (cache == NULL) return NULL;

    cache->num_buckets = max_entries > 256 ? max_entries : 256;
    cache->buckets = (cache_entry_t **)calloc(cache->num_buckets,
        sizeof(cache_entry_t *));
    if (cache->buckets == NULL) {
        free(cache);
        return NULL;
    }

    cache->max_entries = max_entries;
    cache->ttl_seconds = ttl_seconds;
    cache->current_entries = 0;
    memset(&cache->stats, 0, sizeof(cache->stats));

    return cache;
}

void query_cache_destroy(query_cache_t *cache)
{
    if (cache == NULL) return;

    for (size_t i = 0; i < cache->num_buckets; i++) {
        cache_entry_t *entry = cache->buckets[i];
        while (entry) {
            cache_entry_t *next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(cache->buckets);
    free(cache);
}

bool query_cache_get(query_cache_t *cache, const char *key,
                    void *value, size_t *value_size)
{
    if (cache == NULL || key == NULL) return false;

    unsigned int h = cache_hash(key) % cache->num_buckets;
    cache_entry_t *entry = cache->buckets[h];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            /* 检查过期 */
            if (cache->ttl_seconds > 0 &&
                entry->expires_at < (uint64_t)time(NULL)) {
                /* 已过期 */
                break;
            }

            if (value && value_size) {
                size_t copy_size = entry->value_size < *value_size ?
                                  entry->value_size : *value_size;
                memcpy(value, entry->value, copy_size);
                *value_size = entry->value_size;
            }
            cache->stats.hits++;
            return true;
        }
        entry = entry->next;
    }

    cache->stats.misses++;
    return false;
}

void query_cache_set(query_cache_t *cache, const char *key,
                    const void *value, size_t value_size)
{
    if (cache == NULL || key == NULL || value == NULL) return;

    unsigned int h = cache_hash(key) % cache->num_buckets;

    /* 查找或创建 */
    cache_entry_t *entry = cache->buckets[h];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            /* 更新现有条目 */
            free(entry->value);
            entry->value = malloc(value_size);
            memcpy(entry->value, value, value_size);
            entry->value_size = value_size;
            if (cache->ttl_seconds > 0) {
                entry->expires_at = (uint64_t)time(NULL) + cache->ttl_seconds;
            }
            return;
        }
        entry = entry->next;
    }

    /* LRU 驱逐 */
    if (cache->current_entries >= cache->max_entries) {
        /* 简化：驱逐第一个 */
        cache_entry_t *old = cache->buckets[h];
        if (old) {
            cache->buckets[h] = old->next;
            free(old->key);
            free(old->value);
            free(old);
            cache->current_entries--;
            cache->stats.evictions++;
        }
    }

    /* 创建新条目 */
    entry = (cache_entry_t *)malloc(sizeof(cache_entry_t));
    if (entry == NULL) return;

    entry->key = strdup(key);
    entry->value = malloc(value_size);
    memcpy(entry->value, value, value_size);
    entry->value_size = value_size;
    entry->expires_at = cache->ttl_seconds > 0 ?
        (uint64_t)time(NULL) + cache->ttl_seconds : 0;
    entry->next = cache->buckets[h];
    cache->buckets[h] = entry;
    cache->current_entries++;
}

void query_cache_invalidate(query_cache_t *cache, const char *key)
{
    if (cache == NULL) return;

    if (key == NULL) {
        /* 清除所有 */
        for (size_t i = 0; i < cache->num_buckets; i++) {
            cache_entry_t *entry = cache->buckets[i];
            while (entry) {
                cache_entry_t *next = entry->next;
                free(entry->key);
                free(entry->value);
                free(entry);
                entry = next;
            }
            cache->buckets[i] = NULL;
        }
        cache->current_entries = 0;
    } else {
        /* 清除指定 */
        unsigned int h = cache_hash(key) % cache->num_buckets;
        cache_entry_t **prev = &cache->buckets[h];
        cache_entry_t *entry = cache->buckets[h];

        while (entry) {
            if (strcmp(entry->key, key) == 0) {
                *prev = entry->next;
                free(entry->key);
                free(entry->value);
                free(entry);
                cache->current_entries--;
                return;
            }
            prev = &entry->next;
            entry = entry->next;
        }
    }
}

const query_cache_stats_t *query_cache_get_stats(const query_cache_t *cache)
{
    return cache ? &cache->stats : NULL;
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

int get_cpu_count(void)
{
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
    #elif defined(__linux__)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
    #else
    return 4;  /* 默认值 */
    #endif
}

uint64_t get_available_memory(void)
{
    #ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return statex.ullTotalPhys;
    }
    #elif defined(__linux__)
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0) {
        return (uint64_t)(pages * page_size);
    }
    #endif
    return 8ULL * 1024 * 1024 * 1024;  /* 默认 8GB */
}

int estimate_optimal_workers(int workload_type)
{
    int cpus = get_cpu_count();

    if (workload_type == 0) {
        /* CPU 密集型：使用所有核心 */
        return cpus;
    } else {
        /* IO 密集型：可以使用更多线程 */
        return cpus * 2;
    }
}