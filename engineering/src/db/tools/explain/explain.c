/**
 * @file explain.c
 * @brief EXPLAIN 执行计划分析工具核心实现
 */

#include "db/tools/explain.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────────────────────────
 * 节点类型名称映射
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取节点类型的名称字符串
 */
static const char *plan_node_type_name(PlanNodeType type)
{
    switch (type) {
        case PLAN_NODE_SEQ_SCAN:    return "Seq Scan";
        case PLAN_NODE_INDEX_SCAN:  return "Index Scan";
        case PLAN_NODE_BITMAP_SCAN: return "Bitmap Heap Scan";
        case PLAN_NODE_NESTED_LOOP: return "Nested Loop";
        case PLAN_NODE_HASH_JOIN:   return "Hash Join";
        case PLAN_NODE_MERGE_JOIN:  return "Merge Join";
        case PLAN_NODE_AGGREGATE:   return "Aggregate";
        case PLAN_NODE_SORT:        return "Sort";
        case PLAN_NODE_LIMIT:       return "Limit";
        case PLAN_NODE_GATHER:      return "Gather";
        default:                    return "Unknown";
    }
}

/* ─────────────────────────────────────────────────────────────────
 * EXPLAIN 上下文结构
 * ───────────────────────────────────────────────────────────────── */

struct ExplainContext_s {
    ExplainOptions options;
    char           error_msg[256];
};

/* ─────────────────────────────────────────────────────────────────
 * 默认选项与上下文管理
 * ───────────────────────────────────────────────────────────────── */

ExplainOptions explain_default_options(void)
{
    ExplainOptions opts = {
        .format  = EXPLAIN_FORMAT_TEXT,
        .analyze = false,
        .costs   = true,
        .buffers = false,
        .timing  = false,
        .verbose = false
    };
    return opts;
}

ExplainContext *explain_create(ExplainOptions *options)
{
    ExplainContext *ctx = calloc(1, sizeof(ExplainContext));
    if (!ctx) {
        return NULL;
    }

    if (options) {
        ctx->options = *options;
    } else {
        ctx->options = explain_default_options();
    }

    ctx->error_msg[0] = '\0';
    return ctx;
}

void explain_destroy(ExplainContext *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 计划节点管理
 * ───────────────────────────────────────────────────────────────── */

ExplainPlanNode *explain_create_node(PlanNodeType type)
{
    ExplainPlanNode *node = calloc(1, sizeof(ExplainPlanNode));
    if (!node) {
        return NULL;
    }

    node->type = type;
    node->n_children = 0;
    node->children = NULL;

    /* 初始化成本信息 */
    node->startup_cost = 0.0;
    node->total_cost = 0.0;
    node->plan_rows = 0;
    node->plan_width = 0;

    /* 初始化执行统计 */
    node->actual_time = 0.0;
    node->actual_rows = 0;
    node->actual_loops = 0;

    /* 初始化缓冲区统计 */
    node->shared_hit = 0;
    node->shared_read = 0;
    node->shared_dirtied = 0;
    node->shared_written = 0;

    return node;
}

int explain_add_child(ExplainPlanNode *parent, ExplainPlanNode *child)
{
    if (!parent || !child) {
        return -1;
    }

    /* 动态扩容 children 数组 */
    int new_count = parent->n_children + 1;
    ExplainPlanNode **new_children = realloc(
        parent->children,
        new_count * sizeof(ExplainPlanNode *)
    );

    if (!new_children) {
        return -1;
    }

    parent->children = new_children;
    parent->children[parent->n_children] = child;
    parent->n_children = new_count;

    return 0;
}

void explain_free_plan(ExplainPlanNode *node)
{
    if (!node) {
        return;
    }

    /* 递归释放子节点 */
    for (int i = 0; i < node->n_children; i++) {
        explain_free_plan(node->children[i]);
    }

    /* 释放 children 数组 */
    if (node->children) {
        free(node->children);
    }

    /* 释放节点自身 */
    free(node);
}

/* ─────────────────────────────────────────────────────────────────
 * TEXT 格式输出（内部函数）
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 输出 TEXT 格式的计划节点（递归）
 */
static void explain_output_text_node(ExplainPlanNode *node, FILE *fp, int indent, ExplainOptions *opts)
{
    /* 缩进 */
    for (int i = 0; i < indent; i++) {
        fprintf(fp, "  ");
    }

    /* 节点类型 */
    fprintf(fp, "-> %s", plan_node_type_name(node->type));

    /* 关系名 */
    if (node->relation_name) {
        fprintf(fp, " on %s", node->relation_name);
    }

    /* 成本信息 */
    if (opts->costs) {
        fprintf(fp, "  (cost=%.2f..%.2f rows=%lld width=%d)",
                node->startup_cost, node->total_cost,
                (long long)node->plan_rows, node->plan_width);
    }

    /* 执行统计（analyze 模式） */
    if (opts->analyze && node->actual_loops > 0) {
        fprintf(fp, " (actual time=%.3f..%.3f rows=%lld loops=%lld)",
                node->actual_time, node->actual_time,
                (long long)node->actual_rows, (long long)node->actual_loops);
    }

    fprintf(fp, "\n");

    /* 过滤条件 */
    if (node->filter) {
        for (int i = 0; i < indent + 1; i++) {
            fprintf(fp, "  ");
        }
        fprintf(fp, "Filter: %s\n", node->filter);
    }

    /* 索引名 */
    if (node->index_name) {
        for (int i = 0; i < indent + 1; i++) {
            fprintf(fp, "  ");
        }
        fprintf(fp, "Index Cond: (%s)\n", node->index_name);
    }

    /* 缓冲区统计 */
    if (opts->buffers && (node->shared_hit > 0 || node->shared_read > 0)) {
        for (int i = 0; i < indent + 1; i++) {
            fprintf(fp, "  ");
        }
        fprintf(fp, "Buffers: shared hit=%lld read=%lld",
                (long long)node->shared_hit, (long long)node->shared_read);
        if (node->shared_dirtied > 0) {
            fprintf(fp, " dirtied=%lld", (long long)node->shared_dirtied);
        }
        if (node->shared_written > 0) {
            fprintf(fp, " written=%lld", (long long)node->shared_written);
        }
        fprintf(fp, "\n");
    }

    /* 递归输出子节点 */
    for (int i = 0; i < node->n_children; i++) {
        explain_output_text_node(node->children[i], fp, indent + 1, opts);
    }
}

/* ─────────────────────────────────────────────────────────────────
 * JSON 格式输出（内部函数）
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 输出 JSON 格式的计划节点（递归）
 */
static void explain_output_json_node(ExplainPlanNode *node, FILE *fp, int indent, ExplainOptions *opts)
{
    /* 缩进字符串 */
    char ind[64] = "";
    for (int i = 0; i < indent; i++) {
        strcat(ind, "  ");
    }

    fprintf(fp, "%s{\n", ind);

    /* 节点类型 */
    fprintf(fp, "%s  \"Node Type\": \"%s\",\n", ind, plan_node_type_name(node->type));

    /* 关系名 */
    if (node->relation_name) {
        fprintf(fp, "%s  \"Relation Name\": \"%s\",\n", ind, node->relation_name);
    }

    /* 索引名 */
    if (node->index_name) {
        fprintf(fp, "%s  \"Index Name\": \"%s\",\n", ind, node->index_name);
    }

    /* 成本信息 */
    if (opts->costs) {
        fprintf(fp, "%s  \"Startup Cost\": %.2f,\n", ind, node->startup_cost);
        fprintf(fp, "%s  \"Total Cost\": %.2f,\n", ind, node->total_cost);
        fprintf(fp, "%s  \"Plan Rows\": %lld,\n", ind, (long long)node->plan_rows);
        fprintf(fp, "%s  \"Plan Width\": %d,\n", ind, node->plan_width);
    }

    /* 执行统计 */
    if (opts->analyze && node->actual_loops > 0) {
        fprintf(fp, "%s  \"Actual Total Time\": %.3f,\n", ind, node->actual_time);
        fprintf(fp, "%s  \"Actual Rows\": %lld,\n", ind, (long long)node->actual_rows);
        fprintf(fp, "%s  \"Actual Loops\": %lld,\n", ind, (long long)node->actual_loops);
    }

    /* 过滤条件 */
    if (node->filter) {
        fprintf(fp, "%s  \"Filter\": \"%s\",\n", ind, node->filter);
    }

    /* 索引条件 */
    if (node->index_name && (node->type == PLAN_NODE_INDEX_SCAN)) {
        fprintf(fp, "%s  \"Index Cond\": \"(%s)\",\n", ind, node->index_name);
    }

    /* 缓冲区统计 */
    if (opts->buffers && (node->shared_hit > 0 || node->shared_read > 0)) {
        fprintf(fp, "%s  \"Shared Hit Blocks\": %lld,\n", ind, (long long)node->shared_hit);
        fprintf(fp, "%s  \"Shared Read Blocks\": %lld,\n", ind, (long long)node->shared_read);
        if (node->shared_dirtied > 0) {
            fprintf(fp, "%s  \"Shared Dirtied Blocks\": %lld,\n", ind, (long long)node->shared_dirtied);
        }
        if (node->shared_written > 0) {
            fprintf(fp, "%s  \"Shared Written Blocks\": %lld,\n", ind, (long long)node->shared_written);
        }
    }

    /* 子节点 */
    if (node->n_children > 0) {
        fprintf(fp, "%s  \"Plans\": [\n", ind);
        for (int i = 0; i < node->n_children; i++) {
            explain_output_json_node(node->children[i], fp, indent + 2, opts);
            if (i < node->n_children - 1) {
                fprintf(fp, ",\n");
            } else {
                fprintf(fp, "\n");
            }
        }
        fprintf(fp, "%s  ]\n", ind);
    } else {
        /* 移除最后一个逗号 - 重新格式化输出 */
        /* 这里简化处理，直接输出闭合 */
        fprintf(fp, "%s  \"_\": null\n", ind);  /* 占位字段 */
    }

    fprintf(fp, "%s}", ind);
}

/* ─────────────────────────────────────────────────────────────────
 * 统一输出接口
 * ───────────────────────────────────────────────────────────────── */

int explain_output(ExplainContext *ctx, ExplainPlanNode *plan, FILE *fp)
{
    if (!ctx || !plan || !fp) {
        return -1;
    }

    switch (ctx->options.format) {
        case EXPLAIN_FORMAT_TEXT:
            explain_output_text_node(plan, fp, 0, &ctx->options);
            break;

        case EXPLAIN_FORMAT_JSON:
            fprintf(fp, "{\n");
            fprintf(fp, "  \"Plan\": ");
            explain_output_json_node(plan, fp, 2, &ctx->options);
            fprintf(fp, "\n}\n");
            break;

        case EXPLAIN_FORMAT_YAML:
            /* YAML 格式暂未实现 */
            fprintf(fp, "# YAML format not implemented yet\n");
            explain_output_text_node(plan, fp, 0, &ctx->options);
            break;

        default:
            return -1;
    }

    return 0;
}