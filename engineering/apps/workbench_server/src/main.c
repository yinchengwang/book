/**
 * @file main.c
 * @brief workbench-server 独立 HTTP micro-service 入口（spec §5）
 *
 * Phase 4 Task 1：仅 /api/health 端点 + SQLite 连接 + JWT 配置校验。
 * Task 2 增量挂载 6 unit 业务路由。
 */

#include "http_server.h"
#include "http_router.h"
#include "db_pool.h"
#include "middleware.h"
#include "workbench_server_config.h"
#include "workbench_metrics.h"
#include "metrics.h"
#include "card_handler/card_handler.h"
#include "card_storage/card_migrate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static Router g_router;
static WorkbenchServerConfig g_cfg;

static void signal_handler(int sig) {
    (void)sig;
    printf("\n[workbench-server] shutting down...\n");
    http_server_stop();
}

/* /api/health — 免鉴权（spec §13 沿用 web-server 规则） */
static void handle_health(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"status\":\"ok\",\"version\":\"phase4\",\"db_open\":%s}",
             db_handle() != NULL ? "true" : "false");
    http_respond_json(resp, 200, buf);
}

/* Task 22-C：HTTP 请求计数 + 延迟直方图（metrics_inc + metrics_observe） */
static const char *status_str(int code) {
    switch (code) {
        case 200: return "200";
        case 201: return "201";
        case 204: return "204";
        case 301: return "301";
        case 302: return "302";
        case 400: return "400";
        case 401: return "401";
        case 403: return "403";
        case 404: return "404";
        case 409: return "409";
        case 500: return "500";
        case 502: return "502";
        case 503: return "503";
        default: return "other";
    }
}

/* W3C Trace Context：从 header 列表查 traceparent（OpenTelemetry 分布式追踪） */
static const char *find_traceparent(const HttpRequest *req) {
    if (!req) return NULL;
    for (int i = 0; i < req->header_count; i++) {
        if (strncasecmp(req->headers[i], "traceparent:", 11) == 0) {
            const char *v = req->headers[i] + 11;
            while (*v == ' ' || *v == '\t') v++;
            return v;
        }
    }
    return NULL;
}

static void request_callback(const HttpRequest *req, HttpResponse *resp) {
    /* Task 22-C：workbench-server HTTP 延迟直方图 */
    long long t0_us = metrics_time_us();

    /* OpenTelemetry W3C Trace Context：解析上游 traceparent，生成 child span */
    {
        w3c_trace_ctx_t trace_ctx;
        const char *tp = find_traceparent(req);
        if (tp && w3c_parse_traceparent(tp, &trace_ctx) == 0) {
            w3c_child_span_id(trace_ctx.span_id, sizeof(trace_ctx.span_id));
            fprintf(stderr, "[workbench-server] child span trace_id=%s span_id=%s path=%s\n",
                    trace_ctx.trace_id, trace_ctx.span_id, req->path);
        } else {
            char new_tp[W3C_TRACEPARENT_MAX];
            w3c_format_traceparent(new_tp, sizeof(new_tp));
            fprintf(stderr, "[workbench-server] new trace %s path=%s\n", new_tp, req->path);
        }
    }

    /* Phase 5 配置中心（方案 B）：每次请求前检查 mtime 文件，发现变化则 reload env
     *   初始值设为 -1 确保首次请求必触发 load（覆盖 stack 启动前 mtime 文件已存在的场景） */
    {
        static long long s_last_mtime = -1;
        const char *mtime_path = getenv("JWT_KEYRING_MTIME_PATH");
        if (mtime_path && *mtime_path) {
            long long mtime = jwt_keyring_read_mtime(mtime_path);
            if (mtime != s_last_mtime) {
                s_last_mtime = mtime;
                if (jwt_keyring_load_from_env() == 0) {
                    fprintf(stderr, "[workbench-server] keyring reloaded from env (mtime=%lld)\n", mtime);
                }
            }
        }
    }

    /* Phase 4 Task 3：JWT 鉴权中间件
     *  - 仅 /api/health 与 /api/auth/ 免鉴权
     *  - 其他 /api/cards/... 与 /api/stats/... 强制 Bearer token
     *  - 鉴权通过后写 user_id 到 req 供 handler 端 current_user_id() 读取 */
    if (strncmp(req->path, "/api/health", 11) != 0 &&
        strncmp(req->path, "/api/auth/", 10) != 0 &&
        strncmp(req->path, "/metrics", 8) != 0) {
        char user_id[64] = {0};
        if (require_auth(req, user_id, sizeof(user_id)) != 0) {
            http_respond_error(resp, 401, "unauthorized");
            long long elapsed = metrics_time_us() - t0_us;
            char norm_path[128]; metrics_normalize_path(req->path, norm_path, sizeof(norm_path));
            metrics_observe("http_request_duration_seconds", elapsed / 1e6, "method", "GET", "path", norm_path, (const char *)NULL);
            metrics_inc("http_requests_total", 1, "method", "GET", "path", norm_path, "status", status_str(resp->status_code), (const char *)NULL);
            return;
        }
        /* req 虽为 const，但实际是栈上 struct（http_server 同步栈帧），
         * 同步单线程场景下 const_cast 安全。 */
        memcpy(((HttpRequest *)req)->user_id, user_id, sizeof(user_id));
    }
    if (router_dispatch(&g_router, req, resp) != 0) {
        http_respond_error(resp, 404, "not found");
    }

    /* metrics：记录 workbench HTTP 延迟 + counter */
    long long elapsed = metrics_time_us() - t0_us;
    char norm_path[128]; metrics_normalize_path(req->path, norm_path, sizeof(norm_path));
    metrics_observe("http_request_duration_seconds", elapsed / 1e6, "method", "GET", "path", norm_path, (const char *)NULL);
    metrics_inc("http_requests_total", 1, "method", "GET", "path", norm_path, "status", status_str(resp->status_code), (const char *)NULL);
}

/**
 * @brief workbench-server 主流程（可被测试调用）
 */
int workbench_server_main(int argc, char **argv) {
    workbench_server_config_init_defaults(&g_cfg);
    if (workbench_server_config_parse_env(&g_cfg) != 0) return 1;
    if (workbench_server_config_parse_args(&g_cfg, argc, argv) != 0) return 1;
    if (workbench_server_config_validate(&g_cfg) != 0) return 1;

    if (db_open(g_cfg.db_path) != 0) {
        fprintf(stderr, "[workbench-server] failed to open db: %s\n", g_cfg.db_path);
        return 1;
    }

    /* Phase 4 Task 2：workbench schema 幂等创建（cards + 10 子表 + card_relations） */
    if (card_migrate_run(db_handle()) != 0) {
        fprintf(stderr, "[workbench-server] card_migrate_run failed\n");
        return 1;
    }

    /* Phase 1/2 状态机 install（全局注册 enum ↔ string 映射） */
    extern void bug_state_machine_install(void);
    extern void learning_state_machine_install(void);
    bug_state_machine_install();
    learning_state_machine_install();

    router_init(&g_router);
    router_add(&g_router, 'G', "/api/health", handle_health);
    router_add(&g_router, 'G', "/metrics", handle_workbench_metrics);

    /* 初始化 workbench 专用指标 */
    workbench_metrics_init();

    /* Phase 4 Task 2：card_handler 路由挂载（/api/cards + /api/stats 路径前缀） */
    if (card_handler_install(&g_router) != 0) {
        fprintf(stderr, "[workbench-server] card_handler_install failed\n");
        return 1;
    }

    signal(SIGINT, signal_handler);
    printf("[workbench-server] phase4 starting on port %d, db=%s\n",
           g_cfg.port, g_cfg.db_path);
    fflush(stdout);

    http_server_start(g_cfg.port, request_callback);

    router_free(&g_router);
    db_close();
    return 0;
}

int main(int argc, char **argv) {
    return workbench_server_main(argc, argv);
}
