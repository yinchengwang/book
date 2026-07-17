/**
 * @file sql_driver.c
 * @brief SQL 执行驱动实现 - 端到端 SQL 执行入口
 *
 * Task 5.2: SQL 解析器端到端集成
 *
 * 实现 execute_sql() 入口函数，整合：
 * 1. sql_parser - SQL 文本解析
 * 2. planner - 查询计划生成
 * 3. executor - 计划执行
 * 4. QueryResult - 结果收集
 */

#include "db/sql/sql_driver.h"
#include "db/parser/sql/sql.h"
#include "db/sql/sql_planner.h"
#include "db/sql/executor.h"
#include "db/sql/sql_executor.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ========================================================================
 * QueryResult 管理
 * ======================================================================== */

/**
 * @brief 创建空的 QueryResult
 */
QueryResult *CreateQueryResult(void) {
    QueryResult *result = (QueryResult *)calloc(1, sizeof(QueryResult));
    if (result == NULL) {
        return NULL;
    }

    result->nrows = 0;
    result->ncols = 0;
    result->col_names = NULL;
    result->rows = NULL;
    result->error_msg = NULL;

    return result;
}

/**
 * @brief 设置 QueryResult 的错误信息
 */
void SetQueryResultError(QueryResult *result, const char *fmt, ...) {
    if (result == NULL || fmt == NULL) {
        return;
    }

    /* 格式化错误信息 */
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    /* 分配并复制错误信息 */
    result->error_msg = strdup(buffer);
}

/**
 * @brief 释放查询结果
 */
void FreeQueryResult(QueryResult *result) {
    if (result == NULL) {
        return;
    }

    /* 释放列名 */
    if (result->col_names != NULL) {
        for (int i = 0; i < result->ncols; i++) {
            if (result->col_names[i] != NULL) {
                free(result->col_names[i]);
            }
        }
        free(result->col_names);
    }

    /* 释放行数据 */
    if (result->rows != NULL) {
        for (int i = 0; i < result->nrows; i++) {
            if (result->rows[i] != NULL) {
                for (int j = 0; j < result->ncols; j++) {
                    if (result->rows[i][j] != NULL) {
                        free(result->rows[i][j]);
                    }
                }
                free(result->rows[i]);
            }
        }
        free(result->rows);
    }

    /* 释放错误信息 */
    if (result->error_msg != NULL) {
        free(result->error_msg);
    }

    /* 释放结构本身 */
    free(result);
}

/**
 * @brief 打印 QueryResult（调试用）
 */
void PrintQueryResult(const QueryResult *result) {
    if (result == NULL) {
        printf("QueryResult: (null)\n");
        return;
    }

    if (result->error_msg != NULL) {
        printf("Error: %s\n", result->error_msg);
        return;
    }

    printf("QueryResult: %d rows, %d cols\n", result->nrows, result->ncols);

    /* 打印列名 */
    if (result->col_names != NULL) {
        printf("Columns: ");
        for (int i = 0; i < result->ncols; i++) {
            printf("%s%s", result->col_names[i], (i < result->ncols - 1) ? ", " : "");
        }
        printf("\n");
    }

    /* 打印数据行 */
    for (int i = 0; i < result->nrows; i++) {
        printf("Row %d: ", i);
        if (result->rows != NULL && result->rows[i] != NULL) {
            for (int j = 0; j < result->ncols; j++) {
                printf("%s%s",
                       result->rows[i][j] ? result->rows[i][j] : "NULL",
                       (j < result->ncols - 1) ? ", " : "");
            }
        }
        printf("\n");
    }
}

/* ========================================================================
 * 核心执行 API
 * ======================================================================== */

/**
 * @brief 执行 SQL 语句
 *
 * 完整的 SQL 执行流程：
 * 1. 解析 SQL 文本生成 AST
 * 2. 生成查询计划
 * 3. 初始化执行器
 * 4. 执行查询并收集结果
 * 5. 返回 QueryResult
 */
QueryResult *execute_sql(const char *sql, void *db) {
    QueryResult *result = CreateQueryResult();
    if (result == NULL) {
        return NULL;
    }

    if (sql == NULL) {
        SetQueryResultError(result, "SQL 语句为空");
        return result;
    }

    /* Step 1: 解析 SQL */
    sql_node_t *parse_result = sql_parse_one(sql);
    if (parse_result == NULL) {
        SetQueryResultError(result, "SQL 解析失败");
        return result;
    }

    /* Step 2: 创建计划器上下文 */
    PlannerContext *planner_ctx = planner_create();
    if (planner_ctx == NULL) {
        SetQueryResultError(result, "计划器创建失败");
        sql_node_free(parse_result);
        return result;
    }

    /* Step 3: 生成逻辑计划 */
    LogicalPlan *logical_plan = planner_logical_plan(planner_ctx, parse_result);
    if (logical_plan == NULL) {
        SetQueryResultError(result, "逻辑计划生成失败");
        planner_destroy(planner_ctx);
        sql_node_free(parse_result);
        return result;
    }

    /* Step 4: 优化逻辑计划 */
    planner_optimize(planner_ctx, logical_plan);

    /* Step 5: 生成物理计划 */
    PhysPlan *phys_plan = planner_physical_plan(planner_ctx, logical_plan);
    if (phys_plan == NULL) {
        SetQueryResultError(result, "物理计划生成失败");
        planner_free_logical_plan(logical_plan);
        planner_destroy(planner_ctx);
        sql_node_free(parse_result);
        return result;
    }

    /* Step 6: 创建 PlanState（Volcano 框架） */
    PlanState *plan_state = planner_create_plan_state(phys_plan);
    if (plan_state == NULL) {
        SetQueryResultError(result, "执行器状态创建失败");
        planner_free_physical_plan(phys_plan);
        planner_free_logical_plan(logical_plan);
        planner_destroy(planner_ctx);
        sql_node_free(parse_result);
        return result;
    }

    /* Step 7: 创建 QueryDesc 并执行 */
    QueryDesc *query_desc = CreateQueryDesc((Plan *)phys_plan, plan_state);
    if (query_desc == NULL) {
        SetQueryResultError(result, "查询描述符创建失败");
        planner_destroy_plan_state(plan_state);
        planner_free_physical_plan(phys_plan);
        planner_free_logical_plan(logical_plan);
        planner_destroy(planner_ctx);
        sql_node_free(parse_result);
        return result;
    }

    /* Step 8: 执行器启动 */
    ExecutorStart(query_desc, 0);

    /* Step 9: 执行查询 */
    ExecutorRun(query_desc, ForwardScanDirection, 0);

    /* Step 10: 收集结果（简化版本） */
    /* 从 query_desc->estate->es_processed 获取处理的元组数 */
    if (query_desc->estate != NULL) {
        result->nrows = (int)query_desc->estate->es_processed;
    }

    /* Step 11: 执行器结束 */
    ExecutorFinish(query_desc);
    ExecutorEnd(query_desc);

    /* Step 12: 清理资源 */
    FreeQueryDesc(query_desc);
    /* 注意：plan_state 已由 ExecutorEnd 释放 */
    planner_free_physical_plan(phys_plan);
    planner_free_logical_plan(logical_plan);
    planner_destroy(planner_ctx);
    sql_node_free(parse_result);

    return result;
}

/**
 * @brief 执行 DDL 语句
 */
int execute_ddl(const char *sql, void *db) {
    if (sql == NULL) {
        return -1;
    }

    QueryResult *result = execute_sql(sql, db);
    if (result == NULL) {
        return -1;
    }

    int ret = 0;
    if (result->error_msg != NULL) {
        ret = -1;
    }

    FreeQueryResult(result);
    return ret;
}

/**
 * @brief 执行 DML 语句（不返回结果）
 */
int execute_dml(const char *sql, void *db) {
    if (sql == NULL) {
        return -1;
    }

    QueryResult *result = execute_sql(sql, db);
    if (result == NULL) {
        return -1;
    }

    int rows_affected = -1;
    if (result->error_msg == NULL) {
        rows_affected = result->nrows;
    }

    FreeQueryResult(result);
    return rows_affected;
}