/**
 * @file main.c
 * @brief 执行算子演示：Scan + Join + Aggregate + Modify
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 火山模型 (Volcano Iterator Model)
 * ======================================================================== */

/**
 * 火山模型：每个算子实现 next() 接口
 * - open(): 初始化
 * - next(): 获取下一个元组
 * - close(): 清理
 */

typedef struct Tuple {
    int id;
    const char *name;
} Tuple;

typedef void *PlanState;

/* ========================================================================
 * 扫描算子
 * ======================================================================== */

typedef struct {
    const char *table;
    int current_row;
    int total_rows;
} SeqScanState;

PlanState seqscan_open(const char *table, int rows) {
    SeqScanState *s = calloc(1, sizeof(SeqScanState));
    s->table = table;
    s->current_row = 0;
    s->total_rows = rows;
    return s;
}

Tuple *seqscan_next(PlanState state) {
    SeqScanState *s = (SeqScanState *)state;
    if (s->current_row >= s->total_rows) {
        return NULL;  // 扫描结束
    }
    Tuple *t = malloc(sizeof(Tuple));
    t->id = ++s->current_row;
    t->name = "row";
    return t;
}

void seqscan_close(PlanState state) {
    free(state);
}

/* ========================================================================
 * 过滤算子
 * ======================================================================== */

typedef struct {
    PlanState child;
    int (*pred)(Tuple *);
} FilterState;

PlanState filter_open(PlanState child, int (*pred)(Tuple *)) {
    FilterState *s = calloc(1, sizeof(FilterState));
    s->child = child;
    s->pred = pred;
    return s;
}

Tuple *filter_next(PlanState state) {
    FilterState *s = (FilterState *)state;
    while (1) {
        Tuple *t = seqscan_next(s->child);
        if (!t) return NULL;
        if (s->pred(t)) return t;
        free(t);
    }
}

void filter_close(PlanState state) {
    FilterState *s = (FilterState *)state;
    seqscan_close(s->child);
    free(s);
}

/* ========================================================================
 * 投影算子
 * ======================================================================== */

typedef struct {
    PlanState child;
    int col_index;
} ProjectState;

PlanState project_open(PlanState child, int col) {
    ProjectState *s = calloc(1, sizeof(ProjectState));
    s->child = child;
    s->col_index = col;
    return s;
}

Tuple *project_next(PlanState state) {
    ProjectState *s = (ProjectState *)state;
    return seqscan_next(s->child);  // 简化：直接透传
}

void project_close(PlanState state) {
    ProjectState *s = (ProjectState *)state;
    seqscan_close(s->child);
    free(s);
}

/* ========================================================================
 * 聚合算子 (Hash Aggregate)
 * ======================================================================== */

typedef struct {
    PlanState child;
    int group_key;
    int count;
    int sum;
} AggState;

PlanState agg_open(PlanState child, int group_key) {
    AggState *s = calloc(1, sizeof(AggState));
    s->child = child;
    s->group_key = group_key;
    s->count = 0;
    s->sum = 0;
    return s;
}

Tuple *agg_next(PlanState state) {
    AggState *s = (AggState *)state;
    Tuple *t;
    while ((t = seqscan_next(s->child)) != NULL) {
        s->count++;
        s->sum += t->id;
        free(t);
    }
    Tuple *result = malloc(sizeof(Tuple));
    result->id = s->sum;  // SUM(id)
    result->name = "count";
    return result;
}

void agg_close(PlanState state) {
    AggState *s = (AggState *)state;
    seqscan_close(s->child);
    free(s);
}

/* ========================================================================
 * 修改算子
 * ======================================================================== */

typedef struct {
    const char *table;
    int insert_count;
    int update_count;
    int delete_count;
} ModifyState;

typedef enum { OP_INSERT, OP_UPDATE, OP_DELETE } ModifyOp;

PlanState modify_open(const char *table, ModifyOp op) {
    ModifyState *s = calloc(1, sizeof(ModifyState));
    s->table = table;
    return s;
}

Tuple *modify_exec(PlanState state, ModifyOp op) {
    ModifyState *s = (ModifyState *)state;
    switch (op) {
        case OP_INSERT: s->insert_count++; break;
        case OP_UPDATE: s->update_count++; break;
        case OP_DELETE: s->delete_count++; break;
    }
    return NULL;
}

void modify_close(PlanState state) {
    ModifyState *s = (ModifyState *)state;
    printf("  统计: insert=%d, update=%d, delete=%d\n",
           s->insert_count, s->update_count, s->delete_count);
    free(s);
}

/* ========================================================================
 * 谓词函数
 * ======================================================================== */

int pred_age_gt_18(Tuple *t) {
    return t->id > 18;  // 简化判断
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    printf("=== 执行算子演示 (Volcano 模型) ===\n\n");

    /* 1. SeqScan */
    printf("--- 1. SeqScan ---\n");
    PlanState scan = seqscan_open("users", 5);
    Tuple *t;
    while ((t = seqscan_next(scan)) != NULL) {
        printf("  获取元组: id=%d\n", t->id);
        free(t);
    }
    seqscan_close(scan);
    printf("\n");

    /* 2. Filter */
    printf("--- 2. Filter ---\n");
    PlanState filter = filter_open(seqscan_open("users", 5), pred_age_gt_18);
    while ((t = filter_next(filter)) != NULL) {
        printf("  通过过滤: id=%d\n", t->id);
        free(t);
    }
    filter_close(filter);
    printf("\n");

    /* 3. Aggregate */
    printf("--- 3. HashAggregate ---\n");
    PlanState agg = agg_open(seqscan_open("users", 5), 0);
    t = agg_next(agg);
    if (t) {
        printf("  聚合结果: SUM(id)=%d\n", t->id);
        free(t);
    }
    agg_close(agg);
    printf("\n");

    /* 4. Modify */
    printf("--- 4. Modify ---\n");
    PlanState mod = modify_open("users", OP_INSERT);
    modify_exec(mod, OP_INSERT);
    modify_exec(mod, OP_INSERT);
    modify_exec(mod, OP_UPDATE);
    modify_close(mod);
    printf("\n");

    printf("=== 演示完成 ===\n");
    return 0;
}