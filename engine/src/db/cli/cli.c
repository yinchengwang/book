/*
 * cli.c - 数据库 CLI 交互界面实现
 *
 * T10 重接线：
 *   - 启动序列改为 canonical bootstrap（catalog/buf/heapam/btreeam/rel）
 *   - SQL 执行走 sql_driver.h 的 execute_sql(sql, NULL) → QueryResult*
 *   - 移除 vector_api / sql_exec_t 依赖
 *   - MMDB_ENABLE_RELATIONAL / MMDB_ENABLE_KV 模态守卫
 *   - KV 子命令（.kvput/.kvget/.kvdel/.kvstats）在 MMDB_ENABLE_KV 下懒打开
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <db/cli/cli.h>
#include <db/multimodal_config.h>
#ifdef MMDB_ENABLE_RELATIONAL
#include <db/sql/sql_driver.h>
#include <db/catalog.h>
#include <db/buf.h>
#include <db/heapam.h>
#include <db/btreeam.h>
#include <db/rel.h>
#endif
#ifdef MMDB_ENABLE_KV
#include <db/kv.h>
#endif

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
#ifdef MMDB_ENABLE_KV
    kv_t *kv;                     /* KV 数据库句柄（懒打开） */
#endif
    line_buffer_t multi_line;     /* 多行输入缓冲 */
    bool in_multiline;            /* 是否在多行模式 */
};

/* ─────────────────────────────────────────────────────────────────
 * QueryResult 打印辅助
 * ───────────────────────────────────────────────────────────────── */

/**
 * 打印一行结果（QueryResult 接口）
 */
static void print_qr_row(const QueryResult *r, int row_idx)
{
    if (!r || !r->rows || row_idx < 0 || row_idx >= r->nrows) return;
    printf("| ");
    for (int col = 0; col < r->ncols; col++) {
        const char *val = r->rows[row_idx][col];
        if (val) {
            printf("%s", val);
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
static void print_qr_header(const QueryResult *r)
{
    if (!r) return;
    printf("+");
    for (int col = 0; col < r->ncols; col++) {
        printf("--------------+");
    }
    printf("\n|");
    for (int col = 0; col < r->ncols; col++) {
        const char *name = (r->col_names && r->col_names[col]) ? r->col_names[col] : "?";
        printf(" %-12s |", name);
    }
    printf("\n");
}

/**
 * JSON 转义字符串
 */
static void json_escape(const char *str, FILE *fp)
{
    if (!str) return;
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
 * 打印 JSON 格式结果（不包含外层 success 字段）
 */
static void print_qr_json(const QueryResult *r)
{
    if (!r) return;
    printf("\"columns\":[");
    for (int col = 0; col < r->ncols; col++) {
        const char *name = (r->col_names && r->col_names[col]) ? r->col_names[col] : "";
        if (col > 0) printf(",");
        printf("\"%s\"", name);
    }
    printf("],\"rows\":[");
    for (int i = 0; i < r->nrows; i++) {
        if (i > 0) printf(",");
        printf("[");
        for (int col = 0; col < r->ncols; col++) {
            const char *val = r->rows[i][col];
            if (col > 0) printf(",");
            if (val) {
                printf("\"");
                json_escape(val, stdout);
                printf("\"");
            } else {
                printf("null");
            }
        }
        printf("]");
    }
    printf("],\"row_count\":%d}", r->nrows);
}

/* ─────────────────────────────────────────────────────────────────
 * SQL 执行（canonical execute_sql 包装）
 * ───────────────────────────────────────────────────────────────── */

/**
 * 执行 SQL 并打印结果（cli 内部包装，避开与 sql_driver.h 同名）
 *
 * @return 0 成功，-1 解析错误，1 执行错误
 */
static int cli_run_sql(db_cli_t *cli, const char *sql)
{
    clock_t start = 0, end = 0;
    double elapsed = 0;
    int ret = 0;  /* 默认成功 */

#ifdef MMDB_ENABLE_RELATIONAL
    if (cli->config.echo) {
        printf("%s\n", sql);
    }

    start = clock();

    QueryResult *r = execute_sql(sql, NULL);
    if (!r) {
        printf("执行错误: 内部错误（QueryResult 为 NULL）\n");
        return 1;
    }

    /* 错误优先 */
    if (r->error_msg) {
        printf("SQL解析/执行失败: %s\n", r->error_msg);
        ret = (r->nrows > 0 || r->ncols > 0) ? 1 : -1;
        FreeQueryResult(r);
        end = clock();
        if (cli->config.show_timing) {
            elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000;
            printf("执行时间: %.2f ms\n", elapsed);
        }
        return ret;
    }

    /* JSON 输出模式 */
    if (cli->config.json_output) {
        printf("{\"success\":true,");
        print_qr_json(r);
        printf("}\n");
        FreeQueryResult(r);
        return 0;
    }

    /* 普通模式 */
    if (r->ncols > 0 && r->nrows > 0) {
        /* SELECT 类查询 */
        print_qr_header(r);
        for (int i = 0; i < r->nrows; i++) {
            print_qr_row(r, i);
        }
        printf("(%d 行)\n", r->nrows);
    } else {
        /* DDL / INSERT / UPDATE / DELETE（无结果集） */
        printf("操作成功。\n");
    }

    FreeQueryResult(r);

    end = clock();
    if (cli->config.show_timing) {
        elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        printf("执行时间: %.2f ms\n", elapsed);
    }

    return ret;
#else
    (void)cli;
    (void)sql;
    printf("relational modality not enabled in this build\n");
    return 1;
#endif
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
 * KV 子命令（懒打开）
 * ───────────────────────────────────────────────────────────────── */

#ifdef MMDB_ENABLE_KV
/**
 * 懒打开 KV 句柄
 */
static kv_t *cli_kv_ensure_open(db_cli_t *cli)
{
    if (!cli) return NULL;
    if (cli->kv) return cli->kv;
    if (!cli->config.db_path) {
        fprintf(stderr, "错误: 未配置数据库路径\n");
        return NULL;
    }
    cli->kv = kv_open(cli->config.db_path);
    if (!cli->kv) {
        fprintf(stderr, "错误: 打开 KV 数据库失败 (%s)\n", cli->config.db_path);
    }
    return cli->kv;
}

/**
 * 分割 ".kvput KEY VALUE" / ".kvget KEY" 等指令参数
 * 返回 -1 错误；out_key/out_value 由调用方负责释放（free）
 */
static int split_kv_args(const char *input, char **out_key, char **out_value)
{
    if (!input || !out_key || !out_value) return -1;
    *out_key = NULL;
    *out_value = NULL;

    /* 跳过指令名 + 1 个空格 */
    const char *p = strchr(input, ' ');
    if (!p) return -1;
    while (*p == ' ') p++;
    if (*p == '\0') return -1;

    *out_key = strdup(p);
    if (!*out_key) return -1;

    /* 在 key 内部找第一个空格分隔 key 与 value */
    char *sp = strchr(*out_key, ' ');
    if (sp) {
        *sp = '\0';
        const char *vp = sp + 1;
        while (*vp == ' ') vp++;
        if (*vp) {
            *out_value = strdup(vp);
            if (!*out_value) {
                free(*out_key);
                *out_key = NULL;
                return -1;
            }
        }
    }
    return 0;
}
#endif /* MMDB_ENABLE_KV */

/* ─────────────────────────────────────────────────────────────────
 * 命令处理
 * ───────────────────────────────────────────────────────────────── */

/**
 * 处理内置命令
 * @return 0 已处理，1 不是命令（交给 SQL 执行）
 */
static int handle_command(db_cli_t *cli, const char *input)
{
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

#ifdef MMDB_ENABLE_KV
    /* KV dot-commands */
    if (strncmp(input, ".kvput ", 7) == 0) {
        char *key = NULL, *value = NULL;
        if (split_kv_args(input, &key, &value) != 0 || !key) {
            printf("用法: .kvput <key> <value>\n");
            free(key); free(value);
            return 0;
        }
        kv_t *kvh = cli_kv_ensure_open(cli);
        if (!kvh) { free(key); free(value); return 0; }
        kv_result_t rc = kv_put(kvh, key, strlen(key), value ? value : "",
                                value ? strlen(value) : 0);
        if (rc == KV_OK) {
            printf("OK\n");
        } else {
            printf("错误: kv_put 失败 (%s)\n", kv_errmsg(kvh));
        }
        free(key); free(value);
        return 0;
    }

    if (strncmp(input, ".kvget ", 7) == 0) {
        char *key = NULL, *value = NULL;
        if (split_kv_args(input, &key, &value) != 0 || !key) {
            printf("用法: .kvget <key>\n");
            free(key); free(value);
            return 0;
        }
        (void)value;
        kv_t *kvh = cli_kv_ensure_open(cli);
        if (!kvh) { free(key); free(value); return 0; }
        void *out = NULL;
        size_t out_len = 0;
        kv_result_t rc = kv_get(kvh, key, strlen(key), &out, &out_len);
        if (rc == KV_OK) {
            /* 不可打印字节用 '.' 替代 */
            printf("\"");
            for (size_t i = 0; i < out_len; i++) {
                unsigned char c = ((unsigned char *)out)[i];
                putchar((c >= 0x20 && c < 0x7f) ? c : '.');
            }
            printf("\"\n");
            free(out);
        } else if (rc == KV_NOT_FOUND) {
            printf("(nil)\n");
        } else {
            printf("错误: kv_get 失败 (%s)\n", kv_errmsg(kvh));
        }
        free(key); free(value);
        return 0;
    }

    if (strncmp(input, ".kvdel ", 7) == 0) {
        char *key = NULL, *value = NULL;
        if (split_kv_args(input, &key, &value) != 0 || !key) {
            printf("用法: .kvdel <key>\n");
            free(key); free(value);
            return 0;
        }
        (void)value;
        kv_t *kvh = cli_kv_ensure_open(cli);
        if (!kvh) { free(key); free(value); return 0; }
        kv_result_t rc = kv_delete(kvh, key, strlen(key));
        if (rc == KV_OK) {
            printf("OK（已删除）\n");
        } else if (rc == KV_NOT_FOUND) {
            printf("(nil)\n");
        } else {
            printf("错误: kv_delete 失败 (%s)\n", kv_errmsg(kvh));
        }
        free(key); free(value);
        return 0;
    }

    if (strcmp(input, ".kvstats") == 0) {
        kv_t *kvh = cli_kv_ensure_open(cli);
        if (!kvh) return 0;
        kv_stats_t st;
        kv_result_t rc = kv_stats(kvh, &st);
        if (rc == KV_OK) {
            printf("keys=%zu  size=%zu  pages=%zu  hit_rate=%.2f\n",
                   st.num_keys, st.total_size, st.page_count, st.cache_hit_rate);
        } else {
            printf("错误: kv_stats 失败 (%s)\n", kv_errmsg(kvh));
        }
        return 0;
    }
#endif /* MMDB_ENABLE_KV */

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
                cli_run_sql(cli, cli->multi_line.data);
                cli->in_multiline = false;
                cli->multi_line.length = 0;
                if (cli->multi_line.data) cli->multi_line.data[0] = '\0';
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
                cli_run_sql(cli, input);
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
    return cli_run_sql(cli, sql);
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

#ifdef MMDB_ENABLE_RELATIONAL
    /* canonical bootstrap: catalog → buf → heapam → btreeam → rel */
    if (catalog_init() != 0) {
        fprintf(stderr, "存储引擎初始化失败: catalog_init\n");
        free(cli);
        return NULL;
    }
    /* buf_init("") = 内存 buffer pool（无磁盘文件） */
    if (buf_init("") != 0) {
        fprintf(stderr, "存储引擎初始化失败: buf_init\n");
        catalog_shutdown();
        free(cli);
        return NULL;
    }
    if (heapam_init() != 0) {
        fprintf(stderr, "存储引擎初始化失败: heapam_init\n");
        buf_shutdown();
        catalog_shutdown();
        free(cli);
        return NULL;
    }
    if (btreeam_init() != 0) {
        fprintf(stderr, "存储引擎初始化失败: btreeam_init\n");
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
        free(cli);
        return NULL;
    }
    if (rel_init() != 0) {
        fprintf(stderr, "存储引擎初始化失败: rel_init\n");
        btreeam_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
        free(cli);
        return NULL;
    }
    /* KV 句柄懒打开；不预打开 */
#endif

    return cli;
}

void db_cli_destroy(db_cli_t *cli)
{
    if (!cli) return;
#ifdef MMDB_ENABLE_KV
    if (cli->kv) {
        kv_close(cli->kv);
        cli->kv = NULL;
    }
#endif
#ifdef MMDB_ENABLE_RELATIONAL
    /* canonical teardown: rel → btreeam → heapam → buf → catalog */
    rel_shutdown();
    btreeam_shutdown();
    heapam_shutdown();
    buf_shutdown();
    catalog_shutdown();
#endif
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
    printf("  .open FILE 打开指定数据库\n");
#ifdef MMDB_ENABLE_KV
    printf("  .kvput KEY VALUE  写入 KV\n");
    printf("  .kvget KEY        读取 KV\n");
    printf("  .kvdel KEY        删除 KV\n");
    printf("  .kvstats          显示 KV 统计\n");
#endif
    printf("\nSQL 示例:\n");
    printf("  CREATE TABLE users (id INT, name VARCHAR(100));\n");
    printf("  INSERT INTO users VALUES (1, 'Alice');\n");
    printf("  SELECT * FROM users WHERE id = 1;\n");
    printf("  UPDATE users SET name = 'Bob' WHERE id = 1;\n");
    printf("  DELETE FROM users WHERE id = 1;\n");
    printf("  DROP TABLE users;\n\n");
}

void db_cli_print_error(const char *msg)
{
    printf("错误: %s\n", msg ? msg : "(null)");
}
