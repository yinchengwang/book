#include "workbench_metrics.h"
#include "metrics.h"
#include <string.h>

void workbench_metrics_init(void) {
    /* 初始化 workbench 专用 gauge（注意：metrics_set 必须以 NULL 终止可变参数） */
    metrics_set("jwt_keyring_count", 0, (const char *)NULL);
    metrics_set("workbench_cards_total", 0, "kind", "bug", (const char *)NULL);
    metrics_set("workbench_cards_total", 0, "kind", "learning", (const char *)NULL);
}

/* DB 操作埋点 helper：记录 db_query_total + db_query_duration_seconds */
void workbench_metrics_db_observe(const char *op, long long elapsed_us, int ok) {
    metrics_inc("db_query_total", 1, "op", op, "status", ok ? "ok" : "error");
    metrics_observe("db_query_duration_seconds", elapsed_us / 1e6, "op", op);
}

void handle_workbench_metrics(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    char buf[16384];
    metrics_render(buf, sizeof(buf));
    http_respond_json(resp, 200, buf);
}
