/**
 * @file vector_cli.c
 * @brief 向量 CLI 命令实现
 *
 * 实现向量集合管理的交互式 CLI 命令。
 */
#include <db/cli/vector_cli.h>
#include <db/api/vector_api.h>
#include <db/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 向量 CLI 上下文 */
struct VectorCLI_s {
    VectorAPI *api;              /**< 向量 API 句柄 */
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 去除首尾空白
 */
static char *trim(char *str) {
    if (!str) return NULL;

    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

/**
 * @brief 分割字符串
 */
static int split_string(const char *str, char delim, char ***out_parts, int *out_count) {
    if (!str) {
        *out_parts = NULL;
        *out_count = 0;
        return 0;
    }

    /* 复制字符串以便修改 */
    char *copy = strdup(str);
    if (!copy) return -1;

    /* 计算分割后的部分数 */
    int count = 1;
    for (char *p = copy; *p; p++) {
        if (*p == delim) count++;
    }

    char **parts = (char **)malloc(count * sizeof(char *));
    if (!parts) {
        free(copy);
        return -1;
    }

    int idx = 0;
    char *saveptr;
    char *token = strtok_r(copy, &delim, &saveptr);
    while (token && idx < count) {
        parts[idx++] = strdup(trim(token));
        token = strtok_r(NULL, &delim, &saveptr);
    }
    *out_count = idx;
    *out_parts = parts;

    free(copy);
    return 0;
}

/**
 * @brief 解析索引类型字符串
 */
static VectorIndexType parse_index_type(const char *str) {
    if (!str) return VECTOR_INDEX_HNSW;
    if (strcmp(str, "diskann") == 0) return VECTOR_INDEX_DISKANN;
    if (strcmp(str, "ivf") == 0) return VECTOR_INDEX_IVF;
    return VECTOR_INDEX_HNSW;
}

/**
 * @brief 解析度量类型字符串
 */
static VectorMetricType parse_metric_type(const char *str) {
    if (!str) return VECTOR_METRIC_L2;
    if (strcmp(str, "cosine") == 0) return VECTOR_METRIC_COSINE;
    if (strcmp(str, "ip") == 0) return VECTOR_METRIC_IP;
    return VECTOR_METRIC_L2;
}

/**
 * @brief 打印集合信息
 */
static void print_collection_info(const VectorCollectionInfo *info) {
    printf("集合信息:\n");
    printf("  名称: %s\n", info->name);
    printf("  维度: %d\n", info->dimension);
    printf("  向量数: %d\n", info->size);
    printf("  索引类型: ");
    switch (info->index_type) {
        case VECTOR_INDEX_HNSW: printf("HNSW\n"); break;
        case VECTOR_INDEX_DISKANN: printf("DiskANN\n"); break;
        case VECTOR_INDEX_IVF: printf("IVF\n"); break;
        default: printf("Unknown\n"); break;
    }
    printf("  度量类型: ");
    switch (info->metric_type) {
        case VECTOR_METRIC_L2: printf("L2\n"); break;
        case VECTOR_METRIC_COSINE: printf("Cosine\n"); break;
        case VECTOR_METRIC_IP: printf("Inner Product\n"); break;
        default: printf("Unknown\n"); break;
    }
}

/* ========================================================================
 * 命令处理
 * ======================================================================== */

/**
 * @brief 处理 create collection 命令
 */
static int cmd_create_collection(VectorCLI *cli, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: create collection <name> <dimension> [--index hnsw|diskann|ivf] [--metric l2|cosine|ip]\n");
        return -1;
    }

    const char *name = argv[0];
    int32_t dim = atoi(argv[1]);
    if (dim <= 0 || dim > 65536) {
        printf("错误: 无效的维度值 %d\n", dim);
        return -1;
    }

    VectorIndexType index_type = VECTOR_INDEX_HNSW;
    VectorMetricType metric_type = VECTOR_METRIC_L2;
    int ef_search = 100;
    int m = 16;

    /* 解析可选参数 */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_type = parse_index_type(argv[++i]);
        } else if (strcmp(argv[i], "--metric") == 0 && i + 1 < argc) {
            metric_type = parse_metric_type(argv[++i]);
        } else if (strcmp(argv[i], "--ef-search") == 0 && i + 1 < argc) {
            ef_search = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--m") == 0 && i + 1 < argc) {
            m = atoi(argv[++i]);
        }
    }

    VectorCreateParams params = {
        .name = name,
        .dimension = dim,
        .index_type = index_type,
        .metric_type = metric_type,
        .index_ef_search = ef_search,
        .index_m = m,
        .index_ef_construction = 200
    };

    int ret = vector_api_create_collection(cli->api, &params);
    if (ret == VECTOR_API_OK) {
        printf("集合创建成功: %s (dim=%d)\n", name, dim);
        return 0;
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
        return ret;
    }
}

/**
 * @brief 处理 list collections 命令
 */
static int cmd_list_collections(VectorCLI *cli) {
    char **names = NULL;
    int32_t n = 0;

    int ret = vector_api_list_collections(cli->api, &names, &n);
    if (ret != VECTOR_API_OK) {
        printf("错误: %s\n", vector_api_error(cli->api));
        return ret;
    }

    if (n == 0) {
        printf("无集合\n");
    } else {
        printf("集合列表 (%d 个):\n", n);
        for (int32_t i = 0; i < n; i++) {
            VectorCollectionInfo info;
            if (vector_api_get_collection(cli->api, names[i], &info) == VECTOR_API_OK) {
                printf("  - %s (dim=%d, vectors=%d)\n", names[i], info.dimension, info.size);
            } else {
                printf("  - %s\n", names[i]);
            }
        }
    }

    vector_api_free_names(names, n);
    return 0;
}

/**
 * @brief 处理 drop collection 命令
 */
static int cmd_drop_collection(VectorCLI *cli, const char *name) {
    if (!name) {
        printf("用法: drop collection <name>\n");
        return -1;
    }

    int ret = vector_api_drop_collection(cli->api, name);
    if (ret == VECTOR_API_OK) {
        printf("集合已删除: %s\n", name);
        return 0;
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
        return ret;
    }
}

/**
 * @brief 处理 insert vectors 命令
 */
static int cmd_insert_vectors(VectorCLI *cli, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: insert vectors <collection> <n> [vector1] [vector2] ...\n");
        printf("       或: insert vectors <collection> json <file.json>\n");
        return -1;
    }

    const char *collection = argv[0];

    /* 检查是否是 JSON 文件模式 */
    if (strcmp(argv[1], "json") == 0 && argc >= 3) {
        int64_t *ids = NULL;
        float **vectors = NULL;
        int32_t n = 0, dim = 0;

        int ret = vector_cli_parse_vectors_json(argv[2], &ids, &vectors, &n, &dim);
        if (ret != 0) {
            printf("错误: 解析 JSON 文件失败\n");
            return ret;
        }

        VectorInsertParams params = {
            .collection = collection,
            .n = n,
            .ids = ids,
            .vectors = vectors,
            .metadata = NULL,
            .metadata_sizes = NULL
        };

        int inserted = vector_api_insert(cli->api, &params, NULL);
        if (inserted >= 0) {
            printf("成功插入 %d 个向量到集合 %s\n", inserted, collection);
        } else {
            printf("错误: %s\n", vector_api_error(cli->api));
        }

        /* 释放内存 */
        for (int32_t i = 0; i < n; i++) {
            free(vectors[i]);
        }
        free(vectors);
        free(ids);

        return inserted >= 0 ? 0 : inserted;
    }

    /* 命令行模式 */
    int32_t n = atoi(argv[1]);
    if (n <= 0) {
        printf("错误: 无效的数量\n");
        return -1;
    }

    if (argc < 2 + n) {
        printf("错误: 向量数量不足\n");
        return -1;
    }

    float **vectors = (float **)malloc(n * sizeof(float *));
    int32_t dim = 0;

    for (int32_t i = 0; i < n; i++) {
        /* 解析向量字符串 "v1,v2,v3,..." */
        char **parts = NULL;
        int parts_count = 0;
        split_string(argv[2 + i], ',', &parts, &parts_count);

        if (i == 0) {
            dim = parts_count;
        } else if (parts_count != dim) {
            printf("错误: 向量维度不一致\n");
            for (int32_t j = 0; j < i; j++) free(vectors[j]);
            free(vectors);
            for (int j = 0; j < parts_count; j++) free(parts[j]);
            free(parts);
            return -1;
        }

        vectors[i] = (float *)malloc(dim * sizeof(float));
        if (!vectors[i]) {
            /* 清理已分配的内存 */
            for (int32_t k = 0; k < i; k++) free(vectors[k]);
            free(vectors);
            for (int j = 0; j < parts_count; j++) free(parts[j]);
            free(parts);
            return -1;
        }
        for (int32_t j = 0; j < dim; j++) {
            vectors[i][j] = (float)atof(parts[j]);
            free(parts[j]);
        }
        free(parts);
    }

    VectorInsertParams params = {
        .collection = collection,
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int inserted = vector_api_insert(cli->api, &params, NULL);
    if (inserted >= 0) {
        printf("成功插入 %d 个向量到集合 %s (dim=%d)\n", inserted, collection, dim);
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
    }

    for (int32_t i = 0; i < n; i++) free(vectors[i]);
    free(vectors);

    return inserted >= 0 ? 0 : inserted;
}

/**
 * @brief 处理 search vectors 命令
 */
static int cmd_search_vectors(VectorCLI *cli, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: search vectors <collection> <query_vector> <top_k>\n");
        printf("       查询向量格式: v1,v2,v3,...\n");
        return -1;
    }

    const char *collection = argv[0];
    int32_t top_k = atoi(argv[argc - 1]);
    if (top_k <= 0) top_k = 10;

    /* 解析查询向量 */
    char **parts = NULL;
    int parts_count = 0;
    split_string(argv[1], ',', &parts, &parts_count);

    float *query = (float *)malloc(parts_count * sizeof(float));
    for (int i = 0; i < parts_count; i++) {
        query[i] = (float)atof(parts[i]);
        free(parts[i]);
    }
    free(parts);

    VectorSearchParams params = {
        .collection = collection,
        .query = query,
        .top_k = top_k,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    VectorSearchResults *results = vector_api_search(cli->api, &params);
    free(query);

    if (!results) {
        printf("错误: %s\n", vector_api_error(cli->api));
        return -1;
    }

    int32_t count = vector_api_results_count(results);
    if (count == 0) {
        printf("无搜索结果\n");
    } else {
        printf("搜索结果 (top %d):\n", count);
        printf("  ID        距离\n");
        printf("  ---------- ----------\n");
        for (int32_t i = 0; i < count; i++) {
            const VectorSearchResult *r = vector_api_results_get(results, i);
            if (r) {
                printf("  %-10lld %.6f\n", (long long)r->id, r->distance);
            }
        }
    }

    vector_api_results_free(results);
    return 0;
}

/**
 * @brief 处理 delete vectors 命令
 */
static int cmd_delete_vectors(VectorCLI *cli, const char *collection, const char *ids_str) {
    if (!collection || !ids_str) {
        printf("用法: delete vectors <collection> <id1,id2,...>\n");
        return -1;
    }

    /* 解析 ID 列表 */
    char **parts = NULL;
    int parts_count = 0;
    split_string(ids_str, ',', &parts, &parts_count);

    int64_t *ids = (int64_t *)malloc(parts_count * sizeof(int64_t));
    for (int i = 0; i < parts_count; i++) {
        ids[i] = atoll(parts[i]);
        free(parts[i]);
    }
    free(parts);

    int deleted = vector_api_delete(cli->api, collection, ids, parts_count);
    free(ids);

    if (deleted >= 0) {
        printf("成功删除 %d 个向量\n", deleted);
        return 0;
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
        return deleted;
    }
}

/**
 * @brief 处理 collection info 命令
 */
static int cmd_collection_info(VectorCLI *cli, const char *name) {
    if (!name) {
        printf("用法: collection info <name>\n");
        return -1;
    }

    VectorCollectionInfo info;
    int ret = vector_api_get_collection(cli->api, name, &info);
    if (ret == VECTOR_API_OK) {
        print_collection_info(&info);
        return 0;
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
        return ret;
    }
}

/**
 * @brief 处理 save 命令
 */
static int cmd_save(VectorCLI *cli) {
    int ret = vector_api_save(cli->api);
    if (ret == VECTOR_API_OK) {
        printf("数据已保存\n");
        return 0;
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
        return ret;
    }
}

/**
 * @brief 处理 load 命令
 */
static int cmd_load(VectorCLI *cli) {
    int ret = vector_api_load(cli->api);
    if (ret == VECTOR_API_OK) {
        printf("数据已加载\n");
        return 0;
    } else {
        printf("错误: %s\n", vector_api_error(cli->api));
        return ret;
    }
}

/* ========================================================================
 * CLI 生命周期
 * ======================================================================== */

VectorCLI *vector_cli_create(void *api) {
    VectorCLI *cli = (VectorCLI *)calloc(1, sizeof(*cli));
    if (!cli) return NULL;
    cli->api = (VectorAPI *)api;
    return cli;
}

void vector_cli_destroy(VectorCLI *cli) {
    free(cli);
}

const char *vector_cli_help(void) {
    static const char *help =
        "向量 CLI 命令:\n"
        "  create collection <name> <dim> [--index hnsw|diskann|ivf] [--metric l2|cosine|ip]\n"
        "      创建集合\n"
        "  list collections\n"
        "      列出所有集合\n"
        "  drop collection <name>\n"
        "      删除集合\n"
        "  insert vectors <collection> <n> [vector1] [vector2] ...\n"
        "      插入向量 (命令行模式)\n"
        "  insert vectors <collection> json <file.json>\n"
        "      插入向量 (JSON 文件模式)\n"
        "  search vectors <collection> <query_vector> <top_k>\n"
        "      搜索向量\n"
        "  delete vectors <collection> <id1,id2,...>\n"
        "      删除向量\n"
        "  collection info <name>\n"
        "      显示集合信息\n"
        "  save\n"
        "      保存数据到磁盘\n"
        "  load\n"
        "      从磁盘加载数据\n"
        "  help\n"
        "      显示帮助\n"
        "  quit\n"
        "      退出\n"
        "\n"
        "示例:\n"
        "  create collection test 128\n"
        "  insert vectors test 2 1.0,2.0,3.0,... 4.0,5.0,6.0,...\n"
        "  search vectors test 1.0,2.0,3.0,... 10\n";
    return help;
}

int vector_cli_exec(VectorCLI *cli, const char *cmd) {
    if (!cli || !cmd) return -1;

    char *trimmed = trim((char *)cmd);
    if (!*trimmed) return 0;

    /* 处理退出命令 */
    if (strcmp(trimmed, "quit") == 0 || strcmp(trimmed, "exit") == 0) {
        return 1;  /* 返回 1 表示退出 */
    }

    /* 处理帮助命令 */
    if (strcmp(trimmed, "help") == 0) {
        printf("%s\n", vector_cli_help());
        return 0;
    }

    /* 分割命令和参数 */
    char **parts = NULL;
    int parts_count = 0;
    split_string(trimmed, ' ', &parts, &parts_count);

    if (parts_count == 0) {
        return 0;
    }

    int ret = 0;

    /* 主命令分发 */
    if (strcmp(parts[0], "create") == 0 && parts_count >= 3 && strcmp(parts[1], "collection") == 0) {
        ret = cmd_create_collection(cli, parts_count - 2, parts + 2);
    } else if (strcmp(parts[0], "list") == 0 && parts_count >= 2 && strcmp(parts[1], "collections") == 0) {
        ret = cmd_list_collections(cli);
    } else if (strcmp(parts[0], "drop") == 0 && parts_count >= 3 && strcmp(parts[1], "collection") == 0) {
        ret = cmd_drop_collection(cli, parts[2]);
    } else if (strcmp(parts[0], "insert") == 0 && parts_count >= 3 && strcmp(parts[1], "vectors") == 0) {
        ret = cmd_insert_vectors(cli, parts_count - 2, parts + 2);
    } else if (strcmp(parts[0], "search") == 0 && parts_count >= 3 && strcmp(parts[1], "vectors") == 0) {
        ret = cmd_search_vectors(cli, parts_count - 2, parts + 2);
    } else if (strcmp(parts[0], "delete") == 0 && parts_count >= 3 && strcmp(parts[1], "vectors") == 0) {
        /* 提取集合名和 ID 列表 */
        ret = cmd_delete_vectors(cli, parts[2], parts_count > 3 ? parts[3] : NULL);
    } else if (strcmp(parts[0], "collection") == 0 && parts_count >= 3 && strcmp(parts[1], "info") == 0) {
        ret = cmd_collection_info(cli, parts[2]);
    } else if (strcmp(parts[0], "save") == 0) {
        ret = cmd_save(cli);
    } else if (strcmp(parts[0], "load") == 0) {
        ret = cmd_load(cli);
    } else {
        printf("未知命令: %s\n", parts[0]);
        printf("输入 'help' 查看可用命令\n");
        ret = -1;
    }

    /* 释放内存 */
    for (int i = 0; i < parts_count; i++) {
        free(parts[i]);
    }
    free(parts);

    return ret;
}

int vector_cli_run(VectorCLI *cli) {
    if (!cli) return -1;

    printf("==============================================\n");
    printf("  MiniVecDB - 向量 CLI\n");
    printf("  输入 'help' 查看命令\n");
    printf("  输入 'quit' 退出\n");
    printf("==============================================\n\n");

    char line[4096];
    while (1) {
        printf("vec> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* 去除换行符 */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        int ret = vector_cli_exec(cli, line);
        if (ret == 1) break;  /* 退出 */
    }

    return 0;
}

/* ========================================================================
 * JSON 工具
 * ======================================================================== */

int vector_cli_parse_vectors_json(const char *filename,
                                   int64_t **out_ids,
                                   float ***out_vectors,
                                   int32_t *out_n,
                                   int32_t *out_dim) {
    if (!filename || !out_vectors || !out_n || !out_dim) {
        return -1;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("错误: 无法打开文件 %s\n", filename);
        return -1;
    }

    /* 读取整个文件 */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = (char *)malloc(fsize + 1);
    fread(content, 1, fsize, fp);
    fclose(fp);
    content[fsize] = '\0';

    /* 简单解析 JSON（简化实现） */
    /* 期望格式: {"vectors": [[...], [...]], "ids": [1, 2, ...]} */

    /* 释放内存 */
    free(content);

    /* TODO: 实现完整的 JSON 解析 */
    printf("提示: JSON 解析功能待完善，请使用命令行模式\n");
    *out_vectors = NULL;
    *out_n = 0;
    *out_dim = 0;
    if (out_ids) *out_ids = NULL;

    return -1;
}

char *vector_cli_format_results_json(const void *results, bool with_distance) {
    /* TODO: 实现 JSON 格式化 */
    return strdup("[]");
}
