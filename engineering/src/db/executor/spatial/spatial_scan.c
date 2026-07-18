/**
 * @file spatial_scan.c
 * @brief 空间扫描算子实现
 *
 * 实现 Volcano 迭代器协议的空间边界框查询算子。
 * 直接调用 spatial_engine_query_bbox() 进行空间查询。
 */
#include "db/executor/spatial/spatial_scan.h"
#include "db/spatial_engine.h"
#include <stdlib.h>
#include <string.h>

/* 空间扫描内部状态 */
typedef struct spatial_scan_internal_s {
    spatial_engine_db_t *engine;         /**< 空间引擎句柄 */
    spatial_query_result_t *results;     /**< 查询结果数组 */
    uint32_t num_results;                /**< 结果数量 */
    uint32_t current_index;              /**< 当前结果索引 */
    bool done;                           /**< 是否已返回所有结果 */
    char *collection_name;               /**< 集合名称 */
} spatial_scan_internal_t;

SpatialScanState *exec_spatial_scan_init(PlanState *parent,
    void *geometry, int spatial_op, void *bbox)
{
    SpatialScanState *state = (SpatialScanState *)calloc(1, sizeof(SpatialScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_SPATIAL_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->geometry = geometry;
    state->spatial_op = spatial_op;
    state->bbox = bbox;

    /* 分配内部状态 */
    spatial_scan_internal_t *internal = (spatial_scan_internal_t *)calloc(1, sizeof(spatial_scan_internal_t));
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

TupleTableSlot *exec_spatial_scan_next(SpatialScanState *state)
{
    if (state == NULL) return NULL;

    spatial_scan_internal_t *internal = (spatial_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时执行查询 */
    if (internal->current_index == 0 && internal->results == NULL) {
        /* 打开空间引擎 */
        internal->engine = (spatial_engine_db_t *)spatial_engine_open(internal->collection_name, ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }

        /* 准备查询参数 */
        spatial_query_args_t args;
        memset(&args, 0, sizeof(args));

        if (state->bbox != NULL) {
            bbox_t *query_bbox = (bbox_t *)state->bbox;
            args.bbox = *query_bbox;
        } else {
            /* 无边界框时使用全局范围 */
            args.bbox = bbox_create(-1e308, -1e308, 1e308, 1e308);
        }
        args.limit = 1000;  /* 默认限制 */
        args.offset = 0;

        /* 分配结果数组 */
        internal->results = (spatial_query_result_t *)calloc(args.limit, sizeof(spatial_query_result_t));
        if (internal->results == NULL) {
            spatial_engine_close(internal->engine);
            internal->engine = NULL;
            internal->done = true;
            return NULL;
        }

        /* 执行边界框查询 */
        int rc = spatial_engine_query_bbox(internal->engine, &args,
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
    /* 列：id (int64), bbox_min_x (double), bbox_min_y (double), bbox_max_x (double), bbox_max_y (double), distance (double) */
    ExecTupleDesc *desc = exec_make_tuple_desc(6);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充值 */
    slot->tts_values[0] = (void *)(uintptr_t)result->id;

    /* 对于边界框，我们使用 distance 字段临时存储坐标信息 */
    /* 实际应用中应该从 geometry 数据中解析 bbox */
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;

    /* 如果有几何数据，尝试解析 bbox（简化实现，假设 data 是 point_t） */
    if (result->data != NULL && result->len >= sizeof(point_t)) {
        point_t *pt = (point_t *)result->data;
        min_x = max_x = pt->x;
        min_y = max_y = pt->y;
    }

    /* 存储双精度浮点数需要通过位模式转换 */
    slot->tts_values[1] = (void *)(uintptr_t)(*(uint64_t *)&min_x);
    slot->tts_values[2] = (void *)(uintptr_t)(*(uint64_t *)&min_y);
    slot->tts_values[3] = (void *)(uintptr_t)(*(uint64_t *)&max_x);
    slot->tts_values[4] = (void *)(uintptr_t)(*(uint64_t *)&max_y);
    slot->tts_values[5] = (void *)(uintptr_t)(*(uint64_t *)&result->distance);
    slot->tts_nvalid = 6;

    return slot;
}

void exec_spatial_scan_close(SpatialScanState *state)
{
    if (state == NULL) return;

    spatial_scan_internal_t *internal = (spatial_scan_internal_t *)state->ps.state;
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
    if (state->geometry) {
        free(state->geometry);
    }
    if (state->bbox) {
        free(state->bbox);
    }
    free(state);
}
