/*
 * optimizer.h - 查询优化器公共接口
 */

#ifndef DB_OPTIMIZER_H
#define DB_OPTIMIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 计划节点类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    PLAN_SCAN_SEQ,        /* 顺序扫描 */
    PLAN_SCAN_INDEX,      /* 索引扫描 */
    PLAN_SCAN_INDEX_ONLY, /* 仅索引扫描 */
    PLAN_SCAN_VECTOR,     /* 向量索引扫描 */
    PLAN_FILTER,          /* 过滤 */
    PLAN_PROJECT,         /* 投影 */
    PLAN_JOIN_HASH,       /* Hash 连接 */
    PLAN_JOIN_NESTED,     /* 嵌套循环连接 */
    PLAN_JOIN_MERGE,      /* 归并连接 */
    PLAN_AGGREGATE,       /* 聚合 */
    PLAN_SORT,            /* 排序 */
    PLAN_LIMIT,           /* 限制 */
    PLAN_HASH,            /* Hash 表构建 */
    PLAN_VALUES,          /* VALUES 子句 */
    PLAN_RESULT           /* 结果 */
} plan_node_type_t;

/* ─────────────────────────────────────────────────────────────────
 * 表达式类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    EXPR_COLUMN,      /* 列引用 */
    EXPR_CONSTANT,    /* 常量 */
    EXPR_BINARY_OP,   /* 二元操作 */
    EXPR_UNARY_OP,    /* 一元操作 */
    EXPR_FUNC,        /* 函数调用 */
    EXPR_SUBQUERY,    /* 子查询 */
    EXPR_PARAM        /* 参数 */
} expr_type_t;

/* ─────────────────────────────────────────────────────────────────
 * 表达式定义
 * ───────────────────────────────────────────────────────────────── */

typedef struct expr {
    expr_type_t type;
    char *alias;              /* 别名 */
    union {
        /* 列引用 */
        struct {
            int table_id;     /* 表 ID */
            int column_id;    /* 列 ID */
            char *table_name;
            char *column_name;
        } column;

        /* 常量 */
        struct {
            int vtype;        /* 值类型 */
            union {
                int64_t int_val;
                double double_val;
                char *str_val;
            } val;
        } constant;

        /* 二元操作 */
        struct {
            int op;           /* 操作符 */
            struct expr *left;
            struct expr *right;
        } binary;

        /* 一元操作 */
        struct {
            int op;
            struct expr *child;
        } unary;

        /* 函数 */
        struct {
            char *func_name;
            int arg_count;
            struct expr **args;
        } func;
    } u;
} expr_t;

/* ─────────────────────────────────────────────────────────────────
 * 计划节点定义
 * ───────────────────────────────────────────────────────────────── */

/**
 * 扫描计划
 */
typedef struct scan_plan {
    plan_node_type_t type;    /* PLAN_SCAN_SEQ, PLAN_SCAN_INDEX, 或 PLAN_SCAN_VECTOR */
    int table_id;             /* 表 ID */
    char *table_name;         /* 表名 */
    expr_t *filter;           /* 过滤条件 */
    int index_id;             /* 使用的索引 ID（索引扫描时） */
    char *index_name;         /* 索引名 */
    bool index_only;          /* 是否仅索引扫描 */

    /* 向量搜索扩展 */
    int vector_dim;           /* 向量维度（向量扫描时） */
    int top_k;                /* 返回的最近邻数量 */
    float *query_vector;      /* 查询向量（可选） */
    float radius;             /* 搜索半径（可选） */
    char *reranker_type;      /* 精排类型（可选） */
} scan_plan_t;

/**
 * 过滤计划
 */
typedef struct filter_plan {
    expr_t *condition;        /* 过滤条件 */
} filter_plan_t;

/**
 * 投影计划
 */
typedef struct project_plan {
    int expr_count;           /* 表达式数量 */
    expr_t **expressions;     /* 投影表达式 */
    char **aliases;           /* 别名 */
} project_plan_t;

/**
 * 连接计划
 */
typedef struct join_plan {
    plan_node_type_t type;    /* JOIN 类型 */
    expr_t *condition;        /* 连接条件 */
    int join_type;            /* INNER/LEFT/RIGHT/FULL */
} join_plan_t;

/**
 * 聚合计划
 */
typedef struct aggregate_plan {
    int group_by_count;       /* GROUP BY 列数 */
    int *group_by_cols;       /* GROUP BY 列 ID */
    int agg_func_count;       /* 聚合函数数 */
    struct {
        int func;             /* SUM/COUNT/AVG/MIN/MAX */
        expr_t *arg;          /* 参数表达式 */
        char *alias;          /* 别名 */
    } *agg_funcs;
} aggregate_plan_t;

/**
 * 排序计划
 */
typedef struct sort_plan {
    int order_by_count;       /* 排序列数 */
    struct {
        expr_t *expr;         /* 排序表达式 */
        bool desc;            /* 降序 */
        bool nulls_first;     /* NULL 在前 */
    } *order_by_items;
} sort_plan_t;

/**
 * 通用计划节点
 */
typedef struct plan_node {
    plan_node_type_t type;

    /* 代价信息 */
    double startup_cost;      /* 启动代价 */
    double total_cost;        /* 总代价 */
    double plan_rows;         /* 估计行数 */
    int plan_width;           /* 估计行宽度 */

    /* 子计划 */
    struct plan_node *left;   /* 左子计划 */
    struct plan_node *right;  /* 右子计划（用于连接） */
    struct plan_node *subplan;/* 子计划（用于子查询） */

    /* 类型特定数据 */
    union {
        scan_plan_t scan;
        filter_plan_t filter;
        project_plan_t project;
        join_plan_t join;
        aggregate_plan_t aggregate;
        sort_plan_t sort;
    } data;

    /* 链表指针（用于列表管理） */
    struct plan_node *next;
} plan_node_t;

/* ─────────────────────────────────────────────────────────────────
 * 统计信息
 * ───────────────────────────────────────────────────────────────── */

/**
 * 列统计信息
 */
typedef struct column_stats {
    int column_id;
    int64_t n_distinct;       /* 不同值数量 */
    double null_frac;         /* NULL 比例 */
    double min_value;         /* 最小值（数值） */
    double max_value;         /* 最大值（数值） */
} column_stats_t;

/**
 * 表统计信息
 */
typedef struct table_stats {
    int64_t row_count;        /* 估计行数 */
    int32_t page_count;       /* 页面数 */
    double rel_density;       /* 行密度 */
    int column_count;         /* 列统计数量 */
    column_stats_t *columns;  /* 列统计 */
} table_stats_t;

/* ─────────────────────────────────────────────────────────────────
 * 代价常量
 * ───────────────────────────────────────────────────────────────── */

#define SEQ_PAGE_COST 1.0           /* 顺序页面 IO 成本 */
#define RANDOM_PAGE_COST 4.0        /* 随机页面 IO 成本 */
#define CPU_TUPLE_COST 0.01         /* 每行 CPU 成本 */
#define CPU_INDEX_TUPLE_COST 0.005  /* 索引行 CPU 成本 */

/* ─────────────────────────────────────────────────────────────────
 * 表达式操作
 * ───────────────────────────────────────────────────────────────── */

expr_t *expr_column_create(int table_id, int column_id,
                           const char *table_name, const char *column_name);
expr_t *expr_constant_create_int(int64_t val);
expr_t *expr_constant_create_double(double val);
expr_t *expr_constant_create_string(const char *val);
expr_t *expr_binary_create(int op, expr_t *left, expr_t *right);
expr_t *expr_unary_create(int op, expr_t *child);
expr_t *expr_copy(const expr_t *expr);
void expr_destroy(expr_t *expr);
bool expr_evaluate(const expr_t *expr, void *row);
bool expr_is_constant(const expr_t *expr);
expr_t *expr_constant_fold(const expr_t *expr);

/* ─────────────────────────────────────────────────────────────────
 * 计划节点操作
 * ───────────────────────────────────────────────────────────────── */

plan_node_t *plan_node_create(plan_node_type_t type);
void plan_node_destroy(plan_node_t *plan);
plan_node_t *plan_node_copy(const plan_node_t *plan);
void plan_node_add_child(plan_node_t *parent, plan_node_t *child);
void plan_node_add_subplan(plan_node_t *parent, plan_node_t *subplan);

/* ─────────────────────────────────────────────────────────────────
 * 代价估算
 * ───────────────────────────────────────────────────────────────── */

double cost_seq_scan(const table_stats_t *stats, double selectivity);
double cost_index_scan(const table_stats_t *stats, int index_depth,
                       double index_selectivity, bool index_only);
double cost_hash_join(double outer_rows, double inner_rows, double inner_pages);
double cost_nested_loop(double outer_rows, double inner_cost_per_row);
double selectivity_equal(int64_t n_distinct);
double selectivity_range(double low, double high, double min, double max);
double selectivity_default(void);

/* 向量索引代价估算 */
double cost_vector_hnsw(int32_t ef_search, int64_t num_vectors, int32_t top_k);
double cost_vector_ivf(int32_t nprobe, int64_t num_vectors, int32_t top_k);
double cost_vector_diskann(int32_t search_list_size, int64_t num_vectors, int32_t top_k);
const char *select_best_vector_index(int64_t num_vectors, int32_t top_k,
                                     int32_t ef_search, int32_t nprobe);

/* ─────────────────────────────────────────────────────────────────
 * 统计信息
 * ───────────────────────────────────────────────────────────────── */

table_stats_t *table_stats_create(int64_t row_count, int32_t page_count);
void table_stats_destroy(table_stats_t *stats);
void table_stats_update(table_stats_t *stats, int64_t row_count);
column_stats_t *column_stats_create(int column_id, int64_t n_distinct,
                                     double null_frac);
double analyze_compute_selectivity(const table_stats_t *stats,
                                    const expr_t *condition);

/* ─────────────────────────────────────────────────────────────────
 * 索引信息
 * ───────────────────────────────────────────────────────────────── */

/**
 * 索引类型
 */
typedef enum {
    INDEX_TYPE_BTREE,     /* B+Tree 索引 */
    INDEX_TYPE_HASH,      /* Hash 索引 */
    INDEX_TYPE_BITMAP,    /* Bitmap 索引 */
    INDEX_TYPE_GIN,       /* 倒排索引 */
    INDEX_TYPE_GIST,      /* GiST 索引 */
    INDEX_TYPE_FULLTEXT   /* 全文索引 */
} index_type_t;

/**
 * 索引信息
 */
typedef struct index_info {
    int index_id;             /* 索引 ID */
    char *index_name;         /* 索引名 */
    char *table_name;         /* 表名 */
    index_type_t type;        /* 索引类型 */
    int column_id;            /* 索引列 ID */
    int depth;                /* 索引深度（B+Tree） */
    double index_selectivity; /* 索引选择性 */
} index_info_t;

/**
 * @brief 创建索引信息
 */
index_info_t *index_info_create(int index_id, const char *name,
                                 const char *table, index_type_t type,
                                 int column_id);

/**
 * @brief 销毁索引信息
 */
void index_info_destroy(index_info_t *info);

/**
 * @brief 选择最优索引
 * @param filter 过滤条件
 * @param indexes 可用索引列表
 * @param index_count 索引数量
 * @param stats 表统计信息
 * @return 最优索引，未找到返回 NULL
 */
index_info_t *index_select_best(const expr_t *filter,
                                const index_info_t **indexes,
                                int index_count,
                                const table_stats_t *stats);

/* ─────────────────────────────────────────────────────────────────
 * 优化器主入口
 * ───────────────────────────────────────────────────────────────── */

/**
 * 优化查询
 * @param ast SQL 解析生成的 AST
 * @return 优化后的执行计划
 */
plan_node_t *optimizer_optimize(void *ast);

/**
 * 打印执行计划（用于 EXPLAIN）
 * @param plan 执行计划
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int explain_plan(const plan_node_t *plan, char *buf, size_t buf_size);

/**
 * 获取计划文本格式
 * @param plan 执行计划
 * @return 格式化的计划文本（需手动释放）
 */
char *explain_plan_text(const plan_node_t *plan);

#ifdef __cplusplus
}
#endif

#endif /* DB_OPTIMIZER_H */