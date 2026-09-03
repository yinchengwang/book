/*
 * cli.c - 数据库 CLI 交互界面实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <db/cli/cli.h>
#include <db/kv.h>
#include <db/parser/sql/sql.h>
#include <db/executor/sql/sql_exec.h>

/* ─────────────────────────────────────────────────────────────────
 * 行缓冲区
 * ───────────────────────────────────────────────────────────────── */

#define MAX_LINE_LEN 4096

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
} line_buffer_t;

static void line_buffer_init(line_buffer_t *buf)
{
    buf->capacity = 256;
    buf->data = (char *)malloc(buf->capacity);
    buf->length = 0;
    if (buf->data) buf->data[0] = '\0';
}

static void line_buffer_free(line_buffer_t *buf)
{
    if (buf->data) free(buf->data);
}

static void line_buffer_append(line_buffer_t *buf, const char *str, size_t len)
{
    if (buf->length + len + 1 > buf->capacity) {
        buf->capacity = (buf->length + len + 256) * 2;
        buf->data = (char *)realloc(buf->data, buf->capacity);
    }
    if (buf->data) {
        memcpy(buf->data + buf->length, str, len);
        buf->length += len;
        buf->data[buf->length] = '\0';
    }
}

static void line_buffer_trim(line_buffer_t *buf)
{
    while (buf->length > 0 && isspace((unsigned char)buf->data[buf->length - 1])) {
        buf->data[--buf->length] = '\0';
    }
}

/* ─────────────────────────────────────────────────────────────────
 * CLI 内部结构
 * ───────────────────────────────────────────────────────────────── */

struct db_cli_s {
    db_cli_config_t config;
    kv_t *db;                      /* KV 数据库句柄 */
    sql_exec_t *exec;              /* SQL 执行器 */
    line_buffer_t multi_line;      /* 多行输入缓冲 */
    bool in_multiline;             /* 是否在多行模式 */
};

/* ─────────────────────────────────────────────────────────────────
 * SQL 执行
 * ───────────────────────────────────────────────────────────────── */

/**
 * 打印一行结果
 */
static void print_row(const sql_result_t *result, size_t row_idx)
{
    size_t num_cols = sql_result_num_columns(result);
    printf("| ");
    for (size_t col = 0; col < num_cols; col++) {
        void *values = NULL;
        sql_result_get_row(result, row_idx, &values);
        if (values) {
            printf("%s", (char *)values);
            free(values);
        } else {
            printf("NULL");
        }
        printf(" |");
    }
    printf("\n");
}

/**
 * 打印表头
 */
static void print_header(const sql_result_t *result)
{
    size_t num_cols = sql_result_num_columns(result);
    printf("+");
    for (size_t col = 0; col < num_cols; col++) {
        printf("--------------+");
    }
    printf("\n|");
    for (size_t col = 0; col < num_cols; col++) {
        const char *name = sql_result_column_name(result, col);
        printf(" %-12s |", name ? name : "?");
    }
    printf("\n");
}

/**
 * JSON 转义字符串
 */
static void json_escape(const char *str, FILE *fp)
{
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  fprintf(fp, "\\\""); break;
            case '\\': fprintf(fp, "\\\\"); break;
            case '\n': fprintf(fp, "\\n"); break;
            case '\r': fprintf(fp, "\\r"); break;
            case '\t': fprintf(fp, "\\t"); break;
            default:   fprintf(fp, "%c", *p); break;
        }
    }
}

/**
 * 打印 JSON 格式结果（不包含外层大括号和success字段）
 */
static void print_json_result(const sql_result_t *result, size_t num_rows)
{
    size_t num_cols = sql_result_num_columns(result);
    printf("\"columns\":[");
    for (size_t col = 0; col < num_cols; col++) {
        const char *name = sql_result_column_name(result, col);
        if (col > 0) printf(",");
        printf("\"%s\"", name ? name : "");
    }
    printf("],\"rows\":[");
    for (size_t i = 0; i < num_rows; i++) {
        if (i > 0) printf(",");
        printf("[");
        for (size_t col = 0; col < num_cols; col++) {
            void *values = NULL;
            sql_result_get_row(result, i, &values);
            if (col > 0) printf(",");
            if (values) {
                printf("\"");
                json_escape((char *)values, stdout);
                printf("\"");
                free(values);
            } else {
                printf("null");
            }
        }
        printf("]");
    }
    printf("],\"row_count\":%zu}", num_rows);
}

/**
 * 执行 SQL 并打印结果
 * @return 0 成功，-1 解析错误，1 执行错误
 */
static int execute_sql(db_cli_t *cli, const char *sql)
{
    clock_t start, end;
    double elapsed = 0;
    int ret = 0;  /* 默认成功 */

    if (cli->config.echo) {
        printf("%s\n", sql);
    }

    start = clock();

    /* 解析 SQL */
    sql_node_t *node = sql_parse_one(sql);
    if (!node) {
        const char *err = sql_get_last_parse_error();
        printf("SQL解析失败: %s\n", err && err[0] ? err : "未知错误");
        return -1;
    }

    /* JSON 输出模式 */
    if (cli->config.json_output) {
        if (node->type == SQL_NODE_CREATE_TABLE || node->type == SQL_NODE_DROP_TABLE) {
            sql_exec_result_t exec_ret = sql_exec_ddl(cli->exec, node);
            if (exec_ret == SQL_EXEC_OK) {
                printf("{\"success\":true,\"affected_rows\":1}\n");
            } else {
                printf("{\"success\":false,\"error\":\"%s\"}\n", sql_exec_errmsg(cli->exec));
                ret = 1;
            }
        } else {
            sql_result_t *result = sql_exec(cli->exec, node);
            if (!result) {
                printf("{\"success\":false,\"error\":\"%s\"}\n", sql_exec_errmsg(cli->exec));
                ret = 1;
            } else {
                size_t num_rows = sql_result_num_rows(result);
                printf("{\"success\":true,");
                print_json_result(result, num_rows);
                sql_result_free(result);
            }
        }
        sql_node_free(node);
        return ret;
    }

    /* 普通模式 */
    if (node->type == SQL_NODE_CREATE_TABLE || node->type == SQL_NODE_DROP_TABLE) {
        /* DDL */
        sql_exec_result_t exec_ret = sql_exec_ddl(cli->exec, node);
        if (exec_ret == SQL_EXEC_OK) {
            printf("操作成功。\n");
        } else {
            printf("执行错误: %s\n", sql_exec_errmsg(cli->exec));
            ret = 1;
        }
    } else {
        /* DML */
        sql_result_t *result = sql_exec(cli->exec, node);
        if (!result) {
            printf("执行错误: %s\n", sql_exec_errmsg(cli->exec));
            ret = 1;
        } else {
            size_t num_cols = sql_result_num_columns(result);
            size_t num_rows = sql_result_num_rows(result);

            if (num_cols > 0) {
                /* SELECT 查询 */
                print_header(result);
                for (size_t i = 0; i < num_rows; i++) {
                    print_row(result, i);
                }
                printf("(%zu 行)\n", num_rows);
            } else {
                /* INSERT/UPDATE/DELETE */
                printf("操作成功，影响 %zu 行。\n", num_rows);
            }

            sql_result_free(result);
        }
    }

    sql_node_free(node);

    end = clock();
    if (cli->config.show_timing) {
        elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        printf("执行时间: %.2f ms\n", elapsed);
    }

    return ret;
}

/* ─────────────────────────────────────────────────────────────────
 * 行读取
 * ───────────────────────────────────────────────────────────────── */

/**
 * 检查是否需要继续多行输入
 */
static bool needs_more_input(const char *line)
{
    int depth = 0;
    bool in_string = false;

    for (const char *p = line; *p; p++) {
        if (in_string) {
            if (*p == '\\') { p++; continue; }
            if (*p == '\'') in_string = false;
            continue;
        }
        if (*p == '\'') {
            in_string = true;
        } else if (*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if (*p == ')' || *p == ']' || *p == '}') {
            depth--;
        }
    }
    return depth > 0 || in_string;
}

/**
 * 读取一行输入
 */
static char *read_line(const char *prompt)
{
    static char line[MAX_LINE_LEN];

    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return NULL;
    }

    /* 去掉换行符 */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    return line;
}

/* ─────────────────────────────────────────────────────────────────
 * 命令处理
 * ───────────────────────────────────────────────────────────────── */

/**
 * 处理内置命令
 * @return 0 已处理，1 不是命令（交给 SQL 执行）
 */
static int handle_command(db_cli_t *cli, const char *input)
{
    (void)cli;
    if (input[0] != '.') return 1;

    if (strcmp(input, ".quit") == 0 || strcmp(input, ".exit") == 0) {
        printf("再见！\n");
        return -1;  /* 特殊返回值表示退出 */
    }

    if (strcmp(input, ".help") == 0) {
        db_cli_print_help();
        return 0;
    }

    if (strcmp(input, ".tables") == 0) {
        printf("(表列表功能待实现)\n");
        return 0;
    }

    if (strcmp(input, ".schema") == 0) {
        printf("(表结构功能待实现)\n");
        return 0;
    }

    if (strncmp(input, ".open ", 6) == 0) {
        printf("切换数据库暂不支持。\n");
        return 0;
    }

    printf("未知命令: %s\n", input);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 主循环
 * ───────────────────────────────────────────────────────────────── */

int db_cli_run(db_cli_t *cli)
{
    db_cli_print_welcome();

    line_buffer_init(&cli->multi_line);
    cli->in_multiline = false;

    while (1) {
        const char *prompt = cli->in_multiline ? ".. " : cli->config.prompt;
        char *input = read_line(prompt);

        if (!input) {
            /* EOF */
            printf("\n");
            break;
        }

        /* 跳过空行 */
        if (input[0] == '\0') continue;

        /* 处理内置命令 */
        int cmd_ret = handle_command(cli, input);
        if (cmd_ret < 0) break;  /* 退出 */
        if (cmd_ret == 0) continue;  /* 命令已处理 */

        /* 多行模式 */
        if (cli->in_multiline) {
            line_buffer_append(&cli->multi_line, input, strlen(input));
            line_buffer_append(&cli->multi_line, " ", 1);

            if (!needs_more_input(input)) {
                /* 多行输入结束 */
                execute_sql(cli, cli->multi_line.data);
                cli->in_multiline = false;
                cli->multi_line.length = 0;
                cli->multi_line.data[0] = '\0';
            }
        } else {
            if (needs_more_input(input)) {
                /* 开始多行模式 */
                cli->in_multiline = true;
                line_buffer_trim(&cli->multi_line);
                line_buffer_append(&cli->multi_line, input, strlen(input));
                line_buffer_append(&cli->multi_line, " ", 1);
            } else {
                /* 单行执行 */
                execute_sql(cli, input);
            }
        }
    }

    line_buffer_free(&cli->multi_line);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 单命令模式
 * ───────────────────────────────────────────────────────────────── */

int db_cli_exec(db_cli_t *cli, const char *sql)
{
    return execute_sql(cli, sql);
}

/* ─────────────────────────────────────────────────────────────────
 * 创建/销毁
 * ───────────────────────────────────────────────────────────────── */

db_cli_t *db_cli_create(const db_cli_config_t *config)
{
    db_cli_t *cli = (db_cli_t *)calloc(1, sizeof(db_cli_t));
    if (!cli) return NULL;

    if (config) {
        cli->config = *config;
    } else {
        cli->config.db_path = "./test.db";
        cli->config.prompt = "db> ";
        cli->config.history_size = 100;
        cli->config.echo = true;
        cli->config.show_timing = true;
    }

    /* 打开数据库（kv_open 会自动创建） */
    cli->db = kv_open(cli->config.db_path);
    if (!cli->db) {
        free(cli);
        return NULL;
    }

    /* 创建执行器 */
    cli->exec = sql_exec_create(cli->db);
    if (!cli->exec) {
        kv_close(cli->db);
        free(cli);
        return NULL;
    }

    return cli;
}

void db_cli_destroy(db_cli_t *cli)
{
    if (!cli) return;
    if (cli->exec) sql_exec_destroy(cli->exec);
    if (cli->db) kv_close(cli->db);
    free(cli);
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

void db_cli_print_welcome(void)
{
    printf("==============================================\n");
    printf("  Build My DB - 交互式 SQL Shell\n");
    printf("  输入 .help 查看帮助\n");
    printf("  输入 .quit 退出\n");
    printf("==============================================\n\n");
}

void db_cli_print_help(void)
{
    printf("内置命令:\n");
    printf("  .help      显示帮助信息\n");
    printf("  .quit      退出\n");
    printf("  .exit      退出\n");
    printf("  .tables    列出所有表\n");
    printf("  .schema    显示表结构\n");
    printf("  .open FILE 打开指定数据库\n\n");
    printf("SQL 示例:\n");
    printf("  CREATE TABLE users (id INT, name VARCHAR(100));\n");
    printf("  INSERT INTO users VALUES (1, 'Alice');\n");
    printf("  SELECT * FROM users WHERE id = 1;\n");
    printf("  UPDATE users SET name = 'Bob' WHERE id = 1;\n");
    printf("  DELETE FROM users WHERE id = 1;\n");
    printf("  DROP TABLE users;\n\n");
}

void db_cli_print_error(const char *msg)
{
    printf("错误: %s\n", msg);
}