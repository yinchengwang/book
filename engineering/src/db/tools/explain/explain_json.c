/**
 * @file explain_json.c
 * @brief EXPLAIN JSON 格式输出实现
 *
 * 实现 JSON 格式的执行计划输出，输出合法的 JSON 字符串。
 *
 * 示例输出：
 * {
 *   "Plan": {
 *     "Node Type": "Seq Scan",
 *     "Relation Name": "users",
 *     "Startup Cost": 0.00,
 *     "Total Cost": 100.00,
 *     "Plan Rows": 1000,
 *     "Plan Width": 64,
 *     "Filter": "(age > 18)"
 *   }
 * }
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
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 构建指定长度的缩进字符串
 *
 * @param buf    输出缓冲区
 * @param size   缓冲区大小
 * @param indent 缩进层级（每级 2 空格）
 */
static void build_indent(char *buf, size_t size, int indent)
{
    int i;
    int pos = 0;
    for (i = 0; i < indent && pos < (int)size - 1; i++) {
        buf[pos++] = ' ';
        buf[pos++] = ' ';
    }
    buf[pos] = '\0';
}

/* ─────────────────────────────────────────────────────────────────
 * JSON 格式递归输出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 递归输出 JSON 格式的计划节点
 *
 * @param node   当前节点
 * @param fp     输出文件流
 * @param indent 缩进层级
 * @param opts   EXPLAIN 选项
 * @param is_last 是否为兄弟节点中的最后一个（控制逗号）
 */
static void output_json_node(const ExplainPlanNode *node, FILE *fp,
                              int indent, const ExplainOptions *opts)
{
    char ind[128] = "";
    char ind_child[128] = "";

    build_indent(ind, sizeof(ind), indent);
    build_indent(ind_child, sizeof(ind_child), indent + 1);

    /* 节点对象起始 */
    fprintf(fp, "%s{\n", ind);

    /* Node Type */
    fprintf(fp, "%s  \"Node Type\": \"%s\"", ind,
            plan_node_type_name(node->type));

    /* Relation Name */
    if (node->relation_name) {
        fprintf(fp, ",\n%s  \"Relation Name\": \"%s\"", ind, node->relation_name);
    }

    /* Index Name */
    if (node->index_name) {
        fprintf(fp, ",\n%s  \"Index Name\": \"%s\"", ind, node->index_name);
    }

    /* 成本信息 */
    if (opts->costs) {
        fprintf(fp, ",\n%s  \"Startup Cost\": %.2f", ind, node->startup_cost);
        fprintf(fp, ",\n%s  \"Total Cost\": %.2f", ind, node->total_cost);
        fprintf(fp, ",\n%s  \"Plan Rows\": %lld", ind, (long long)node->plan_rows);
        fprintf(fp, ",\n%s  \"Plan Width\": %d", ind, node->plan_width);
    }

    /* 过滤条件 */
    if (node->filter) {
        fprintf(fp, ",\n%s  \"Filter\": \"%s\"", ind, node->filter);
    }

    /* 索引条件 */
    if (node->index_name && (node->type == PLAN_NODE_INDEX_SCAN)) {
        fprintf(fp, ",\n%s  \"Index Cond\": \"(%s)\"", ind, node->index_name);
    }

    /* 执行统计（ANALYZE 模式） */
    if (opts->analyze && node->actual_loops > 0) {
        fprintf(fp, ",\n%s  \"Actual Total Time\": %.3f", ind, node->actual_time);
        fprintf(fp, ",\n%s  \"Actual Rows\": %lld", ind, (long long)node->actual_rows);
        fprintf(fp, ",\n%s  \"Actual Loops\": %lld", ind, (long long)node->actual_loops);
    }

    /* 缓冲区统计 */
    if (opts->buffers && (node->shared_hit > 0 || node->shared_read > 0)) {
        fprintf(fp, ",\n%s  \"Shared Hit Blocks\": %lld", ind, (long long)node->shared_hit);
        fprintf(fp, ",\n%s  \"Shared Read Blocks\": %lld", ind, (long long)node->shared_read);
        if (node->shared_dirtied > 0) {
            fprintf(fp, ",\n%s  \"Shared Dirtied Blocks\": %lld", ind, (long long)node->shared_dirtied);
        }
        if (node->shared_written > 0) {
            fprintf(fp, ",\n%s  \"Shared Written Blocks\": %lld", ind, (long long)node->shared_written);
        }
    }

    /* 子节点 */
    if (node->n_children > 0) {
        fprintf(fp, ",\n%s  \"Plans\": [\n", ind);
        for (int i = 0; i < node->n_children; i++) {
            output_json_node(node->children[i], fp, indent + 2, opts);
            if (i < node->n_children - 1) {
                fputc(',', fp);
            }
            fputc('\n', fp);
        }
        fprintf(fp, "%s  ]", ind);
    }

    /* 节点对象结束 */
    fprintf(fp, "\n%s}", ind);
}

/* ─────────────────────────────────────────────────────────────────
 * 公开 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 输出 JSON 格式的执行计划
 *
 * @param plan   计划根节点
 * @param fp     输出文件流
 * @param indent 初始缩进层级
 * @return 0 成功，非 0 失败
 */
int explain_output_json(const ExplainPlanNode *plan, FILE *fp, int indent)
{
    if (!plan || !fp) {
        return -1;
    }

    ExplainOptions opts = explain_default_options();

    /* 外层包裹对象 */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"Plan\": ");
    output_json_node(plan, fp, indent + 2, &opts);
    fprintf(fp, "\n}\n");

    return 0;
}