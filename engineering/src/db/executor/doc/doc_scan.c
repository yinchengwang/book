/**
 * @file doc_scan.c
 * @brief 文档扫描算子实现
 *
 * 实现 Volcano 迭代器协议的文档扫描算子。
 * 直接调用 doc_engine 进行 JSONPath 查询或全文搜索，
 * 将结果逐行返回给上层算子。
 */
#include "db/executor/doc/doc_scan.h"
#include "db/doc_engine.h"
#include <stdlib.h>
#include <string.h>

/* 文档扫描内部状态 */
typedef struct doc_scan_internal_s {
    doc_engine_db_t        *engine;          /**< 文档引擎句柄 */
    doc_jsonpath_result_t  *jsonpath_results; /**< JSONPath 查询结果 */
    doc_query_result_t      query_results;   /**< 基础查询结果 */
    int                     current_index;   /**< 当前结果索引 */
    int                     total_count;     /**< 结果总数 */
    bool                    done;            /**< 是否已返回所有结果 */
    bool                    engine_opened;   /**< 引擎是否已打开 */
    bool                    query_executed;  /**< 查询是否已执行 */
} doc_scan_internal_t;

DocScanState *exec_doc_scan_init(PlanState *parent,
    void *json_path, void *fulltext_query)
{
    DocScanState *state = (DocScanState *)calloc(1, sizeof(DocScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_DOCUMENT_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->json_path = json_path;
    state->fulltext_query = fulltext_query;
    state->is_fulltext = (fulltext_query != NULL) ? 1 : 0;

    /* 分配内部状态 */
    doc_scan_internal_t *internal = (doc_scan_internal_t *)calloc(1, sizeof(doc_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->engine_opened = false;
    internal->query_executed = false;
    internal->current_index = 0;
    internal->total_count = 0;
    internal->jsonpath_results = NULL;
    memset(&internal->query_results, 0, sizeof(doc_query_result_t));
    state->ps.state = internal;

    return state;
}

TupleTableSlot *exec_doc_scan_next(DocScanState *state)
{
    if (state == NULL) return NULL;

    doc_scan_internal_t *internal = (doc_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时打开引擎并执行查询 */
    if (!internal->engine_opened) {
        internal->engine = (doc_engine_db_t *)doc_engine_open("default", ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }
        internal->engine_opened = true;
    }

    /* 首次调用时执行查询 */
    if (!internal->query_executed) {
        internal->query_executed = true;

        if (state->is_fulltext) {
            /* 全文搜索 */
            const char *fts_query = (const char *)state->fulltext_query;
            uint32_t max_results = 100;
            int rc = doc_engine_query_jsonpath(internal->engine,
                                               fts_query,
                                               &internal->jsonpath_results,
                                               max_results);
            if (rc <= 0) {
                internal->done = true;
                return NULL;
            }
            internal->total_count = rc;
        } else {
            /* JSONPath 查询 */
            const char *jp = (const char *)state->json_path;
            if (jp == NULL) {
                internal->done = true;
                return NULL;
            }

            uint32_t max_results = 100;
            int rc = doc_engine_query_jsonpath(internal->engine,
                                               jp,
                                               &internal->jsonpath_results,
                                               max_results);
            if (rc <= 0) {
                internal->done = true;
                return NULL;
            }
            internal->total_count = rc;
        }
    }

    /* 遍历结果 */
    if (internal->current_index >= internal->total_count) {
        internal->done = true;
        return NULL;
    }

    doc_jsonpath_result_t *result = &internal->jsonpath_results[internal->current_index];
    internal->current_index++;

    /* 创建元组描述符：3 列 — doc_id(string), field_value(string), json_path(string) */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充列值 — 字符串指针直接存储，上层负责拷贝 */
    slot->tts_values[0] = result->doc_id;      /* 文档 ID */
    slot->tts_values[1] = result->value;        /* 字段值 */
    slot->tts_values[2] = result->jsonpath;     /* 匹配的 JSONPath */
    slot->tts_nvalid = 3;

    return slot;
}

void exec_doc_scan_close(DocScanState *state)
{
    if (state == NULL) return;

    doc_scan_internal_t *internal = (doc_scan_internal_t *)state->ps.state;
    if (internal) {
        if (internal->engine_opened && internal->engine) {
            doc_engine_close(internal->engine);
        }
        if (internal->jsonpath_results) {
            doc_engine_free_jsonpath_results(internal->jsonpath_results,
                                             (uint32_t)internal->total_count);
        }
        if (internal->query_results.docs) {
            doc_engine_free_results(&internal->query_results);
        }
        free(internal);
    }
    free(state);
}