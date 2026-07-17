/*
 * optimizer.c - 查询优化器核心实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <db/optimizer/optimizer.h>
#include <db/parser/sql/sql.h>

/* ─────────────────────────────────────────────────────────────────
 * 表达式操作实现
 * ───────────────────────────────────────────────────────────────── */

expr_t *expr_column_create(int table_id, int column_id,
                           const char *table_name, const char *column_name)
{
    expr_t *expr = (expr_t *)malloc(sizeof(expr_t));
    if (expr == NULL) return NULL;

    expr->type = EXPR_COLUMN;
    expr->alias = NULL;
    expr->u.column.table_id = table_id;
    expr->u.column.column_id = column_id;
    expr->u.column.table_name = table_name ? strdup(table_name) : NULL;
    expr->u.column.column_name = column_name ? strdup(column_name) : NULL;

    return expr;
}

expr_t *expr_constant_create_int(int64_t val)
{
    expr_t *expr = (expr_t *)malloc(sizeof(expr_t));
    if (expr == NULL) return NULL;

    expr->type = EXPR_CONSTANT;
    expr->alias = NULL;
    expr->u.constant.vtype = 'I';  /* Integer */
    expr->u.constant.val.int_val = val;

    return expr;
}

expr_t *expr_constant_create_double(double val)
{
    expr_t *expr = (expr_t *)malloc(sizeof(expr_t));
    if (expr == NULL) return NULL;

    expr->type = EXPR_CONSTANT;
    expr->alias = NULL;
    expr->u.constant.vtype = 'D';  /* Double */
    expr->u.constant.val.double_val = val;

    return expr;
}

expr_t *expr_constant_create_string(const char *val)
{
    expr_t *expr = (expr_t *)malloc(sizeof(expr_t));
    if (expr == NULL) return NULL;

    expr->type = EXPR_CONSTANT;
    expr->alias = NULL;
    expr->u.constant.vtype = 'S';  /* String */
    expr->u.constant.val.str_val = val ? strdup(val) : NULL;

    return expr;
}

expr_t *expr_binary_create(int op, expr_t *left, expr_t *right)
{
    expr_t *expr = (expr_t *)malloc(sizeof(expr_t));
    if (expr == NULL) return NULL;

    expr->type = EXPR_BINARY_OP;
    expr->alias = NULL;
    expr->u.binary.op = op;
    expr->u.binary.left = left;
    expr->u.binary.right = right;

    return expr;
}

expr_t *expr_unary_create(int op, expr_t *child)
{
    expr_t *expr = (expr_t *)malloc(sizeof(expr_t));
    if (expr == NULL) return NULL;

    expr->type = EXPR_UNARY_OP;
    expr->alias = NULL;
    expr->u.unary.op = op;
    expr->u.unary.child = child;

    return expr;
}

expr_t *expr_copy(const expr_t *expr)
{
    if (expr == NULL) return NULL;

    expr_t *copy = (expr_t *)malloc(sizeof(expr_t));
    if (copy == NULL) return NULL;

    memcpy(copy, expr, sizeof(expr_t));

    if (expr->alias) copy->alias = strdup(expr->alias);

    switch (expr->type) {
        case EXPR_COLUMN:
            if (expr->u.column.table_name)
                copy->u.column.table_name = strdup(expr->u.column.table_name);
            if (expr->u.column.column_name)
                copy->u.column.column_name = strdup(expr->u.column.column_name);
            break;
        case EXPR_CONSTANT:
            if (expr->u.constant.vtype == 'S' && expr->u.constant.val.str_val)
                copy->u.constant.val.str_val = strdup(expr->u.constant.val.str_val);
            break;
        case EXPR_BINARY_OP:
            copy->u.binary.left = expr_copy(expr->u.binary.left);
            copy->u.binary.right = expr_copy(expr->u.binary.right);
            break;
        case EXPR_UNARY_OP:
            copy->u.unary.child = expr_copy(expr->u.unary.child);
            break;
        default:
            break;
    }

    return copy;
}

void expr_destroy(expr_t *expr)
{
    if (expr == NULL) return;

    if (expr->alias) free(expr->alias);

    switch (expr->type) {
        case EXPR_COLUMN:
            if (expr->u.column.table_name) free(expr->u.column.table_name);
            if (expr->u.column.column_name) free(expr->u.column.column_name);
            break;
        case EXPR_CONSTANT:
            if (expr->u.constant.vtype == 'S' && expr->u.constant.val.str_val)
                free(expr->u.constant.val.str_val);
            break;
        case EXPR_BINARY_OP:
            expr_destroy(expr->u.binary.left);
            expr_destroy(expr->u.binary.right);
            break;
        case EXPR_UNARY_OP:
            expr_destroy(expr->u.unary.child);
            break;
        case EXPR_FUNC:
            if (expr->u.func.args) {
                for (int i = 0; i < expr->u.func.arg_count; i++) {
                    expr_destroy(expr->u.func.args[i]);
                }
                free(expr->u.func.args);
            }
            break;
        default:
            break;
    }

    free(expr);
}

bool expr_is_constant(const expr_t *expr)
{
    if (expr == NULL) return false;
    return expr->type == EXPR_CONSTANT;
}

/* ─────────────────────────────────────────────────────────────────
 * 计划节点操作实现
 * ───────────────────────────────────────────────────────────────── */

plan_node_t *plan_node_create(plan_node_type_t type)
{
    plan_node_t *node = (plan_node_t *)malloc(sizeof(plan_node_t));
    if (node == NULL) return NULL;

    memset(node, 0, sizeof(plan_node_t));
    node->type = type;
    node->startup_cost = 0;
    node->total_cost = 0;
    node->plan_rows = 0;
    node->plan_width = 0;

    return node;
}

void plan_node_destroy(plan_node_t *plan)
{
    if (plan == NULL) return;

    /* 递归销毁子计划 */
    if (plan->left) plan_node_destroy(plan->left);
    if (plan->right) plan_node_destroy(plan->right);
    if (plan->subplan) plan_node_destroy(plan->subplan);

    /* 销毁类型特定数据 */
    switch (plan->type) {
        case PLAN_PROJECT:
            if (plan->data.project.expressions) {
                for (int i = 0; i < plan->data.project.expr_count; i++) {
                    expr_destroy(plan->data.project.expressions[i]);
                }
                free(plan->data.project.expressions);
            }
            if (plan->data.project.aliases) {
                for (int i = 0; i < plan->data.project.expr_count; i++) {
                    if (plan->data.project.aliases[i])
                        free(plan->data.project.aliases[i]);
                }
                free(plan->data.project.aliases);
            }
            break;
        case PLAN_AGGREGATE:
            if (plan->data.aggregate.agg_funcs) {
                for (int i = 0; i < plan->data.aggregate.agg_func_count; i++) {
                    expr_destroy(plan->data.aggregate.agg_funcs[i].arg);
                    if (plan->data.aggregate.agg_funcs[i].alias)
                        free(plan->data.aggregate.agg_funcs[i].alias);
                }
                free(plan->data.aggregate.agg_funcs);
            }
            if (plan->data.aggregate.group_by_cols)
                free(plan->data.aggregate.group_by_cols);
            break;
        case PLAN_SORT:
            if (plan->data.sort.order_by_items) {
                for (int i = 0; i < plan->data.sort.order_by_count; i++) {
                    expr_destroy(plan->data.sort.order_by_items[i].expr);
                }
                free(plan->data.sort.order_by_items);
            }
            break;
        default:
            /* 其他类型使用 expr_destroy 统一处理 */
            break;
    }

    /* 销毁关联的表达式 */
    switch (plan->type) {
        case PLAN_SCAN_SEQ:
        case PLAN_SCAN_INDEX:
            expr_destroy(plan->data.scan.filter);
            if (plan->data.scan.table_name) free(plan->data.scan.table_name);
            if (plan->data.scan.index_name) free(plan->data.scan.index_name);
            break;
        case PLAN_FILTER:
            expr_destroy(plan->data.filter.condition);
            break;
        case PLAN_JOIN_HASH:
        case PLAN_JOIN_NESTED:
        case PLAN_JOIN_MERGE:
            expr_destroy(plan->data.join.condition);
            break;
        default:
            break;
    }

    free(plan);
}

void plan_node_add_child(plan_node_t *parent, plan_node_t *child)
{
    if (parent == NULL) return;

    if (parent->left == NULL) {
        parent->left = child;
    } else if (parent->right == NULL) {
        parent->right = child;
    }
}

void plan_node_add_subplan(plan_node_t *parent, plan_node_t *subplan)
{
    if (parent == NULL) return;
    parent->subplan = subplan;
}

/* ─────────────────────────────────────────────────────────────────
 * 代价估算实现
 * ───────────────────────────────────────────────────────────────── */

double cost_seq_scan(const table_stats_t *stats, double selectivity)
{
    if (stats == NULL) return 1000.0;  /* 默认估计 */

    double rows = stats->row_count * selectivity;
    double pages = stats->page_count * selectivity;

    /* 代价 = 页面 IO 成本 + CPU 成本 */
    return pages * SEQ_PAGE_COST + rows * CPU_TUPLE_COST;
}

double cost_index_scan(const table_stats_t *stats, int index_depth,
                       double index_selectivity, bool index_only)
{
    if (stats == NULL) return 1000.0;

    double rows = stats->row_count * index_selectivity;

    /* 索引扫描代价 = 索引页读取 + (回表页读取) + CPU 成本 */
    double cost = index_depth * RANDOM_PAGE_COST;

    if (!index_only) {
        /* 需要回表 */
        double table_pages = stats->page_count * index_selectivity;
        cost += table_pages * RANDOM_PAGE_COST;
    }

    cost += rows * CPU_INDEX_TUPLE_COST;

    return cost;
}

double cost_hash_join(double outer_rows, double inner_rows, double inner_pages)
{
    (void)inner_rows;  /* 预留用于更精确的模型 */
    /* Hash Join 代价 = Hash 构建代价 + Hash 探测代价 */
    double build_cost = inner_pages * SEQ_PAGE_COST;
    double probe_cost = outer_rows * CPU_TUPLE_COST;

    return build_cost + probe_cost;
}

/* ─────────────────────────────────────────────────────────────────
 * 向量搜索代价模型
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief HNSW 搜索代价估算
 * @param ef_search 搜索参数
 * @param num_vectors 向量总数
 * @param top_k 返回数量
 * @return 估计代价
 */
double cost_vector_hnsw(int32_t ef_search, int64_t num_vectors, int32_t top_k) {
    /* HNSW 搜索复杂度: O(ef_search * log(num_vectors)) */
    /* 这里简化为线性模型 */
    double log_n = num_vectors > 0 ? log((double)num_vectors) : 1.0;
    double search_cost = ef_search * log_n;

    /* 精排代价与 top_k 相关 */
    double rerank_cost = top_k * CPU_TUPLE_COST * 10;

    return search_cost + rerank_cost;
}

/**
 * @brief IVF 搜索代价估算
 * @param nprobe探测的聚类数
 * @param num_vectors 向量总数
 * @param top_k 返回数量
 * @return 估计代价
 */
double cost_vector_ivf(int32_t nprobe, int64_t num_vectors, int32_t top_k) {
    /* IVF 搜索复杂度: O(nprobe + top_k * log(nprobe)) */
    double nprobe_cost = (double)nprobe;

    /* 每个聚类平均向量数 */
    double avg_cluster_size = num_vectors > 0 ? (double)num_vectors / 1024.0 : 1.0;
    double probe_cost = nprobe * avg_cluster_size * RANDOM_PAGE_COST;

    /* 精排代价 */
    double rerank_cost = top_k * CPU_TUPLE_COST * 10;

    return nprobe_cost + probe_cost + rerank_cost;
}

/**
 * @brief DiskANN 搜索代价估算
 * @param search_list_size 搜索列表大小
 * @param num_vectors 向量总数
 * @param top_k 返回数量
 * @return 估计代价
 */
double cost_vector_diskann(int32_t search_list_size, int64_t num_vectors, int32_t top_k) {
    /* DiskANN 搜索复杂度: O(search_list_size + top_k) */
    double search_cost = (double)search_list_size;

    /* 磁盘 I/O 成本（如果是 SSD 或 HDD，需要不同的成本模型） */
    double io_cost = search_cost * RANDOM_PAGE_COST;

    /* 精排代价 */
    double rerank_cost = top_k * CPU_TUPLE_COST * 10;

    return io_cost + rerank_cost;
}

/**
 * @brief 选择最优向量索引类型
 * @param num_vectors 向量总数
 * @param top_k 返回数量
 * @param ef_search HNSW ef_search 参数
 * @param nprobe IVF nprobe 参数
 * @return 最优索引类型
 */
const char *select_best_vector_index(int64_t num_vectors, int32_t top_k,
                                      int32_t ef_search, int32_t nprobe) {
    double hnsw_cost = cost_vector_hnsw(ef_search, num_vectors, top_k);
    double ivf_cost = cost_vector_ivf(nprobe, num_vectors, top_k);
    double diskann_cost = cost_vector_diskann(ef_search, num_vectors, top_k);

    if (hnsw_cost <= ivf_cost && hnsw_cost <= diskann_cost) {
        return "HNSW";
    } else if (ivf_cost <= hnsw_cost && ivf_cost <= diskann_cost) {
        return "IVF";
    } else {
        return "DiskANN";
    }
}

double cost_nested_loop(double outer_rows, double inner_cost_per_row)
{
    /* 嵌套循环代价 = 外表行数 × 内表代价 */
    return outer_rows * inner_cost_per_row;
}

double selectivity_equal(int64_t n_distinct)
{
    if (n_distinct <= 0) return selectivity_default();
    return 1.0 / (double)n_distinct;
}

double selectivity_range(double low, double high, double min, double max)
{
    if (max <= min) return selectivity_default();

    double range = high - low;
    double total_range = max - min;

    if (range <= 0 || total_range <= 0) return selectivity_default();

    double sel = range / total_range;
    return (sel < 0) ? 0 : (sel > 1 ? 1 : sel);
}

double selectivity_default(void)
{
    return 0.1;  /* 默认 10% 选择率 */
}

/* ─────────────────────────────────────────────────────────────────
 * 统计信息实现
 * ───────────────────────────────────────────────────────────────── */

table_stats_t *table_stats_create(int64_t row_count, int32_t page_count)
{
    table_stats_t *stats = (table_stats_t *)malloc(sizeof(table_stats_t));
    if (stats == NULL) return NULL;

    stats->row_count = row_count;
    stats->page_count = page_count;
    stats->rel_density = 1.0;
    stats->column_count = 0;
    stats->columns = NULL;

    return stats;
}

void table_stats_destroy(table_stats_t *stats)
{
    if (stats == NULL) return;
    if (stats->columns) free(stats->columns);
    free(stats);
}

void table_stats_update(table_stats_t *stats, int64_t row_count)
{
    if (stats == NULL) return;
    stats->row_count = row_count;
}

column_stats_t *column_stats_create(int column_id, int64_t n_distinct,
                                     double null_frac)
{
    column_stats_t *stats = (column_stats_t *)malloc(sizeof(column_stats_t));
    if (stats == NULL) return NULL;

    stats->column_id = column_id;
    stats->n_distinct = n_distinct;
    stats->null_frac = null_frac;
    stats->min_value = 0;
    stats->max_value = 0;

    return stats;
}

double analyze_compute_selectivity(const table_stats_t *stats,
                                   const expr_t *condition)
{
    if (condition == NULL) return 1.0;

    /* 基于统计信息和条件类型估算选择率 */
    switch (condition->type) {
        case EXPR_COLUMN: {
            /* 列条件，检查是否有列统计 */
            if (stats && stats->columns) {
                for (int i = 0; i < stats->column_count; i++) {
                    if (stats->columns[i].column_id == condition->u.column.column_id) {
                        return selectivity_equal(stats->columns[i].n_distinct);
                    }
                }
            }
            return selectivity_default();
        }

        case EXPR_CONSTANT:
            /* 标量条件 */
            return selectivity_equal(100);  /* 假设 100 个不同值 */

        case EXPR_BINARY_OP: {
            int op = condition->u.binary.op;
            expr_t *left = condition->u.binary.left;
            expr_t *right = condition->u.binary.right;

            /* 等值比较：使用整数 1 (=) 和 0 (!=) */
            if (op == 1 || op == 0) {  /* 1 = '=', 0 = '!=' */
                if (left->type == EXPR_COLUMN && right->type == EXPR_CONSTANT) {
                    if (stats && stats->columns) {
                        for (int i = 0; i < stats->column_count; i++) {
                            if (stats->columns[i].column_id == left->u.column.column_id) {
                                return selectivity_equal(stats->columns[i].n_distinct);
                            }
                        }
                    }
                    return selectivity_equal(100);
                }
                return selectivity_equal(100);
            }

            /* 范围比较：<, >, <=, >= */
            if (op == '<' || op == '>' || op == 2 || op == 3) {  /* 2 = <=, 3 = >= */
                return 0.25;  /* 范围查询约 25% */
            }

            /* AND 条件：乘积 */
            if (op == '&') {
                double left_sel = analyze_compute_selectivity(stats, left);
                double right_sel = analyze_compute_selectivity(stats, right);
                return left_sel * right_sel;
            }

            /* OR 条件：1 - (1-p1)(1-p2) */
            if (op == '|') {
                double left_sel = analyze_compute_selectivity(stats, left);
                double right_sel = analyze_compute_selectivity(stats, right);
                return 1.0 - (1.0 - left_sel) * (1.0 - right_sel);
            }

            return selectivity_default();
        }

        case EXPR_UNARY_OP:
            if (condition->u.unary.op == SQL_TOKEN_NOT) {
                /* NOT 条件 */
                double child_sel = analyze_compute_selectivity(stats, condition->u.unary.child);
                return 1.0 - child_sel;
            }
            return selectivity_default();

        default:
            return selectivity_default();
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 索引信息实现
 * ───────────────────────────────────────────────────────────────── */

index_info_t *index_info_create(int index_id, const char *name,
                                 const char *table, index_type_t type,
                                 int column_id)
{
    index_info_t *info = (index_info_t *)malloc(sizeof(index_info_t));
    if (info == NULL) return NULL;

    info->index_id = index_id;
    info->index_name = name ? strdup(name) : NULL;
    info->table_name = table ? strdup(table) : NULL;
    info->type = type;
    info->column_id = column_id;
    info->depth = 3;  /* 默认深度 */
    info->index_selectivity = 0.1;  /* 默认选择性 */

    return info;
}

void index_info_destroy(index_info_t *info)
{
    if (info == NULL) return;
    if (info->index_name) free(info->index_name);
    if (info->table_name) free(info->table_name);
    free(info);
}

/* ─────────────────────────────────────────────────────────────────
 * 索引选择实现
 * ───────────────────────────────────────────────────────────────── */

/**
 * 检查过滤条件是否可以使用索引
 */
static bool can_use_index_for_expr(const expr_t *expr, int column_id)
{
    if (expr == NULL) return false;

    switch (expr->type) {
        case EXPR_COLUMN:
            return expr->u.column.column_id == column_id;

        case EXPR_CONSTANT:
            return true;

        case EXPR_BINARY_OP: {
            /* 支持的索引操作符 */
            int op = expr->u.binary.op;
            if (op != 1 && op != 0 && op != '<' && op != '>' &&
                op != 2 && op != 3) {  /* =, !=, <, >, <=, >= */
                return false;
            }
            /* 检查左操作数是否是要索引的列 */
            return can_use_index_for_expr(expr->u.binary.left, column_id);
        }

        case EXPR_UNARY_OP:
            /* NOT 条件下不能使用索引 */
            return false;

        default:
            return false;
    }
}

/**
 * 计算使用索引的代价
 */
static double estimate_index_cost(const index_info_t *index,
                                  const expr_t *filter,
                                  const table_stats_t *stats)
{
    if (index == NULL || stats == NULL) return 1e10;

    /* 索引扫描代价 = 索引页读取 + 回表页读取 + CPU 成本 */
    double selectivity = analyze_compute_selectivity(stats, filter);
    double index_pages = 1 + index->depth;  /* 根页 + 路径页 */

    /* 需要回表的行数 */
    double rows_to_fetch = stats->row_count * selectivity;

    /* 回表代价（随机 IO） */
    double fetch_cost = rows_to_fetch * RANDOM_PAGE_COST;

    /* 索引扫描代价（顺序读） */
    double scan_cost = index_pages * SEQ_PAGE_COST;

    /* CPU 代价 */
    double cpu_cost = rows_to_fetch * CPU_INDEX_TUPLE_COST;

    return scan_cost + fetch_cost + cpu_cost;
}

/**
 * 计算顺序扫描的代价
 */
static double estimate_seq_scan_cost(const table_stats_t *stats,
                                     const expr_t *filter)
{
    if (stats == NULL) return 1e10;

    double selectivity = filter ?
        analyze_compute_selectivity(stats, filter) : 1.0;

    return cost_seq_scan(stats, selectivity);
}

index_info_t *index_select_best(const expr_t *filter,
                                const index_info_t **indexes,
                                int index_count,
                                const table_stats_t *stats)
{
    if (index_count == 0 || indexes == NULL || stats == NULL) {
        return NULL;
    }

    index_info_t *best = NULL;
    double best_cost = 1e10;
    double seq_scan_cost = estimate_seq_scan_cost(stats, filter);

    for (int i = 0; i < index_count; i++) {
        const index_info_t *idx = indexes[i];

        /* 检查是否可以使用此索引 */
        if (!can_use_index_for_expr(filter, idx->column_id)) {
            continue;
        }

        /* 计算索引扫描代价 */
        double cost = estimate_index_cost(idx, filter, stats);

        /* 如果索引代价小于顺序扫描代价，选择索引 */
        if (cost < seq_scan_cost) {
            if (cost < best_cost) {
                best_cost = cost;
                best = (index_info_t *)idx;
            }
        }
    }

    return best;
}

/* ─────────────────────────────────────────────────────────────────
 * AST → 表达式转换
 * ───────────────────────────────────────────────────────────────── */

/* 前向声明 */
static expr_t *ast_to_expr(const void *ast);

static expr_t *ast_column_ref_to_expr(const void *ast)
{
    if (ast == NULL) return NULL;
    const sql_node_t *node = (const sql_node_t *)ast;

    if (node->type != SQL_NODE_COLUMN_REF) return NULL;

    return expr_column_create(0, 0, NULL, node->u.column_ref.name);
}

static expr_t *ast_constant_to_expr(const void *ast)
{
    if (ast == NULL) return NULL;
    const sql_node_t *node = (const sql_node_t *)ast;

    if (node->type != SQL_NODE_CONSTANT) return NULL;

    switch (node->u.constant.type) {
        case SQL_TYPE_INT:
            return expr_constant_create_int(node->u.constant.int_value);
        case SQL_TYPE_VARCHAR:
        case SQL_TYPE_TEXT:
            return expr_constant_create_string(node->u.constant.str_value);
        default:
            return NULL;
    }
}

static expr_t *ast_binary_op_to_expr(const void *ast)
{
    if (ast == NULL) return NULL;
    const sql_node_t *node = (const sql_node_t *)ast;

    if (node->type != SQL_NODE_BINARY_OP) return NULL;

    /* 转换操作符：直接使用枚举值或 ASCII 码 */
    int op = node->u.binary_op.op;

    expr_t *left = ast_to_expr(node->u.binary_op.left);
    expr_t *right = ast_to_expr(node->u.binary_op.right);

    if (left == NULL || right == NULL) {
        expr_destroy(left);
        expr_destroy(right);
        return NULL;
    }

    return expr_binary_create(op, left, right);
}

static expr_t *ast_to_expr(const void *ast)
{
    if (ast == NULL) return NULL;
    const sql_node_t *node = (const sql_node_t *)ast;

    switch (node->type) {
        case SQL_NODE_COLUMN_REF:
            return ast_column_ref_to_expr(ast);
        case SQL_NODE_CONSTANT:
            return ast_constant_to_expr(ast);
        case SQL_NODE_BINARY_OP:
            return ast_binary_op_to_expr(ast);
        default:
            return NULL;
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 常量折叠
 * ───────────────────────────────────────────────────────────────── */

static int eval_int_expr(const expr_t *expr)
{
    if (expr == NULL) return 0;

    switch (expr->type) {
        case EXPR_CONSTANT:
            if (expr->u.constant.vtype == 'I') {
                return (int)expr->u.constant.val.int_val;
            }
            break;
        case EXPR_BINARY_OP: {
            int left = eval_int_expr(expr->u.binary.left);
            int right = eval_int_expr(expr->u.binary.right);
            switch (expr->u.binary.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : 0;
                default: break;
            }
            break;
        }
        case EXPR_UNARY_OP: {
            int child = eval_int_expr(expr->u.unary.child);
            switch (expr->u.unary.op) {
                case '-': return -child;
                default: break;
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

expr_t *expr_constant_fold(const expr_t *expr)
{
    if (expr == NULL) return NULL;

    expr_t *folded = NULL;

    switch (expr->type) {
        case EXPR_CONSTANT:
            folded = expr_copy(expr);
            break;

        case EXPR_COLUMN:
            folded = expr_copy(expr);
            break;

        case EXPR_BINARY_OP: {
            expr_t *left = expr_constant_fold(expr->u.binary.left);
            expr_t *right = expr_constant_fold(expr->u.binary.right);

            if (left && right &&
                left->type == EXPR_CONSTANT && right->type == EXPR_CONSTANT &&
                left->u.constant.vtype == 'I' && right->u.constant.vtype == 'I') {
                int result = eval_int_expr(expr);
                folded = expr_constant_create_int(result);
                expr_destroy(left);
                expr_destroy(right);
            } else {
                folded = expr_binary_create(expr->u.binary.op, left, right);
            }
            break;
        }

        case EXPR_UNARY_OP: {
            expr_t *child = expr_constant_fold(expr->u.unary.child);

            if (child && child->type == EXPR_CONSTANT &&
                child->u.constant.vtype == 'I') {
                int result = eval_int_expr(expr);
                folded = expr_constant_create_int(result);
                expr_destroy(child);
            } else {
                folded = expr_unary_create(expr->u.unary.op, child);
            }
            break;
        }

        default:
            folded = expr_copy(expr);
            break;
    }

    return folded;
}

/* ─────────────────────────────────────────────────────────────────
 * 向量搜索优化
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建向量索引扫描计划
 * @param table_name 表名
 * @param vector_dim 向量维度
 * @param top_k 返回数量
 * @param query_vector 查询向量（可选）
 * @param ef_search HNSW ef_search 参数
 * @param nprobe IVF nprobe 参数
 * @return 优化后的计划节点
 */
plan_node_t *plan_node_create_vector_scan(const char *table_name,
                                           int vector_dim,
                                           int32_t top_k,
                                           const float *query_vector,
                                           int32_t ef_search,
                                           int32_t nprobe) {
    plan_node_t *plan = plan_node_create(PLAN_SCAN_VECTOR);
    if (!plan) return NULL;

    /* 扫描信息 */
    plan->data.scan.table_name = table_name ? strdup(table_name) : NULL;
    plan->data.scan.vector_dim = vector_dim;
    plan->data.scan.top_k = top_k;
    plan->data.scan.reranker_type = "MMR";  /* 默认使用 MMR 精排 */

    /* 代价估算 */
    int64_t num_vectors = 1000000;  /* 假设默认 100 万向量 */
    const char *best_index = select_best_vector_index(num_vectors, top_k, ef_search, nprobe);

    if (strcmp(best_index, "HNSW") == 0) {
        plan->total_cost = cost_vector_hnsw(ef_search, num_vectors, top_k);
    } else if (strcmp(best_index, "IVF") == 0) {
        plan->total_cost = cost_vector_ivf(nprobe, num_vectors, top_k);
    } else {
        plan->total_cost = cost_vector_diskann(ef_search, num_vectors, top_k);
    }

    plan->plan_rows = top_k;
    plan->plan_width = vector_dim * sizeof(float);

    return plan;
}

/**
 * @brief 优化向量搜索查询
 * @param table_name 表名
 * @param vector_dim 向量维度
 * @param top_k 返回数量
 * @param ef_search HNSW ef_search 参数
 * @param nprobe IVF nprobe 参数
 * @return 优化后的计划节点
 */
plan_node_t *optimizer_optimize_vector(const char *table_name,
                                        int vector_dim,
                                        int32_t top_k,
                                        int32_t ef_search,
                                        int32_t nprobe) {
    return plan_node_create_vector_scan(table_name, vector_dim, top_k,
                                         NULL, ef_search, nprobe);
}

/* ─────────────────────────────────────────────────────────────────
 * 优化器主入口
 * ───────────────────────────────────────────────────────────────── */

plan_node_t *optimizer_optimize(void *ast)
{
    plan_node_t *plan = NULL;
    expr_t *filter = NULL;
    char *table_name = NULL;

    if (ast == NULL) {
        plan = plan_node_create(PLAN_SCAN_SEQ);
        if (plan) {
            plan->total_cost = 100.0;
            plan->plan_rows = 1000;
            plan->plan_width = 100;
        }
        return plan;
    }

    /* 将 AST 转换为优化器内部表达式 */
    const sql_node_t *node = (const sql_node_t *)ast;

    switch (node->type) {
        case SQL_NODE_SELECT:
            table_name = node->u.select.table_name ?
                strdup(node->u.select.table_name) : NULL;
            if (node->u.select.where_cond) {
                filter = ast_to_expr(node->u.select.where_cond);
                if (filter) {
                    filter = expr_constant_fold(filter);
                }
            }
            break;

        case SQL_NODE_UPDATE:
            table_name = node->u.update.table_name ?
                strdup(node->u.update.table_name) : NULL;
            if (node->u.update.where_cond) {
                filter = ast_to_expr(node->u.update.where_cond);
                if (filter) {
                    filter = expr_constant_fold(filter);
                }
            }
            break;

        case SQL_NODE_DELETE:
            table_name = node->u.del.table_name ?
                strdup(node->u.del.table_name) : NULL;
            if (node->u.del.where_cond) {
                filter = ast_to_expr(node->u.del.where_cond);
                if (filter) {
                    filter = expr_constant_fold(filter);
                }
            }
            break;

        default:
            break;
    }

    /* 创建扫描计划 */
    plan = plan_node_create(PLAN_SCAN_SEQ);
    if (plan && table_name) {
        plan->data.scan.table_name = table_name;
        plan->data.scan.filter = filter;

        /* 估算代价 */
        table_stats_t *stats = table_stats_create(1000, 10);
        double sel = filter ? analyze_compute_selectivity(stats, filter) : 1.0;
        plan->total_cost = cost_seq_scan(stats, sel);
        plan->plan_rows = 1000 * sel;
        plan->plan_width = 100;
        table_stats_destroy(stats);
    } else {
        if (filter) expr_destroy(filter);
        if (plan) {
            plan->total_cost = 100.0;
            plan->plan_rows = 1000;
        }
    }

    return plan;
}

/* ─────────────────────────────────────────────────────────────────
 * EXPLAIN 实现
 * ───────────────────────────────────────────────────────────────── */

static int explain_node(const plan_node_t *plan, int depth, char *buf, size_t buf_size)
{
    if (plan == NULL || buf == NULL) return 0;

    int written = 0;
    char indent[64] = {0};

    /* 生成缩进 */
    for (int i = 0; i < depth * 4 && i < (int)sizeof(indent) - 1; i++) {
        indent[i] = ' ';
    }

    /* 输出节点信息 */
    const char *type_name = "?";
    switch (plan->type) {
        case PLAN_SCAN_SEQ: type_name = "Seq Scan"; break;
        case PLAN_SCAN_INDEX: type_name = "Index Scan"; break;
        case PLAN_SCAN_INDEX_ONLY: type_name = "Index Only Scan"; break;
        case PLAN_FILTER: type_name = "Filter"; break;
        case PLAN_PROJECT: type_name = "Projection"; break;
        case PLAN_JOIN_HASH: type_name = "Hash Join"; break;
        case PLAN_JOIN_NESTED: type_name = "Nested Loop"; break;
        case PLAN_JOIN_MERGE: type_name = "Merge Join"; break;
        case PLAN_AGGREGATE: type_name = "Aggregate"; break;
        case PLAN_SORT: type_name = "Sort"; break;
        case PLAN_LIMIT: type_name = "Limit"; break;
        case PLAN_HASH: type_name = "Hash"; break;
        case PLAN_RESULT: type_name = "Result"; break;
        default: type_name = "Unknown"; break;
    }

    /* 节点基本信息 */
    char node_info[256] = "";
    switch (plan->type) {
        case PLAN_SCAN_SEQ:
        case PLAN_SCAN_INDEX:
        case PLAN_SCAN_INDEX_ONLY:
            if (plan->data.scan.table_name) {
                snprintf(node_info, sizeof(node_info), " on %s", plan->data.scan.table_name);
            }
            break;
        default:
            break;
    }

    written += snprintf(buf + written, buf_size - written,
                        "%s-> %s%s  (cost=%.2f..%.2f rows=%.0f)\n",
                        indent, type_name, node_info,
                        plan->startup_cost, plan->total_cost, plan->plan_rows);

    /* 递归输出子节点 */
    if (plan->left) {
        written += explain_node(plan->left, depth + 1, buf + written, buf_size - written);
    }
    if (plan->right) {
        written += explain_node(plan->right, depth + 1, buf + written, buf_size - written);
    }
    if (plan->subplan) {
        written += snprintf(buf + written, buf_size - written, "%s    SubPlan\n", indent);
        written += explain_node(plan->subplan, depth + 2, buf + written, buf_size - written);
    }

    return written;
}

int explain_plan(const plan_node_t *plan, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) return 0;
    buf[0] = '\0';

    if (plan == NULL) {
        snprintf(buf, buf_size, "(empty plan)");
        return (int)strlen(buf);
    }

    return explain_node(plan, 0, buf, buf_size);
}

char *explain_plan_text(const plan_node_t *plan)
{
    char *result = (char *)malloc(4096);
    if (result == NULL) return NULL;

    explain_plan(plan, result, 4096);
    return result;
}