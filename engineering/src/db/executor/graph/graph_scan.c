/**
 * @file graph_scan.c
 * @brief 图扫描算子实现
 *
 * 实现 Volcano 迭代器协议的图扫描算子。
 * 支持全图扫描、BFS 遍历和 DFS 遍历。
 *
 * 注意：当前实现硬编码集合名 "test_graph" 以匹配测试用例。
 * 未来应通过 PgCtx 或 PlanState 参数传递集合名。
 */
#include "db/executor/graph/graph_scan.h"
#include "db/graph_engine.h"
#include "db/graph/graph.h"
#include "db/graph/traverse.h"
#include <stdlib.h>
#include <string.h>

/* 图扫描内部状态 */
typedef struct graph_scan_internal_s {
    graph_t *graph;                    /**< 图句柄 */
    graph_traverser_t *traverser;      /**< 遍历器 */
    bool done;                         /**< 是否完成 */
    bool is_full_scan;                 /**< 是否全图扫描 */
    graph_vertex_id_t *all_vertices;   /**< 全图扫描缓存 */
    size_t num_vertices;               /**< 顶点数 */
    size_t current_index;              /**< 当前索引 */
    char *collection_name;             /**< 集合名称 */
    int max_depth;                     /**< 最大遍历深度 */
    graph_traverse_mode_t mode;        /**< 遍历模式 (BFS/DFS) */
    graph_engine_db_t *engine_db;      /**< 图引擎数据库句柄 */
} graph_scan_internal_t;

/* 全图扫描收集上下文 */
typedef struct {
    graph_vertex_id_t *vids;
    size_t count;
    size_t capacity;
} vertex_collect_ctx_t;

/* 全图扫描回调 */
static int collect_vertex_callback(graph_vertex_id_t vid, void *data) {
    vertex_collect_ctx_t *ctx = (vertex_collect_ctx_t *)data;
    if (!ctx) return -1;

    if (ctx->count >= ctx->capacity) {
        ctx->capacity = ctx->capacity ? ctx->capacity * 2 : 64;
        ctx->vids = realloc(ctx->vids, sizeof(graph_vertex_id_t) * ctx->capacity);
        if (!ctx->vids) return -1;
    }

    ctx->vids[ctx->count++] = vid;
    return 0;
}

GraphScanState *exec_graph_scan_init(PlanState *parent,
    void *start_vertex, int max_depth, void *traversal_pattern,
    const char *collection_name)
{
    GraphScanState *state = (GraphScanState *)calloc(1, sizeof(GraphScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_GRAPH_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->start_vertex = start_vertex;
    state->max_depth = max_depth;
    state->traversal_pattern = traversal_pattern;

    /* 分配内部状态 */
    graph_scan_internal_t *internal = (graph_scan_internal_t *)calloc(1, sizeof(graph_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }

    internal->done = false;
    internal->current_index = 0;
    internal->all_vertices = NULL;
    internal->num_vertices = 0;
    /* 集合名：优先使用参数，否则使用默认值 */
    internal->collection_name = collection_name ? strdup(collection_name) : strdup("default_graph");
    internal->max_depth = max_depth;
    internal->is_full_scan = (start_vertex == NULL);

    /* 从 traversal_pattern 获取遍历模式 */
    if (traversal_pattern != NULL) {
        internal->mode = *(graph_traverse_mode_t *)traversal_pattern;
    } else {
        internal->mode = GRAPH_TRAVERSE_BFS;  /* 默认 BFS */
    }

    state->ps.state = internal;

    return state;
}

TupleTableSlot *exec_graph_scan_next(GraphScanState *state)
{
    if (state == NULL) return NULL;

    graph_scan_internal_t *internal = (graph_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时初始化 */
    if (internal->current_index == 0 && internal->graph == NULL) {
        /* 打开图引擎 */
        internal->engine_db = (graph_engine_db_t *)graph_engine_open(internal->collection_name, ACCESS_MODE_READ);
        if (internal->engine_db == NULL) {
            internal->done = true;
            return NULL;
        }
        internal->graph = internal->engine_db->graph;

        if (internal->is_full_scan) {
            /* 全图扫描：收集所有顶点 */
            vertex_collect_ctx_t ctx = { .vids = NULL, .count = 0, .capacity = 0 };
            graph_scan_vertices(internal->graph, NULL, collect_vertex_callback, &ctx);

            internal->all_vertices = ctx.vids;
            internal->num_vertices = ctx.count;
        } else {
            /* 遍历模式：创建遍历器 */
            graph_traverse_config_t config = {
                .mode = internal->mode,
                .direction = GRAPH_TRAVERSE_OUT,
                .max_depth = internal->max_depth,
                .visited_set = true
            };

            internal->traverser = graph_traverser_create(internal->graph, &config);
            if (internal->traverser == NULL) {
                graph_engine_close(internal->engine_db);
                internal->engine_db = NULL;
                internal->graph = NULL;
                internal->done = true;
                return NULL;
            }

            /* 启动遍历 */
            graph_vertex_id_t start_vid = *(graph_vertex_id_t *)state->start_vertex;
            graph_traverser_start(internal->traverser, start_vid);
        }
    }

    graph_vertex_id_t vid = GRAPH_INVALID_ID;
    int depth = 0;

    if (internal->is_full_scan) {
        /* 全图扫描 */
        if (internal->current_index >= internal->num_vertices) {
            internal->done = true;
            return NULL;
        }
        vid = internal->all_vertices[internal->current_index++];
        depth = 0;  /* 全图扫描没有深度概念 */
    } else {
        /* 遍历模式 */
        if (!graph_traverser_next(internal->traverser, &vid, &depth)) {
            internal->done = true;
            return NULL;
        }
    }

    /* 获取顶点信息 */
    graph_vertex_t *vertex = NULL;
    graph_vertex_get(internal->graph, vid, &vertex);

    /* 获取边数量 */
    uint32_t num_edges = 0;
    if (vertex) {
        num_edges = vertex->out_degree + vertex->in_degree;
    }

    /* 创建元组描述符：vertex_id, depth, label, num_edges */
    ExecTupleDesc *desc = exec_make_tuple_desc(4);
    if (desc == NULL) {
        if (vertex) free(vertex);
        return NULL;
    }

    /* 设置列信息 */
    desc->attrs[0].name = "vertex_id";
    desc->attrs[0].type = 20;  /* int8 / uint64 */

    desc->attrs[1].name = "depth";
    desc->attrs[1].type = 23;  /* int4 */

    desc->attrs[2].name = "label";
    desc->attrs[2].type = 25;  /* text */

    desc->attrs[3].name = "num_edges";
    desc->attrs[3].type = 23;  /* int4 */

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        if (vertex) free(vertex);
        return NULL;
    }

    /* 填充值 */
    slot->tts_values[0] = (void *)(uintptr_t)vid;
    slot->tts_isnull[0] = false;

    /* depth 作为 int32 存储 */
    int32_t depth_val = (int32_t)depth;
    slot->tts_values[1] = (void *)(uintptr_t)(*(uint32_t *)&depth_val);
    slot->tts_isnull[1] = false;

    /* label */
    const char *label_name = NULL;
    if (vertex && vertex->n_labels > 0) {
        label_name = graph_get_label_name(internal->graph, vertex->label_ids[0]);
    }
    slot->tts_values[2] = (void *)label_name;
    slot->tts_isnull[2] = (label_name == NULL);

    /* num_edges */
    slot->tts_values[3] = (void *)(uintptr_t)num_edges;
    slot->tts_isnull[3] = false;

    slot->tts_nvalid = 4;

    if (vertex) free(vertex);

    return slot;
}

void exec_graph_scan_close(GraphScanState *state)
{
    if (state == NULL) return;

    graph_scan_internal_t *internal = (graph_scan_internal_t *)state->ps.state;
    if (internal) {
        /* 销毁遍历器 */
        if (internal->traverser) {
            graph_traverser_destroy(internal->traverser);
            internal->traverser = NULL;
        }

        /* 释放全图扫描缓存 */
        if (internal->all_vertices) {
            free(internal->all_vertices);
            internal->all_vertices = NULL;
        }

        /* 关闭图引擎 */
        if (internal->engine_db) {
            graph_engine_close(internal->engine_db);
            internal->engine_db = NULL;
            internal->graph = NULL;
        }

        /* 释放集合名称 */
        if (internal->collection_name) {
            free(internal->collection_name);
            internal->collection_name = NULL;
        }

        free(internal);
        state->ps.state = NULL;
    }

    /* 注意：不释放 state->start_vertex 和 state->traversal_pattern，
     * 这些指针由调用者拥有（可能指向栈变量或外部 malloc 内存） */

    free(state);
}
