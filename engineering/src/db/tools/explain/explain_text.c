/**
 * @file explain_text.c
 * @brief EXPLAIN TEXT 格式输出实现
 *
 * 实现 TEXT 格式的执行计划输出，参考 PostgreSQL EXPLAIN 风格。
 *
 * 示例输出：
 * -> Seq Scan on users  (cost=0.00..8.00 rows=100 width=64)
 *      Filter: (age > 18)
 */

#include "db/tools/explain.h"
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────
 * 节点类型名称映射表
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取计划节点类型对应的显示名称
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
 * TEXT 格式递归输出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 递归输出 TEXT 格式的计划节点
 *
 * @param node  当前节点
 * @param fp    输出文件流
 * @param indent 缩进层级
 * @param opts  EXPLAIN 选项
 */
static void output_text_node(const ExplainPlanNode *node, FILE *fp, int indent, const ExplainOptions *opts)
{
    if (!node || !fp || !opts) {
        return;
    }

    /* ── 缩进 ── */
    for (int i = 0; i < indent; i++) {
        fputs("  ", fp);
    }

    /* ── 节点类型 ── */
    fprintf(fp, "-> %s", plan_node_type_name(node->type));

    /* ── 关系名 ── */
    if (node->relation_name) {
        fprintf(fp, " on %s", node->relation_name);
    }

    /* ── 成本估算 ── */
    if (opts->costs) {
        fprintf(fp, "  (cost=%.2f..%.2f rows=%lld width=%d)",
                node->startup_cost,
                node->total_cost,
                (long long)node->plan_rows,
                node->plan_width);
    }

    /* ── 实际执行统计（ANALYZE 模式） ── */
    if (opts->analyze && node->actual_loops > 0) {
        fprintf(fp, " (actual time=%.3f..%.3f rows=%lld loops=%lld)",
                node->actual_time,
                node->actual_time,
                (long long)node->actual_rows,
                (long long)node->actual_loops);
    }

    fputc('\n', fp);

    /* ── 过滤条件 ── */
    if (node->filter) {
        for (int i = 0; i < indent + 1; i++) {
            fputs("  ", fp);
        }
        fprintf(fp, "Filter: %s\n", node->filter);
    }

    /* ── 索引条件 ── */
    if (node->index_name && (node->type == PLAN_NODE_INDEX_SCAN)) {
        for (int i = 0; i < indent + 1; i++) {
            fputs("  ", fp);
        }
        fprintf(fp, "Index Cond: (%s)\n", node->index_name);
    }

    /* ── 缓冲区统计（BUFFERS 模式） ── */
    if (opts->buffers && (node->shared_hit > 0 || node->shared_read > 0)) {
        for (int i = 0; i < indent + 1; i++) {
            fputs("  ", fp);
        }
        fprintf(fp, "Buffers: shared hit=%lld read=%lld",
                (long long)node->shared_hit,
                (long long)node->shared_read);

        if (node->shared_dirtied > 0) {
            fprintf(fp, " dirtied=%lld", (long long)node->shared_dirtied);
        }
        if (node->shared_written > 0) {
            fprintf(fp, " written=%lld", (long long)node->shared_written);
        }
        fputc('\n', fp);
    }

    /* ── 递归输出子节点 ── */
    for (int i = 0; i < node->n_children; i++) {
        output_text_node(node->children[i], fp, indent + 1, opts);
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 公开 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 输出 TEXT 格式的执行计划
 *
 * @param plan   计划根节点
 * @param fp     输出文件流
 * @param indent 初始缩进层级
 * @return 0 成功，非 0 失败
 */
int explain_output_text(const ExplainPlanNode *plan, FILE *fp, int indent)
{
    if (!plan || !fp) {
        return -1;
    }

    ExplainOptions opts = explain_default_options();
    output_text_node(plan, fp, indent, &opts);

    return 0;
}