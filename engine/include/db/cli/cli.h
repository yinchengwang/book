/*
 * cli.h - 数据库 CLI 交互界面
 */

#ifndef DB_CLI_H
#define DB_CLI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * CLI 配置
 * ───────────────────────────────────────────────────────────────── */

/** CLI 配置 */
typedef struct db_cli_config_s {
    const char *db_path;           /* 数据库路径 */
    const char *prompt;            /* 命令提示符 */
    size_t history_size;           /* 历史记录条数 */
    bool echo;                     /* 回显 SQL 语句 */
    bool show_timing;              /* 显示执行时间 */
    bool json_output;              /* JSON 输出模式（用于程序化调用） */
} db_cli_config_t;

/** 默认配置 */
#define DB_CLI_DEFAULT_CONFIG { \
    .db_path = "./test.db",       \
    .prompt = "db> ",             \
    .history_size = 100,          \
    .echo = true,                 \
    .show_timing = true,          \
    .json_output = false          \
}

/* ─────────────────────────────────────────────────────────────────
 * CLI 状态
 * ───────────────────────────────────────────────────────────────── */

typedef struct db_cli_s db_cli_t;

/* ─────────────────────────────────────────────────────────────────
 * CLI 创建/销毁
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建 CLI 实例
 * @param config 配置（NULL 使用默认配置）
 * @return CLI 句柄
 */
db_cli_t *db_cli_create(const db_cli_config_t *config);

/**
 * @brief 销毁 CLI 实例
 * @param cli CLI
 */
void db_cli_destroy(db_cli_t *cli);

/* ─────────────────────────────────────────────────────────────────
 * 交互模式
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 运行交互式 Shell
 * @param cli CLI
 * @return 0 正常退出
 */
int db_cli_run(db_cli_t *cli);

/**
 * @brief 单命令模式（执行一条 SQL 后退出）
 * @param cli CLI
 * @param sql SQL 语句
 * @return 0 成功
 */
int db_cli_exec(db_cli_t *cli, const char *sql);

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 打印欢迎信息
 */
void db_cli_print_welcome(void);

/**
 * @brief 打印帮助信息
 */
void db_cli_print_help(void);

/**
 * @brief 打印错误信息
 * @param msg 错误信息
 */
void db_cli_print_error(const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* DB_CLI_H */