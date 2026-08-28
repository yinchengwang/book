#ifndef WORKBENCH_SERVER_CONFIG_H
#define WORKBENCH_SERVER_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief workbench-server 启动配置（spec §8）
 */
typedef struct {
    int  port;                       /* 监听端口（默认 5801） */
    char jwt_secret[256];            /* JWT HMAC 密钥（兼容占位，spec §14 v4：仍可作为 fallback，
                                     *   主路径为 JWT_SECRETS，apps/common/middleware.c 加载） */
    char jwt_secrets[1024];          /* JWT 多密钥集合（kid1:secret1,kid2:secret2,...，主路径） */
    char db_path[512];               /* SQLite 文件路径（默认 todo-app.db） */
    int  log_level;                  /* 0=info, 1=debug */
} WorkbenchServerConfig;

/* 初始化默认值（port=5801, jwt_secret="", db_path="todo-app.db", log_level=0） */
void workbench_server_config_init_defaults(WorkbenchServerConfig *cfg);

/* 从环境变量读 JWT_SECRET / WORKBENCH_SERVER_PORT / WORKBENCH_DB_PATH */
int  workbench_server_config_parse_env(WorkbenchServerConfig *cfg);

/* 解析命令行参数（覆盖 env）：--port=5801 --jwt-secret=xxx --db=xxx --log-level=1 */
int  workbench_server_config_parse_args(WorkbenchServerConfig *cfg, int argc, char **argv);

/* 校验配置：jwt_secret 非空 + port 范围合法 + db_path 非空 */
int  workbench_server_config_validate(const WorkbenchServerConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif /* WORKBENCH_SERVER_CONFIG_H */