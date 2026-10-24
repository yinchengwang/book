/**
 * @file result.c
 * @brief mmdb_result_t / mmdb_path_t 释放辅助
 */
#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"

#include <stdlib.h>

/* 释放单个 result item 的堆内存 */
static void result_item_free(mmdb_result_item_t* it) {
    if (!it) return;
    free(it->id);
    free(it->metadata_json);
    free(it->text);
    it->id = NULL;
    it->metadata_json = NULL;
    it->text = NULL;
}

void mmdb_result_release(mmdb_result_t* result) {
    if (!result) return;
    if (result->items) {
        for (size_t i = 0; i < result->count; i++) {
            result_item_free(&result->items[i]);
        }
        free(result->items);
    }
    result->items = NULL;
    result->count = 0;
}

void mmdb_result_free(mmdb_result_t* result) {
    /* mmdb_result_t 可由调用方栈分配或堆分配；本函数仅释放内部堆内存，
     * 不调用 free(result)。如需释放结构体本身，调用方应自行管理。 */
    mmdb_result_release(result);
}

static void path_node_free(mmdb_path_node_t* n) {
    if (!n) return;
    /* path_node_t 字段均为 const char*，由 mmdb_path_t 拥有 */
    (void)n;
}

static void edge_free(mmdb_edge_t* e) {
    if (!e) return;
    /* mmdb_edge_t 字段均为 const char*，由 mmdb_path_t 拥有 */
    (void)e;
}

void mmdb_path_release(mmdb_path_t* path) {
    if (!path) return;
    if (path->nodes) {
        for (size_t i = 0; i < path->node_count; i++) {
            path_node_free(&path->nodes[i]);
        }
        free((void*)path->nodes);
        path->nodes = NULL;
    }
    if (path->edges) {
        for (size_t i = 0; i < path->edge_count; i++) {
            edge_free(&path->edges[i]);
        }
        free((void*)path->edges);
        path->edges = NULL;
    }
    path->node_count = 0;
    path->edge_count = 0;
}

void mmdb_path_free(mmdb_path_t* path) {
    /* 同 mmdb_result_free：仅释放内部堆内存 */
    mmdb_path_release(path);
}