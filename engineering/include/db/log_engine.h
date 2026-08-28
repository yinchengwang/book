/**
 * @file log_engine.h
 * @brief 可观测日志引擎接口（C3-3）
 */
#ifndef DB_LOG_ENGINE_H
#define DB_LOG_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct log_engine_s log_engine_t;
typedef struct log_labels_s {
    const char **keys;
    const char **values;
    size_t n_labels;
} log_labels_t;

typedef struct log_line_s {
    int64_t timestamp;
    const char *line;
    size_t line_len;
} log_line_t;

log_engine_t *log_engine_create(const char *data_dir);
log_engine_t *log_engine_open(const char *data_dir);
void log_engine_close(log_engine_t *engine);

int log_push(log_engine_t *engine, const log_labels_t *labels,
             const log_line_t *lines, size_t n_lines);

int log_query(log_engine_t *engine,
              const char *selector,  /* {k="v"} | label */
              const char *filter,    /* |= "keyword" */
              int64_t start_ms, int64_t end_ms,
              log_line_t *out, size_t max_out, size_t *out_count);

int log_rate(log_engine_t *engine,
             const char *selector,
             int64_t start_ms, int64_t end_ms, int64_t step_ms,
             double *out_values, size_t max_out, size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* DB_LOG_ENGINE_H */