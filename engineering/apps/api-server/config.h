#ifndef API_CONFIG_H
#define API_CONFIG_H

typedef struct {
    int port;
    char db_path[512];
    char static_dir[512];  /* knowledge_hub 构建产物目录 */
} ApiConfig;

extern ApiConfig g_config;

void config_init_defaults(void);
int config_parse_args(int argc, char *argv[]);

#endif /* API_CONFIG_H */
