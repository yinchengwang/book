#include "workbench_server_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void workbench_server_config_init_defaults(WorkbenchServerConfig *cfg) {
    if (cfg == NULL) return;
    cfg->port = 5801;
    cfg->jwt_secret[0] = '\0';
    cfg->jwt_secrets[0] = '\0';
    /* spec §14 Path C：workbench-server 走独立 DB 文件（workbench.db），保留 WAL mode，
     * 避免与 web-server 共享 todo-app.db 引发的 SQLite 锁竞争。环境变量 WORKBENCH_DB_PATH
     * 仍可覆盖（用于本地 dev / 集成测试）。 */
    strncpy(cfg->db_path, "workbench.db", sizeof(cfg->db_path) - 1);
    cfg->db_path[sizeof(cfg->db_path) - 1] = '\0';
    cfg->log_level = 0;
}

static int parse_int(const char *s, int *out) {
    if (s == NULL || *s == '\0') return -1;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || (*end != '\0' && *end != ' ')) return -1;
    *out = (int)v;
    return 0;
}

int workbench_server_config_parse_env(WorkbenchServerConfig *cfg) {
    if (cfg == NULL) return -1;
    const char *env;
    if ((env = getenv("WORKBENCH_SERVER_PORT")) != NULL) {
        int p; if (parse_int(env, &p) != 0 || p <= 0 || p > 65535) {
            fprintf(stderr, "[config] invalid WORKBENCH_SERVER_PORT=%s\n", env);
            return -1;
        }
        cfg->port = p;
    }
    if ((env = getenv("JWT_SECRET")) != NULL) {
        strncpy(cfg->jwt_secret, env, sizeof(cfg->jwt_secret) - 1);
        cfg->jwt_secret[sizeof(cfg->jwt_secret) - 1] = '\0';
    }
    /* spec §14 v4：JWT_SECRETS 多密钥集合（主路径，apps/common/middleware.c 解析）。
     *   jwt_secret 字段保留作为兼容占位（env 缺失时回退）。 */
    if ((env = getenv("JWT_SECRETS")) != NULL) {
        strncpy(cfg->jwt_secrets, env, sizeof(cfg->jwt_secrets) - 1);
        cfg->jwt_secrets[sizeof(cfg->jwt_secrets) - 1] = '\0';
    }
    if ((env = getenv("WORKBENCH_DB_PATH")) != NULL) {
        strncpy(cfg->db_path, env, sizeof(cfg->db_path) - 1);
        cfg->db_path[sizeof(cfg->db_path) - 1] = '\0';
    }
    if ((env = getenv("WORKBENCH_LOG_LEVEL")) != NULL) {
        int lvl; if (parse_int(env, &lvl) == 0 && lvl >= 0 && lvl <= 2) {
            cfg->log_level = lvl;
        }
    }
    return 0;
}

int workbench_server_config_parse_args(WorkbenchServerConfig *cfg, int argc, char **argv) {
    if (cfg == NULL || argv == NULL) return -1;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strncmp(arg, "--port=", 7) == 0) {
            int p; if (parse_int(arg + 7, &p) != 0) return -1;
            cfg->port = p;
        } else if (strncmp(arg, "--jwt-secret=", 13) == 0) {
            strncpy(cfg->jwt_secret, arg + 13, sizeof(cfg->jwt_secret) - 1);
            cfg->jwt_secret[sizeof(cfg->jwt_secret) - 1] = '\0';
        } else if (strncmp(arg, "--db=", 5) == 0) {
            strncpy(cfg->db_path, arg + 5, sizeof(cfg->db_path) - 1);
            cfg->db_path[sizeof(cfg->db_path) - 1] = '\0';
        } else if (strncmp(arg, "--log-level=", 12) == 0) {
            int lvl; if (parse_int(arg + 12, &lvl) != 0) return -1;
            cfg->log_level = lvl;
        } else {
            fprintf(stderr, "[config] unknown arg: %s\n", arg);
            return -1;
        }
    }
    return 0;
}

int workbench_server_config_validate(const WorkbenchServerConfig *cfg) {
    if (cfg == NULL) return -1;
    /* spec §14 v4：JWT_SECRETS 主路径，JWT_SECRET 兼容回退 */
    if (cfg->jwt_secrets[0] == '\0' && cfg->jwt_secret[0] == '\0') {
        fprintf(stderr, "[config] JWT_SECRETS or JWT_SECRET must be set (env or --jwt-secret)\n");
        return -1;
    }
    if (cfg->port <= 0 || cfg->port > 65535) {
        fprintf(stderr, "[config] invalid port: %d\n", cfg->port);
        return -1;
    }
    if (cfg->db_path[0] == '\0') {
        fprintf(stderr, "[config] db_path empty\n");
        return -1;
    }
    return 0;
}
