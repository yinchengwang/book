/**
 * @file sql_driver.c
 * @brief SQL 执行驱动实现 - 端到端 SQL 执行入口
 *
 * T7（SQL 栈全收敛）：复活于 canonical Bison AST（sql_parse, T6）。
 *
 * 实现 execute_sql() 入口函数，整合：
 * 1. sql_parse        - canonical SQL 文本解析（parsenodes AST）
 * 2. DDL 直通道       - CREATE/DROP TABLE：语义检查 + catalog + 存储对象
 * 3. planner 链       - SELECT/DML：逻辑计划 → 优化 → 物理计划
 * 4. QueryResult      - 结果/错误返回
 */

#include "db/sql/sql_driver.h"
#include "db/parser/sql/sql_parse.h"   /* sql_parse()：canonical Bison AST（T6） */
#include "db/sql/sql_planner.h"        /* planner 链（SELECT/DML 路径） */
#include "db/catalog.h"                /* catalog_create_table / lookup / drop */
#include "db/rel.h"                    /* relation_open/close（DDL 存储对象） */

/*
 * T7 阵营说明（重要）：
 * 本文件只引用"旧执行器阵营"（sql_planner.h / sql_executor.h 的
 * struct PlanState_s 世界）与 canonical 解析器头，绝不同时引入
 * "新 Volcano 阵营"（executor.h / execnodes.h 的 struct PlanState 世界）。
 * 两阵营的 PlanState/TupleTableSlot typedef 同名不同型，且
 * PLANSTATE_DEFINED guard 被两头共享（execnodes.h 守护完整前向声明集，
 * sql_planner.h 只守护 PlanState 一个），同一 TU 混引必然爆炸
 * （T1 spike §2.4 的 43 个错误）。guard 改名/ tag 统一不可行：
 * expr.c 需要 PlanState=struct PlanState（execnodes.h 先引入），
 * planner.c/sql_executor.c 需要 PlanState=struct PlanState_s
 * （planner.c 直接解引用 state->exec_proc 等旧框架字段）。两阵营合并
 * 属 T8+ 统一执行器工作。
 *
 * 路由设计（SQL 栈全收敛计划的既定设计）：
 *   - DDL（CREATE/DROP TABLE）：直通道——解析 → 语义检查 →
 *     catalog_create_table/drop + 存储对象创建，不经 planner；
 *   - SELECT/DML：planner 链（logical → optimize → physical）。
 *     计划之后的结果物化/执行（CreateQueryDesc/ExecutorStart 新框架运行时）
 *     依赖阵营合并，T7 以真实错误明示该边界，不做静默桩。
 */

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
 * DDL 直通道（不经 planner）
 * ======================================================================== */

/**
 * @brief 将 TypeName 映射为 catalog 类型 OID
 *
 * 类型名取自 gram.y 的 makeTypeName() 调用点（int4/int8/int2/float4/
 * float8/varchar/char/text/bool/date/time/timestamp）。
 *
 * @return PG 类型 OID；未知类型返回 InvalidOid 并填充 errbuf
 */
static Oid driver_type_oid(const TypeName *tn, char *errbuf, size_t errlen) {
    const char *name = NULL;

    /* makeTypeName：names 为单元素 String 列表（见 makefuncs.c） */
    if (tn != NULL && tn->names != NULL && tn->names->head != NULL) {
        String *s = (String *)lfirst(tn->names->head);
        if (s != NULL) {
            name = s->str;
        }
    }
    if (name == NULL) {
        snprintf(errbuf, errlen, "列类型缺失");
        return InvalidOid;
    }

    if (strcmp(name, "int2") == 0)      return 21;
    if (strcmp(name, "int4") == 0)      return 23;
    if (strcmp(name, "int8") == 0)      return 20;
    if (strcmp(name, "float4") == 0)    return 700;
    if (strcmp(name, "float8") == 0)    return 701;
    if (strcmp(name, "bool") == 0)      return 16;
    if (strcmp(name, "text") == 0)      return 25;
    if (strcmp(name, "varchar") == 0)   return 1043;
    if (strcmp(name, "char") == 0)      return 1042;
    if (strcmp(name, "date") == 0)      return 1082;
    if (strcmp(name, "time") == 0)      return 1083;
    if (strcmp(name, "timestamp") == 0) return 1114;

    snprintf(errbuf, errlen, "不支持的列类型 '%s'", name);
    return InvalidOid;
}

/**
 * @brief CREATE TABLE 直通道：语义检查 + catalog + 存储对象
 */
static QueryResult *execute_create_table(QueryResult *result, CreateStmt *stmt) {
    char errbuf[128];

    /* ---- 语义检查 ---- */
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "CREATE TABLE 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;

    int ncols = list_length(stmt->tableElts);
    if (ncols <= 0) {
        SetQueryResultError(result, "CREATE TABLE %s: 至少需要一个列定义", tname);
        return result;
    }

    /* 表是否已存在 */
    if (catalog_lookup_table(tname) != InvalidOid) {
        if (stmt->if_not_exists) {
            return result;  /* IF NOT EXISTS：幂等成功 */
        }
        SetQueryResultError(result, "表 '%s' 已存在", tname);
        return result;
    }

    /* ---- 列定义转换（含逐列语义检查） ---- */
    column_def_t *cols = (column_def_t *)calloc((size_t)ncols, sizeof(column_def_t));
    if (cols == NULL) {
        SetQueryResultError(result, "内存分配失败");
        return result;
    }

    int i = 0;
    ListCell *lc;
    foreach (lc, stmt->tableElts) {
        Node *elt = (Node *)lfirst(lc);
        if (elt == NULL || elt->type != T_ColumnDef) {
            SetQueryResultError(result,
                "CREATE TABLE %s: 表级约束暂不支持（仅支持列定义）", tname);
            free(cols);
            return result;
        }
        ColumnDef *cd = (ColumnDef *)elt;
        if (cd->colname == NULL || cd->typeName == NULL) {
            SetQueryResultError(result, "CREATE TABLE %s: 第 %d 列定义不完整", tname, i + 1);
            free(cols);
            return result;
        }

        /* 重名列检查 */
        for (int j = 0; j < i; j++) {
            if (strcmp(cols[j].name, cd->colname) == 0) {
                SetQueryResultError(result, "CREATE TABLE %s: 列名 '%s' 重复",
                                    tname, cd->colname);
                free(cols);
                return result;
            }
        }

        errbuf[0] = '\0';
        Oid type_oid = driver_type_oid(cd->typeName, errbuf, sizeof(errbuf));
        if (type_oid == InvalidOid) {
            SetQueryResultError(result, "CREATE TABLE %s: 列 '%s': %s",
                                tname, cd->colname, errbuf);
            free(cols);
            return result;
        }

        snprintf(cols[i].name, NAMEDATALEN, "%s", cd->colname);
        cols[i].type_oid = type_oid;
        cols[i].typmod = cd->typeName->typemod;
        cols[i].not_null = cd->is_not_null;
        cols[i].has_default = (cd->default_expr != NULL);
        i++;
    }

    /* ---- catalog 建表 ---- */
    Oid table_oid = catalog_create_table(tname, cols, ncols);
    free(cols);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "CREATE TABLE %s: catalog_create_table 失败", tname);
        return result;
    }

    /* ---- 存储对象创建（与 sqlExecutor.c 的既定路径一致） ---- */
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (rel != NULL) {
        relation_close(rel, 0);
    }

    result->nrows = 0;
    return result;
}

/**
 * @brief DROP TABLE 直通道
 *
 * 注：gram.y 目前只为 DROP TABLE 生成 DropStmt，且将 removeType 置 1
 * （与 parsenodes.h 中 OBJECT_TABLE=0 的枚举值不一致，属语法文件既有
 * 问题，不在 T7 修复范围）。此处按"单表名 String 列表"处理。
 */
static QueryResult *execute_drop_table(QueryResult *result, DropStmt *stmt) {
    if (stmt->objects == NULL || stmt->objects->head == NULL) {
        SetQueryResultError(result, "DROP TABLE 缺少表名");
        return result;
    }
    String *s = (String *)lfirst(stmt->objects->head);
    if (s == NULL || s->str == NULL) {
        SetQueryResultError(result, "DROP TABLE 缺少表名");
        return result;
    }
    const char *tname = s->str;

    Oid table_oid = catalog_lookup_table(tname);
    if (table_oid == InvalidOid) {
        if (stmt->missing_ok) {
            return result;  /* IF EXISTS：幂等成功 */
        }
        SetQueryResultError(result, "表 '%s' 不存在", tname);
        return result;
    }

    if (catalog_drop_table(table_oid) != CATALOG_SUCCESS) {
        SetQueryResultError(result, "DROP TABLE %s: catalog_drop_table 失败", tname);
        return result;
    }

    result->nrows = 0;
    return result;
}

/* ========================================================================
 * SELECT / DML：planner 链路径
 * ======================================================================== */

/**
 * @brief SELECT/DML 经 planner 链处理
 *
 * 完成 解析 → 逻辑计划 → 优化 → 物理计划。计划之后的结果物化与执行
 * （新 Volcano 框架运行时）依赖执行器阵营合并，属 T8 范围；此处释放
 * 计划资源后经 error_msg 明示该边界（非静默桩）。
 */
static QueryResult *execute_via_planner(QueryResult *result, Node *ast) {
    PlannerContext *planner_ctx = planner_create();
    if (planner_ctx == NULL) {
        SetQueryResultError(result, "计划器创建失败");
        return result;
    }

    LogicalPlan *logical_plan = planner_logical_plan(planner_ctx, ast);
    if (logical_plan == NULL) {
        SetQueryResultError(result, "逻辑计划生成失败");
        planner_destroy(planner_ctx);
        return result;
    }

    planner_optimize(planner_ctx, logical_plan);

    PhysPlan *phys_plan = planner_physical_plan(planner_ctx, logical_plan);
    if (phys_plan == NULL) {
        SetQueryResultError(result, "物理计划生成失败");
        planner_free_logical_plan(logical_plan);
        planner_destroy(planner_ctx);
        return result;
    }

    /* T8 边界：执行/结果物化待执行器阵营合并后接通 */
    planner_free_physical_plan(phys_plan);
    planner_free_logical_plan(logical_plan);
    planner_destroy(planner_ctx);

    SetQueryResultError(result,
        "语句已解析并完成计划，执行与结果物化待 T8 接通 (NodeTag=%d)",
        (int)ast->type);
    return result;
}

/* ========================================================================
 * 核心执行 API
 * ======================================================================== */

/**
 * @brief 执行 SQL 语句
 *
 * SQL 执行流程（T7 版本）：
 * 1. sql_parse() 解析为 canonical parsenodes AST（静态存储，无需释放）
 * 2. 按 NodeTag 路由：
 *    - T_CreateStmt / T_DropStmt：DDL 直通道（catalog + 存储对象）
 *    - T_SelectStmt / T_InsertStmt / T_UpdateStmt / T_DeleteStmt：planner 链
 * 3. 结果经 QueryResult 返回；任何失败路径都填充 error_msg
 */
QueryResult *execute_sql(const char *sql, void *db) {
    QueryResult *result = CreateQueryResult();
    if (result == NULL) {
        return NULL;
    }

    (void)db;  /* 句柄预留给会话/事务接入（T8+） */

    if (sql == NULL) {
        SetQueryResultError(result, "SQL 语句为空");
        return result;
    }

    /* Step 1: 解析 SQL（canonical Bison 解析器，T6） */
    Node *ast = sql_parse(sql);
    if (ast == NULL) {
        SetQueryResultError(result, "SQL 解析失败: %s", sql_parse_last_error());
        return result;
    }

    /* Step 2: 按语句类型路由 */
    switch (ast->type) {
    case T_CreateStmt:
        return execute_create_table(result, (CreateStmt *)ast);
    case T_DropStmt:
        return execute_drop_table(result, (DropStmt *)ast);
    case T_SelectStmt:
    case T_InsertStmt:
    case T_UpdateStmt:
    case T_DeleteStmt:
        return execute_via_planner(result, ast);
    default:
        SetQueryResultError(result, "不支持的语句类型 (NodeTag=%d)", (int)ast->type);
        return result;
    }
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