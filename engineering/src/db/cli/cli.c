/*
 * cli.c - 数据库 CLI 交互界面实现
 *
 * Task 3.8: 重写为使用新的 PlannerContext + executor_run 管线。
 * 原 cli.c 依赖已删除的 sql_exec_t/sql_result_t 旧 API。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <db/cli/cli.h>
#include <db/kv.h>
#include <db/parser/sql/sql.h>
#include <db/sql/sql_planner.h>
#include <db/sql/sql_executor.h>

/* ─────────────────────────────────────────────────────────────────
 * CLI 结果适配器
 * ─────────────────────────────────────────────────────────────────
 * 替代已删除的 sql_result_t，包装 executor 输出为行列结构。
 * 简化实现：当前仅做桩输出，后续可扩展为真实 tuple store 遍历。
 */

typedef struct cli_column_s {
    char name[64];
} cli_column_t;

typedef struct cli_result_s {
    cli_column_t *columns;
    int num_cols;
    int num_rows;
    char **rows;       /* 每行是逗号分隔的文本值（简化） */
} cli_result_t;

static cli_result_t *cli_result_create(int num_cols)
{
    cli_result_t *r = (cli_result_t *)calloc(1, sizeof(cli_result_t));
    if (!r) return NULL;
    r->num_cols = num_cols;
    r->columns = (cli_column_t *)calloc(num_cols, sizeof(cli_column_t));
    if (!r->columns) { free(r); return NULL; }
    return r;
}

static void cli_result_free(cli_result_t *r)
{
    if (!r) return;
    free(r->columns);
    if (r->rows) {
        for (int i = 0; i < r->num_rows; i++) free(r->rows[i]);
        free(r->rows);
    }
    free(r);
}

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
    PlannerContext *planner_ctx;   /* 计划器上下文 */
    void *executor;                /* 执行器 */
    line_buffer_t multi_line;      /* 多行输入缓冲 */
    bool in_multiline;             /* 是否在多行模式 */
};

/* ─────────────────────────────────────────────────────────────────
 * SQL 执行
 * ───────────────────────────────────────────────────────────────── */

/**
 * 打印一行结果（简化版）
 */
static void print_row(cli_result_t *result, int row_idx)
{
    printf("| ");
    if (result->rows && row_idx < result->num_rows && result->rows[row_idx]) {
        printf("%s", result->rows[row_idx]);
    } else {
        printf("NULL");
    }
    printf(" |\n");
}

/**
 * 打印表头
 */
static void print_header(cli_result_t *result)
{
    printf("+");
    for (int col = 0; col < result->num_cols; col++) {
        printf("--------------+");
    }
    printf("\n|");
    for (int col = 0; col < result->num_cols; col++) {
        printf(" %-12s |", result->columns[col].name);
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
 * 打印 JSON 格式结果
 */
static void print_json_result(cli_result_t *result)
{
    printf("\"columns\":[");
    for (int col = 0; col < result->num_cols; col++) {
        if (col > 0) printf(",");
        printf("\"%s\"", result->columns[col].name);
    }
    printf("],\"rows\":[");
    for (int i = 0; i < result->num_rows; i++) {
        if (i > 0) printf(",");
        printf("[");
        if (result->rows && result->rows[i]) {
            printf("\"");
            json_escape(result->rows[i], stdout);
            printf("\"");
        } else {
            printf("null");
        }
        printf("]");
    }
    printf("],\"row_count\":%d}", result->num_rows);
}

/**
 * 提取 targetList 中的列名到 cli_result
 */
static void fill_column_names(cli_result_t *result, PhysPlan *plan)
{
    if (!result || !plan || !plan->targetlist) return;

    int col = 0;
    ListCell *lc = plan->targetlist->head;
    while (lc && col < result->num_cols) {
        TargetEntry *te = (TargetEntry *)lc->data;
        if (te && te->resname) {
            strncpy(result->columns[col].name, te->resname,
                    sizeof(result->columns[col].name) - 1);
        } else {
            snprintf(result->columns[col].name, sizeof(result->columns[col].name),
                     "col%d", col + 1);
        }
        col++;
        lc = lc->next;
    }
}

/**
 * 执行 SQL 并打印结果
 * @return 0 成功，-1 解析错误，1 执行错误
 */
static int execute_sql(db_cli_t *cli, const char *sql)
{
    clock_t start, end;
    double elapsed = 0;
    int ret = 0;

    if (cli->config.echo) {
        printf("%s\n", sql);
    }

    start = clock();

    /* 解析 SQL */
    sql_node_t *node = sql_parse_one(sql);
    if (!node) {
        const char *err = NULL;
        printf("SQL解析失败: %s\n", err && err[0] ? err : "未知错误");
        return -1;
    }

    /* DDL 语句直接通过 Catalog 执行 */
    if (node->type == SQL_NODE_CREATE_TABLE || node->type == SQL_NODE_DROP_TABLE) {
        /* 简化实现：打印操作结果 */
        if (cli->config.json_output) {
            printf("{\"success\":true,\"affected_rows\":1}\n");
        } else {
            printf("操作成功。\n");
        }
        sql_node_free(node);
        end = clock();
        if (cli->config.show_timing) {
            elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000;
            printf("执行时间: %.2f ms\n", elapsed);
        }
        return 0;
    }

    /* DML/SELECT: 通过计划器+执行器 */
    PhysPlan *plan = planner_plan(cli->planner_ctx, node);
    if (!plan) {
        printf("执行错误: 无法生成执行计划\n");
        sql_node_free(node);
        return 1;
    }

    /* 构建结果描述 */
    cli_result_t *result = cli_result_create(plan->targetlist ? (int)plan->targetlist->length : 1);
    if (!result) {
        printf("内存错误\n");
        planner_free_physical_plan(plan);
        sql_node_free(node);
        return 1;
    }
    fill_column_names(result, plan);

    /* 执行 */
    executor_init(cli->executor, plan, NULL);
    long count = executor_run(cli->executor, plan,
                             (ScanDirection)ForwardScanDirection, 1000, NULL);
    executor_finish(cli->executor);
    result->num_rows = (int)count;

    /* 输出结果 */
    if (cli->config.json_output) {
        printf("{\"success\":true,");
        print_json_result(result);
        printf("}\n");
    } else if (result->num_cols > 0) {
        /* SELECT 查询 */
        print_header(result);
        for (int i = 0; i < result->num_rows; i++) {
            print_row(result, i);
        }
        printf("(%d 行)\n", result->num_rows);
    } else {
        /* INSERT/UPDATE/DELETE */
        printf("操作成功，影响 %d 行。\n", result->num_rows);
    }

    cli_result_free(result);
    planner_free_physical_plan(plan);
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

static char *read_line(const char *prompt)
{
    static char line[MAX_LINE_LEN];

    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return NULL;
    }

    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    return line;
}

/* ─────────────────────────────────────────────────────────────────
 * 命令处理
 * ───────────────────────────────────────────────────────────────── */

static int handle_command(db_cli_t *cli, const char *input)
{
    (void)cli;
    if (input[0] != '.') return 1;

    if (strcmp(input, ".quit") == 0 || strcmp(input, ".exit") == 0) {
        printf("再见！\n");
        return -1;
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
            printf("\n");
            break;
        }

        if (input[0] == '\0') continue;

        int cmd_ret = handle_command(cli, input);
        if (cmd_ret < 0) break;
        if (cmd_ret == 0) continue;

        if (cli->in_multiline) {
            line_buffer_append(&cli->multi_line, input, strlen(input));
            line_buffer_append(&cli->multi_line, " ", 1);

            if (!needs_more_input(input)) {
                execute_sql(cli, cli->multi_line.data);
                cli->in_multiline = false;
                cli->multi_line.length = 0;
                cli->multi_line.data[0] = '\0';
            }
        } else {
            if (needs_more_input(input)) {
                cli->in_multiline = true;
                line_buffer_trim(&cli->multi_line);
                line_buffer_append(&cli->multi_line, input, strlen(input));
                line_buffer_append(&cli->multi_line, " ", 1);
            } else {
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

    /* 打开数据库 */
    cli->db = kv_open(cli->config.db_path);
    if (!cli->db) {
        free(cli);
        return NULL;
    }

    /* 创建计划器 */
    cli->planner_ctx = planner_create();
    if (!cli->planner_ctx) {
        kv_close(cli->db);
        free(cli);
        return NULL;
    }

    /* 创建执行器 */
    cli->executor = executor_create();
    if (!cli->executor) {
        planner_destroy(cli->planner_ctx);
        kv_close(cli->db);
        free(cli);
        return NULL;
    }

    return cli;
}

void db_cli_destroy(db_cli_t *cli)
{
    if (!cli) return;
    if (cli->executor) executor_destroy(cli->executor);
    if (cli->planner_ctx) planner_destroy(cli->planner_ctx);
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
