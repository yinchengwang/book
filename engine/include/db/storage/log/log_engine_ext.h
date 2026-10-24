#ifndef DB_LOG_ENGINE_EXT_H
#define DB_LOG_ENGINE_EXT_H

#include "db/log_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C6.2 LogQL */
int log_parse_logql_selector(const char *selector,
                           char ***out_labels, char ***out_values, int *out_n);
void log_free_logql_parsed(char **labels, char **values, int n);

/* C6.4 WAL */
int log_push_wal(void *wal, const log_labels_t *labels,
                const log_line_t *lines, size_t n_lines,
                uint64_t stream_id);

/* C6.5 TTL drop */
int logEngine_drop_expired(log_engine_t *engine, int64_t ttl_ms);

#ifdef __cplusplus
}
#endif

#endif
