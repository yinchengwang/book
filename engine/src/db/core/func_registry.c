/**
 * @file func_registry.c
 * @brief SQL 函数注册表实现
 */

#include "db/core/func_registry.h"
#include "db/storage/doc/doc_vector.h"
#include "db/storage/ts/ts_sql_functions.h"
#include "db/graph/graph_sql_functions.h"
#include "db/storage/spatial/spatial_geo.h"
#include "db/storage/yang/yang_tree.h"
#include <stdlib.h>
#include <string.h>
#include <uthash.h>

/* ========================================================================
 * 函数哈希表
 * ======================================================================== */

typedef struct FuncHashEntry_s {
    char key[256];              /* schema.funcname.num_args */
    FunctionInfo info;
    UT_hash_handle hh;
} FuncHashEntry;

static FuncHashEntry *g_functions = NULL;
static size_t g_func_count = 0;

/* ========================================================================
 * 类型哈希表
 * ======================================================================== */

typedef struct TypeHashEntry_s {
    int oid;
    TypeInfo info;
    UT_hash_handle hh;
} TypeHashEntry;

static TypeHashEntry *g_types = NULL;
static int g_next_oid = 1000;

static char *make_func_key(const char *name, const char *schema, int num_args) {
    static char key[256];
    snprintf(key, sizeof(key), "%s.%s.%d", schema ? schema : "public", name, num_args);
    return key;
}

/* ========================================================================
 * 初始化
 * ======================================================================== */

int func_registry_init(void) {
    g_functions = NULL;
    g_types = NULL;
    g_func_count = 0;
    g_next_oid = 1000;

    /* 初始化类型系统 */
    func_registry_init_types();

    /* 注册内置函数 */
    return func_registry_register_builtins();
}

void func_registry_shutdown(void) {
    FuncHashEntry *f, *tmp_f;
    HASH_ITER(hh, g_functions, f, tmp_f) {
        HASH_DEL(g_functions, f);
        free(f);
    }

    TypeHashEntry *t, *tmp_t;
    HASH_ITER(hh, g_types, t, tmp_t) {
        HASH_DEL(g_types, t);
        free(t);
    }
}

/* ========================================================================
 * 注册/注销
 * ======================================================================== */

int func_registry_register(const FunctionInfo *info) {
    if (!info || !info->name) return -1;

    FuncHashEntry *entry = (FuncHashEntry *)calloc(1, sizeof(FuncHashEntry));
    if (!entry) return -1;

    char *key = make_func_key(info->name, info->schema, info->num_args);
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    entry->info = *info;

    HASH_ADD_STR(g_functions, key, entry);
    g_func_count++;

    return 0;
}

int func_registry_unregister(const char *name, const char *schema) {
    char *key = make_func_key(name, schema, -1);

    FuncHashEntry *entry;
    HASH_FIND_STR(g_functions, key, entry);
    if (entry) {
        HASH_DEL(g_functions, entry);
        free(entry);
        g_func_count--;
        return 0;
    }
    return -1;
}

const FunctionInfo *func_registry_lookup(const char *name, const char *schema, int num_args) {
    /* 先精确匹配 */
    char *key = make_func_key(name, schema, num_args);
    FuncHashEntry *entry;
    HASH_FIND_STR(g_functions, key, entry);

    if (entry) return &entry->info;

    /* 尝试任意参数数量 */
    FuncHashEntry *f, *tmp;
    HASH_ITER(hh, g_functions, f, tmp) {
        if (strcmp(f->info.name, name) == 0 &&
            (f->info.schema == schema ||
             (f->info.schema && schema && strcmp(f->info.schema, schema) == 0)) {
            return &f->info;
        }
    }

    return NULL;
}

const FunctionInfo **func_registry_list_by_model(DataModel model, size_t *out_count) {
    if (!out_count) return NULL;

    size_t count = 0;
    FunctionInfo **results = (FunctionInfo **)calloc(64, sizeof(FunctionInfo *));

    FuncHashEntry *f, *tmp;
    HASH_ITER(hh, g_functions, f, tmp) {
        if (f->info.model == model || f->info.model == MODEL_COUNT) {
            results[count++] = &f->info;
        }
    }

    *out_count = count;
    return results;
}

const FunctionInfo **func_registry_list_by_class(FunctionClass func_class, size_t *out_count) {
    if (!out_count) return NULL;

    size_t count = 0;
    FunctionInfo **results = (FunctionInfo **)calloc(64, sizeof(FunctionInfo *));

    FuncHashEntry *f, *tmp;
    HASH_ITER(hh, g_functions, f, tmp) {
        if (f->info.func_class == func_class) {
            results[count++] = &f->info;
        }
    }

    *out_count = count;
    return results;
}

size_t func_registry_count(void) {
    return g_func_count;
}

bool func_registry_validate_args(const FunctionInfo *func, const int *arg_types, int num_args) {
    if (!func) return false;

    if (func->num_args >= 0 && func->num_args != num_args) {
        return false;
    }

    for (int i = 0; i < num_args && i < func->num_args; i++) {
        if (func->args[i].type_oid != arg_types[i]) {
            return false;
        }
    }

    return true;
}

/* ========================================================================
 * 简化注册宏
 * ======================================================================== */

#define REGISTER_SCALAR_FUNC(name, schema, model, ret_oid, min_args, max_args) \
    do { \
        static FunctionInfo info = { \
            #name, schema, FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, \
            min_args, NULL, -1, ret_oid, NULL, NULL, #name, model, false \
        }; \
        info.num_args = max_args; \
        func_registry_register(&info); \
    } while(0)

/* ========================================================================
 * 内置函数注册
 * ======================================================================== */

int func_registry_register_builtins(void) {
    int ret = 0;
    ret |= func_registry_register_vector();
    ret |= func_registry_register_timeseries();
    ret |= func_registry_register_graph();
    ret |= func_registry_register_document();
    ret |= func_registry_register_spatial();
    ret |= func_registry_register_kv();
    ret |= func_registry_register_tree();
    return ret;
}

int func_registry_register_vector(void) {
    static FunctionInfo funcs[] = {
        {"VECTOR_SEARCH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 3, NULL, -1, 0, NULL, NULL, "向量搜索", MODEL_VECTOR, false},
        {"VECTOR_DISTANCE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 3, NULL, -1, 0, NULL, NULL, "向量距离", MODEL_VECTOR, false},
        {"EMBED_TEXT", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 1, NULL, -1, 0, NULL, NULL, "文本嵌入", MODEL_VECTOR, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

int func_registry_register_timeseries(void) {
    static FunctionInfo funcs[] = {
        {"TIME_SERIES_AGG", "public", FUNC_CLASS_AGGREGATE, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "时序聚合", MODEL_TIMESERIES, false},
        {"FIRST", "public", FUNC_CLASS_AGGREGATE, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "首值", MODEL_TIMESERIES, false},
        {"LAST", "public", FUNC_CLASS_AGGREGATE, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "末值", MODEL_TIMESERIES, false},
        {"RATE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "变化率", MODEL_TIMESERIES, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

int func_registry_register_graph(void) {
    static FunctionInfo funcs[] = {
        {"GRAPH_TRAVERSE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "图遍历", MODEL_GRAPH, false},
        {"GRAPH_SHORTEST_PATH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 3, NULL, -1, 0, NULL, NULL, "最短路径", MODEL_GRAPH, false},
        {"GRAPH_PAGERANK", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 1, NULL, -1, 0, NULL, NULL, "PageRank", MODEL_GRAPH, false},
        {"GRAPH_NEIGHBORS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "邻居查询", MODEL_GRAPH, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

int func_registry_register_document(void) {
    static FunctionInfo funcs[] = {
        {"MATCH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "全文匹配", MODEL_DOCUMENT, false},
        {"HIGHLIGHT", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "高亮", MODEL_DOCUMENT, false},
        {"JSON_EXTRACT", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "JSON 提取", MODEL_DOCUMENT, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

int func_registry_register_spatial(void) {
    static FunctionInfo funcs[] = {
        {"ST_Distance", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间距离", MODEL_SPATIAL, false},
        {"ST_Contains", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间包含", MODEL_SPATIAL, false},
        {"ST_Within", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间被包含", MODEL_SPATIAL, false},
        {"ST_Intersects", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间相交", MODEL_SPATIAL, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

int func_registry_register_kv(void) {
    static FunctionInfo funcs[] = {
        {"ZADD", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 3, NULL, -1, 0, NULL, NULL, "有序集合添加", MODEL_KV, false},
        {"ZSCORE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "获取分数", MODEL_KV, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

int func_registry_register_tree(void) {
    static FunctionInfo funcs[] = {
        {"TREE_ANCESTORS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 1, NULL, -1, 0, NULL, NULL, "祖先节点", MODEL_TREE, false},
        {"TREE_DESCENDANTS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "后代节点", MODEL_TREE, false},
        {"TREE_DEPTH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 1, NULL, -1, 0, NULL, NULL, "节点深度", MODEL_TREE, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}

/* ========================================================================
 * 函数执行器
 * ======================================================================== */

FuncExecutor *func_executor_create(const FunctionInfo *func) {
    if (!func) return NULL;

    FuncExecutor *exec = (FuncExecutor *)calloc(1, sizeof(FuncExecutor));
    if (exec) {
        exec->func = func;
        exec->args = (void **)calloc(func->num_args > 0 ? func->num_args : 16, sizeof(void *));
    }
    return exec;
}

int func_executor_set_arg(FuncExecutor *exec, int arg_idx, void *value) {
    if (!exec || !exec->func || arg_idx < 0) return -1;

    int max_args = exec->func->num_args > 0 ? exec->func->num_args : 16;
    if (arg_idx >= max_args) return -1;

    exec->args[arg_idx] = value;
    if (arg_idx >= exec->num_args) exec->num_args = arg_idx + 1;

    return 0;
}

int func_executor_execute(FuncExecutor *exec) {
    if (!exec || !exec->func) return -1;
    /* 简化实现 */
    return 0;
}

void *func_executor_get_result(FuncExecutor *exec) {
    return exec ? exec->result : NULL;
}

void func_executor_destroy(FuncExecutor *exec) {
    if (!exec) return;
    free(exec->args);
    free(exec);
}

/* ========================================================================
 * 类型系统
 * ======================================================================== */

int func_registry_init_types(void) {
    static TypeInfo builtin_types[] = {
        {"int4", 23, sizeof(int32_t), true, NULL, NULL},
        {"int8", 20, sizeof(int64_t), true, NULL, NULL},
        {"float4", 700, sizeof(float), true, NULL, NULL},
        {"float8", 701, sizeof(double), true, NULL, NULL},
        {"bool", 16, sizeof(bool), true, NULL, NULL},
        {"text", 25, -1, false, NULL, NULL},
        {"bytea", 17, -1, false, NULL, NULL},
        {"timestamp", 1114, sizeof(int64_t), false, NULL, NULL},
    };

    for (size_t i = 0; i < sizeof(builtin_types) / sizeof(builtin_types[0]); i++) {
        func_registry_register_type(&builtin_types[i]);
    }

    return 0;
}

int func_registry_register_type(const TypeInfo *info) {
    if (!info) return -1;

    TypeHashEntry *entry = (TypeHashEntry *)calloc(1, sizeof(TypeHashEntry));
    if (!entry) return -1;

    entry->oid = info->oid > 0 ? info->oid : g_next_oid++;
    entry->info = *info;
    entry->info.oid = entry->oid;

    HASH_ADD_INT(g_types, oid, entry);
    return 0;
}

const TypeInfo *func_registry_lookup_type(int oid) {
    TypeHashEntry *entry;
    HASH_FIND_INT(g_types, &oid, entry);
    return entry ? &entry->info : NULL;
}

int func_registry_get_type_oid(const char *name) {
    TypeHashEntry *t, *tmp;
    HASH_ITER(hh, g_types, t, tmp) {
        if (strcmp(t->info.name, name) == 0) {
            return t->info.oid;
        }
    }
    return 0;
}
