/**
 * @file gql_exec.c
 * @brief GQL 执行引擎
 *
 * 执行 GQL 查询（CREATE, MATCH, DELETE 等）
 */
#include "db/parser/graph/gql.h"
#include "db/executor/graph/traverse/traverse.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 执行上下文
 * ============================================================ */

/** 变量绑定项 */
typedef struct gql_binding_s {
    char                name[64];
    graph_vertex_id_t   vid;
} gql_binding_t;

/** 执行上下文 */
typedef struct gql_exec_ctx_s {
    graph_t             *graph;
    gql_binding_t       *bindings;
    size_t              binding_count;
} gql_exec_ctx_t;

/* ============================================================
 * 工具函数
 * ============================================================ */

static int add_binding(gql_exec_ctx_t *ctx, const char *name, graph_vertex_id_t vid) {
    if (!ctx || !name) return -1;
    for (size_t i = 0; i < ctx->binding_count; i++) {
        if (strcmp(ctx->bindings[i].name, name) == 0) {
            ctx->bindings[i].vid = vid;
            return 0;
        }
    }
    ctx->bindings = realloc(ctx->bindings, sizeof(gql_binding_t) * (ctx->binding_count + 1));
    strncpy(ctx->bindings[ctx->binding_count].name, name, 63);
    ctx->bindings[ctx->binding_count++].vid = vid;
    return 0;
}

static graph_vertex_id_t get_binding(gql_exec_ctx_t *ctx, const char *name) {
    if (!ctx || !name) return GRAPH_INVALID_ID;
    for (size_t i = 0; i < ctx->binding_count; i++) {
        if (strcmp(ctx->bindings[i].name, name) == 0) {
            return ctx->bindings[i].vid;
        }
    }
    return GRAPH_INVALID_ID;
}

/* ============================================================
 * 顶点收集上下文
 * ============================================================ */

typedef struct {
    graph_vertex_id_t *vids;
    size_t count;
    size_t capacity;
} collect_ctx_t;

static int collect_vertex_callback(graph_vertex_id_t vid, void *data) {
    collect_ctx_t *ctx = (collect_ctx_t *)data;
    if (!ctx) return -1;
    if (ctx->count >= ctx->capacity) {
        ctx->capacity = ctx->capacity ? ctx->capacity * 2 : 64;
        ctx->vids = realloc(ctx->vids, sizeof(graph_vertex_id_t) * ctx->capacity);
    }
    if (ctx->vids) {
        ctx->vids[ctx->count++] = vid;
    }
    return 0;
}

/* ============================================================
 * 模式匹配
 * ============================================================ */

static int match_node_pattern(gql_exec_ctx_t *ctx,
                              gql_node_t *pattern,
                              graph_vertex_id_t **out_vids,
                              size_t *out_count) {
    if (!pattern || pattern->type != GQL_NODE_NODE_PATTERN) {
        return -1;
    }

    *out_vids = NULL;
    *out_count = 0;

    const char *label = pattern->u.node_pattern.label;

    collect_ctx_t collect_ctx = { .vids = NULL, .count = 0, .capacity = 0 };
    graph_scan_vertices(ctx->graph, label, collect_vertex_callback, &collect_ctx);

    *out_vids = collect_ctx.vids;
    *out_count = collect_ctx.count;

    return 0;
}

/* ============================================================
 * 属性辅助
 * ============================================================ */

static graph_prop_t *create_props_from_map(gql_node_t *props_node, size_t *out_count) {
    *out_count = 0;
    if (!props_node || props_node->type != GQL_NODE_PROPERTY_MAP) return NULL;

    size_t n = props_node->u.property_map.count;
    graph_prop_t *props = calloc(n, sizeof(graph_prop_t));
    if (!props) return NULL;

    for (size_t j = 0; j < n; j++) {
        strncpy(props[j].key, props_node->u.property_map.keys[j], GRAPH_MAX_PROP_NAME - 1);
        gql_node_t *val = props_node->u.property_map.values[j];
        if (val && val->type == GQL_NODE_CONSTANT) {
            props[j].type = val->u.constant.value_type;
            memcpy(&props[j].value, &val->u.constant.value.u, sizeof(props[j].value));
        }
    }

    *out_count = n;
    return props;
}

/* ============================================================
 * 执行 CREATE
 * ============================================================ */

static int exec_create(graph_t *g, gql_node_t *node, gql_result_t **out_result) {
    if (!node || node->type != GQL_NODE_CREATE) return -1;

    gql_result_t *result = calloc(1, sizeof(gql_result_t));
    if (!result) return -1;

    gql_node_t *patterns = node->u.create.patterns;
    if (!patterns || patterns->type != GQL_NODE_PATTERN_LIST) {
        gql_result_free(result);
        return -1;
    }

    gql_exec_ctx_t ctx = { .graph = g, .bindings = NULL, .binding_count = 0 };

    for (size_t i = 0; i < patterns->u.pattern_list.count; i++) {
        gql_node_t *pattern = patterns->u.pattern_list.patterns[i];

        if (pattern && pattern->type == GQL_NODE_NODE_PATTERN) {
            const char *label = pattern->u.node_pattern.label;
            graph_prop_t *props = NULL;
            size_t n_props = 0;

            if (pattern->u.node_pattern.props) {
                props = create_props_from_map(pattern->u.node_pattern.props, &n_props);
            }

            graph_vertex_id_t vid = graph_vertex_create(g, label, props, n_props);

            if (props) free(props);

            if (vid != GRAPH_INVALID_ID) {
                /* 记录变量绑定 */
                if (pattern->u.node_pattern.variable) {
                    add_binding(&ctx, pattern->u.node_pattern.variable, vid);
                }
                result->row_count++;
            }
        }
    }

    free(ctx.bindings);
    *out_result = result;
    return 0;
}

/* ============================================================
 * 执行 MATCH
 * ============================================================ */

static int exec_match(graph_t *g, gql_node_t *node, gql_result_t **out_result) {
    if (!node || node->type != GQL_NODE_QUERY) return -1;

    gql_result_t *result = calloc(1, sizeof(gql_result_t));
    if (!result) return -1;

    gql_exec_ctx_t ctx = { .graph = g, .bindings = NULL, .binding_count = 0 };
    gql_node_t *patterns = node->u.query.patterns;
    gql_node_t *ret = node->u.query.ret;

    if (!patterns || patterns->type != GQL_NODE_PATTERN_LIST) {
        free(ctx.bindings);
        *out_result = result;
        return 0;
    }

    /* 收集 RETURN 列信息 */
    if (ret && ret->type == GQL_NODE_PATTERN_LIST) {
        result->col_count = ret->u.pattern_list.count;
        result->columns = calloc(result->col_count, sizeof(char*));
        for (size_t c = 0; c < result->col_count; c++) {
            gql_node_t *item = ret->u.pattern_list.patterns[c];
            if (item->type == GQL_NODE_VARIABLE) {
                result->columns[c] = strdup(item->u.variable.name);
            }
        }
    }

    /* 找出第一个节点模式 */
    gql_node_t *node_pattern = NULL;
    for (size_t i = 0; i < patterns->u.pattern_list.count; i++) {
        gql_node_t *p = patterns->u.pattern_list.patterns[i];
        if (p && p->type == GQL_NODE_NODE_PATTERN) {
            node_pattern = p;
            break;
        }
    }

    if (node_pattern) {
        graph_vertex_id_t *vids = NULL;
        size_t count = 0;
        match_node_pattern(&ctx, node_pattern, &vids, &count);

        for (size_t i = 0; i < count; i++) {
            graph_vertex_id_t vid = vids[i];

            /* 绑定变量 */
            if (node_pattern->u.node_pattern.variable) {
                add_binding(&ctx, node_pattern->u.node_pattern.variable, vid);
            }

            /* 添加结果行 */
            result->rows = realloc(result->rows, sizeof(void*) * (result->row_count + 1));
            uint64_t *row = malloc(sizeof(uint64_t) * (result->col_count > 0 ? result->col_count : 1));

            for (size_t c = 0; c < result->col_count; c++) {
                gql_node_t *item = ret->u.pattern_list.patterns[c];
                if (item->type == GQL_NODE_VARIABLE) {
                    const char *var_name = item->u.variable.name;
                    if (strcmp(var_name, "*") == 0) {
                        row[c] = vid;
                    } else {
                        graph_vertex_id_t bound_vid = get_binding(&ctx, var_name);
                        row[c] = bound_vid != GRAPH_INVALID_ID ? bound_vid : 0;
                    }
                }
            }

            result->rows[result->row_count++] = row;
        }

        free(vids);
    }

    free(ctx.bindings);
    *out_result = result;
    return 0;
}

/* ============================================================
 * 执行 DELETE
 * ============================================================ */

static int exec_delete(graph_t *g, gql_node_t *node, gql_result_t **out_result) {
    (void)g;
    (void)node;
    if (!node || node->type != GQL_NODE_DELETE) return -1;

    gql_result_t *result = calloc(1, sizeof(gql_result_t));
    if (!result) return -1;

    *out_result = result;
    return 0;
}

/* ============================================================
 * 执行入口
 * ============================================================ */

int gql_exec(graph_t *g, const char *query, gql_result_t **out_result) {
    if (!g || !query || !out_result) return -1;

    *out_result = NULL;

    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return -1;

    gql_node_t *node = gql_parse_one(parser);
    if (!node) {
        gql_parser_destroy(parser);
        return -1;
    }

    int ret = gql_exec_ast(g, node, out_result);

    gql_node_free(node);
    gql_parser_destroy(parser);

    return ret;
}

int gql_exec_ast(graph_t *g, gql_node_t *node, gql_result_t **out_result) {
    if (!g || !node || !out_result) return -1;

    *out_result = NULL;

    switch (node->type) {
        case GQL_NODE_CREATE:
            return exec_create(g, node, out_result);
        case GQL_NODE_QUERY:
            return exec_match(g, node, out_result);
        case GQL_NODE_DELETE:
            return exec_delete(g, node, out_result);
        default:
            return -1;
    }
}

void gql_result_free(gql_result_t *result) {
    if (!result) return;

    for (size_t i = 0; i < result->col_count; i++) {
        free(result->columns[i]);
    }
    free(result->columns);

    for (size_t i = 0; i < result->row_count; i++) {
        free(result->rows[i]);
    }
    free(result->rows);

    free(result);
}

void gql_result_print(const gql_result_t *result) {
    if (!result) {
        printf("(empty result)\n");
        return;
    }

    for (size_t c = 0; c < result->col_count; c++) {
        if (c > 0) printf("\t");
        printf("%s", result->columns[c] ? result->columns[c] : "col");
    }
    printf("\n");

    for (size_t r = 0; r < result->row_count; r++) {
        uint64_t *row = (uint64_t*)result->rows[r];
        for (size_t c = 0; c < result->col_count; c++) {
            if (c > 0) printf("\t");
            printf("%lu", (unsigned long)row[c]);
        }
        printf("\n");
    }
}
