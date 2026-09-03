/**
 * @file main.c
 * @brief 物理计划演示：执行算子选择 + 索引选择 + join 重排
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 物理计划节点
 * ======================================================================== */

typedef enum {
    PHYS_SEQ_SCAN,
    PHYS_INDEX_SCAN,
    PHYS_INDEX_ONLY_SCAN,
    PHYS_HASH_JOIN,
    PHYS_NESTLOOP_JOIN,
    PHYS_MERGE_JOIN,
    PHYS_HASH_AGG,
    PHYS_SORT_AGG,
    PHYS_SORT,
    PHYS_HASH,
    PHYS_PROJECT,
    PHYS_FILTER
} PhysOpType;

typedef struct PhysNode {
    PhysOpType type;
    char *name;
    struct PhysNode **children;
    int nchildren;
    double est_cost;
    double est_rows;
} PhysNode;

/* ========================================================================
 * 统计信息
 * ======================================================================== */

typedef struct {
    const char *table;
    double rows;
    double ndistinct;    // 不同值数量
    double avg_width;    // 平均行宽
} TableStats;

typedef struct {
    const char *index;
    const char *table;
    const char *column;
    double selectivity;
} IndexStats;

/* ========================================================================
 * 物理操作符工厂
 * ======================================================================== */

PhysNode *phys_seq_scan(const char *table, double rows) {
    PhysNode *n = calloc(1, sizeof(PhysNode));
    n->type = PHYS_SEQ_SCAN;
    n->name = strdup(table);
    n->est_rows = rows;
    n->est_cost = rows * 0.1;
    return n;
}

PhysNode *phys_index_scan(const char *table, const char *idx, double rows) {
    PhysNode *n = calloc(1, sizeof(PhysNode));
    n->type = PHYS_INDEX_SCAN;
    int len = strlen(table) + strlen(idx) + 2;
    n->name = malloc(len);
    snprintf(n->name, len, "%s.%s", table, idx);
    n->est_rows = rows;
    n->est_cost = rows * 0.2;  // 索引扫描代价略高
    return n;
}

PhysNode *phys_hash_join(PhysNode *left, PhysNode *right, double rows) {
    PhysNode *n = calloc(1, sizeof(PhysNode));
    n->type = PHYS_HASH_JOIN;
    n->name = strdup("HashJoin");
    n->children = malloc(2 * sizeof(PhysNode *));
    n->children[0] = left;
    n->children[1] = right;
    n->nchildren = 2;
    n->est_rows = rows;
    n->est_cost = left->est_cost + right->est_cost + rows * 0.05;
    return n;
}

PhysNode *phys_nestloop(PhysNode *outer, PhysNode *inner, double rows) {
    PhysNode *n = calloc(1, sizeof(PhysNode));
    n->type = PHYS_NESTLOOP_JOIN;
    n->name = strdup("NestLoop");
    n->children = malloc(2 * sizeof(PhysNode *));
    n->children[0] = outer;
    n->children[1] = inner;
    n->nchildren = 2;
    n->est_rows = rows;
    n->est_cost = outer->est_cost + outer->est_rows * inner->est_cost;
    return n;
}

/* ========================================================================
 * 打印物理计划
 * ======================================================================== */

const char *phys_op_name(PhysOpType t) {
    switch (t) {
        case PHYS_SEQ_SCAN: return "Seq Scan";
        case PHYS_INDEX_SCAN: return "Index Scan";
        case PHYS_INDEX_ONLY_SCAN: return "Index Only Scan";
        case PHYS_HASH_JOIN: return "Hash Join";
        case PHYS_NESTLOOP_JOIN: return "Nested Loop";
        case PHYS_MERGE_JOIN: return "Merge Join";
        case PHYS_HASH_AGG: return "Hash Aggregate";
        case PHYS_SORT: return "Sort";
        default: return "Unknown";
    }
}

void phys_print(PhysNode *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("-> %s  rows=%.0f cost=%.2f\n",
           phys_op_name(n->type), n->est_rows, n->est_cost);
    for (int i = 0; i < n->nchildren; i++) {
        phys_print(n->children[i], indent + 1);
    }
}

/* ========================================================================
 * Join 重排
 * ======================================================================== */

void reorder_joins(void) {
    printf("\n[优化] Join 重排\n");
    printf("  A ⋈ B ⋈ C:\n");
    printf("    方案1: (A ⋈ B) ⋈ C  cost=5000\n");
    printf("    方案2: (A ⋈ C) ⋈ B  cost=8000\n");
    printf("    选择: 方案1 (代价最低)\n");
}

/* ========================================================================
 * 索引选择
 * ======================================================================== */

void select_index(const char *table, const char *cond) {
    printf("\n[优化] 索引选择\n");
    printf("  条件: %s\n", cond);
    printf("  可用索引: idx_user_id (selectivity=0.01)\n");
    printf("  选择: idx_user_id (更优选择性)\n");
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    printf("=== 物理计划演示 ===\n\n");

    /* 构建物理计划 */
    printf("--- 1. 构建物理计划 ---\n");
    printf("逻辑计划: Filter(Join(users, orders), age>18)\n\n");

    PhysNode *scan_users = phys_seq_scan("users", 10000);
    PhysNode *scan_orders = phys_index_scan("orders", "idx_user_id", 1000);
    PhysNode *join = phys_hash_join(scan_users, scan_orders, 1000);
    PhysNode *filter = calloc(1, sizeof(PhysNode));
    filter->type = PHYS_FILTER;
    filter->name = strdup("age>18");
    filter->children = malloc(sizeof(PhysNode *));
    filter->children[0] = join;
    filter->nchildren = 1;
    filter->est_rows = 100;
    filter->est_cost = join->est_cost + 100 * 0.01;

    printf("物理计划:\n");
    phys_print(filter, 0);

    /* 代价对比 */
    printf("\n--- 2. 代价对比 ---\n");
    printf("SeqScan orders: cost=10000\n");
    printf("IndexScan orders (idx): cost=1000\n");
    printf("选择 IndexScan (快 10x)\n");

    /* Join 重排 */
    reorder_joins();

    /* 索引选择 */
    select_index("orders", "user_id=123");

    printf("\n=== 演示完成 ===\n");
    return 0;
}