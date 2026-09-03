/**
 * @file cli_main.c
 * @brief CLI 主程序入口（独立编译，不包含在库中）
 */
#include <stdio.h>
#include <string.h>
#include "cli.h"

int main(int argc, char *argv[])
{
    db_cli_config_t config = DB_CLI_DEFAULT_CONFIG;
    const char *sql_cmd = NULL;
    const char *sql_file = NULL;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            sql_file = argv[++i];
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            sql_cmd = argv[++i];
        }
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config.db_path = argv[++i];
        }
        else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
            config.json_output = true;
            config.echo = false;
            config.show_timing = false;
        }
        else if (strcmp(argv[i], "-h") == 0) {
            printf("用法: db_cli [选项]\n");
            printf("选项:\n");
            printf("  -f FILE    执行 SQL 文件\n");
            printf("  -c SQL     执行单条 SQL\n");
            printf("  -d PATH    数据库文件路径\n");
            printf("  -j, --json JSON 输出模式\n");
            printf("  -h        显示帮助\n");
            return 0;
        }
    }

    /* 执行模式 */
    if (sql_file) {
        /* 文件模式 */
        FILE *fp = fopen(sql_file, "r");
        if (!fp) {
            printf("无法打开文件: %s\n", sql_file);
            return 1;
        }

        db_cli_t *cli = db_cli_create(&config);
        if (!cli) {
            fclose(fp);
            return 1;
        }

        char line[4096];
        while (fgets(line, sizeof(line), fp)) {
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            if (line[0] == '#' || line[0] == '\0') continue;
            db_cli_exec(cli, line);
        }

        db_cli_destroy(cli);
        fclose(fp);
        return 0;
    }
    else if (sql_cmd) {
        /* 单条命令模式 */
        db_cli_t *cli = db_cli_create(&config);
        if (!cli) {
            return 1;
        }
        int ret = db_cli_exec(cli, sql_cmd);
        db_cli_destroy(cli);
        return ret;
    }

    /* 交互模式 */
    db_cli_t *cli = db_cli_create(&config);
    if (!cli) {
        return 1;
    }
    int ret = db_cli_run(cli);
    db_cli_destroy(cli);
    return ret;
}
