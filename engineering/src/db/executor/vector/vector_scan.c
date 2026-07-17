/**
 * @file vector_scan.c
 * @brief 向量扫描算子实现（暴力搜索）
 *
 * 实现 Volcano 迭代器协议的向量暴力搜索算子。
 * 直接调用 vector_engine_search() 进行全量搜索。
 */
#include "db/executor/vector/vector_scan.h"
#include "db/vector_engine.h"
#include "db/storage/vector/vector_engine.h"
#include <stdlib.h>
#include <string.h>

/* 向量扫描内部状态 */
typedef struct vector_scan_internal_s {
    vector_engine_db_t *engine;         /**< 向量引擎句柄 */
    vector_search_results_t results;    /**< 搜索结果 */
    int current_index;                  /**< 当前结果索引 */
    bool done;                          /**< 是否已返回所有结果 */
} vector_scan_internal_t;

VectorScanState *exec_vector_scan_init(PlanState *parent,
    float *query_vector, int top_k, int distance_metric)
{
    VectorScanState *state = (VectorScanState *)calloc(1, sizeof(VectorScanState));
    if (state == NULL) return NULL;

    state->ss.ps.type = EXEC_VECTOR_SCAN;
    state->ss.ps.left = NULL;
    state->ss.ps.right = NULL;
    if (parent) {
        state->ss.ps.expr_context = parent->expr_context;
    }
    state->query_vector = query_vector;
    state->top_k = top_k;
    state->distance_metric = distance_metric;
    state->index = NULL;
    state->ef = 0;
    state->nprobe = 0;

    /* 分配内部状态 */
    vector_scan_internal_t *internal = (vector_scan_internal_t *)calloc(1, sizeof(vector_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->current_index = 0;
    state->ss.ps.state = internal;

    return state;
}

TupleTableSlot *exec_vector_scan_next(VectorScanState *state)
{
    if (state == NULL) return NULL;

    vector_scan_internal_t *internal = (vector_scan_internal_t *)state->ss.ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时执行搜索 */
    if (internal->current_index == 0 && internal->results.count == 0) {
        /* 打开向量引擎 */
        internal->engine = (vector_engine_db_t *)vector_engine_open("default", ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }

        /* 执行暴力搜索 */
        int rc = vector_engine_search(internal->engine,
                                      state->query_vector,
                                      internal->engine->dimension,
                                      state->top_k,
                                      &internal->results);
        if (rc != 0) {
            internal->done = true;
            return NULL;
        }
    }

    /* 遍历结果 */
    if (internal->current_index >= internal->results.count) {
        internal->done = true;
        return NULL;
    }

    vector_search_result_t *result = &internal->results.results[internal->current_index];
    internal->current_index++;

    /* 创建并填充元组槽 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充：id (int64), distance (float), index (int32) */
    float dist = result->distance;
    slot->tts_values[0] = (void *)(uintptr_t)result->id;
    slot->tts_values[1] = (void *)(uintptr_t)(*(uint64_t *)&dist);
    slot->tts_values[2] = (void *)(uintptr_t)(internal->current_index - 1);
    slot->tts_nvalid = 3;

    return slot;
}

void exec_vector_scan_close(VectorScanState *state)
{
    if (state == NULL) return;

    vector_scan_internal_t *internal = (vector_scan_internal_t *)state->ss.ps.state;
    if (internal) {
        if (internal->engine) {
            vector_engine_close(internal->engine);
        }
        vector_engine_free_results(&internal->results);
        free(internal);
    }
    free(state);
}