/**
 * @file metrics.c
 * @brief Prometheus 指标收集实现
 */
#include <db/metrics.h>
#include <db/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 指标项 */
typedef struct Metric_s {
    char *name;
    char *help;
    MetricType type;
    char **label_keys;
    int label_count;

    /* 值存储 */
    double value;
    double *bucket_values;  /* 直方图 bucket */
    double *bucket_bounds; /* 直方图边界 */
    int bucket_count;

    struct Metric_s *next;
} Metric;

/** 全局状态 */
static struct {
    pthread_mutex_t mutex;
    Metric *metrics;
    double buffer_hit_ratio;
    bool initialized;
} g_metrics = {
    .initialized = false
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *metric_type_string(MetricType type) {
    switch (type) {
        case METRIC_TYPE_COUNTER: return "counter";
        case METRIC_TYPE_GAUGE:   return "gauge";
        case METRIC_TYPE_HISTOGRAM: return "histogram";
        case METRIC_TYPE_SUMMARY:   return "summary";
        default: return "untyped";
    }
}

static double default_histogram_buckets[] = {
    0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0
};
static const int DEFAULT_BUCKET_COUNT = 12;

/* ========================================================================
 * 指标注册
 * ======================================================================== */

void metrics_init(void) {
    pthread_mutex_lock(&g_mutex);
    if (!g_metrics.initialized) {
        g_metrics.initialized = true;
        g_metrics.buffer_hit_ratio = 0.0;
        LOG_INFO("Metrics system initialized");
    }
    pthread_mutex_unlock(&g_mutex);
}

static Metric *find_or_create_metric(const char *name, MetricType type,
                                    const char *help,
                                    const char *const *labels, int label_count) {
    pthread_mutex_lock(&g_mutex);

    /* 查找已存在的 */
    Metric *m = g_metrics.metrics;
    while (m) {
        if (strcmp(m->name, name) == 0) {
            pthread_mutex_unlock(&g_mutex);
            return m;
        }
        m = m->next;
    }

    /* 创建新指标 */
    m = calloc(1, sizeof(Metric));
    m->name = strdup(name);
    m->help = help ? strdup(help) : NULL;
    m->type = type;
    m->label_count = label_count;

    if (label_count > 0 && labels) {
        m->label_keys = malloc(label_count * sizeof(char *));
        for (int i = 0; i < label_count; i++) {
            m->label_keys[i] = strdup(labels[i]);
        }
    }

    if (type == METRIC_TYPE_HISTOGRAM) {
        m->bucket_count = DEFAULT_BUCKET_COUNT;
        m->bucket_bounds = malloc(m->bucket_count * sizeof(double));
        m->bucket_values = calloc(m->bucket_count + 1, sizeof(double));
        memcpy(m->bucket_bounds, default_histogram_buckets,
               DEFAULT_BUCKET_COUNT * sizeof(double));
    }

    /* 添加到链表 */
    m->next = g_metrics.metrics;
    g_metrics.metrics = m;

    pthread_mutex_unlock(&g_mutex);
    return m;
}

Metric *metrics_counter(const char *name, const char *help,
                       const char *const *labels, int label_count) {
    return find_or_create_metric(name, METRIC_TYPE_COUNTER, help, labels, label_count);
}

Metric *metrics_gauge(const char *name, const char *help,
                     const char *const *labels, int label_count) {
    return find_or_create_metric(name, METRIC_TYPE_GAUGE, help, labels, label_count);
}

Metric *metrics_histogram(const char *name, const char *help, double *buckets, int bucket_count,
                         const char *const *labels, int label_count) {
    Metric *m = find_or_create_metric(name, METRIC_TYPE_HISTOGRAM, help, labels, label_count);

    if (buckets && bucket_count > 0) {
        pthread_mutex_lock(&g_mutex);
        m->bucket_count = bucket_count;
        free(m->bucket_bounds);
        free(m->bucket_values);
        m->bucket_bounds = malloc(bucket_count * sizeof(double));
        m->bucket_values = calloc(bucket_count + 1, sizeof(double));
        memcpy(m->bucket_bounds, buckets, bucket_count * sizeof(double));
        pthread_mutex_unlock(&g_mutex);
    }

    return m;
}

/* ========================================================================
 * 指标操作
 * ======================================================================== */

void metrics_counter_inc(Metric *m, ...) {
    if (!m) return;
    pthread_mutex_lock(&g_mutex);
    m->value += 1.0;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_counter_add(Metric *m, double value, ...) {
    if (!m) return;
    pthread_mutex_lock(&g_mutex);
    m->value += value;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_gauge_set(Metric *m, double value, ...) {
    if (!m) return;
    pthread_mutex_lock(&g_mutex);
    m->value = value;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_gauge_inc(Metric *m, ...) {
    if (!m) return;
    pthread_mutex_lock(&g_mutex);
    m->value += 1.0;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_gauge_dec(Metric *m, ...) {
    if (!m) return;
    pthread_mutex_lock(&g_mutex);
    m->value -= 1.0;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_histogram_observe(Metric *m, double value, ...) {
    if (!m || m->type != METRIC_TYPE_HISTOGRAM) return;

    pthread_mutex_lock(&g_mutex);

    m->value += value;

    /* 更新 bucket */
    for (int i = 0; i < m->bucket_count; i++) {
        if (value <= m->bucket_bounds[i]) {
            m->bucket_values[i]++;
        }
    }
    m->bucket_values[m->bucket_count]++; /* +Inf bucket */

    pthread_mutex_unlock(&g_mutex);
}

/* ========================================================================
 * 预定义指标
 * ======================================================================== */

static Metric *g_vectors_total;
static Metric *g_collections_total;
static Metric *g_request_total;
static Metric *g_search_duration;
static Metric *g_insert_duration;

void metrics_inc_vectors_total(int64_t delta) {
    if (!g_vectors_total) {
        g_vectors_total = metrics_gauge("minivecdb_vectors_total",
            "Total number of vectors in the database", NULL, 0);
    }
    pthread_mutex_lock(&g_mutex);
    g_vectors_total->value += delta;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_inc_collections_total(int32_t delta) {
    if (!g_collections_total) {
        g_collections_total = metrics_gauge("minivecdb_collections_total",
            "Total number of collections", NULL, 0);
    }
    pthread_mutex_lock(&g_mutex);
    g_collections_total->value += delta;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_inc_request_total(const char *method, const char *path, int status) {
    if (!g_request_total) {
        const char *labels[] = {"method", "path", "status"};
        g_request_total = metrics_counter("minivecdb_request_total",
            "Total number of HTTP requests", labels, 3);
    }
    pthread_mutex_lock(&g_mutex);
    /* 简化处理：直接累加计数 */
    (void)method; (void)path; (void)status;
    g_request_total->value += 1.0;
    pthread_mutex_unlock(&g_mutex);
}

void metrics_observe_search_duration(double seconds) {
    if (!g_search_duration) {
        g_search_duration = metrics_histogram("minivecdb_search_duration_seconds",
            "Search request duration in seconds", NULL, 0, NULL, 0);
    }
    metrics_histogram_observe(g_search_duration, seconds);
}

void metrics_observe_insert_duration(double seconds) {
    if (!g_insert_duration) {
        g_insert_duration = metrics_histogram("minivecdb_insert_duration_seconds",
            "Insert request duration in seconds", NULL, 0, NULL, 0);
    }
    metrics_histogram_observe(g_insert_duration, seconds);
}

double metrics_get_buffer_hit_ratio(void) {
    pthread_mutex_lock(&g_mutex);
    double ratio = g_metrics.buffer_hit_ratio;
    pthread_mutex_unlock(&g_mutex);
    return ratio;
}

void metrics_set_buffer_hit_ratio(double ratio) {
    pthread_mutex_lock(&g_mutex);
    g_metrics.buffer_hit_ratio = ratio;
    pthread_mutex_unlock(&g_mutex);
}

/* ========================================================================
 * 指标输出
 * ======================================================================== */

static void format_labels(char *buf, size_t buf_size, Metric *m) {
    if (m->label_count == 0) {
        buf[0] = '\0';
        return;
    }

    char *p = buf;
    size_t remaining = buf_size;

    int written = snprintf(p, remaining, "{");
    p += written;
    remaining -= written;

    for (int i = 0; i < m->label_count; i++) {
        if (i > 0) {
            written = snprintf(p, remaining, ",");
            p += written;
            remaining -= written;
        }
        written = snprintf(p, remaining, "%s=\"%s\"", m->label_keys[i], m->label_keys[i]);
        p += written;
        remaining -= written;
    }

    snprintf(p, remaining, "}");
}

int metrics_get_all(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return 0;

    int offset = 0;

    pthread_mutex_lock(&g_mutex);

    /* 输出所有指标 */
    Metric *m = g_metrics.metrics;
    while (m) {
        /* HELP */
        if (m->help) {
            offset += snprintf(buf + offset, buf_size - offset,
                "# HELP %s %s\n", m->name, m->help);
        }

        /* TYPE */
        offset += snprintf(buf + offset, buf_size - offset,
            "# TYPE %s %s\n", m->name, metric_type_string(m->type));

        /* 标签 */
        char labels[256] = {0};
        format_labels(labels, sizeof(labels), m);

        if (m->type == METRIC_TYPE_HISTOGRAM) {
            /* 直方图每个 bucket 一行 */
            double sum = 0.0;
            for (int i = 0; i < m->bucket_count; i++) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "%s_bucket{le=\"%.3f\"%s} %.0f\n",
                    m->name, m->bucket_bounds[i], labels, m->bucket_values[i]);
                sum += m->bucket_values[i];
            }
            /* +Inf bucket */
            offset += snprintf(buf + offset, buf_size - offset,
                "%s_bucket{le=\"+Inf\"%s} %.0f\n",
                m->name, labels, m->bucket_values[m->bucket_count]);
            /* sum 和 count */
            offset += snprintf(buf + offset, buf_size - offset,
                "%s_sum%s %.6f\n", m->name, labels, m->value);
            offset += snprintf(buf + offset, buf_size - offset,
                "%s_count%s %.0f\n", m->name, labels, sum);
        } else {
            /* 普通指标 */
            offset += snprintf(buf + offset, buf_size - offset,
                "%s%s %.6f\n", m->name, labels, m->value);
        }

        m = m->next;
    }

    /* Buffer hit ratio */
    offset += snprintf(buf + offset, buf_size - offset,
        "# HELP minivecdb_buffer_hit_ratio Buffer pool hit ratio\n");
    offset += snprintf(buf + offset, buf_size - offset,
        "# TYPE minivecdb_buffer_hit_ratio gauge\n");
    offset += snprintf(buf + offset, buf_size - offset,
        "minivecdb_buffer_hit_ratio %.4f\n", g_metrics.buffer_hit_ratio);

    pthread_mutex_unlock(&g_mutex);

    return offset;
}
