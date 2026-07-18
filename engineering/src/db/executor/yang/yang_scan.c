/**
 * @file yang_scan.c
 * @brief Yang 路径扫描算子实现
 *
 * 实现 Volcano 迭代器协议的 Yang 路径扫描算子。
 * 支持全树遍历、路径匹配和 XPath 过滤。
 */
#include "db/executor/yang/yang_scan.h"
#include "db/storage/yang/yang_engine.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>

/* Yang 扫描内部状态 */
typedef struct yang_scan_internal_s {
    yang_engine_db_t *engine;          /**< Yang 引擎句柄 */
    yang_node_t **node_buffer;         /**< 节点缓存 */
    uint32_t num_nodes;                /**< 节点数量 */
    uint32_t current_index;            /**< 当前索引 */
    bool done;                         /**< 是否完成 */
    bool traversed;                    /**< 是否已遍历 */
    char *collection_name;             /**< 集合名称 */
    char *xpath_filter;                /**< XPath 过滤条件 */
} yang_scan_internal_t;

/* 遍历回调上下文 */
typedef struct traverse_ctx_s {
    yang_node_t **buffer;              /**< 节点缓存 */
    uint32_t capacity;                 /**< 缓存容量 */
    uint32_t count;                    /**< 当前计数 */
} traverse_ctx_t;

/* XPath 过滤辅助函数 */
static bool match_xpath_filter(const char *path, const char *xpath_filter)
{
    if (xpath_filter == NULL || xpath_filter[0] == '\0') {
        return true;  /* 无过滤条件，全部通过 */
    }

    /* 简化 XPath 匹配：支持前缀匹配和通配符 */
    size_t filter_len = strlen(xpath_filter);

    /* ** 匹配所有后代 */
    if (strcmp(xpath_filter, "**") == 0) {
        return true;
    }

    /* 前缀匹配：/config/system/ 通配 */
    if (filter_len > 0 && xpath_filter[filter_len - 1] == '*') {
        size_t prefix_len = filter_len - 1;
        if (prefix_len > 0 && xpath_filter[prefix_len - 1] == '/') {
            prefix_len--;
        }
        return strncmp(path, xpath_filter, prefix_len) == 0;
    }

    /* 精确匹配 */
    return strcmp(path, xpath_filter) == 0;
}

/* 遍历回调函数 */
static bool traverse_callback(const char *path, yang_node_t *node, void *ctx)
{
    (void)path;
    traverse_ctx_t *tctx = (traverse_ctx_t *)ctx;

    if (tctx->count >= tctx->capacity) {
        /* 扩容 */
        uint32_t new_capacity = tctx->capacity * 2;
        yang_node_t **new_buffer = (yang_node_t **)realloc(tctx->buffer,
                                                            new_capacity * sizeof(yang_node_t *));
        if (new_buffer == NULL) {
            return false;  /* 内存不足，停止遍历 */
        }
        tctx->buffer = new_buffer;
        tctx->capacity = new_capacity;
    }

    tctx->buffer[tctx->count++] = node;
    return true;
}

YangScanState *exec_yang_scan_init(PlanState *parent, void *yang_path, void *xpath_filter)
{
    YangScanState *state = (YangScanState *)calloc(1, sizeof(YangScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_VALUES_SCAN;  /* 复用 VALUES_SCAN 类型 */
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->yang_path = yang_path;
    state->xpath_filter = xpath_filter;

    /* 分配内部状态 */
    yang_scan_internal_t *internal = (yang_scan_internal_t *)calloc(1, sizeof(yang_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->traversed = false;
    internal->current_index = 0;
    internal->node_buffer = NULL;
    internal->num_nodes = 0;
    internal->collection_name = strdup("test_yang");
    internal->xpath_filter = xpath_filter ? strdup((const char *)xpath_filter) : NULL;
    state->ps.state = internal;

    return state;
}

/**
 * @brief 初始化集合并加载节点
 *
 * @param state 扫描状态
 * @param collection_name 集合名称
 * @return 0 成功，非 0 失败
 */
static int init_collection(yang_scan_internal_t *internal, const char *collection_name)
{
    /* 引擎应该已经在外部初始化 */
    /* 打开集合 */
    internal->engine = (yang_engine_db_t *)yang_engine_open(collection_name, ACCESS_MODE_READ);
    if (internal->engine == NULL) {
        return -1;
    }

    return 0;
}

TupleTableSlot *exec_yang_scan_next(YangScanState *state)
{
    if (state == NULL) return NULL;

    yang_scan_internal_t *internal = (yang_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时初始化集合 */
    if (!internal->traversed) {
        internal->traversed = true;

        int rc = init_collection(internal, internal->collection_name);
        if (rc != 0) {
            internal->done = true;
            return NULL;
        }

        /* 分配初始缓存 */
        uint32_t initial_capacity = 64;
        internal->node_buffer = (yang_node_t **)calloc(initial_capacity, sizeof(yang_node_t *));
        if (internal->node_buffer == NULL) {
            yang_engine_close(internal->engine);
            internal->engine = NULL;
            internal->done = true;
            return NULL;
        }

        if (state->yang_path != NULL) {
            /* 路径匹配模式：查找特定节点 */
            const char *path = (const char *)state->yang_path;
            yang_node_t *node = yang_engine_find(internal->engine, path);
            if (node != NULL) {
                internal->node_buffer[0] = node;
                internal->num_nodes = 1;
            } else {
                internal->num_nodes = 0;
            }
        } else {
            /* 全树遍历模式 */
            traverse_ctx_t ctx = {
                .buffer = internal->node_buffer,
                .capacity = initial_capacity,
                .count = 0
            };

            int count = yang_engine_traverse(internal->engine, traverse_callback, &ctx);
            internal->node_buffer = ctx.buffer;
            internal->num_nodes = (uint32_t)count;
        }
    }

    /* 检查是否遍历完毕 */
    if (internal->current_index >= internal->num_nodes) {
        internal->done = true;
        return NULL;
    }

    /* 获取当前节点 */
    yang_node_t *node = internal->node_buffer[internal->current_index];
    internal->current_index++;

    /* XPath 过滤 */
    if (internal->xpath_filter != NULL) {
        if (!match_xpath_filter(node->path, internal->xpath_filter)) {
            /* 跳过不匹配的节点，递归获取下一个 */
            return exec_yang_scan_next(state);
        }
    }

    /* 创建元组描述符：4 列 */
    /* 列：path (const char*), name (const char*), value (const char*), type (int32) */
    ExecTupleDesc *desc = exec_make_tuple_desc(4);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充值：直接存储指针（指向 intern 数据，不释放） */
    slot->tts_values[0] = (void *)node->path;
    slot->tts_values[1] = (void *)node->name;
    slot->tts_values[2] = (void *)node->value;
    slot->tts_values[3] = (void *)(uintptr_t)node->type;
    slot->tts_nvalid = 4;

    return slot;
}

void exec_yang_scan_close(YangScanState *state)
{
    if (state == NULL) return;

    yang_scan_internal_t *internal = (yang_scan_internal_t *)state->ps.state;
    if (internal) {
        /* 关闭引擎 */
        if (internal->engine) {
            yang_engine_close(internal->engine);
        }

        /* 释放节点缓存（不释放节点本身，它们属于引擎） */
        if (internal->node_buffer) {
            free(internal->node_buffer);
        }

        if (internal->collection_name) {
            free(internal->collection_name);
        }

        if (internal->xpath_filter) {
            free(internal->xpath_filter);
        }

        free(internal);
    }

    free(state);
}