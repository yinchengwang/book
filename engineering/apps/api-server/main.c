#include "../common/http_server.h"
#include "../common/http_router.h"
#include "../common/db_pool.h"
#include "config.h"
#include "modules/health.h"
#include "modules/todo_api.h"
#include "modules/notes_api_mod.h"
#include "modules/quiz_api.h"
#include "modules/interview_api.h"
#include "modules/digest_api.h"
#include "modules/review_api.h"
#include "modules/static_files.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static int g_running = 1;
static Router g_router;

static void signal_handler(int sig) {
    (void)sig;
    printf("\n[api-server] shutting down...\n");
    g_running = 0;
    http_server_stop();
}

static void request_callback(const HttpRequest *req, HttpResponse *resp) {
    if (router_dispatch(&g_router, req, resp) != 0) {
        /* 未命中 API 路由 → 尝试静态文件服务 */
        if (strncmp(req->path, "/api/", 5) != 0) {
            if (static_files_serve(req, resp, g_config.static_dir) == 0) {
                return;
            }
        }
        http_respond_error(resp, 404, "not found");
    }
}
int main(int argc, char *argv[]) {
    config_init_defaults();
    if (config_parse_args(argc, argv) != 0) {
        return 1;
    }

    if (db_open(g_config.db_path) != 0) {
        fprintf(stderr, "[api-server] failed to open database: %s\n", g_config.db_path);
        return 1;
    }

    router_init(&g_router);
    register_health_routes(&g_router);
    register_todo_routes(&g_router);
    register_notes_routes(&g_router, "learning/notes");
    register_quiz_routes(&g_router);
    register_interview_routes(&g_router);
    register_digest_routes(&g_router);
    register_review_routes(&g_router);
    register_static_files(&g_router, g_config.static_dir);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("\n");
    printf("  ==================================\n");
    printf("   Knowledge Hub API Server\n");
    printf("   Port: %d\n", g_config.port);
    printf("   DB:   %s\n", g_config.db_path);
    printf("  ==================================\n");
    printf("\n");

    http_server_start(g_config.port, request_callback);

    router_free(&g_router);
    db_close();
    printf("[api-server] shutdown complete\n");
    return 0;
}
