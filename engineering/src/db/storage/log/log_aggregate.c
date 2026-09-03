#include "db/log_engine.h"
#include "db/core/log.h"
#include "db/storage/log/log_engine_ext.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 辅助：从 log line 文本中提取指定字段值（double） */
static int parse_field_double(const char *line, size_t line_len,
                               const char *field_name, double *out_val) {
    if (!line || !field_name || !out_val) return -1;
    /* 查找 field_name=" 或 field_name= */
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", field_name);
    const char *p = strstr(line, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    /* 解析数值（支持整数、小数、负数） */
    char *end;
    double v = strtod(p, &end);
    if (end == p) return -1;  /* 没有解析到数值 */
    *out_val = v;
    return 0;
}

/* C3-3 T8: 聚合算子 */
double log_aggregate_count(const log_line_t *lines, size_t n) {
    return (double)n;
}

double log_aggregate_sum(const log_line_t *lines, size_t n) {
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double v;
        if (parse_field_double(lines[i].line, lines[i].line_len, "value", &v) == 0) {
            sum += v;
        }
    }
    return sum;
}

double log_aggregate_avg(const log_line_t *lines, size_t n) {
    double sum = log_aggregate_sum(lines, n);
    return n > 0 ? sum / (double)n : 0.0;
}

double log_aggregate_max(const log_line_t *lines, size_t n) {
    double max_val = -INFINITY;
    for (size_t i = 0; i < n; ++i) {
        double v;
        if (parse_field_double(lines[i].line, lines[i].line_len, "value", &v) == 0) {
            if (v > max_val) max_val = v;
        }
    }
    return (max_val == -INFINITY) ? 0.0 : max_val;
}

double log_aggregate_min(const log_line_t *lines, size_t n) {
    double min_val = INFINITY;
    for (size_t i = 0; i < n; ++i) {
        double v;
        if (parse_field_double(lines[i].line, lines[i].line_len, "value", &v) == 0) {
            if (v < min_val) min_val = v;
        }
    }
    return (min_val == INFINITY) ? 0.0 : min_val;
}
