#ifndef WORKBENCH_METRICS_H
#define WORKBENCH_METRICS_H

#include "http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 workbench 专用指标 */
void workbench_metrics_init(void);

/* DB 操作埋点：记录 db_query_total + db_query_duration_seconds */
void workbench_metrics_db_observe(const char *op, long long elapsed_us, int ok);

/* GET /metrics handler（免鉴权） */
void handle_workbench_metrics(const HttpRequest *req, HttpResponse *resp);

#ifdef __cplusplus
}
#endif
#endif
