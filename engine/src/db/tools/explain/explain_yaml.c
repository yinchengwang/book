/**
 * @file explain_yaml.c
 * @brief YAML 格式 EXPLAIN 输出实现
 *
 * 实现 YAML 格式的执行计划输出，输出符合 YAML 1.2 规范的字符串。
 *
 * 示例输出：
 * - Node Type: Seq Scan
 *   Relation Name: users
 *   Cost:
 *     Startup: 0.00
 *     Total: 100.00
 *   Plan Rows: 1000
 *   Plan Width: 64
 *   Filter: (age > 18)
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
 * @brief 输出 YAML 缩进
 * @param fp     输出文件流
 * @param indent 缩进层级（每级 2 空格）
 */
static void yaml_indent(FILE *fp, int indent)
{
    for (int i = 0; i < indent; i++) {
        fputc(' ', fp);
    }
}

/**
 * @brief 输出 YAML 字符串值（含引号转义）
 * @param fp   输出文件流
 * @param str  输入字符串
 */
static void yaml_write_string(FILE *fp, const char *str)
{
    bool need_quotes = false;

    /* 检查是否需要引号 */
    for (const char *p = str; *p; p++) {
        if (*p == ':' || *p == '#' || *p == '{' || *p == '}' ||
            *p == '[' || *p == ']' || *p == ',' || *p == '&' ||
            *p == '*' || *p == '?' || *p == '-' || *p == '|' ||
            *p == '<' || *p == '>' || *p == '=' || *p == '!' ||
            *p == '%' || *p == '@' || *p == '`' || *p == '\'' ||
            *p == '"' || *p == '\n' || *p == '\r') {
            need_quotes = true;
            break;
        }
    }

    if (need_quotes) {
        fputc('\'', fp);
        for (const char *p = str; *p; p++) {
            if (*p == '\'') {
                fputs("''", fp);  /* YAML 单引号转义 */
            } else {
                fputc(*p, fp);
            }
        }
        fputc('\'', fp);
    } else {
        fputs(str, fp);
    }
}

/* ─────────────────────────────────────────────────────────────────
 * YAML 格式递归输出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 递归输出 YAML 格式的计划节点
 *
 * @param node   当前节点
 * @param fp     输出文件流
 * @param indent 缩进层级
 */
int explain_output_yaml(ExplainPlanNode *node, FILE *fp, int indent)
{
    if (!node || !fp) return -1;

    yaml_indent(fp, indent);
    fprintf(fp, "- Node Type: %s\n", plan_node_type_name(node->type));

    if (node->relation_name) {
        yaml_indent(fp, indent);
        fprintf(fp, "  Relation Name: ");
        yaml_write_string(fp, node->relation_name);
        fputc('\n', fp);
    }

    if (node->index_name) {
        yaml_indent(fp, indent);
        fprintf(fp, "  Index Name: ");
        yaml_write_string(fp, node->index_name);
        fputc('\n', fp);
    }

    if (node->filter) {
        yaml_indent(fp, indent);
        fprintf(fp, "  Filter: ");
        yaml_write_string(fp, node->filter);
        fputc('\n', fp);
    }

    /* 成本信息 */
    yaml_indent(fp, indent);
    fprintf(fp, "  Cost:\n");
    yaml_indent(fp, indent);
    fprintf(fp, "    Startup: %.2f\n", node->startup_cost);
    yaml_indent(fp, indent);
    fprintf(fp, "    Total: %.2f\n", node->total_cost);

    yaml_indent(fp, indent);
    fprintf(fp, "  Plan Rows: %ld\n", node->plan_rows);
    yaml_indent(fp, indent);
    fprintf(fp, "  Plan Width: %d\n", node->plan_width);

    /* 子节点 */
    if (node->n_children > 0) {
        yaml_indent(fp, indent);
        fprintf(fp, "  Plans:\n");

        for (int i = 0; i < node->n_children; i++) {
            explain_output_yaml(node->children[i], fp, indent + 4);
        }
    }

    return 0;
}