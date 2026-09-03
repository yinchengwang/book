/**
 * @file metrics.h
 * @brief Prometheus 指标收集系统
 */
#ifndef DB_METRICS_H
#define DB_METRICS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 指标类型
 * ======================================================================== */

typedef enum MetricType_e {
    METRIC_TYPE_COUNTER,   /**< 计数器，只增不减 */
    METRIC_TYPE_GAUGE,     /**< 仪表，可增可减 */
    METRIC_TYPE_HISTOGRAM, /**< 直方图，统计分布 */
    METRIC_TYPE_SUMMARY    /**< 摘要，统计分位数 */
} MetricType;

/* ========================================================================
 * 指标句柄
 * ======================================================================== */

typedef struct Metric_s Metric;

/* ========================================================================
 * 指标注册
 * ======================================================================== */

/**
 * @brief 初始化指标系统
 */
void metrics_init(void);

/**
 * @brief 创建/获取计数器
 */
Metric *metrics_counter(const char *name, const char *help, const char *const *labels, int label_count);

/**
 * @brief 创建/获取仪表
 */
Metric *metrics_gauge(const char *name, const char *help, const char *const *labels, int label_count);

/**
 * @brief 创建/获取直方图
 */
Metric *metrics_histogram(const char *name, const char *help, double *buckets, int bucket_count,
                         const char *const *labels, int label_count);

/* ========================================================================
 * 指标操作
 * ======================================================================== */

/**
 * @brief 计数器增加
 */
void metrics_counter_inc(Metric *m, ...);

/**
 * @brief 计数器增加值
 */
void metrics_counter_add(Metric *m, double value, ...);

/**
 * @brief 仪表设置值
 */
void metrics_gauge_set(Metric *m, double value, ...);

/**
 * @brief 仪表增加
 */
void metrics_gauge_inc(Metric *m, ...);

/**
 * @brief 仪表减少
 */
void metrics_gauge_dec(Metric *m, ...);

/**
 * @brief 直方图观察值
 */
void metrics_histogram_observe(Metric *m, double value, ...);

/* ========================================================================
 * 预定义指标
 * ======================================================================== */

void metrics_inc_vectors_total(int64_t delta);
void metrics_inc_collections_total(int32_t delta);
void metrics_inc_request_total(const char *method, const char *path, int status);
void metrics_observe_search_duration(double seconds);
void metrics_observe_insert_duration(double seconds);
double metrics_get_buffer_hit_ratio(void);
void metrics_set_buffer_hit_ratio(double ratio);

/* ========================================================================
 * 指标输出
 * ======================================================================== */

/**
 * @brief 获取所有指标（Prometheus 格式）
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字节数
 */
int metrics_get_all(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_METRICS_H */
