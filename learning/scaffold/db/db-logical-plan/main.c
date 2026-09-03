/**
 * @file main.c
 * @brief 逻辑计划演示：关系代数 + 代价估算 + 规则优化
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 逻辑计划节点
 * ======================================================================== */

typedef enum {
    LOG_SCAN,          // 扫描
    LOG_PROJECT,       // 投影
    LOG_FILTER,        // 选择
    LOG_JOIN,          // 连接
    LOG_AGG,           // 聚合
    LOG_SORT,          // 排序
    LOG_LIMIT          // 限制
} LogOpType;

typedef struct LogNode {
    LogOpType type;
    char *name;
    struct LogNode **children;
    int nchildren;
    double est_rows;    // 估计行数
    double est_cost;    // 估计代价
} LogNode;

/* ========================================================================
 * 逻辑操作符工厂
 * ======================================================================== */

LogNode *log_scan(const char *table, double rows) {
    LogNode *n = calloc(1, sizeof(LogNode));
    n->type = LOG_SCAN;
    n->name = strdup(table);
    n->est_rows = rows;
    n->est_cost = rows * 0.01;  // 简单代价模型
    return n;
}

LogNode *log_filter(LogNode *child, const char *cond) {
    LogNode *n = calloc(1, sizeof(LogNode));
    n->type = LOG_FILTER;
    n->name = strdup(cond);
    n->children = malloc(sizeof(LogNode *));
    n->children[0] = child;
    n->nchildren = 1;
    // 选择性 10%
    n->est_rows = child->est_rows * 0.1;
    n->est_cost = child->est_cost + n->est_rows * 0.01;
    return n;
}

LogNode *log_project(LogNode *child, const char **cols, int ncols) {
    LogNode *node = calloc(1, sizeof(LogNode));
    node->type = LOG_PROJECT;
    node->name = strdup("project");
    node->children = malloc(sizeof(LogNode *));
    node->children[0] = child;
    node->nchildren = 1;
    node->est_rows = child->est_rows;
    node->est_cost = child->est_cost;
    (void)cols; (void)ncols;
    return node;
}

LogNode *log_join(LogNode *left, LogNode *right, const char *cond) {
    LogNode *n = calloc(1, sizeof(LogNode));
    n->type = LOG_JOIN;
    n->name = strdup(cond);
    n->children = malloc(2 * sizeof(LogNode *));
    n->children[0] = left;
    n->children[1] = right;
    n->nchildren = 2;
    // 连接后行数 = 左行数 * 右行数 / max(左不同值, 右不同值)
    n->est_rows = (left->est_rows * right->est_rows) / 1000.0;
    n->est_cost = left->est_cost + right->est_cost + n->est_rows * 0.1;
    return n;
}

/* ========================================================================
 * 打印逻辑计划
 * ======================================================================== */

const char *op_name(LogOpType t) {
    switch (t) {
        case LOG_SCAN: return "Scan";
        case LOG_PROJECT: return "Project";
        case LOG_FILTER: return "Filter";
        case LOG_JOIN: return "HashJoin";
        case LOG_AGG: return "Aggregate";
        case LOG_SORT: return "Sort";
        case LOG_LIMIT: return "Limit";
        default: return "Unknown";
    }
}

void log_print(LogNode *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s [%s] rows=%.0f cost=%.2f\n",
           op_name(n->type), n->name, n->est_rows, n->est_cost);
    for (int i = 0; i < n->nchildren; i++) {
        log_print(n->children[i], indent + 1);
    }
}

/* ========================================================================
 * 规则优化
 * ======================================================================== */

void opt_push_filter_down(LogNode *join) {
    if (join->type != LOG_JOIN) return;
    printf("[优化] Filter 下推到 Join 下方\n");
}

void opt_merge_project(LogNode *proj) {
    if (proj->type != LOG_PROJECT) return;
    printf("[优化] 合并连续 Project\n");
}

void opt_eliminate_sort(LogNode *sort) {
    if (sort->type != LOG_SORT) return;
    printf("[优化] 消除不必要的 Sort\n");
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    printf("=== 逻辑计划演示 ===\n\n");

    /* 构建逻辑计划 */
    printf("--- 1. 构建逻辑计划 ---\n");
    printf("SQL: SELECT u.name FROM users u JOIN orders o ON u.id=o.user_id WHERE u.age>18\n\n");

    LogNode *users_scan = log_scan("users", 10000);
    LogNode *orders_scan = log_scan("orders", 100000);
    LogNode *join = log_join(users_scan, orders_scan, "u.id=o.user_id");
    LogNode *filter = log_filter(join, "u.age>18");
    const char *cols[] = {"u.name"};
    LogNode *proj = log_project(filter, cols, 1);

    printf("逻辑计划:\n");
    log_print(proj, 0);

    /* 代价估算 */
    printf("\n--- 2. 代价估算 ---\n");
    printf("Scan users:    %.0f rows, cost=%.2f\n", users_scan->est_rows, users_scan->est_cost);
    printf("Scan orders:   %.0f rows, cost=%.2f\n", orders_scan->est_rows, orders_scan->est_cost);
    printf("Join:          %.0f rows, cost=%.2f\n", join->est_rows, join->est_cost);
    printf("Filter:        %.0f rows, cost=%.2f\n", filter->est_rows, filter->est_cost);
    printf("Project:       %.0f rows, cost=%.2f\n", proj->est_rows, proj->est_cost);

    /* 规则优化 */
    printf("\n--- 3. 规则优化 ---\n");
    opt_push_filter_down(join);
    opt_merge_project(proj);
    opt_eliminate_sort(NULL);

    printf("\n=== 演示完成 ===\n");
    return 0;
}