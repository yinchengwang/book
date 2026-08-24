/**
 * @file mmdb_metrics.h
 * @brief 运行时监控指标（Prometheus 格式输出）
 *
 * 提供 SDK 运行时的关键指标查询能力，支持 Prometheus 格式导出。
 * 所有指标存储在进程级全局变量中，线程安全通过原子操作保证（C11 stdatomic.h）。
 *
 * 使用示例：
 * @code
 *   const mmdb_metrics_t* m = mmdb_metrics_get();
 *   printf("vectors_total = %lu\n", (unsigned long)m->vectors_total);
 *
 *   char buf[4096];
 *   size_t n = mmdb_metrics_prometheus_format(buf, sizeof(buf));
 *   write(STDOUT_FILENO, buf, n);
 * @endcode
 */
#ifndef SDK_MMDB_METRICS_H
#define SDK_MMDB_METRICS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SDK 运行时指标快照
 *
 * 字段分类：
 *   - 容量指标：vectors_total（向量总数）
 *   - 查询指标：queries_total/success/failed + 延迟统计
 *   - 缓存指标：cache_hits/misses + 命中率（计算字段）
 *   - 资源指标：内存 / 磁盘使用量
 *   - 索引指标：HNSW 构建次数与耗时
 *
 * 注：query_latency_avg_ms / p50 / p99 简化实现（指数移动平均 + 计数器），
 *     未来可替换为更精确的分位数算法（如 t-digest）。
 */
typedef struct {
    /* 容量 */
    uint64_t    vectors_total;
    /* 查询 */
    uint64_t    queries_total;
    uint64_t    queries_success;
    uint64_t    queries_failed;
    double      query_latency_avg_ms;
    double      query_latency_p50_ms;
    double      query_latency_p99_ms;
    /* 缓存 */
    uint64_t    cache_hits;
    uint64_t    cache_misses;
    double      cache_hit_rate;
    /* 资源 */
    size_t      memory_used_bytes;
    size_t      memory_total_bytes;
    size_t      disk_used_bytes;
    size_t      disk_total_bytes;
    /* HNSW */
    uint64_t    hnsw_build_total;
    double      hnsw_build_time_ms;
} mmdb_metrics_t;

/**
 * @brief 获取当前指标快照（线程安全）
 *
 * 返回全局指标对象的只读指针。调用方不得修改返回值指向的结构。
 *
 * @return 指向全局指标的指针（非 NULL）
 */
const mmdb_metrics_t* mmdb_metrics_get(void);

/**
 * @brief 重置所有计数器为零（线程安全）
 *
 * 主要用于测试场景或运维主动重置监控数据。
 */
void mmdb_metrics_reset(void);

/**
 * @brief 输出 Prometheus 文本格式指标
 *
 * 输出格式示例：
 * @code
 *   # HELP mmdb_vectors_total Number of vectors stored
 *   # TYPE mmdb_vectors_total counter
 *   mmdb_vectors_total 12345
 *   ...
 * @endcode
 *
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区容量
 * @return 实际写入字节数（不含结尾 '\0'）；buf 为 NULL 或 buf_size=0 返回 0
 */
size_t mmdb_metrics_prometheus_format(char* buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_METRICS_H */
