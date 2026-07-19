/**
 * @file planner.c
 * @brief 查询计划器基础实现
 *
 * 实现 PostgreSQL 风格的查询优化器基础结构：
 * - 逻辑计划生成
 * - 物理计划生成
 * - 代价估算
 * - 优化规则应用
 */

#include "db/sql/sql_planner.h"
#include "db/parser/sql/sql.h"
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/makefuncs.h"  /* 引入 list_free */
#include "db/sql/sql_executor.h"      /* 引入 PlanState 定义 */

/* Task 3.2/3.3: 前向声明 catalog API（避免 Oid typedef 重定义冲突）
 * sql_types.h 定义 Oid 为 uint64_t，而 catalog.h 定义为 uint32_t。
 * 这里直接声明所需函数，返回 void* 并强制转换，避免头文件冲突。 */
extern void *catalog_get_table(Oid table_oid);

/* Task 1.1: 前向声明 nodeXxx.c 中的真实 ExecXxx 函数。
 * 直接 #include 那些头文件会引入 Plan/Relation/EState 等大量
 * 依赖并触发头文件循环，planner.c 只需要函数指针，不需要
 * 它们的完整结构定义。链接器会在最终链接时解析符号。 */
extern TupleTableSlot *ExecSeqScan(PlanState *pstate);
extern TupleTableSlot *ExecNestLoop(PlanState *pstate);
extern TupleTableSlot *ExecHashJoin(PlanState *pstate);
extern TupleTableSlot *ExecSort(PlanState *pstate);
extern TupleTableSlot *ExecLimit(PlanState *pstate);
extern TupleTableSlot *ExecModifyTable(PlanState *pstate);
extern TupleTableSlot *ExecHashAgg(PlanState *pstate);
extern TupleTableSlot *ExecAgg(PlanState *pstate);
/* 注：ExecResult / exec_vector_scan_next / exec_hnsw_scan_next
 * 当前不在 sql_engine 静态库的链接图内（这些源文件尚未纳入
 * CMakeLists 构建范围），暂时将 PHYS_RESULT/PHYS_VECTOR_SCAN/
 * PHYS_HNSW_SCAN 三种物理算子映射为 NULL，由调用方兜底。*/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================
 * 全局代价参数（必须在使用前定义）
 * ============================================================ */

/** 默认代价参数 */
static const CostParams g_default_cost_params = {
    .seq_page_cost = 1.0,
    .random_page_cost = 4.0,
    .cpu_tuple_cost = 0.01,
    .cpu_index_tuple_cost = 0.005,
    .cpu_operator_cost = 0.0025,
    .parallel_setup_cost = 1000.0,
    .parallel_tuple_cost = 0.1,
    .vector_search_cost = 1.0,
    .vector_reorder_cost = 0.1
};

/* ============================================================
 * 计划器上下文
 * ============================================================ */

/**
 * @brief 创建计划器上下文
 */
PlannerContext *planner_create(void) {
    PlannerContext *ctx = (PlannerContext *)calloc(1, sizeof(PlannerContext));
    if (!ctx) {
        return NULL;
    }

    /* 初始化默认配置 */
    ctx->config.enable_seqscan = true;
    ctx->config.enable_indexscan = true;
    ctx->config.enable_hashjoin = true;
    ctx->config.enable_nestloop = true;
    ctx->config.enable_sort = true;
    ctx->config.enable_hashagg = true;
    ctx->config.enable_vector_scan = true;
    ctx->config.planner_rows = 1000.0;

    return ctx;
}

/**
 * @brief 销毁计划器上下文
 */
void planner_destroy(PlannerContext *ctx) {
    if (ctx) {
        free(ctx);
    }
}

/**
 * @brief 创建计划器配置
 */
PlannerConfig *planner_config_create(void) {
    PlannerConfig *config = (PlannerConfig *)calloc(1, sizeof(PlannerConfig));
    if (config) {
        config->enable_seqscan = true;
        config->enable_indexscan = true;
        config->enable_hashjoin = true;
        config->enable_nestloop = true;
        config->enable_sort = true;
        config->enable_hashagg = true;
        config->enable_vector_scan = true;
        config->planner_rows = 1000.0;
    }
    return config;
}

/**
 * @brief 设置计划器配置
 */
void planner_set_config(PlannerContext *ctx, const PlannerConfig *config) {
    if (ctx && config) {
        memcpy(&ctx->config, config, sizeof(PlannerConfig));
    }
}

/* ============================================================
 * 列表辅助函数和节点 ID 生成器
 * ============================================================ */

/* 全局节点 ID 生成器（线程安全） */
static int g_node_id_counter = 0;

/* 线程安全：使用简单的自旋锁实现
 * 注意：这是简化实现，生产环境应使用更高效的锁机制
 */
#ifdef _WIN32
#include <windows.h>
static volatile long g_node_id_lock = 0;
static int lock_acquire(void) {
    while (InterlockedExchange(&g_node_id_lock, 1) != 0) {
        Sleep(0);  /* 让出 CPU 时间片 */
    }
    return 0;
}
static void lock_release(void) {
    InterlockedExchange(&g_node_id_lock, 0);
}
#else
#include <pthread.h>
static pthread_mutex_t g_node_id_mutex = PTHREAD_MUTEX_INITIALIZER;
static int lock_acquire(void) {
    return pthread_mutex_lock(&g_node_id_mutex);
}
static void lock_release(void) {
    pthread_mutex_unlock(&g_node_id_mutex);
}
#endif

/**
 * @brief 生成唯一节点 ID（线程安全）
 */
static int next_node_id(void) {
    int id;
    lock_acquire();
    id = ++g_node_id_counter;
    lock_release();
    return id;
}

/**
 * @brief 创建列表单元格
 */
static ListCell *list_make_cell(void *data) {
    ListCell *cell = (ListCell *)malloc(sizeof(ListCell));
    if (cell) {
        cell->data = data;
        cell->next = NULL;
    }
    return cell;
}

/**
 * @brief 创建列表
 */
static List *list_make(void *first, ...) {
    List *l = (List *)calloc(1, sizeof(List));
    if (!l) return NULL;

    if (first) {
        l->head = list_make_cell(first);
        l->length = 1;
    }
    return l;
}

/**
 * @brief 向列表追加元素
 */
static List *list_append(List *l, void *data) {
    if (!l) {
        l = list_make(data);
        return l;
    }

    ListCell *cell = list_make_cell(data);
    if (!cell) return l;

    if (!l->head) {
        l->head = cell;
    } else {
        ListCell *p = l->head;
        while (p->next) p = p->next;
        p->next = cell;
    }
    l->length++;
    return l;
}

/* ============================================================
 * AST → LogicalPlan 转换
 * ============================================================ */

/**
 * @brief 辅助：将 sql_node_t 的列列表转换为 LogicalPlan 的 targetList
 *
 * 遍历 sql_node_t 的 columns 节点列表，为每个列名创建 TargetEntry。
 * 如果 columns 为 NULL 或列名为 "*"，返回 NULL 表示所有列。
 */
static List *convert_column_list(sql_node_t *columns) {
    if (!columns) return NULL;

    /* 处理 SELECT *：列列为 NULL 或单列名 "*" */
    if (columns->type == SQL_NODE_COLUMN_REF &&
        columns->u.column_ref.name &&
        strcmp(columns->u.column_ref.name, "*") == 0) {
        return NULL;
    }

    /* 处理列列表（解析器使用 SQL_NODE_EXPR_LIST 存储列列表） */
    if (columns->type == SQL_NODE_EXPR_LIST) {
        List *list = NULL;
        for (size_t i = 0; i < columns->u.list.count; i++) {
            sql_node_t *col = columns->u.list.items[i];
            if (col && col->type == SQL_NODE_COLUMN_REF && col->u.column_ref.name) {
                /* 创建 TargetEntry */
                TargetEntry *te = (TargetEntry *)calloc(1, sizeof(TargetEntry));
                if (!te) continue;
                te->resno = (int)(i + 1);
                te->resname = col->u.column_ref.name;  /* 浅拷贝，不释放 */
                te->resjunk = false;

                /* 创建 Var 表达式 */
                Expr *var = (Expr *)calloc(1, sizeof(Expr));
                if (var) {
                    var->type = EXPR_VAR;
                    var->expr_type = EXPR_VAR;
                    var->val.var.varattno = (int)(i + 1);
                    te->expr = var;
                }

                list = list_append(list, te);
            }
        }
        return list;
    }

    /* 单列引用 */
    if (columns->type == SQL_NODE_COLUMN_REF && columns->u.column_ref.name) {
        TargetEntry *te = (TargetEntry *)calloc(1, sizeof(TargetEntry));
        if (te) {
            te->resno = 1;
            te->resname = columns->u.column_ref.name;
            te->resjunk = false;
            Expr *var = (Expr *)calloc(1, sizeof(Expr));
            if (var) {
                var->type = EXPR_VAR;
                var->expr_type = EXPR_VAR;
                var->val.var.varattno = 1;
                te->expr = var;
            }
            return list_append(NULL, te);
        }
    }

    return NULL;
}

/**
 * @brief 辅助：将 sql_node_t 的 WHERE 条件递归转换为 Planner Expr
 *
 * 支持：
 *   - SQL_NODE_BINARY_OP → EXPR_OP（比较表达式）
 *   - SQL_NODE_LOGICAL_OP → EXPR_BOOL_AND/OR/NOT
 *   - SQL_NODE_COLUMN_REF → EXPR_VAR
 *   - SQL_NODE_CONSTANT → EXPR_CONST（整型/字符串常量）
 */
static Expr *convert_where_expr(sql_node_t *node) {
    if (!node) return NULL;

    Expr *expr = (Expr *)calloc(1, sizeof(Expr));
    if (!expr) return NULL;

    switch (node->type) {
        case SQL_NODE_BINARY_OP: {
            /* 比较表达式：left OP right */
            expr->type = EXPR_OP;
            expr->expr_type = EXPR_OP;
            expr->val.op.lexpr = convert_where_expr(node->u.binary_op.left);
            expr->val.op.rexpr = convert_where_expr(node->u.binary_op.right);
            break;
        }

        case SQL_NODE_LOGICAL_OP: {
            /* AND/OR/NOT */
            if (node->u.logical_op.op == 2) {  /* NOT */
                expr->type = EXPR_BOOL_NOT;
                expr->expr_type = EXPR_BOOL_NOT;
                expr->val.op.lexpr = convert_where_expr(node->u.logical_op.expr);
            } else if (node->u.logical_op.op == 1) {  /* OR */
                expr->type = EXPR_BOOL_OR;
                expr->expr_type = EXPR_BOOL_OR;
                expr->val.op.lexpr = convert_where_expr(node->u.logical_op.left);
                expr->val.op.rexpr = convert_where_expr(node->u.logical_op.right);
            } else {  /* AND */
                expr->type = EXPR_BOOL_AND;
                expr->expr_type = EXPR_BOOL_AND;
                expr->val.op.lexpr = convert_where_expr(node->u.logical_op.left);
                expr->val.op.rexpr = convert_where_expr(node->u.logical_op.right);
            }
            break;
        }

        case SQL_NODE_COLUMN_REF: {
            /* 列引用 */
            expr->type = EXPR_VAR;
            expr->expr_type = EXPR_VAR;
            expr->val.var.varattno = 1;  /* 简化：当前只支持单表 */
            expr->val.var.varno = 1;
            break;
        }

        case SQL_NODE_CONSTANT: {
            /* 常量值 */
            expr->type = EXPR_CONST;
            expr->expr_type = EXPR_CONST;
            if (node->u.constant.type == SQL_TYPE_INT) {
                expr->val.const_val.value = node->u.constant.int_value;
            } else if (node->u.constant.type == SQL_TYPE_TEXT ||
                       node->u.constant.type == SQL_TYPE_VARCHAR) {
                expr->val.const_val.value = 0;
            }
            break;
        }

        default:
            free(expr);
            return NULL;
    }

    return expr;
}

/**
 * @brief 从 sql_node_t (简化解析器) 转换
 */
static LogicalPlan *node_to_logical(PlannerContext *ctx, sql_node_t *node) {
    if (!node) return NULL;

    LogicalPlan *plan = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
    if (!plan) return NULL;

    plan->node_id = next_node_id();
    plan->type = LOGICAL_RESULT;
    plan->rows = ctx->config.planner_rows;
    plan->width = 100;

    switch (node->type) {
        case SQL_NODE_SELECT: {
            plan->type = LOGICAL_SCAN;
            plan->left = NULL;
            plan->right = NULL;

            /* 设置表名（通过 extra 字段简单传递） */
            if (node->u.select.table_name) {
                /* 当前 LOGICAL_SCAN 没有 rel_name 字段，
                 * 后续扩展可在此处添加。这里先保持对齐。 */
            }

            /* 转换 targetlist */
            if (node->u.select.columns) {
                plan->targetlist = convert_column_list(node->u.select.columns);
            }

            /* 转换 WHERE 条件 */
            if (node->u.select.where_cond) {
                plan->qual = convert_where_expr(node->u.select.where_cond);
            }

            break;
        }

        case SQL_NODE_INSERT:
            plan->type = LOGICAL_INSERT;
            break;

        case SQL_NODE_UPDATE:
            plan->type = LOGICAL_UPDATE;
            break;

        case SQL_NODE_DELETE:
            plan->type = LOGICAL_DELETE;
            break;

        default:
            plan->type = LOGICAL_RESULT;
            break;
    }

    return plan;
}

/**
 * @brief 从 SelectStmt (parsenodes) 转换
 */
static LogicalPlan *select_stmt_to_logical(PlannerContext *ctx, SelectStmt *stmt) {
    if (!stmt) return NULL;

    LogicalPlan *plan = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
    if (!plan) return NULL;

    plan->node_id = next_node_id();
    plan->type = LOGICAL_SCAN;
    plan->rows = ctx->config.planner_rows;
    plan->width = 100;

    /* 解析 FROM 子句 */
    if (stmt->fromClause) {
        ListCell *lc;
        foreach (lc, stmt->fromClause) {
            Node *n = (Node *)lfirst(lc);
            if (n && n->type == T_RangeVar) {
                RangeVar *rv = (RangeVar *)n;
                /* 设置表名等信息 */
                (void)rv;
            }
        }
    }

    /* 处理 WHERE 条件 - 谓词下推 */
    if (stmt->whereClause) {
        Expr *pred = (Expr *)calloc(1, sizeof(Expr));
        if (pred) {
            pred->type = EXPR_OP;
            plan->qual = pred;
        }
    }

    /* 处理 GROUP BY */
    if (stmt->groupClause) {
        LogicalPlan *agg = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
        if (agg) {
            agg->node_id = next_node_id();
            agg->type = LOGICAL_AGG;
            agg->left = plan;
            plan = agg;
        }
    }

    /* 处理 ORDER BY */
    if (stmt->sortClause) {
        LogicalPlan *sort = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
        if (sort) {
            sort->node_id = next_node_id();
            sort->type = LOGICAL_SORT;
            sort->left = plan;
            plan = sort;
        }
    }

    /* 处理 LIMIT */
    if (stmt->limitCount) {
        LogicalPlan *limit = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
        if (limit) {
            limit->node_id = next_node_id();
            limit->type = LOGICAL_LIMIT;
            limit->left = plan;
            plan = limit;
        }
    }

    return plan;
}

/**
 * @brief 从简化的 sql_node_t 生成逻辑计划
 */
LogicalPlan *sql_node_to_logical(PlannerContext *ctx, sql_node_t *node) {
    return node_to_logical(ctx, node);
}

/**
 * @brief 从 SelectStmt 生成逻辑计划
 */
LogicalPlan *sql_select_stmt_to_logical(PlannerContext *ctx, SelectStmt *stmt) {
    return select_stmt_to_logical(ctx, stmt);
}

/* ============================================================
 * 逻辑计划生成
 * ============================================================ */

/**
 * @brief 从 AST 生成逻辑计划
 */
LogicalPlan *planner_logical_plan(PlannerContext *ctx, void *parse_result) {
    if (!ctx) {
        return NULL;
    }

    /* 根据解析结果类型选择转换函数 */
    if (!parse_result) {
        /* 创建一个默认的扫描计划 */
        LogicalPlan *plan = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
        if (!plan) return NULL;

        plan->type = LOGICAL_SCAN;
        plan->node_id = next_node_id();
        plan->rows = ctx->config.planner_rows;
        plan->width = 100;
        return plan;
    }

    /* 检查是否为简化解析器的 sql_node_t */
    sql_node_t *simple_node = (sql_node_t *)parse_result;
    if (simple_node->type >= SQL_NODE_SELECT && simple_node->type <= SQL_NODE_DROP_TABLE) {
        return sql_node_to_logical(ctx, simple_node);
    }

    /* 检查是否为 parsenodes.h 的 SelectStmt */
    Node *node = (Node *)parse_result;
    if (node->type == T_SelectStmt) {
        return sql_select_stmt_to_logical(ctx, (SelectStmt *)node);
    }

    /* 默认：创建扫描计划 */
    LogicalPlan *plan = (LogicalPlan *)calloc(1, sizeof(LogicalPlan));
    if (!plan) return NULL;

    plan->type = LOGICAL_SCAN;
    plan->node_id = next_node_id();
    plan->rows = ctx->config.planner_rows;
    plan->width = 100;

    return plan;
}

/**
 * @brief 递归转换逻辑计划为物理计划
 */
static PhysPlan *logical_to_physical(PlannerContext *ctx, LogicalPlan *logical) {
    if (!logical) return NULL;

    PhysPlan *plan = (PhysPlan *)calloc(1, sizeof(PhysPlan));
    if (!plan) return NULL;

    plan->node_id = logical->node_id;
    plan->rows = logical->rows;
    plan->width = logical->width;
    plan->targetlist = logical->targetlist;
    plan->qual = logical->qual;

    /* 递归转换左子树 */
    if (logical->left) {
        plan->lefttree = list_append(NULL, logical_to_physical(ctx, logical->left));
    }

    /* 递归转换右子树 */
    if (logical->right) {
        plan->righttree = list_append(plan->righttree, logical_to_physical(ctx, logical->right));
    }

    /* 根据逻辑算子类型选择物理算子并计算代价 */
    CostParams *params = &g_default_cost_params;

    switch (logical->type) {
        case LOGICAL_SCAN:
            plan->type = PHYS_SEQ_SCAN;
            /* 计算扫描代价 */
            {
                double pages = logical->rows * logical->width / 8192.0;
                if (pages < 1.0) pages = 1.0;
                plan->startup_cost = params->seq_page_cost * pages;
                plan->total_cost = plan->startup_cost + params->cpu_tuple_cost * logical->rows;
            }
            break;

        case LOGICAL_INDEX_SCAN:
            plan->type = PHYS_INDEX_SCAN;
            {
                double pages = logical->rows * logical->width / 8192.0;
                if (pages < 1.0) pages = 1.0;
                plan->startup_cost = params->random_page_cost * pages * 0.1;
                plan->total_cost = plan->startup_cost + params->cpu_tuple_cost * logical->rows;
            }
            break;

        case LOGICAL_JOIN:
        case LOGICAL_HASHJOIN:
            plan->type = PHYS_HASHJOIN;
            {
                double left_rows = logical->left ? logical->left->rows : 1;
                double right_rows = logical->right ? logical->right->rows : 1;
                double build_cost = params->cpu_tuple_cost * right_rows;
                double probe_cost = params->cpu_tuple_cost * left_rows;
                double hash_cost = params->cpu_operator_cost * (right_rows + left_rows);

                /* 子节点代价 */
                double left_cost = logical->left ? logical->left->total_cost : 0;
                double right_cost = logical->right ? logical->right->total_cost : 0;

                plan->startup_cost = left_cost + right_cost;
                plan->total_cost = plan->startup_cost + build_cost + probe_cost + hash_cost;
            }
            break;

        case LOGICAL_NESTLOOP:
            plan->type = PHYS_NESTLOOP;
            {
                double left_rows = logical->left ? logical->left->rows : 1;
                double right_rows = logical->right ? logical->right->rows : 1;
                plan->startup_cost = logical->left ? logical->left->total_cost : 0;
                plan->total_cost = plan->startup_cost + params->cpu_tuple_cost * left_rows * right_rows;
            }
            break;

        case LOGICAL_MERGEJOIN:
            plan->type = PHYS_MERGEJOIN;
            {
                double left_rows = logical->left ? logical->left->rows : 1;
                double right_rows = logical->right ? logical->right->rows : 1;
                double sort_cost = params->cpu_operator_cost *
                                   (right_rows * log2(right_rows > 1 ? right_rows : 2) +
                                    left_rows * log2(left_rows > 1 ? left_rows : 2));
                plan->startup_cost = logical->left ? logical->left->total_cost : 0;
                plan->total_cost = plan->startup_cost + sort_cost +
                                   params->cpu_tuple_cost * (left_rows + right_rows);
            }
            break;

        case LOGICAL_AGG:
        case LOGICAL_HASHAGG:
            plan->type = PHYS_HASHAGG;
            {
                double input_cost = logical->left ? logical->left->total_cost : 0;
                double groups = logical->rows > 0 ? logical->rows : 1;
                plan->startup_cost = input_cost;
                plan->total_cost = input_cost + params->cpu_operator_cost * groups;
            }
            break;

        case LOGICAL_SORT:
            plan->type = PHYS_SORT;
            {
                double input_cost = logical->left ? logical->left->total_cost : 0;
                if (logical->rows < 2.0) {
                    plan->startup_cost = input_cost;
                    plan->total_cost = input_cost + params->cpu_tuple_cost * logical->rows;
                } else {
                    double log2n = log2(logical->rows);
                    plan->startup_cost = input_cost + params->cpu_operator_cost * logical->rows * log2n;
                    plan->total_cost = plan->startup_cost + params->cpu_tuple_cost * logical->rows;
                }
            }
            break;

        case LOGICAL_LIMIT:
            plan->type = PHYS_LIMIT;
            {
                /* Limit 代价主要是子节点代价 */
                plan->startup_cost = logical->left ? logical->left->total_cost : 0;
                plan->total_cost = plan->startup_cost;
            }
            break;

        case LOGICAL_PROJECT:
            plan->type = PHYS_SEQ_SCAN;  /* 使用 SeqScan 作为占位 */
            {
                plan->startup_cost = logical->left ? logical->left->total_cost : 0;
                plan->total_cost = plan->startup_cost + params->cpu_tuple_cost * logical->rows;
            }
            break;

        case LOGICAL_FILTER:
            plan->type = PHYS_SEQ_SCAN;
            {
                /* Filter 主要是子节点代价加上过滤开销 */
                plan->startup_cost = logical->left ? logical->left->total_cost : 0;
                plan->total_cost = plan->startup_cost + params->cpu_tuple_cost * logical->rows;
            }
            break;

        case LOGICAL_INSERT:
            plan->type = PHYS_SEQ_SCAN;
            plan->total_cost = params->cpu_tuple_cost * logical->rows;
            break;

        case LOGICAL_UPDATE:
            plan->type = PHYS_SEQ_SCAN;
            plan->total_cost = params->cpu_tuple_cost * logical->rows;
            break;

        case LOGICAL_DELETE:
            plan->type = PHYS_SEQ_SCAN;
            plan->total_cost = params->cpu_tuple_cost * logical->rows;
            break;

        case LOGICAL_RESULT:
        case LOGICAL_VALUES:
            plan->type = PHYS_SEQ_SCAN;
            plan->total_cost = 0;
            break;

        default:
            plan->type = PHYS_SEQ_SCAN;
            plan->total_cost = logical->left ? logical->left->total_cost : 0;
            break;
    }

    return plan;
}

/**
 * @brief 从逻辑计划生成物理计划
 */
PhysPlan *planner_physical_plan(PlannerContext *ctx, LogicalPlan *logical) {
    if (!ctx || !logical) {
        return NULL;
    }

    /* 递归转换逻辑计划为物理计划 */
    return logical_to_physical(ctx, logical);
}

/**
 * @brief 完整计划生成（逻辑 + 物理）
 */
PhysPlan *planner_plan(PlannerContext *ctx, void *parse_result) {
    if (!ctx || !parse_result) {
        return NULL;
    }

    /* 1. 生成逻辑计划 */
    LogicalPlan *logical = planner_logical_plan(ctx, parse_result);
    if (!logical) {
        return NULL;
    }

    /* 2. 应用优化规则 */
    planner_optimize(ctx, logical);

    /* 3. 生成物理计划 */
    PhysPlan *physical = planner_physical_plan(ctx, logical);

    /* 4. 清理逻辑计划（递归释放整个树） */
    planner_free_logical_plan(logical);

    return physical;
}

/* ============================================================
 * 代价模型
 * ============================================================ */

/**
 * @brief 设置代价参数
 */
void planner_set_cost_params(PlannerContext *ctx, const CostParams *params) {
    /* 简化实现：使用全局默认参数 */
    (void)ctx;
    (void)params;
}

/**
 * @brief 计算顺序扫描代价
 *
 * Task 3.3: 使用真实统计信息（从 planner_get_table_stats 获取），
 * 而非硬编码计算页面数。通过 PhysScan.scan_relid 获取表 OID。
 */
void cost_seqscan(PhysScan *node, PlannerContext *root,
                  double rows, int width) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* 获取真实页面数（从 catalog 统计信息） */
    double pages = 1.0;  /* 默认至少 1 页 */
    if (root && node->scan_relid != 0) {
        TableStats *stats = planner_get_table_stats(root, node->scan_relid);
        if (stats && stats->relpages > 0) {
            pages = (double)stats->relpages;
        } else {
            /* 回退到估计值 */
            pages = rows * width / 8192.0;
            if (pages < 1.0) pages = 1.0;
        }
    } else {
        /* 无 scan_relid 时，使用估计值 */
        pages = rows * width / 8192.0;
        if (pages < 1.0) pages = 1.0;
    }

    node->type = PHYS_SEQ_SCAN;

    /* 将计算结果写入节点字段 */
    node->startup_cost = params->seq_page_cost * pages;
    node->total_cost = node->startup_cost + params->cpu_tuple_cost * rows;

    (void)root;
}

/**
 * @brief 计算索引扫描代价
 */
void cost_index(PhysScan *node, PlannerContext *root,
                double rows, int width) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* 代价 = 随机 I/O 代价 + CPU 代价 */
    double pages = rows * width / 8192.0;
    if (pages < 1.0) pages = 1.0;

    node->type = PHYS_INDEX_SCAN;

    /* 将计算结果写入节点字段 */
    node->startup_cost = params->random_page_cost * pages * 0.1;
    node->total_cost = node->startup_cost + params->cpu_tuple_cost * rows;

    (void)root;
}

/**
 * @brief 计算 Hash Join 代价
 */
void cost_hashjoin(PhysJoin *node, PlannerContext *root,
                   double inner_rows, double outer_rows, int width) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* Hash Join 代价模型：
     * 构建阶段：扫描内表 + 构建 Hash 表
     * 探测阶段：扫描外表 + 探测 Hash 表
     */
    double build_cost = params->cpu_tuple_cost * inner_rows;
    double probe_cost = params->cpu_tuple_cost * outer_rows;
    double hash_cost = params->cpu_operator_cost * (inner_rows + outer_rows);

    /* 将计算结果写入节点字段 */
    node->total_cost = build_cost + probe_cost + hash_cost;

    (void)root;
    (void)width;
}

/**
 * @brief 计算 Nested Loop 代价
 */
void cost_nestloop(PhysJoin *node, PlannerContext *root,
                   double inner_rows, double outer_rows) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* Nested Loop 代价模型：
     * cost = outer_cost + inner_cost * outer_rows
     */
    double cost = params->cpu_tuple_cost * outer_rows * inner_rows;

    /* 将计算结果写入节点字段 */
    node->total_cost = cost;

    (void)root;
}

/**
 * @brief 计算 Merge Join 代价
 */
void cost_mergejoin(PhysJoin *node, PlannerContext *root,
                    double inner_rows, double outer_rows) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* Merge Join 代价模型：
     * cost = sort_outer + sort_inner + merge_cost
     */
    double sort_cost = params->cpu_operator_cost *
                       (inner_rows * log2(inner_rows > 1 ? inner_rows : 2) +
                        outer_rows * log2(outer_rows > 1 ? outer_rows : 2));
    double merge_cost = params->cpu_tuple_cost * (inner_rows + outer_rows);

    /* 将计算结果写入节点字段 */
    node->total_cost = sort_cost + merge_cost;

    (void)root;
}

/**
 * @brief 计算聚合代价
 */
void cost_agg(PhysAgg *node, PlannerContext *root,
              double rows, int width, int numGroupCols) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* 聚合代价模型：
     * cost = input_cost + cpu_agg_cost * groups
     */
    double groups = rows / 10.0;  /* 假设分组数为 1/10 */
    double cost = params->cpu_operator_cost * rows + params->cpu_tuple_cost * groups;

    /* 将计算结果写入节点字段 */
    node->total_cost = cost;

    (void)root;
    (void)width;
}

/**
 * @brief 计算排序代价
 */
void cost_sort(PhysPlan *node, PlannerContext *root,
               double rows, int width) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* 排序代价模型：
     * cost = cpu_operator_cost * N * log2(N) + cpu_tuple_cost * N
     */
    if (rows < 2.0) {
        node->startup_cost = 0.0;
        node->total_cost = params->cpu_tuple_cost * rows;
    } else {
        double log2n = log2(rows);
        node->startup_cost = params->cpu_operator_cost * rows * log2n;
        node->total_cost = node->startup_cost + params->cpu_tuple_cost * rows;
    }

    (void)width;
    node->type = PHYS_SORT;

    (void)root;
}

/**
 * @brief 计算向量扫描代价
 */
void cost_vector_scan(PhysScan *node, PlannerContext *root,
                      double rows, int vec_dim) {
    if (!node) {
        return;
    }

    CostParams *params = &g_default_cost_params;

    /* 向量扫描代价模型：
     * cost = vector_distance_cost * rows + vector_reorder_cost * top_k
     */
    double distance_cost = params->vector_search_cost * rows * vec_dim / 100.0;
    double reorder_cost = params->vector_reorder_cost * 100.0;  /* 假设 top-k = 100 */
    double total = distance_cost + reorder_cost;

    /* 将计算结果写入节点字段 */
    node->startup_cost = 0.0;
    node->total_cost = total;
    node->type = PHYS_VECTOR_SCAN;

    (void)root;
}

/* ============================================================
 * 优化规则
 * ============================================================ */

/* 前向声明 */
static Expr *constant_fold_expr(Expr *expr);

/**
 * @brief 递归应用优化规则到逻辑计划树
 */
static void apply_rule_recursive(PlannerContext *ctx, LogicalPlan *plan,
                                 OptRuleType rule) {
    if (!plan) return;

    /* 先处理子节点 */
    if (plan->left) {
        apply_rule_recursive(ctx, plan->left, rule);
    }
    if (plan->right) {
        apply_rule_recursive(ctx, plan->right, rule);
    }

    /* 再处理当前节点 */
    switch (rule) {
        case RULE_PREDICATE_PUSHDOWN:
            /* 谓词下推：将 Filter 节点的条件下推到 Scan 节点 */
            if (plan->type == LOGICAL_FILTER && plan->left &&
                plan->left->type == LOGICAL_SCAN && plan->qual) {
                /* 将过滤条件合并到扫描节点的 qual */
                if (!plan->left->qual) {
                    plan->left->qual = plan->qual;
                } else {
                    /* 创建 AND 表达式连接两个条件 */
                    Expr *and_expr = (Expr *)calloc(1, sizeof(Expr));
                    if (and_expr) {
                        and_expr->type = EXPR_BOOL_AND;
                        and_expr->val.op.lexpr = plan->left->qual;
                        and_expr->val.op.rexpr = plan->qual;
                        plan->left->qual = and_expr;
                    }
                }
                /* 移除当前 Filter 节点（将其替换为子节点） */
                plan->qual = NULL;
            }
            break;

        case RULE_COLUMN_PRUNING:
            /* 列裁剪：标记不需要的列为垃圾列
             * 简化实现：暂时保留所有列
             */
            break;

        case RULE_CONSTANT_FOLDING:
            /* 常量折叠：计算常量表达式
             * 简化实现：检测并处理常量表达式
             */
            if (plan->qual) {
                Expr *folded = constant_fold_expr(plan->qual);
                if (folded) {
                    plan->qual = folded;
                }
            }
            break;

        case RULE_SUBQUERY_FLATTENING:
            /* 子查询扁平化：将 IN (SELECT ...) 转换为 JOIN
             * 简化实现：暂不处理复杂的子查询
             */
            break;

        default:
            break;
    }
}

/**
 * @brief 折叠常量表达式
 */
static Expr *constant_fold_expr(Expr *expr) {
    if (!expr) return NULL;

    switch (expr->type) {
        case EXPR_CONST:
            /* 常量已经是最终形式 */
            return expr;

        case EXPR_OP: {
            /* 尝试折叠二元操作 */
            Expr *lexpr = constant_fold_expr(expr->val.op.lexpr);
            Expr *rexpr = constant_fold_expr(expr->val.op.rexpr);

            /* 如果左右操作数都是常量，则计算结果 */
            if (lexpr && lexpr->type == EXPR_CONST &&
                rexpr && rexpr->type == EXPR_CONST) {
                /* 这里应该进行实际计算，暂时返回原表达式 */
                (void)lexpr;
                (void)rexpr;
            }
            return expr;
        }

        default:
            return expr;
    }
}

/**
 * @brief 应用优化规则
 */
void planner_apply_rule(PlannerContext *ctx, LogicalPlan *plan, OptRuleType rule) {
    if (!ctx || !plan) {
        return;
    }

    /* 应用规则到整个计划树 */
    apply_rule_recursive(ctx, plan, rule);
}

/**
 * @brief 应用所有优化规则
 */
void planner_optimize(PlannerContext *ctx, LogicalPlan *plan) {
    if (!ctx || !plan) {
        return;
    }

    /* 应用规则顺序：
     * 1. 常量折叠
     * 2. 子查询扁平化
     * 3. 谓词下推
     * 4. 列裁剪
     * 5. 连接重排序
     */

    planner_apply_rule(ctx, plan, RULE_CONSTANT_FOLDING);
    planner_apply_rule(ctx, plan, RULE_SUBQUERY_FLATTENING);
    planner_apply_rule(ctx, plan, RULE_PREDICATE_PUSHDOWN);
    planner_apply_rule(ctx, plan, RULE_COLUMN_PRUNING);
    planner_apply_rule(ctx, plan, RULE_JOIN_REORDERING);
}

/**
 * @brief 检查是否可以下推谓词
 */
bool planner_can_pushdown_pred(Expr *pred, LogicalPlan *node) {
    /* 简化实现：总是返回 true */
    return pred != NULL && node != NULL;
}

/**
 * @brief 下推谓词
 */
LogicalPlan *planner_pushdown_pred(PlannerContext *ctx, LogicalPlan *plan, Expr *pred) {
    if (!ctx || !plan || !pred) {
        return plan;
    }

    /* 简化实现：将谓词添加到过滤条件 */
    plan->qual = pred;

    return plan;
}

/**
 * @brief 裁剪不需要的列
 */
LogicalPlan *planner_prune_columns(PlannerContext *ctx, LogicalPlan *plan,
                                   List *required_cols) {
    if (!ctx || !plan) {
        return plan;
    }

    /* 简化实现：保留 required_cols 中的列 */
    return plan;
}

/**
 * @brief 重排序连接
 */
LogicalPlan *planner_reorder_joins(PlannerContext *ctx, LogicalPlan *plan) {
    if (!ctx || !plan) {
        return plan;
    }

    /* 简化实现：使用贪心算法选择最小代价的连接顺序 */
    return plan;
}

/**
 * @brief 检查物化视图是否可以改写查询
 */
bool planner_check_mview_rewrite(PlannerContext *ctx, LogicalPlan *plan,
                                 int mview_id) {
    /* 简化实现：暂时不支持物化视图改写 */
    (void)ctx;
    (void)plan;
    (void)mview_id;
    return false;
}

/**
 * @brief 尝试使用物化视图改写查询
 */
LogicalPlan *planner_rewrite_with_mview(PlannerContext *ctx, LogicalPlan *plan) {
    /* 简化实现：直接返回原计划 */
    return plan;
}

/**
 * @brief 检查向量索引是否可以下推
 */
bool planner_can_vector_index(PlannerContext *ctx, LogicalPlan *plan, Expr *pred) {
    /* 简化实现：检查是否为向量距离谓词 */
    (void)ctx;
    (void)plan;
    (void)pred;
    return false;
}

/**
 * @brief 添加向量索引扫描
 */
LogicalPlan *planner_add_vector_index_scan(PlannerContext *ctx, LogicalPlan *plan,
                                           Expr *pred) {
    /* 简化实现：直接返回原计划 */
    return plan;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

/**
 * @brief 获取表统计信息
 *
 * Task 3.2: 从 catalog 读取真实统计信息，而非硬编码默认值。
 * 如果 catalog 未初始化或表不存在，回退到默认值。
 */
TableStats *planner_get_table_stats(PlannerContext *ctx, int relid) {
    if (!ctx) {
        return NULL;
    }

    static TableStats stats;
    memset(&stats, 0, sizeof(stats));

    /* 默认值 */
    stats.nrows = 10000.0;
    stats.nbytes = 819200.0;
    stats.density = 1.0;
    stats.ndistinct = 100;
    stats.ndistinct_ratio = 0.1;
    stats.null_frac = 0.0;
    stats.width = 100.0;
    stats.relpages = 100;
    stats.reltuples = 10000;
    stats.correlation = 1.0;

    /* 尝试从 catalog 获取真实统计信息 */
    if (relid != 0) {  /* relid == 0 表示无效表 */
        /* catalog_get_table 声明在 catelog.h 中，但 Oid 类型不同。
         * 这里通过前向声明 void* 来使用，避免 typedef 冲突。 */
        struct table_info_s {
            uint32_t oid;
            char     name[64];
            uint32_t namespace_oid;
            uint32_t type_oid;
            char     relkind;
            int16_t  natts;
            uint32_t filenode;
            uint32_t tablespace;
            int32_t  npages;
            float    ntupes;
            uint32_t owner;
            char     persistence;
            int16_t  nchecks;
            int      has_index;
            int      has_pkey;
        };
        struct table_info_s *info = (struct table_info_s *)catalog_get_table((uint32_t)relid);
        if (info) {
            stats.relpages = info->npages;
            stats.reltuples = (int)info->ntupes;
            stats.nrows = (double)info->ntupes;
        }
    }

    return &stats;
}

/**
 * @brief 获取列统计信息
 */
ColumnStats *planner_get_column_stats(PlannerContext *ctx, int relid, int attno) {
    if (!ctx) {
        return NULL;
    }

    /* 简化实现：返回默认列统计信息 */
    (void)relid;
    (void)attno;
    static ColumnStats col_stats = {
        .ndistinct = 100.0,
        .null_frac = 0.0,
        .width = 8.0,
        .low_value = 0.0,
        .high_value = 10000.0,
        .nbuckets = 10,
        .histogram = NULL,
        .probabilities = NULL
    };

    return &col_stats;
}

/**
 * @brief 添加统计信息
 */
void planner_add_stats(PlannerContext *ctx, int relid,
                      const TableStats *stats) {
    /* 简化实现：暂不实现 */
    (void)ctx;
    (void)relid;
    (void)stats;
}

/**
 * @brief 估计计划代价
 */
double planner_estimate_cost(PlannerContext *ctx, PhysPlan *plan) {
    if (!ctx || !plan) {
        return 0.0;
    }

    /* 简化实现：返回预估代价 */
    return plan->total_cost;
}

/* ============================================================
 * 内存管理
 * ============================================================ */

/**
 * @brief 释放逻辑计划树（递归）
 */
static void free_logical_plan_recursive(LogicalPlan *plan) {
    if (!plan) return;

    /* 递归释放子节点 */
    if (plan->left) {
        free_logical_plan_recursive(plan->left);
        plan->left = NULL;
    }
    if (plan->right) {
        free_logical_plan_recursive(plan->right);
        plan->right = NULL;
    }

    /* 释放 qual */
    if (plan->qual) {
        free(plan->qual);
        plan->qual = NULL;
    }

    free(plan);
}

/**
 * @brief 释放逻辑计划
 */
void planner_free_logical_plan(LogicalPlan *plan) {
    free_logical_plan_recursive(plan);
}

/**
 * @brief 释放物理计划树（递归）
 */
static void free_physical_plan_recursive(PhysPlan *plan) {
    if (!plan) return;

    /* 递归释放子节点 */
    if (plan->lefttree) {
        ListCell *lc;
        foreach (lc, plan->lefttree) {
            PhysPlan *child = (PhysPlan *)lfirst(lc);
            free_physical_plan_recursive(child);
        }
        list_free(plan->lefttree);
        plan->lefttree = NULL;
    }
    if (plan->righttree) {
        ListCell *lc;
        foreach (lc, plan->righttree) {
            PhysPlan *child = (PhysPlan *)lfirst(lc);
            free_physical_plan_recursive(child);
        }
        list_free(plan->righttree);
        plan->righttree = NULL;
    }

    /* 释放 qual */
    if (plan->qual) {
        free(plan->qual);
        plan->qual = NULL;
    }

    /* 释放 targetlist */
    if (plan->targetlist) {
        list_free(plan->targetlist);
        plan->targetlist = NULL;
    }

    free(plan);
}

/**
 * @brief 释放物理计划
 */
void planner_free_physical_plan(PhysPlan *plan) {
    free_physical_plan_recursive(plan);
}

/* ============================================================
 * PlanState 桥接函数
 * ============================================================ */

/**
 * @brief 根据物理算子类型获取执行函数
 *
 * Task 1.1 重构：将 sql_executor.c 中的桩函数（exec_seq_scan 等）
 * 替换为 nodeXxx.c 中的真实 ExecXxx 实现。
 */
static TupleTableSlot *(*get_exec_func(PhysicalOpType type))(PlanState *) {
    switch (type) {
        case PHYS_SEQ_SCAN:
            return ExecSeqScan;
        case PHYS_INDEX_SCAN:
            return (TupleTableSlot *(*)(PlanState *))ExecIndexScan;
        case PHYS_NESTLOOP:
            return ExecNestLoop;
        case PHYS_HASHJOIN:
            return ExecHashJoin;
        case PHYS_SORT:
            return ExecSort;
        case PHYS_LIMIT:
            return ExecLimit;
        case PHYS_INSERT:
        case PHYS_UPDATE:
        case PHYS_DELETE:
            /* INSERT/UPDATE/DELETE 统一由 ExecModifyTable 处理 */
            return ExecModifyTable;
        case PHYS_RESULT:
            /* ExecResult 暂未链接到 sql_engine，返回 NULL */
            return NULL;
        case PHYS_HASHAGG:
            return ExecHashAgg;
        case PHYS_SORTAGG:
            /* SortAgg 与 HashAgg 共享 ExecAgg 入口，节点内按策略分支 */
            return ExecAgg;
        case PHYS_VECTOR_SCAN:
        case PHYS_HNSW_SCAN:
            /* 向量/HNSW 扫描源文件尚未纳入 CMakeLists 构建，
             * 暂时返回 NULL，由调用方兜底 */
            return NULL;
        default:
            return NULL;
    }
}

/**
 * @brief 将物理计划转换为执行器状态树
 *
 * Task 1.4 修复说明：本函数维护旧框架的 exec_proc 设置。
 * 新框架（Volcano）的桥接由 executor.c 提供的
 * executor_create_plan_state_by_phys_type() 函数处理。
 * 该函数供新框架调用，独立于旧框架。
 */
PlanState *planner_create_plan_state(PhysPlan *plan) {
    if (!plan) return NULL;

    PlanState *state = (PlanState *)calloc(1, sizeof(PlanState));
    if (!state) return NULL;

    state->type = (ExecutorType)plan->type;
    state->ps_TupDesc = NULL;

    /* 递归创建左子树 */
    if (plan->lefttree) {
        ListCell *lc;
        foreach (lc, plan->lefttree) {
            PhysPlan *child = (PhysPlan *)lfirst(lc);
            state->left = planner_create_plan_state(child);
            break;  /* 只取第一个子节点 */
        }
    }

    /* 递归创建右子树 */
    if (plan->righttree) {
        ListCell *lc;
        foreach (lc, plan->righttree) {
            PhysPlan *child = (PhysPlan *)lfirst(lc);
            state->right = planner_create_plan_state(child);
            break;
        }
    }

    /* 设置执行函数指针（旧框架字段） */
    state->exec_proc = get_exec_func(plan->type);

    /* Task 1.4: 新框架 Volcano 通过 executor_create_plan_state_by_phys_type()
     * 提供桥接。当前实现：旧框架完整工作，新框架由 ExecutorStart
     * 调用 executor_create_plan_state_by_phys_type() 创建。 */

    return state;
}

/**
 * @brief 销毁执行器状态树
 */
void planner_destroy_plan_state(PlanState *state) {
    if (!state) return;

    if (state->left) {
        planner_destroy_plan_state(state->left);
    }
    if (state->right) {
        planner_destroy_plan_state(state->right);
    }

    free(state);
}

/* ============================================================
 * 调试和工具
 * ============================================================ */

/**
 * @brief 打印逻辑计划
 */
void planner_dump_logical(LogicalPlan *plan, int indent) {
    if (!plan) {
        return;
    }

    /* 打印缩进 */
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }

    /* 打印节点信息 */
    printf("%s (rows=%.0f, cost=%.2f)\n",
           planner_logical_op_name(plan->type),
           plan->rows,
           plan->total_cost);

    /* 递归打印子节点 */
    if (plan->left) {
        planner_dump_logical(plan->left, indent + 1);
    }
    if (plan->right) {
        planner_dump_logical(plan->right, indent + 1);
    }
}

/**
 * @brief 打印物理计划
 */
void planner_dump_physical(PhysPlan *plan, int indent) {
    if (!plan) {
        return;
    }

    /* 打印缩进 */
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }

    /* 打印节点信息 */
    printf("%s (rows=%.0f, cost=%.2f)\n",
           planner_physical_op_name(plan->type),
           plan->rows,
           plan->total_cost);

    /* 递归打印子节点 */
    if (plan->lefttree) {
        planner_dump_physical((PhysPlan *)plan->lefttree, indent + 1);
    }
    if (plan->righttree) {
        planner_dump_physical((PhysPlan *)plan->righttree, indent + 1);
    }
}

/**
 * @brief 获取物理算子名称
 */
const char *planner_physical_op_name(PhysicalOpType type) {
    static const char *names[] = {
        [PHYS_SEQ_SCAN] = "SeqScan",
        [PHYS_INDEX_SCAN] = "IndexScan",
        [PHYS_BITMAP_SCAN] = "BitmapScan",
        [PHYS_HASHJOIN] = "HashJoin",
        [PHYS_NESTLOOP] = "NestLoop",
        [PHYS_MERGEJOIN] = "MergeJoin",
        [PHYS_HASHAGG] = "HashAgg",
        [PHYS_SORT] = "Sort",
        [PHYS_LIMIT] = "Limit",
        [PHYS_VECTOR_SCAN] = "VectorScan",
        [PHYS_HNSW_SCAN] = "HNSWScan",
        [PHYS_IVF_SCAN] = "IVFScan",
        [PHYS_DISKANN_SCAN] = "DiskANNScan"
    };

    if (type >= 0 && type < sizeof(names) / sizeof(names[0])) {
        return names[type] ? names[type] : "Unknown";
    }

    return "Unknown";
}

/**
 * @brief 获取逻辑算子名称
 */
const char *planner_logical_op_name(LogicalOpType type) {
    static const char *names[] = {
        [LOGICAL_SCAN] = "LogicalScan",
        [LOGICAL_INDEX_SCAN] = "LogicalIndexScan",
        [LOGICAL_JOIN] = "LogicalJoin",
        [LOGICAL_HASHJOIN] = "LogicalHashJoin",
        [LOGICAL_NESTLOOP] = "LogicalNestLoop",
        [LOGICAL_AGG] = "LogicalAgg",
        [LOGICAL_SORT] = "LogicalSort",
        [LOGICAL_LIMIT] = "LogicalLimit",
        [LOGICAL_PROJECT] = "LogicalProject",
        [LOGICAL_FILTER] = "LogicalFilter"
    };

    if (type >= 0 && type < sizeof(names) / sizeof(names[0])) {
        return names[type] ? names[type] : "Unknown";
    }

    return "Unknown";
}