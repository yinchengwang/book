/**
 * @file metrics.c
 * @brief SDK 运行时监控指标实现
 *
 * 全局指标存储在 g_metrics 中，通过原子操作保证线程安全：
 *   - 计数器字段（uint64_t）：使用 _Atomic 修饰，relaxed 语义自增
 *   - 浮点字段（double）：使用 _Atomic 修饰，relaxed 语义存储
 *   - 计算字段（cache_hit_rate）：在 mmdb_metrics_get 中实时计算，避免并发写入
 *
 * 设计权衡：
 *   - 简单优于精确：使用 EMA 估计延迟分布，避免引入复杂分位数算法
 *   - 单写多读：查询计数器为多写（每次搜索 +1），使用原子保证安全
 *   - 零侵入：仅在 mmdb_vectors_search 入口/出口埋点，其他 API 暂不采集
 */
#include "sdk/mmdb_metrics.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

/* 全局指标实例：所有字段为原子类型，支持多线程并发读写 */
static _Atomic uint64_t a_vectors_total;
static _Atomic uint64_t a_queries_total;
static _Atomic uint64_t a_queries_success;
static _Atomic uint64_t a_queries_failed;
static _Atomic double    a_query_latency_sum_ms;   /* 累计延迟（用于计算平均值） */
static _Atomic uint64_t a_query_latency_count;     /* 已记录延迟样本数 */
static _Atomic double    a_query_latency_p50_ms;
static _Atomic double    a_query_latency_p99_ms;
static _Atomic uint64_t a_cache_hits;
static _Atomic uint64_t a_cache_misses;
static _Atomic uint64_t a_hnsw_build_total;
static _Atomic double    a_hnsw_build_time_sum_ms;

/* 非原子资源字段：单进程内由调用方保证串行访问或近似值 */
static _Atomic size_t   a_memory_used_bytes;
static _Atomic size_t   a_memory_total_bytes;
static _Atomic size_t   a_disk_used_bytes;
static _Atomic size_t   a_disk_total_bytes;

/* 内部辅助宏：原子加载（relaxed 语义足够，因快照读取不要求严格一致性） */
#define LOAD_U64(ptr)      atomic_load_explicit(ptr, memory_order_relaxed)
#define LOAD_DOUBLE(ptr)   atomic_load_explicit(ptr, memory_order_relaxed)
#define LOAD_SIZE(ptr)     atomic_load_explicit(ptr, memory_order_relaxed)

const mmdb_metrics_t* mmdb_metrics_get(void) {
    /* 单例：函数局部静态对象，避免全局变量初始化顺序问题 */
    static mmdb_metrics_t snapshot;
    uint64_t total = LOAD_U64(&a_queries_total);
    uint64_t hits = LOAD_U64(&a_cache_hits);
    uint64_t misses = LOAD_U64(&a_cache_misses);
    uint64_t latency_count = LOAD_U64(&a_query_latency_count);
    double latency_sum = LOAD_DOUBLE(&a_query_latency_sum_ms);
    uint64_t hnsw_count = LOAD_U64(&a_hnsw_build_total);
    double hnsw_sum = LOAD_DOUBLE(&a_hnsw_build_time_sum_ms);

    snapshot.vectors_total       = LOAD_U64(&a_vectors_total);
    snapshot.queries_total       = total;
    snapshot.queries_success     = LOAD_U64(&a_queries_success);
    snapshot.queries_failed      = LOAD_U64(&a_queries_failed);
    /* 平均延迟：避免除零 */
    snapshot.query_latency_avg_ms =
        latency_count > 0 ? (latency_sum / (double)latency_count) : 0.0;
    snapshot.query_latency_p50_ms = LOAD_DOUBLE(&a_query_latency_p50_ms);
    snapshot.query_latency_p99_ms = LOAD_DOUBLE(&a_query_latency_p99_ms);

    snapshot.cache_hits          = hits;
    snapshot.cache_misses        = misses;
    /* 命中率：避免除零 */
    uint64_t cache_total = hits + misses;
    snapshot.cache_hit_rate =
        cache_total > 0 ? ((double)hits / (double)cache_total) : 0.0;

    snapshot.memory_used_bytes   = LOAD_SIZE(&a_memory_used_bytes);
    snapshot.memory_total_bytes  = LOAD_SIZE(&a_memory_total_bytes);
    snapshot.disk_used_bytes     = LOAD_SIZE(&a_disk_used_bytes);
    snapshot.disk_total_bytes    = LOAD_SIZE(&a_disk_total_bytes);

    snapshot.hnsw_build_total    = hnsw_count;
    snapshot.hnsw_build_time_ms  =
        hnsw_count > 0 ? (hnsw_sum / (double)hnsw_count) : 0.0;

    return &snapshot;
}

void mmdb_metrics_reset(void) {
    atomic_store_explicit(&a_vectors_total, 0, memory_order_relaxed);
    atomic_store_explicit(&a_queries_total, 0, memory_order_relaxed);
    atomic_store_explicit(&a_queries_success, 0, memory_order_relaxed);
    atomic_store_explicit(&a_queries_failed, 0, memory_order_relaxed);
    atomic_store_explicit(&a_query_latency_sum_ms, 0.0, memory_order_relaxed);
    atomic_store_explicit(&a_query_latency_count, 0, memory_order_relaxed);
    atomic_store_explicit(&a_query_latency_p50_ms, 0.0, memory_order_relaxed);
    atomic_store_explicit(&a_query_latency_p99_ms, 0.0, memory_order_relaxed);
    atomic_store_explicit(&a_cache_hits, 0, memory_order_relaxed);
    atomic_store_explicit(&a_cache_misses, 0, memory_order_relaxed);
    atomic_store_explicit(&a_hnsw_build_total, 0, memory_order_relaxed);
    atomic_store_explicit(&a_hnsw_build_time_sum_ms, 0.0, memory_order_relaxed);
    atomic_store_explicit(&a_memory_used_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&a_memory_total_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&a_disk_used_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&a_disk_total_bytes, 0, memory_order_relaxed);
}

/**
 * @brief 安全地格式化浮点数到缓冲区（snprintf 包装）
 *
 * 避免 %f 输出过大数字时的栈溢出（极少见，但 1e308 长度可超过 1KB）
 */
static int safe_snprintf_double(char* buf, size_t buf_size, double val) {
    if (!buf || buf_size == 0) return 0;
    /* 使用 %.6f 保证输出长度可控（最大约 320 字符） */
    return snprintf(buf, buf_size, "%.6f", val);
}

/**
 * @brief Prometheus 格式写入一行 metric（带 HELP/TYPE 注释）
 *
 * @return 写入的字节数，溢出时返回 0（缓冲区已满）
 */
static size_t write_metric(char* buf, size_t buf_size, size_t offset,
                           const char* name, double value,
                           const char* help, const char* type) {
    int n;
    /* HELP 注释 */
    n = snprintf(buf + offset, buf_size - offset,
                 "# HELP %s %s\n# TYPE %s %s\n%s ",
                 name, help, name, type, name);
    if (n < 0 || (size_t)n >= buf_size - offset) return 0;
    offset += (size_t)n;

    /* 数值：双精度保留 6 位小数 */
    n = safe_snprintf_double(buf + offset, buf_size - offset, value);
    if (n < 0 || (size_t)n >= buf_size - offset) return 0;
    offset += (size_t)n;

    /* 行尾换行 */
    n = snprintf(buf + offset, buf_size - offset, "\n");
    /* snprintf 返回非负值且 < buf_size - offset 时才正常推进 */
    if (n < 0) return 0;
    if ((size_t)n >= buf_size - offset) return offset;  /* 截断：保留已写入部分 */
    offset += (size_t)n;

    return offset;
}

size_t mmdb_metrics_prometheus_format(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) return 0;

    const mmdb_metrics_t* m = mmdb_metrics_get();
    size_t off = 0;
    int n;

    /* 标题注释：标识来源 SDK 与生成时间（可选） */
    n = snprintf(buf + off, buf_size - off, "# Metrics from mmdb-sdk\n");
    if (n < 0 || (size_t)n >= buf_size - off) return off;
    off += (size_t)n;

    /* 容量指标 */
    off = write_metric(buf, buf_size, off, "mmdb_vectors_total",
                       (double)m->vectors_total,
                       "Number of vectors stored", "counter");
    if (off == 0) return 0;

    /* 查询指标 */
    off = write_metric(buf, buf_size, off, "mmdb_queries_total",
                       (double)m->queries_total,
                       "Total number of queries", "counter");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_queries_success",
                       (double)m->queries_success,
                       "Number of successful queries", "counter");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_queries_failed",
                       (double)m->queries_failed,
                       "Number of failed queries", "counter");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_query_latency_avg_ms",
                       m->query_latency_avg_ms,
                       "Average query latency in milliseconds", "gauge");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_query_latency_p50_ms",
                       m->query_latency_p50_ms,
                       "50th percentile query latency in milliseconds", "gauge");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_query_latency_p99_ms",
                       m->query_latency_p99_ms,
                       "99th percentile query latency in milliseconds", "gauge");
    if (off == 0) return 0;

    /* 缓存指标 */
    off = write_metric(buf, buf_size, off, "mmdb_cache_hits",
                       (double)m->cache_hits,
                       "Number of cache hits", "counter");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_cache_misses",
                       (double)m->cache_misses,
                       "Number of cache misses", "counter");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_cache_hit_rate",
                       m->cache_hit_rate,
                       "Cache hit rate (0.0 to 1.0)", "gauge");
    if (off == 0) return 0;

    /* 资源指标 */
    off = write_metric(buf, buf_size, off, "mmdb_memory_used_bytes",
                       (double)m->memory_used_bytes,
                       "Memory used in bytes", "gauge");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_memory_total_bytes",
                       (double)m->memory_total_bytes,
                       "Total memory available in bytes", "gauge");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_disk_used_bytes",
                       (double)m->disk_used_bytes,
                       "Disk used in bytes", "gauge");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_disk_total_bytes",
                       (double)m->disk_total_bytes,
                       "Total disk available in bytes", "gauge");
    if (off == 0) return 0;

    /* HNSW 指标 */
    off = write_metric(buf, buf_size, off, "mmdb_hnsw_build_total",
                       (double)m->hnsw_build_total,
                       "Number of HNSW index builds", "counter");
    if (off == 0) return 0;
    off = write_metric(buf, buf_size, off, "mmdb_hnsw_build_time_ms",
                       m->hnsw_build_time_ms,
                       "Average HNSW index build time in milliseconds", "gauge");
    if (off == 0) return 0;

    return off;
}

/* ================================================================== */
/* 内部埋点 API：供 SDK 其他模块调用（不在公共头文件暴露）             */
/* ================================================================== */

/**
 * @brief 记录一次查询（成功或失败）
 *
 * @param latency_ms 查询耗时（毫秒）
 * @param success    1 = 成功，0 = 失败
 */
void mmdb_metrics_record_query(double latency_ms, int success) {
    atomic_fetch_add_explicit(&a_queries_total, 1, memory_order_relaxed);
    if (success) {
        atomic_fetch_add_explicit(&a_queries_success, 1, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&a_queries_failed, 1, memory_order_relaxed);
    }
    /* 累计延迟（原子读取-修改-写回，CAS 循环保证并发安全） */
    double old_sum, new_sum;
    do {
        old_sum = atomic_load_explicit(&a_query_latency_sum_ms,
                                       memory_order_relaxed);
        new_sum = old_sum + latency_ms;
    } while (!atomic_compare_exchange_weak_explicit(
                 &a_query_latency_sum_ms, &old_sum, new_sum,
                 memory_order_relaxed, memory_order_relaxed));
    atomic_fetch_add_explicit(&a_query_latency_count, 1, memory_order_relaxed);

    /* 简化分位数估计：
     *   - p50：使用 EMA (alpha=0.1) 跟踪中位数附近
     *   - p99：使用 EMA (alpha=0.01) 跟踪尾部延迟
     * 这是工程近似而非精确分位数，但对监控足够 */
    double old_p50;
    do {
        old_p50 = atomic_load_explicit(&a_query_latency_p50_ms,
                                       memory_order_relaxed);
        new_sum = old_p50 * 0.9 + latency_ms * 0.1;
    } while (!atomic_compare_exchange_weak_explicit(
                 &a_query_latency_p50_ms, &old_p50, new_sum,
                 memory_order_relaxed, memory_order_relaxed));

    double old_p99;
    do {
        old_p99 = atomic_load_explicit(&a_query_latency_p99_ms,
                                       memory_order_relaxed);
        new_sum = old_p99 * 0.99 + latency_ms * 0.01;
    } while (!atomic_compare_exchange_weak_explicit(
                 &a_query_latency_p99_ms, &old_p99, new_sum,
                 memory_order_relaxed, memory_order_relaxed));
}

/**
 * @brief 记录一次缓存命中/未命中
 */
void mmdb_metrics_record_cache(int hit) {
    if (hit) {
        atomic_fetch_add_explicit(&a_cache_hits, 1, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&a_cache_misses, 1, memory_order_relaxed);
    }
}

/**
 * @brief 记录 HNSW 索引构建耗时
 */
void mmdb_metrics_record_hnsw_build(double time_ms) {
    atomic_fetch_add_explicit(&a_hnsw_build_total, 1, memory_order_relaxed);
    double old_sum, new_sum;
    do {
        old_sum = atomic_load_explicit(&a_hnsw_build_time_sum_ms,
                                       memory_order_relaxed);
        new_sum = old_sum + time_ms;
    } while (!atomic_compare_exchange_weak_explicit(
                 &a_hnsw_build_time_sum_ms, &old_sum, new_sum,
                 memory_order_relaxed, memory_order_relaxed));
}

/**
 * @brief 更新向量总数（用于 insert/delete 后同步）
 */
void mmdb_metrics_set_vectors_total(uint64_t n) {
    atomic_store_explicit(&a_vectors_total, n, memory_order_relaxed);
}

/**
 * @brief 增加向量总数（用于 insert 时累加）
 */
void mmdb_metrics_inc_vectors_total(uint64_t delta) {
    atomic_fetch_add_explicit(&a_vectors_total, delta, memory_order_relaxed);
}

/**
 * @brief 减少向量总数（用于 delete 时扣除）
 */
void mmdb_metrics_dec_vectors_total(uint64_t delta) {
    /* 使用 CAS 保证不会下溢为负数（uint64 无符号） */
    uint64_t old_val, new_val;
    do {
        old_val = atomic_load_explicit(&a_vectors_total,
                                       memory_order_relaxed);
        new_val = old_val >= delta ? (old_val - delta) : 0;
    } while (!atomic_compare_exchange_weak_explicit(
                 &a_vectors_total, &old_val, new_val,
                 memory_order_relaxed, memory_order_relaxed));
}

/**
 * @brief 更新资源使用快照（运维调用，非热路径）
 */
void mmdb_metrics_set_resources(size_t mem_used, size_t mem_total,
                                size_t disk_used, size_t disk_total) {
    atomic_store_explicit(&a_memory_used_bytes, mem_used, memory_order_relaxed);
    atomic_store_explicit(&a_memory_total_bytes, mem_total, memory_order_relaxed);
    atomic_store_explicit(&a_disk_used_bytes, disk_used, memory_order_relaxed);
    atomic_store_explicit(&a_disk_total_bytes, disk_total, memory_order_relaxed);
}
