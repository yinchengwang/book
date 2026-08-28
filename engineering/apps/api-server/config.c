#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

ApiConfig g_config;

void config_init_defaults(void) {
    g_config.port = 8080;
    /* 默认数据库路径：统一到 backend/data/sqlite/（Task 13） */
    strncpy(g_config.db_path, "backend/data/sqlite/book.db", sizeof(g_config.db_path) - 1);
    /* 默认静态目录：knowledge_hub 构建产物（Task 13） */
    strncpy(g_config.static_dir, "frontend/apps/knowledge_hub/web/dist", sizeof(g_config.static_dir) - 1);
}

int config_parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            g_config.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            strncpy(g_config.db_path, argv[++i], sizeof(g_config.db_path) - 1);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            strncpy(g_config.static_dir, argv[++i], sizeof(g_config.static_dir) - 1);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: api-server [-p port] [-d db_path] [-s static_dir]\n");
            return -1;
        }
    }
    return 0;
}