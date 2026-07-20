/**
 * @file vdb_cli.c
 * @brief MiniVecDB CLI 工具实现
 *
 * 支持命令：
 * - 集合管理: create, drop, list, info
 * - 向量操作: insert, insert-batch, query, delete
 * - 批量操作: import, export, benchmark
 * - 交互模式: shell
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include "vdb_cli.h"
#include "db_sdk.h"

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define MAX_LINE_LEN 4096
#define MAX_ARGS 64
#define DEFAULT_DIM 128
#define DEFAULT_TOP_K 10
#define DEFAULT_BENCHMARK_COUNT 1000

/* 颜色定义 */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** CLI 内部状态 */
struct vdb_cli_s {
    vdb_cli_config_t config;          /**< 配置 */
    db_sdk_t *db;                     /**< 数据库句柄（通过 SDK） */
    vdb_output_format_t output_format; /**< 输出格式 */
    char *last_collection;            /**< 上次使用的集合名 */
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/** 打印带颜色的文本 */
static void print_color(const char *color, const char *fmt, ...) {
    va_list args;
    printf("%s", color);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("%s", COLOR_RESET);
}

/** 获取当前时间（毫秒） */
static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/** 跳过空白字符 */
static const char *skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/** 解析逗号分隔的浮点数数组 */
static float *parse_float_array(const char *str, int32_t *out_count) {
    if (!str || !*str) {
        *out_count = 0;
        return NULL;
    }

    /* 第一次遍历：计数 */
    int32_t count = 1;
    for (const char *p = str; *p; p++) {
        if (*p == ',') count++;
    }

    float *arr = (float *)malloc(count * sizeof(float));
    if (!arr) {
        *out_count = 0;
        return NULL;
    }

    /* 第二次遍历：解析 */
    char *copy = strdup(str);
    char *token = strtok(copy, ",");
    int32_t i = 0;
    while (token && i < count) {
        arr[i++] = (float)strtod(token, NULL);
        token = strtok(NULL, ",");
    }
    free(copy);

    *out_count = i;
    return arr;
}

/** 解析 JSON 元数据（简化版） */
static void *parse_json_metadata(const char *str, int32_t *out_size) {
    if (!str || !*str) {
        *out_size = 0;
        return NULL;
    }
    size_t len = strlen(str);
    void *data = malloc(len + 1);
    if (data) {
        memcpy(data, str, len + 1);
        *out_size = (int32_t)(len + 1);
    }
    return data;
}

/* ========================================================================
 * JSON 输出辅助
 * ======================================================================== */

static void json_escape_string(const char *str) {
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:   printf("%c", *p); break;
        }
    }
}

static void print_json_start(bool success, const char *cmd) {
    printf("{\"success\":%s,\"command\":\"%s\",", success ? "true" : "false", cmd);
}

static void print_json_end(void) {
    printf("}\n");
}

/* ========================================================================
 * 表格输出辅助
 * ======================================================================== */

static void print_table_header(const char **cols, int n) {
    printf("+");
    for (int i = 0; i < n; i++) {
        printf("------------------+");
    }
    printf("\n|");
    for (int i = 0; i < n; i++) {
        printf(" %-15s |", cols[i] ? cols[i] : "");
    }
    printf("\n");
    printf("+");
    for (int i = 0; i < n; i++) {
        printf("------------------+");
    }
    printf("\n");
}

static void print_table_row(const char **vals, int n) {
    printf("|");
    for (int i = 0; i < n; i++) {
        printf(" %-15s |", vals[i] ? vals[i] : "");
    }
    printf("\n");
}

/* ========================================================================
 * 命令实现：create
 * ======================================================================== */

static int cmd_create(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;
    int32_t dimension = DEFAULT_DIM;
    db_sdk_index_type_t index_type = DB_SDK_INDEX_HNSW;
    db_sdk_metric_type_t metric_type = DB_SDK_METRIC_L2;
    int32_t ef_search = 100;
    int32_t index_m = 16;

    /* 解析参数 */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--dim") == 0 && i + 1 < argc) {
            dimension = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            const char *idx = argv[++i];
            if (strcmp(idx, "hnsw") == 0) index_type = DB_SDK_INDEX_HNSW;
            else if (strcmp(idx, "ivf") == 0) index_type = DB_SDK_INDEX_IVF;
            else if (strcmp(idx, "brute") == 0 || strcmp(idx, "brute_force") == 0)
                index_type = DB_SDK_INDEX_BRUTE_FORCE;
            else if (strcmp(idx, "auto") == 0) index_type = DB_SDK_INDEX_AUTO;
        } else if (strcmp(argv[i], "--metric") == 0 && i + 1 < argc) {
            const char *met = argv[++i];
            if (strcmp(met, "l2") == 0) metric_type = DB_SDK_METRIC_L2;
            else if (strcmp(met, "cosine") == 0) metric_type = DB_SDK_METRIC_COSINE;
            else if (strcmp(met, "ip") == 0) metric_type = DB_SDK_METRIC_IP;
        } else if (strcmp(argv[i], "--ef-search") == 0 && i + 1 < argc) {
            ef_search = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--m") == 0 && i + 1 < argc) {
            index_m = atoi(argv[++i]);
        }
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    if (dimension <= 0 || dimension > 65536) {
        vdb_cli_print_error("维度必须在 1-65536 之间");
        return 1;
    }

    db_sdk_collection_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.name, collection, sizeof(config.name) - 1);
    config.dimension = dimension;
    config.index_type = index_type;
    config.metric_type = metric_type;
    config.index_ef_search = ef_search;
    config.index_m = index_m;

    int rc = db_sdk_create_collection(cli->db, collection, &config);

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(rc == DB_SDK_OK, "create");
        if (rc == DB_SDK_OK) {
            printf("\"collection\":\"%s\",\"dimension\":%d", collection, dimension);
        } else {
            printf("\"error\":\"%s\"", db_sdk_error(cli->db));
        }
        print_json_end();
    } else {
        if (rc == DB_SDK_OK) {
            printf("✓ 集合创建成功: %s (dim=%d)\n", collection, dimension);
        } else if (rc == DB_SDK_ERR_EXISTS) {
            printf("集合已存在: %s\n", collection);
        } else {
            vdb_cli_print_error(db_sdk_error(cli->db));
        }
    }

    return rc == DB_SDK_OK ? 0 : 1;
}

/* ========================================================================
 * 命令实现：drop
 * ======================================================================== */

static int cmd_drop(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;
    bool force = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force = true;
        }
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    int rc = db_sdk_drop_collection(cli->db, collection);

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(rc == DB_SDK_OK, "drop");
        if (rc == DB_SDK_OK) {
            printf("\"collection\":\"%s\"", collection);
        } else {
            printf("\"error\":\"集合不存在或删除失败\"");
        }
        print_json_end();
    } else {
        if (rc == DB_SDK_OK) {
            printf("✓ 集合已删除: %s\n", collection);
        } else {
            vdb_cli_print_error("集合不存在");
        }
    }

    return rc == DB_SDK_OK ? 0 : 1;
}

/* ========================================================================
 * 命令实现：list
 * ======================================================================== */

static int cmd_list(vdb_cli_t *cli, int argc, char *argv[]) {
    (void)argc; (void)argv;

    char **names = NULL;
    int32_t count = 0;
    int rc = db_sdk_list_collections(cli->db, &names, &count);

    if (rc != DB_SDK_OK) {
        if (cli->output_format == VDB_OUTPUT_JSON) {
            print_json_start(false, "list");
            printf("\"error\":\"获取集合列表失败\"");
            print_json_end();
        } else {
            vdb_cli_print_error("获取集合列表失败");
        }
        return 1;
    }

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(true, "list");
        printf("\"collections\":[");
        for (int32_t i = 0; i < count; i++) {
            if (i > 0) printf(",");
            printf("\"%s\"", names[i]);
        }
        printf("],\"count\":%d", count);
        print_json_end();
    } else {
        if (count == 0) {
            printf("没有集合。\n");
        } else {
            const char *header[] = {"集合名称", "向量数", "维度", "索引类型"};
            print_table_header(header, 4);

            for (int32_t i = 0; i < count; i++) {
                db_sdk_collection_t *coll = db_sdk_get_collection(cli->db, names[i]);
                if (coll) {
                    db_sdk_collection_config_t info;
                    memset(&info, 0, sizeof(info));
                    db_sdk_collection_info(coll, &info);
                    int32_t size = db_sdk_size(coll);

                    char size_str[32], dim_str[32];
                    snprintf(size_str, sizeof(size_str), "%d", size);
                    snprintf(dim_str, sizeof(dim_str), "%d", info.dimension);

                    const char *idx_name = "unknown";
                    switch (info.index_type) {
                        case DB_SDK_INDEX_HNSW: idx_name = "HNSW"; break;
                        case DB_SDK_INDEX_IVF: idx_name = "IVF"; break;
                        case DB_SDK_INDEX_BRUTE_FORCE: idx_name = "BRUTE_FORCE"; break;
                        case DB_SDK_INDEX_DISKANN: idx_name = "DiskANN"; break;
                        default: idx_name = "AUTO"; break;
                    }

                    const char *row[] = {names[i], size_str, dim_str, idx_name};
                    print_table_row(row, 4);
                }
            }
            printf("共 %d 个集合。\n", count);
        }
    }

    db_sdk_free_names(names, count);
    return 0;
}

/* ========================================================================
 * 命令实现：info
 * ======================================================================== */

static int cmd_info(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            collection = argv[++i];
        }
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    db_sdk_collection_t *coll = db_sdk_get_collection(cli->db, collection);
    if (!coll) {
        vdb_cli_print_error("集合不存在");
        return 1;
    }

    db_sdk_collection_config_t info;
    memset(&info, 0, sizeof(info));
    db_sdk_collection_info(coll, &info);
    int32_t size = db_sdk_size(coll);

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(true, "info");
        printf("\"collection\":\"%s\",", collection);
        printf("\"dimension\":%d,", info.dimension);
        printf("\"size\":%d,", size);
        const char *idx_name = info.index_type == DB_SDK_INDEX_HNSW ? "HNSW" :
                               info.index_type == DB_SDK_INDEX_IVF ? "IVF" :
                               info.index_type == DB_SDK_INDEX_BRUTE_FORCE ? "BRUTE_FORCE" : "AUTO";
        printf("\"index_type\":\"%s\",", idx_name);
        const char *met_name = info.metric_type == DB_SDK_METRIC_COSINE ? "COSINE" :
                               info.metric_type == DB_SDK_METRIC_IP ? "IP" : "L2";
        printf("\"metric_type\":\"%s\"", met_name);
        print_json_end();
    } else {
        printf("集合信息: %s\n", collection);
        printf("  维度: %d\n", info.dimension);
        printf("  向量数: %d\n", size);
        const char *idx_name = info.index_type == DB_SDK_INDEX_HNSW ? "HNSW" :
                               info.index_type == DB_SDK_INDEX_IVF ? "IVF" :
                               info.index_type == DB_SDK_INDEX_BRUTE_FORCE ? "BRUTE_FORCE" : "AUTO";
        printf("  索引类型: %s\n", idx_name);
        const char *met_name = info.metric_type == DB_SDK_METRIC_COSINE ? "COSINE" :
                               info.metric_type == DB_SDK_METRIC_IP ? "IP" : "L2";
        printf("  距离度量: %s\n", met_name);
    }

    return 0;
}

/* ========================================================================
 * 命令实现：insert
 * ======================================================================== */

static int cmd_insert(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;
    const char *vector_str = NULL;
    const char *metadata_str = NULL;
    bool batch = false;
    const char *file = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--vector") == 0 && i + 1 < argc) {
            vector_str = argv[++i];
        } else if (strcmp(argv[i], "--metadata") == 0 && i + 1 < argc) {
            metadata_str = argv[++i];
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file = argv[++i];
        } else if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
            batch = true;
        }
    }

    /* 复用上次集合名 */
    if (!collection && cli->last_collection) {
        collection = cli->last_collection;
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    db_sdk_collection_t *coll = db_sdk_get_collection(cli->db, collection);
    if (!coll) {
        vdb_cli_print_error("集合不存在");
        return 1;
    }

    /* 批量文件导入 */
    if (file) {
        FILE *fp = fopen(file, "r");
        if (!fp) {
            vdb_cli_print_error("无法打开文件");
            return 1;
        }

        char line[MAX_LINE_LEN];
        int32_t count = 0;
        int64_t total_time = 0;

        while (fgets(line, sizeof(line), fp)) {
            /* 解析行格式: id,vector_str 或只有 vector_str */
            char *vec_part = strchr(line, ',');
            if (vec_part) {
                vec_part++;  /* 跳过逗号 */
            } else {
                vec_part = line;
            }

            /* 去除换行 */
            size_t len = strlen(vec_part);
            while (len > 0 && isspace((unsigned char)vec_part[len-1])) {
                vec_part[--len] = '\0';
            }

            int32_t dim = 0;
            float *vec = parse_float_array(vec_part, &dim);
            if (!vec || dim == 0) continue;

            int64_t t0 = now_ms();
            int64_t id = db_sdk_insert(coll, vec, dim, NULL, 0);
            total_time += now_ms() - t0;

            free(vec);
            if (id > 0) count++;
        }

        fclose(fp);

        if (cli->output_format == VDB_OUTPUT_JSON) {
            print_json_start(true, "insert");
            printf("\"count\":%d,\"time_ms\":%lld", count, (long long)total_time);
            print_json_end();
        } else {
            printf("✓ 导入 %d 条向量，耗时 %lld ms\n", count, (long long)total_time);
        }

        return 0;
    }

    /* 单条插入 */
    if (!vector_str) {
        vdb_cli_print_error("缺少 --vector 参数或 --file 参数");
        return 1;
    }

    int32_t dim = 0;
    float *vector = parse_float_array(vector_str, &dim);
    if (!vector || dim == 0) {
        vdb_cli_print_error("无效的向量格式");
        return 1;
    }

    int32_t meta_size = 0;
    void *metadata = metadata_str ? parse_json_metadata(metadata_str, &meta_size) : NULL;

    int64_t t0 = now_ms();
    int64_t id = db_sdk_insert(coll, vector, dim, metadata, meta_size);
    int64_t took = now_ms() - t0;

    free(vector);
    if (metadata) free(metadata);

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(id > 0, "insert");
        if (id > 0) {
            printf("\"id\":%lld,\"time_ms\":%lld", (long long)id, (long long)took);
        } else {
            printf("\"error\":\"插入失败\"");
        }
        print_json_end();
    } else {
        if (id > 0) {
            printf("✓ 向量插入成功: id=%lld，耗时 %lld ms\n", (long long)id, (long long)took);
        } else {
            vdb_cli_print_error("插入失败");
        }
    }

    /* 保存上次集合名 */
    free(cli->last_collection);
    cli->last_collection = strdup(collection);

    return id > 0 ? 0 : 1;
}

/* ========================================================================
 * 命令实现：query
 * ======================================================================== */

static int cmd_query(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;
    const char *vector_str = NULL;
    int32_t top_k = DEFAULT_TOP_K;
    bool with_distance = true;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--vector") == 0 && i + 1 < argc) {
            vector_str = argv[++i];
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            top_k = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--k") == 0 && i + 1 < argc) {
            top_k = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-distance") == 0) {
            with_distance = false;
        }
    }

    /* 复用上次集合名 */
    if (!collection && cli->last_collection) {
        collection = cli->last_collection;
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    if (!vector_str) {
        vdb_cli_print_error("缺少 --vector 参数");
        return 1;
    }

    db_sdk_collection_t *coll = db_sdk_get_collection(cli->db, collection);
    if (!coll) {
        vdb_cli_print_error("集合不存在");
        return 1;
    }

    int32_t dim = 0;
    float *vector = parse_float_array(vector_str, &dim);
    if (!vector || dim == 0) {
        vdb_cli_print_error("无效的向量格式");
        return 1;
    }

    db_sdk_search_params_t params;
    memset(&params, 0, sizeof(params));
    params.top_k = top_k;
    params.with_distance = with_distance;

    int64_t t0 = now_ms();
    db_sdk_results_t *results = db_sdk_search(coll, vector, dim, &params);
    int64_t took = now_ms() - t0;

    free(vector);

    if (!results) {
        if (cli->output_format == VDB_OUTPUT_JSON) {
            print_json_start(false, "query");
            printf("\"error\":\"查询失败\"");
            print_json_end();
        } else {
            vdb_cli_print_error("查询失败");
        }
        return 1;
    }

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(true, "query");
        printf("\"count\":%d,\"time_us\":%lld,\"results\":[", results->count, (long long)results->total_time_us);
        for (int32_t i = 0; i < results->count; i++) {
            if (i > 0) printf(",");
            printf("{\"id\":%lld", (long long)results->items[i].id);
            if (with_distance) {
                printf(",\"distance\":%.4f", results->items[i].distance);
            }
            printf("}");
        }
        printf("]");
        print_json_end();
    } else {
        if (results->count == 0) {
            printf("没有找到匹配的结果。\n");
        } else {
            const char *header[] = {"排名", "ID", "距离"};
            print_table_header(header, 3);

            for (int32_t i = 0; i < results->count; i++) {
                char rank[32], id_str[32], dist_str[32];
                snprintf(rank, sizeof(rank), "%d", i + 1);
                snprintf(id_str, sizeof(id_str), "%lld", (long long)results->items[i].id);
                snprintf(dist_str, sizeof(dist_str), "%.4f", results->items[i].distance);
                const char *row[] = {rank, id_str, with_distance ? dist_str : "-"};
                print_table_row(row, 3);
            }
        }
        printf("找到 %d 个结果，耗时 %.2f ms\n", results->count, took / 1000.0);
    }

    db_sdk_results_free(results);

    /* 保存上次集合名 */
    free(cli->last_collection);
    cli->last_collection = strdup(collection);

    return 0;
}

/* ========================================================================
 * 命令实现：delete
 * ======================================================================== */

static int cmd_delete(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;
    int64_t id = -1;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            id = atoll(argv[++i]);
        }
    }

    /* 复用上次集合名 */
    if (!collection && cli->last_collection) {
        collection = cli->last_collection;
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    if (id < 0) {
        vdb_cli_print_error("缺少 --id 参数");
        return 1;
    }

    db_sdk_collection_t *coll = db_sdk_get_collection(cli->db, collection);
    if (!coll) {
        vdb_cli_print_error("集合不存在");
        return 1;
    }

    int rc = db_sdk_delete(coll, id);

    if (cli->output_format == VDB_OUTPUT_JSON) {
        print_json_start(rc == DB_SDK_OK, "delete");
        if (rc == DB_SDK_OK) {
            printf("\"id\":%lld", (long long)id);
        } else {
            printf("\"error\":\"删除失败\"");
        }
        print_json_end();
    } else {
        if (rc == DB_SDK_OK) {
            printf("✓ 向量已删除: id=%lld\n", (long long)id);
        } else {
            vdb_cli_print_error("删除失败");
        }
    }

    return rc == DB_SDK_OK ? 0 : 1;
}

/* ========================================================================
 * 命令实现：benchmark
 * ======================================================================== */

static int cmd_benchmark(vdb_cli_t *cli, int argc, char *argv[]) {
    const char *collection = NULL;
    const char *mode = "insert";  /* insert 或 query */
    int32_t count = DEFAULT_BENCHMARK_COUNT;
    int32_t dim = DEFAULT_DIM;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--collection") == 0 && i + 1 < argc) {
            collection = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dim") == 0 && i + 1 < argc) {
            dim = atoi(argv[++i]);
        }
    }

    if (!collection) {
        vdb_cli_print_error("缺少 --collection 参数");
        return 1;
    }

    db_sdk_collection_t *coll = db_sdk_get_collection(cli->db, collection);
    if (!coll) {
        vdb_cli_print_error("集合不存在");
        return 1;
    }

    if (strcmp(mode, "insert") == 0) {
        /* 生成随机向量 */
        float *vectors = (float *)malloc(count * dim * sizeof(float));
        if (!vectors) {
            vdb_cli_print_error("内存分配失败");
            return 1;
        }

        srand((unsigned int)time(NULL));
        for (int32_t i = 0; i < count * dim; i++) {
            vectors[i] = (float)rand() / RAND_MAX;
        }

        int64_t t0 = now_ms();
        int32_t inserted = 0;
        for (int32_t i = 0; i < count; i++) {
            int64_t id = db_sdk_insert(coll, vectors + i * dim, dim, NULL, 0);
            if (id > 0) inserted++;
        }
        int64_t took = now_ms() - t0;

        free(vectors);

        if (cli->output_format == VDB_OUTPUT_JSON) {
            print_json_start(true, "benchmark");
            printf("\"mode\":\"insert\",\"count\":%d,\"time_ms\":%lld,\"qps\":%.2f",
                   inserted, (long long)took, inserted * 1000.0 / took);
            print_json_end();
        } else {
            printf("=== 插入性能测试 ===\n");
            printf("集合: %s\n", collection);
            printf("数量: %d\n", inserted);
            printf("耗时: %lld ms\n", (long long)took);
            printf("QPS: %.2f\n", inserted * 1000.0 / took);
        }
    } else if (strcmp(mode, "query") == 0) {
        /* 查询性能测试 */
        int32_t current_size = db_sdk_size(coll);
        if (current_size == 0) {
            vdb_cli_print_error("集合为空，请先插入数据");
            return 1;
        }

        float *query_vec = (float *)malloc(dim * sizeof(float));
        srand((unsigned int)time(NULL));
        for (int32_t i = 0; i < dim; i++) {
            query_vec[i] = (float)rand() / RAND_MAX;
        }

        int32_t actual_count = count < current_size ? count : current_size;
        int64_t total_time = 0;

        for (int32_t i = 0; i < actual_count; i++) {
            db_sdk_search_params_t params = { .top_k = 10, .with_distance = true };
            int64_t t0 = now_ms();
            db_sdk_results_t *results = db_sdk_search(coll, query_vec, dim, &params);
            total_time += now_ms() - t0;
            if (results) db_sdk_results_free(results);
        }

        free(query_vec);

        if (cli->output_format == VDB_OUTPUT_JSON) {
            print_json_start(true, "benchmark");
            printf("\"mode\":\"query\",\"count\":%d,\"time_ms\":%lld,\"qps\":%.2f,\"avg_latency_us\":%.2f",
                   actual_count, (long long)total_time,
                   actual_count * 1000.0 / total_time,
                   total_time * 1000.0 / actual_count);
            print_json_end();
        } else {
            printf("=== 查询性能测试 ===\n");
            printf("集合: %s\n", collection);
            printf("向量数: %d\n", current_size);
            printf("查询次数: %d\n", actual_count);
            printf("总耗时: %lld ms\n", (long long)total_time);
            printf("QPS: %.2f\n", actual_count * 1000.0 / total_time);
            printf("平均延迟: %.2f μs\n", total_time * 1000.0 / actual_count);
        }
    } else {
        vdb_cli_print_error("未知模式: insert 或 query");
        return 1;
    }

    return 0;
}

/* ========================================================================
 * 命令实现：shell (交互式 REPL)
 * ======================================================================== */

static char *read_line(const char *prompt) {
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

static int cmd_shell(vdb_cli_t *cli, int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("\n");
    vdb_cli_print_welcome();
    printf("输入 help 查看帮助，输入 exit 或 quit 退出。\n\n");

    while (1) {
        char *input = read_line(cli->config.prompt);
        if (!input || input[0] == '\0') {
            printf("\n");
            break;
        }

        /* 跳过空白 */
        const char *cmd = skip_ws(input);
        if (*cmd == '\0') continue;

        /* 内置命令 */
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            printf("再见！\n");
            break;
        }

        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
            vdb_cli_print_help(NULL);
            continue;
        }

        if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
            printf("\033[2J\033[H");
            continue;
        }

        /* 解析命令和参数 */
        char *args[MAX_ARGS];
        int argcnt = 0;

        char *cmd_copy = strdup(cmd);
        char *token = strtok(cmd_copy, " \t");
        while (token && argcnt < MAX_ARGS) {
            args[argcnt++] = token;
            token = strtok(NULL, " \t");
        }
        free(cmd_copy);

        if (argcnt == 0) continue;

        /* 执行命令 */
        int ret = vdb_cli_exec(cli, args[0], argcnt - 1, args + 1);
        (void)ret;  /* shell 模式忽略返回值 */
    }

    return 0;
}

/* ========================================================================
 * 帮助信息
 * ======================================================================== */

void vdb_cli_print_welcome(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║       MiniVecDB CLI v1.0                       ║\n");
    printf("║       向量数据库命令行工具                     ║\n");
    printf("╚═══════════════════════════════════════════════╝\n\n");
}

void vdb_cli_print_help(const char *command) {
    if (!command) {
        printf("可用命令:\n");
        printf("  create      创建集合\n");
        printf("  drop        删除集合\n");
        printf("  list        列出所有集合\n");
        printf("  info        显示集合信息\n");
        printf("  insert      插入向量\n");
        printf("  query       查询向量\n");
        printf("  delete      删除向量\n");
        printf("  benchmark   性能测试\n");
        printf("  shell       进入交互式模式\n");
        printf("\n输入 'help <command>' 查看详细帮助。\n");
        return;
    }

    if (strcmp(command, "create") == 0) {
        printf("create - 创建集合\n\n");
        printf("用法: create --collection <name> [--dim <dim>] [--index <type>] [--metric <type>]\n\n");
        printf("参数:\n");
        printf("  --collection, --name  集合名称（必需）\n");
        printf("  --dim                 向量维度（默认: 128）\n");
        printf("  --index               索引类型: hnsw, ivf, brute, auto（默认: hnsw）\n");
        printf("  --metric              距离度量: l2, cosine, ip（默认: l2）\n");
        printf("  --ef-search           HNSW ef_search（默认: 100）\n");
        printf("  --m                   HNSW M（默认: 16）\n\n");
        printf("示例:\n");
        printf("  vdb_cli create --collection test --dim 128\n");
        printf("  vdb_cli create -n myvec --dim 256 --index hnsw\n");
    } else if (strcmp(command, "drop") == 0) {
        printf("drop - 删除集合\n\n");
        printf("用法: drop --collection <name>\n\n");
        printf("参数:\n");
        printf("  --collection, --name  集合名称（必需）\n\n");
        printf("示例:\n");
        printf("  vdb_cli drop --collection test\n");
    } else if (strcmp(command, "list") == 0) {
        printf("list - 列出所有集合\n\n");
        printf("用法: list\n\n");
        printf("示例:\n");
        printf("  vdb_cli list\n");
    } else if (strcmp(command, "info") == 0) {
        printf("info - 显示集合信息\n\n");
        printf("用法: info --collection <name>\n\n");
        printf("参数:\n");
        printf("  --collection, --name  集合名称（必需）\n\n");
        printf("示例:\n");
        printf("  vdb_cli info --collection test\n");
    } else if (strcmp(command, "insert") == 0) {
        printf("insert - 插入向量\n\n");
        printf("用法: insert --collection <name> --vector <vec> [--metadata <json>]\n");
        printf("       insert --collection <name> --file <path>\n\n");
        printf("参数:\n");
        printf("  --collection  集合名称（必需）\n");
        printf("  --vector      向量，逗号分隔（必需，除非使用 --file）\n");
        printf("  --metadata    JSON 格式元数据（可选）\n");
        printf("  --file        从文件批量导入（每行一个向量）\n\n");
        printf("示例:\n");
        printf("  vdb_cli insert -c test --vector 1.0,2.0,3.0,4.0\n");
        printf("  vdb_cli insert -c test --file vectors.txt\n");
    } else if (strcmp(command, "query") == 0) {
        printf("query - 查询向量\n\n");
        printf("用法: query --collection <name> --vector <vec> [-k <k>]\n\n");
        printf("参数:\n");
        printf("  --collection  集合名称（必需）\n");
        printf("  --vector      查询向量，逗号分隔（必需）\n");
        printf("  -k, --k       返回结果数（默认: 10）\n\n");
        printf("示例:\n");
        printf("  vdb_cli query -c test --vector 1.0,2.0,3.0,4.0 -k 5\n");
    } else if (strcmp(command, "delete") == 0) {
        printf("delete - 删除向量\n\n");
        printf("用法: delete --collection <name> --id <id>\n\n");
        printf("参数:\n");
        printf("  --collection  集合名称（必需）\n");
        printf("  --id          向量 ID（必需）\n\n");
        printf("示例:\n");
        printf("  vdb_cli delete -c test --id 123\n");
    } else if (strcmp(command, "benchmark") == 0) {
        printf("benchmark - 性能测试\n\n");
        printf("用法: benchmark --collection <name> --mode <insert|query> [-n <count>]\n\n");
        printf("参数:\n");
        printf("  --collection  集合名称（必需）\n");
        printf("  --mode        测试模式: insert, query（默认: insert）\n");
        printf("  -n, --count   测试数量（默认: 1000）\n\n");
        printf("示例:\n");
        printf("  vdb_cli benchmark -c test --mode insert -n 10000\n");
        printf("  vdb_cli benchmark -c test --mode query -n 1000\n");
    } else if (strcmp(command, "shell") == 0) {
        printf("shell - 进入交互式模式\n\n");
        printf("用法: shell\n\n");
        printf("在交互式模式下，可以直接输入命令。\n");
        printf("  help, ?       显示帮助\n");
        printf("  exit, quit, q 退出\n");
        printf("  clear, cls    清屏\n\n");
        printf("示例:\n");
        printf("  vdb_cli shell\n");
    } else {
        printf("未知命令: %s\n", command);
        printf("输入 'help' 查看可用命令。\n");
    }
}

void vdb_cli_print_error(const char *msg) {
    if (msg && *msg) {
        fprintf(stderr, "错误: %s\n", msg);
    } else {
        fprintf(stderr, "错误: 未知错误\n");
    }
}

void vdb_cli_print_version(void) {
    printf("MiniVecDB CLI v1.0\n");
}

/* ========================================================================
 * 创建/销毁
 * ======================================================================== */

vdb_cli_t *vdb_cli_create(const vdb_cli_config_t *config) {
    vdb_cli_t *cli = (vdb_cli_t *)calloc(1, sizeof(vdb_cli_t));
    if (!cli) return NULL;

    if (config) {
        cli->config = *config;
    } else {
        cli->config.data_dir = "./vdb_data";
        cli->config.prompt = "vdb> ";
        cli->config.history_size = 100;
        cli->config.echo = true;
        cli->config.show_timing = true;
        cli->config.json_output = false;
        cli->config.color_output = true;
    }

    cli->output_format = cli->config.json_output ? VDB_OUTPUT_JSON : VDB_OUTPUT_TABLE;

    /* 打开数据库 */
    cli->db = db_sdk_open(cli->config.data_dir);
    if (!cli->db) {
        free(cli);
        return NULL;
    }

    return cli;
}

void vdb_cli_destroy(vdb_cli_t *cli) {
    if (!cli) return;
    if (cli->db) db_sdk_close(cli->db);
    free(cli->last_collection);
    free(cli);
}

/* ========================================================================
 * 命令分发
 * ======================================================================== */

int vdb_cli_exec(vdb_cli_t *cli, const char *command, int argc, char *argv[]) {
    if (!cli || !command) return 1;

    if (strcmp(command, "create") == 0) {
        return cmd_create(cli, argc, argv);
    } else if (strcmp(command, "drop") == 0) {
        return cmd_drop(cli, argc, argv);
    } else if (strcmp(command, "list") == 0 || strcmp(command, "ls") == 0) {
        return cmd_list(cli, argc, argv);
    } else if (strcmp(command, "info") == 0) {
        return cmd_info(cli, argc, argv);
    } else if (strcmp(command, "insert") == 0) {
        return cmd_insert(cli, argc, argv);
    } else if (strcmp(command, "query") == 0 || strcmp(command, "search") == 0) {
        return cmd_query(cli, argc, argv);
    } else if (strcmp(command, "delete") == 0 || strcmp(command, "del") == 0) {
        return cmd_delete(cli, argc, argv);
    } else if (strcmp(command, "benchmark") == 0 || strcmp(command, "bench") == 0) {
        return cmd_benchmark(cli, argc, argv);
    } else if (strcmp(command, "shell") == 0 || strcmp(command, "repl") == 0) {
        return cmd_shell(cli, argc, argv);
    } else if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
        vdb_cli_print_help(argc > 0 ? argv[0] : NULL);
        return 0;
    } else if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0) {
        vdb_cli_print_version();
        return 0;
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        return -1;  /* 退出码 */
    } else {
        fprintf(stderr, "未知命令: %s\n", command);
        fprintf(stderr, "输入 'help' 查看可用命令。\n");
        return 1;
    }
}

void vdb_cli_set_output_format(vdb_cli_t *cli, vdb_output_format_t format) {
    if (cli) cli->output_format = format;
}

vdb_output_format_t vdb_cli_get_output_format(vdb_cli_t *cli) {
    return cli ? cli->output_format : VDB_OUTPUT_TABLE;
}
