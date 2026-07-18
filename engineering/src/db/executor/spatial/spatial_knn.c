/**
 * @file spatial_knn.c
 * @brief 空间 KNN 搜索算子实现
 *
 * 实现 Volcano 迭代器协议的空间最近邻搜索算子。
 * 直接调用 spatial_engine_nearest() 进行 KNN 查询。
 */
#include "db/executor/spatial/spatial_knn.h"
#include "db/spatial_engine.h"
#include <stdlib.h>
#include <string.h>

/* 空间 KNN 内部状态 */
typedef struct spatial_knn_internal_s {
    spatial_engine_db_t *engine;         /**< 空间引擎句柄 */
    spatial_query_result_t *results;     /**< 搜索结果数组 */
    uint32_t num_results;                /**< 结果数量 */
    uint32_t current_index;              /**< 当前结果索引 */
    bool done;                           /**< 是否已返回所有结果 */
    char *collection_name;               /**< 集合名称 */
} spatial_knn_internal_t;

SpatialKnnState *exec_spatial_knn_init(PlanState *parent,
    void *query_point, int top_k)
{
    SpatialKnnState *state = (SpatialKnnState *)calloc(1, sizeof(SpatialKnnState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_SPATIAL_SCAN;  /* 复用 SPATIAL_SCAN 类型 */
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->query_point = query_point;
    state->top_k = top_k;
    state->index = NULL;

    /* 分配内部状态 */
    spatial_knn_internal_t *internal = (spatial_knn_internal_t *)calloc(1, sizeof(spatial_knn_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->current_index = 0;
    internal->results = NULL;
    internal->num_results = 0;
    internal->collection_name = strdup("default");
    state->ps.state = internal;

    return state;
}

TupleTableSlot *exec_spatial_knn_next(SpatialKnnState *state)
{
    if (state == NULL) return NULL;

    spatial_knn_internal_t *internal = (spatial_knn_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时执行搜索 */
    if (internal->current_index == 0 && internal->results == NULL) {
        /* 打开空间引擎 */
        internal->engine = (spatial_engine_db_t *)spatial_engine_open(internal->collection_name, ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }

        /* 分配结果数组 */
        uint32_t max_results = (uint32_t)state->top_k;
        internal->results = (spatial_query_result_t *)calloc(max_results, sizeof(spatial_query_result_t));
        if (internal->results == NULL) {
            spatial_engine_close(internal->engine);
            internal->engine = NULL;
            internal->done = true;
            return NULL;
        }

        /* 执行最近邻搜索 */
        point_t *pt = (point_t *)state->query_point;
        int rc = spatial_engine_nearest(internal->engine, pt, max_results,
                                         internal->results, &internal->num_results);
        if (rc != 0) {
            free(internal->results);
            internal->results = NULL;
            spatial_engine_close(internal->engine);
            internal->engine = NULL;
            internal->done = true;
            return NULL;
        }
    }

    /* 遍历结果 */
    if (internal->current_index >= internal->num_results) {
        internal->done = true;
        return NULL;
    }

    spatial_query_result_t *result = &internal->results[internal->current_index];
    internal->current_index++;

    /* 创建并填充元组槽 */
    /* 列：id (int64), distance (double), x (double), y (double) */
    ExecTupleDesc *desc = exec_make_tuple_desc(4);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充值 */
    slot->tts_values[0] = (void *)(uintptr_t)result->id;

    /* 距离 */
    double dist = result->distance;
    slot->tts_values[1] = (void *)(uintptr_t)(*(uint64_t *)&dist);

    /* 坐标（从几何数据中解析，假设 data 是 point_t） */
    double x = 0.0, y = 0.0;
    if (result->data != NULL && result->len >= sizeof(point_t)) {
        point_t *pt = (point_t *)result->data;
        x = pt->x;
        y = pt->y;
    }

    slot->tts_values[2] = (void *)(uintptr_t)(*(uint64_t *)&x);
    slot->tts_values[3] = (void *)(uintptr_t)(*(uint64_t *)&y);
    slot->tts_nvalid = 4;

    return slot;
}

void exec_spatial_knn_close(SpatialKnnState *state)
{
    if (state == NULL) return;

    spatial_knn_internal_t *internal = (spatial_knn_internal_t *)state->ps.state;
    if (internal) {
        if (internal->engine) {
            spatial_engine_close(internal->engine);
        }
        if (internal->results) {
            /* 释放每个结果的数据 */
            for (uint32_t i = 0; i < internal->num_results; i++) {
                if (internal->results[i].data != NULL) {
                    free(internal->results[i].data);
                }
            }
            free(internal->results);
        }
        if (internal->collection_name) {
            free(internal->collection_name);
        }
        free(internal);
    }
    if (state->query_point) {
        free(state->query_point);
    }
    free(state);
}