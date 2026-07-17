/**
 * @file mm_storage.c
 * @brief 多模态存储管理器实现
 *
 * 实现统一的多模态数据库存储管理，包括引擎注册、上下文管理、模型路由等功能。
 */
#include "mm_storage.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 数据模型目录名 */
static const char *g_model_dir_names[MODEL_COUNT] = {
    "base",        /* MODEL_RELATIONAL */
    "kv",          /* MODEL_KV */
    "graph",       /* MODEL_GRAPH */
    "vector",      /* MODEL_VECTOR */
    "timeseries",  /* MODEL_TIMESERIES */
    "document",    /* MODEL_DOCUMENT */
    "spatial",     /* MODEL_SPATIAL */
    "yang",        /* MODEL_TREE */
};

/** 全局上下文 */
static mm_context_t *g_context = NULL;

/** 引擎操作表数组 */
static const storage_ops_t *g_engines[MODEL_COUNT] = { NULL };

/** 多模态存储上下文 */
struct mm_context_s {
    char data_dir[512];        /**< 数据根目录 */
    bool initialized;          /**< 初始化标志 */
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 递归创建目录
 */
static int mkpath(char *path) {
    if (path == NULL || strlen(path) == 0) return -1;

    char *p = strdup(path);
    if (p == NULL) return -1;

    /* 处理以 ./ 开头的路径 */
    char *cur = p;
    if (cur[0] == '.' && cur[1] == '/') {
        cur += 2;
    }

    /* 逐层创建目录 */
    for (char *sep = cur; *sep != '\0'; sep++) {
        if (*sep == '/') {
            *sep = '\0';
            if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
                free(p);
                return -1;
            }
            *sep = '/';
        }
    }

    /* 创建最后一层目录 */
    if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }

    free(p);
    return 0;
}

/**
 * @brief 确保目录存在（递归创建）
 */
static int ensure_dir(const char *path) {
    if (path == NULL) return -1;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }

    /* 目录不存在，递归创建 */
    return mkpath((char *)path);
}

/* ========================================================================
 * 上下文管理实现
 * ======================================================================== */

int mm_storage_init(const char *data_dir) {
    if (g_context != NULL) {
        return 0;
    }

    g_context = (mm_context_t *)calloc(1, sizeof(mm_context_t));
    if (g_context == NULL) {
        return -1;
    }

    if (data_dir) {
        strncpy(g_context->data_dir, data_dir, sizeof(g_context->data_dir) - 1);
    } else {
        strcpy(g_context->data_dir, "./data");
    }

    if (ensure_dir(g_context->data_dir) != 0) {
        free(g_context);
        g_context = NULL;
        return -1;
    }

    for (int i = 0; i < MODEL_COUNT; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", g_context->data_dir, g_model_dir_names[i]);
        if (ensure_dir(path) != 0) {
            mm_storage_shutdown();
            return -1;
        }
    }

    g_context->initialized = true;
    return 0;
}

int mm_storage_shutdown(void) {
    if (g_context == NULL) {
        return 0;
    }

    storage_shutdown_all();
    free(g_context);
    g_context = NULL;

    return 0;
}

mm_context_t *mm_get_context(void) {
    return g_context;
}

bool mm_is_initialized(void) {
    return g_context != NULL && g_context->initialized;
}

/* ========================================================================
 * 引擎注册实现
 * ======================================================================== */

int storage_register_engine(DataModel model, const storage_ops_t *ops) {
    if (model < 0 || model >= MODEL_COUNT || ops == NULL) {
        return -1;
    }
    g_engines[model] = ops;
    return 0;
}

const storage_ops_t *storage_get_engine(DataModel model) {
    if (model < 0 || model >= MODEL_COUNT) {
        return NULL;
    }
    return g_engines[model];
}

int storage_init_all(const char *data_dir) {
    for (int i = 0; i < MODEL_COUNT; i++) {
        if (g_engines[i] && g_engines[i]->init) {
            char engine_dir[512];
            snprintf(engine_dir, sizeof(engine_dir), "%s/%s", data_dir, g_model_dir_names[i]);
            if (ensure_dir(engine_dir) != 0) {
                return -1;
            }
            if (g_engines[i]->init(engine_dir) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

void storage_shutdown_all(void) {
    for (int i = 0; i < MODEL_COUNT; i++) {
        if (g_engines[i] && g_engines[i]->shutdown) {
            g_engines[i]->shutdown();
        }
    }
}

const char *storage_model_name(DataModel model) {
    static const char *names[MODEL_COUNT] = {
        "relational", "kv", "graph", "vector",
        "timeseries", "document", "spatial", "tree"
    };
    if (model < 0 || model >= MODEL_COUNT) {
        return "unknown";
    }
    return names[model];
}

/* ========================================================================
 * 模型操作实现
 * ======================================================================== */

int mm_create_model(const char *name, DataModel model, const storage_schema_t *schema) {
    if (!mm_is_initialized() || name == NULL) {
        return -1;
    }

    const storage_ops_t *ops = g_engines[model];
    if (ops == NULL || ops->table_create == NULL) {
        return -1;
    }

    return ops->table_create(name, schema);
}

void *mm_open_model(const char *name, DataModel model, AccessMode mode) {
    if (!mm_is_initialized() || name == NULL) {
        return NULL;
    }

    const storage_ops_t *ops = g_engines[model];
    if (ops == NULL || ops->table_open == NULL) {
        return NULL;
    }

    return ops->table_open(name, mode);
}

int mm_close_model(void *handle) {
    (void)handle;
    return 0;
}

int mm_drop_model(const char *name, DataModel model) {
    if (!mm_is_initialized()) {
        return -1;
    }

    const storage_ops_t *ops = g_engines[model];
    if (ops == NULL || ops->table_drop == NULL) {
        return -1;
    }

    return ops->table_drop(name);
}

bool mm_model_exists(const char *name, DataModel model) {
    if (name == NULL) {
        return false;
    }

    char path[512];
    if (mm_get_model_path(name, model, path, sizeof(path)) != 0) {
        return false;
    }

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ========================================================================
 * 数据操作实现
 * ======================================================================== */

int mm_insert(void *handle, const void *data, size_t len) {
    (void)handle;
    (void)data;
    (void)len;
    return 0;
}

int mm_update(void *handle, const void *old_data, size_t old_len,
              const void *new_data, size_t new_len) {
    (void)handle;
    (void)old_data;
    (void)old_len;
    (void)new_data;
    (void)new_len;
    return 0;
}

int mm_delete(void *handle, const void *key, size_t key_len) {
    (void)handle;
    (void)key;
    (void)key_len;
    return 0;
}

int mm_get(void *handle, const void *key, size_t key_len,
           void **out_data, size_t *out_len) {
    (void)handle;
    (void)key;
    (void)key_len;
    (void)out_data;
    (void)out_len;
    return 0;
}

/* ========================================================================
 * 扫描操作实现
 * ======================================================================== */

scan_desc_t *mm_scan_begin(void *handle, const scan_key_t *keys, int nkeys,
                           ScanDirection direction) {
    (void)handle;
    (void)keys;
    (void)nkeys;
    (void)direction;
    return NULL;
}

int mm_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

int mm_scan_end(scan_desc_t *scan) {
    (void)scan;
    return 0;
}

/* ========================================================================
 * 统计信息实现
 * ======================================================================== */

int mm_get_engine_stats(DataModel model, storage_stats_t *stats) {
    if (!mm_is_initialized() || stats == NULL) {
        return -1;
    }

    const storage_ops_t *ops = g_engines[model];
    if (ops == NULL || ops->get_stats == NULL) {
        return -1;
    }

    return ops->get_stats(NULL, stats);
}

int mm_get_model_stats(const char *name, DataModel model, storage_stats_t *stats) {
    if (!mm_is_initialized() || stats == NULL) {
        return -1;
    }

    const storage_ops_t *ops = g_engines[model];
    if (ops == NULL || ops->get_stats == NULL) {
        return -1;
    }

    return ops->get_stats(name, stats);
}

/* ========================================================================
 * 工具函数实现
 * ======================================================================== */

const char *mm_get_model_dir(DataModel model) {
    if (model < 0 || model >= MODEL_COUNT) {
        return NULL;
    }
    return g_model_dir_names[model];
}

int mm_get_model_path(const char *name, DataModel model,
                      char *out_path, size_t path_size) {
    if (!mm_is_initialized() || name == NULL) {
        return -1;
    }

    if (model < 0 || model >= MODEL_COUNT) {
        return -1;
    }

    snprintf(out_path, path_size, "%s/%s/%s",
             g_context->data_dir, g_model_dir_names[model], name);
    return 0;
}

db_error_t *mm_get_error(void) {
    return db_error_get();
}
