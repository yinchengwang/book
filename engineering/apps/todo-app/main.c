#include "todo_handler.h"
#include "todo_model.h"
#include "todo_migration.h"
#include "todo_calendar.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static int g_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    printf("\n正在停止服务器...\n");
    g_running = 0;
    http_server_stop();
}

int main(int argc, char *argv[]) {
    int port = 8080;
    const char *db_path = "todo_app.json";

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) db_path = argv[++i];
    }

    /* 迁移旧数据（如果存在） */
    todo_migrate_from_legacy();

    /* 加载数据 */
    if (todo_db_load(db_path) != 0) {
        fprintf(stderr, "数据加载失败: %s\n", db_path);
        return 1;
    }

    todo_handler_init();
    calendar_init();
    /* calendar_timer_start();  TODO: 修复 CreateThread 在 Git Bash 环境下 segfault */

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Todo App 已启动\n");
    printf("  数据:   %s\n", db_path);
    printf("  服务器: http://localhost:%d\n", port);
    printf("  API:    http://localhost:%d/api/todos\n", port);
    printf("  统计:   http://localhost:%d/api/todos/stats\n", port);
    printf("  分组:   http://localhost:%d/api/groups\n", port);
    printf("  按 Ctrl+C 停止\n");
    fflush(stdout);

    /* 启动 HTTP 服务器（阻塞） */
    http_server_start(port);

    calendar_shutdown();  /* 服务器停止后清理 */
    todo_db_shutdown();
    return 0;
}
