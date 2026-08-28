#include "db/log_engine.h"
#include "db/core/log.h"
#include <math.h>

/* C3-3 T8: 聚合算子 */
double log_aggregate_count(const log_line_t *lines, size_t n) {
    return (double)n;
}

double log_aggregate_sum(const log_line_t *lines, size_t n) {
    /* 占位：value 字段需从 line 中提取（当前 line 是原始字符串） */
    (void)lines; (void)n;
    return 0.0;
}

double log_aggregate_avg(const log_line_t *lines, size_t n) {
    double sum = log_aggregate_sum(lines, n);
    return n > 0 ? sum / (double)n : 0.0;
}

double log_aggregate_max(const log_line_t *lines, size_t n) {
    (void)lines; (void)n;
    return -INFINITY;
}

double log_aggregate_min(const log_line_t *lines, size_t n) {
    (void)lines; (void)n;
    return INFINITY;
}
