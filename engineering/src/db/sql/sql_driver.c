/**
 * @file sql_driver.c
 * @brief SQL 执行驱动实现 - 端到端 SQL 执行入口
 *
 * T7（SQL 栈全收敛）：复活于 canonical Bison AST（sql_parse, T6）。
 * T8：SELECT 链路接通——新阵营执行器四阶段链 + 结果物化；
 *     INSERT/UPDATE/DELETE 经 DML 直通道真实落存储并上报受影响行数。
 *
 * 实现 execute_sql() 入口函数，整合：
 * 1. sql_parse        - canonical SQL 文本解析（parsenodes AST）
 * 2. DDL 直通道       - CREATE/DROP TABLE：语义检查 + catalog + 存储对象
 * 3. planner 链       - SELECT：逻辑计划 → 优化 → 物理计划（形状校验）
 * 4. 执行器四阶段链   - SELECT：CreateQueryDesc → ExecutorStart →
 *                       拉取循环（驱动充当 DestReceiver）→
 *                       ExecutorFinish → ExecutorEnd
 * 5. DML 直通道       - INSERT/UPDATE/DELETE：行编码 + heapam 写路径
 * 6. QueryResult      - 结果/错误返回
 */

#include "db/sql/sql_driver.h"
#include "db/parser/sql/sql_parse.h"   /* sql_parse()：canonical Bison AST（T6） */
#include "db/catalog.h"                /* catalog_create_table / lookup / drop */
#include "db/rel.h"                    /* relation_open/close；必须先于 executor 头 */
#include "db/heapam.h"                 /* heap_insert/delete/update（DML 直通道） */
#include "db/sql/nodeSeqscan.h"        /* 新阵营 SeqScan + 行编解码（-> execnodes.h） */
#include "db/sql/executor.h"           /* CreateQueryDesc/ExecutorStart/... 四阶段链 */
#include "db/sql/sql_planner.h"        /* planner 链（SELECT 计划形状校验） */
#include "db/sql/memctx.h"             /* AllocSetContextCreate / MemoryContextSwitchTo */

/*
 * T8 阵营说明（更新）：
 * T7 时本文件只能引用旧执行器阵营（sql_planner.h 的 PlanState_s 世界）；
 * T8 完成 SeqScan 阵营统一（nodeSeqscan.c 切换到 execnodes.h/executor.h
 * 世界）后，本文件改为引用"新 Volcano 阵营"，rel.h 先于 executor 头引入
 * （execnodes.h 检测到 DB_REL_H 后让 rel.h 的 TupleDescData 真身生效）。
 * sql_planner.h 末尾 PLANSTATE_DEFINED 守护的旧框架 PlanState typedef
 * 因 guard 已被 execnodes.h 定义而跳过；其声明的 planner_create_plan_state
 * 返回类型因此在本 TU 解析为新框架 PlanState*——本文件不调用该函数
 * （其定义在 planner.c，属旧阵营），仅为编译期声明差异，无运行时影响。
 *
 * 路由设计：
 *   - DDL（CREATE/DROP TABLE）：直通道——解析 → 语义检查 →
 *     catalog_create_table/drop + 存储对象创建，不经 planner；
 *   - SELECT：planner 链（logical → optimize → physical，校验物理计划为
 *     单节点 PHYS_SEQ_SCAN）→ 新阵营执行器四阶段链（CreateQueryDesc →
 *     ExecutorStart → 拉取循环 → ExecutorFinish → ExecutorEnd）→
 *     结果物化进 QueryResult。
 *     注：ExecutorRun 的 DestReceiver 仍是 void* 占位（receiveSlot 回调
 *     类型未定义），无法承担物化；驱动以与 ExecutorRun 相同的
 *     ExecProcNode 拉取循环充当 DestReceiver 角色，其余阶段不变。
 *   - DML（INSERT/UPDATE/DELETE）：直通道——解析 → 语义检查 →
 *     类型化行编码（sql_row_encode）→ heap_insert/delete/update。
 *     ModifyTable 执行器节点存在与 T8 前 SeqScan 同类的跨 TU 签名
 *     分歧（nodeModifyTable.c，旧阵营），接入属后续任务；直通道
 *     与 DDL 既有先例一致，受影响行数经 result->nrows 如实上报。
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
 * SELECT：planner 链 + 新阵营执行器四阶段链 + 结果物化
 * ======================================================================== */

/* SELECT 物化支持的列类型（其余类型以 "<type %u>" 占位显式输出） */
#define DRIVER_OID_INT2    21
#define DRIVER_OID_INT4    23
#define DRIVER_OID_INT8    20
#define DRIVER_OID_TEXT    25
#define DRIVER_OID_CHAR    1042
#define DRIVER_OID_VARCHAR 1043

static bool driver_oid_is_text(Oid t) {
    return t == DRIVER_OID_TEXT || t == DRIVER_OID_CHAR || t == DRIVER_OID_VARCHAR;
}

/**
 * @brief Datum → 字符串（malloc 分配，调用方负责 free）
 *
 * NULL 输出 "NULL"；未接入物化的类型输出 "<type %u>" 占位（显式可见）。
 */
static char *driver_format_datum(Oid coltype, Datum v, bool isnull) {
    if (isnull) {
        return strdup("NULL");
    }
    char buf[96];
    switch (coltype) {
    case DRIVER_OID_INT2:
    case DRIVER_OID_INT4:
        snprintf(buf, sizeof(buf), "%d", (int)(int64_t)v);
        break;
    case DRIVER_OID_INT8:
        snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)v);
        break;
    default:
        if (driver_oid_is_text(coltype)) {
            const char *s = (const char *)(uintptr_t)v;
            return strdup(s != NULL ? s : "");
        }
        snprintf(buf, sizeof(buf), "<type %u>", (unsigned)coltype);
        break;
    }
    return strdup(buf);
}

/**
 * @brief 在 TupleDesc 中按列名找 attno
 * @return 0-based attno；未找到返回 -1
 */
static int driver_find_attno(const TupleDesc td, const char *colname) {
    if (td == NULL || colname == NULL) {
        return -1;
    }
    for (int i = 0; i < td->natts; i++) {
        if (strcmp(td->attrs[i].attname, colname) == 0) {
            return i;
        }
    }
    return -1;
}

/* qsort 比较：按 attnum 升序（即建表声明顺序） */
static int driver_cmp_column_attnum(const void *a, const void *b) {
    return (int)((const column_info_t *)a)->attnum -
           (int)((const column_info_t *)b)->attnum;
}

/**
 * @brief 取表的声明顺序列数组（按 attnum 排序）
 *
 * catalog_get_columns 经桶链头插法返回，实际顺序为声明逆序（既有
 * 行为，修复属 catalog 层后续工作）；而 SQL 语义需要声明顺序——
 * SELECT * 的展开顺序、INSERT 无列名列表时的位置映射均以 attnum
 * 为准。本函数排序后返回，使驱动层不依赖 catalog 的返回顺序。
 *
 * @return catalog 列数组（catalog_free_columns 释放）；失败返回 NULL
 */
static column_info_t *driver_decl_columns(Oid table_oid, int *out_n) {
    int n = 0;
    column_info_t *cols = catalog_get_columns(table_oid, &n);
    if (cols == NULL || n <= 0) {
        if (cols != NULL) {
            catalog_free_columns(cols);
        }
        return NULL;
    }
    qsort(cols, (size_t)n, sizeof(column_info_t), driver_cmp_column_attnum);
    *out_n = n;
    return cols;
}

/**
 * @brief 创建驱动私有执行上下文并切换为 Current
 */
static MemoryContext driver_exec_ctx_begin(MemoryContext *old_out) {
    MemoryContext mcxt = AllocSetContextCreate(
        NULL, "SqlDriverExec", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    if (mcxt == NULL) {
        return NULL;
    }
    *old_out = MemoryContextSwitchTo(mcxt);
    return mcxt;
}

static void driver_exec_ctx_end(MemoryContext mcxt, MemoryContext old) {
    MemoryContextSwitchTo(old);
    delete_memory(mcxt);
}

/**
 * @brief 校验 SELECT 语法子集（单表全行投影）
 *
 * 凡超出子集的特性一律显式报错，不静默忽略。
 */
static bool select_validate_subset(QueryResult *result, const SelectStmt *stmt) {
    if (stmt->larg != NULL || stmt->rarg != NULL) {
        SetQueryResultError(result, "SELECT: 集合操作（UNION/INTERSECT/EXCEPT）暂不支持");
        return false;
    }
    if (stmt->withClause != NULL) {
        SetQueryResultError(result, "SELECT: WITH 子句暂不支持");
        return false;
    }
    if (stmt->intoClause != NULL) {
        SetQueryResultError(result, "SELECT INTO 暂不支持");
        return false;
    }
    if (stmt->distinctClause) {
        SetQueryResultError(result, "SELECT: DISTINCT 暂不支持");
        return false;
    }
    if (stmt->whereClause != NULL) {
        SetQueryResultError(result, "SELECT: WHERE 暂不支持（执行器过滤未接入）");
        return false;
    }
    if (stmt->groupClause != NULL || stmt->havingClause != NULL) {
        SetQueryResultError(result, "SELECT: GROUP BY/HAVING 暂不支持");
        return false;
    }
    if (stmt->windowClause != NULL) {
        SetQueryResultError(result, "SELECT: WINDOW 子句暂不支持");
        return false;
    }
    if (stmt->sortClause != NULL) {
        SetQueryResultError(result, "SELECT: ORDER BY 暂不支持");
        return false;
    }
    if (stmt->limitCount != NULL || stmt->limitOffset != NULL) {
        SetQueryResultError(result, "SELECT: LIMIT/OFFSET 暂不支持");
        return false;
    }
    if (stmt->fromClause == NULL || list_length(stmt->fromClause) != 1) {
        SetQueryResultError(result, "SELECT: 仅支持单表 FROM（JOIN/多表暂不支持）");
        return false;
    }
    Node *from = (Node *)lfirst(stmt->fromClause->head);
    if (from == NULL || from->type != T_RangeVar ||
        ((RangeVar *)from)->relname == NULL) {
        SetQueryResultError(result, "SELECT: FROM 仅支持普通表名");
        return false;
    }
    if (stmt->targetList == NULL || list_length(stmt->targetList) == 0) {
        SetQueryResultError(result, "SELECT: 目标列列表为空");
        return false;
    }
    return true;
}

/**
 * @brief 从 targetList 构建投影（输出列 → 表 attno 映射）
 *
 * 仅接受 ColumnRef（单字段列名或 "*"）；FuncCall 等显式报错。
 * "*" 按声明顺序（decl_cols，attnum 升序）展开为全部列，
 * 且不允许与其他目标混用。
 *
 * @return 投影数组（calloc，调用方 free）；失败返回 NULL 并填 error_msg
 */
static int *select_build_projection(QueryResult *result, const SelectStmt *stmt,
                                    const TupleDesc td,
                                    const column_info_t *decl_cols, int ndecl,
                                    int *out_ncols) {
    int ntargets = list_length(stmt->targetList);

    /* "*" 检查 */
    ListCell *lc;
    foreach (lc, stmt->targetList) {
        Node *t = (Node *)lfirst(lc);
        if (t == NULL || t->type != T_ColumnRef) {
            SetQueryResultError(result,
                "SELECT: 目标列仅支持列名或 *（表达式/函数暂不支持）");
            return NULL;
        }
        ColumnRef *cref = (ColumnRef *)t;
        if (cref->fields == NULL || list_length(cref->fields) != 1) {
            SetQueryResultError(result, "SELECT: 表名.列名 限定写法暂不支持");
            return NULL;
        }
        Node *f = (Node *)lfirst(cref->fields->head);
        if (f == NULL || f->type != T_String) {
            SetQueryResultError(result, "SELECT: 非法的列引用");
            return NULL;
        }
        if (strcmp(((String *)f)->str, "*") == 0 && ntargets > 1) {
            SetQueryResultError(result, "SELECT: * 不能与其他目标列混用");
            return NULL;
        }
    }

    /* "*"：按声明顺序展开为全部列 */
    Node *t0 = (Node *)lfirst(stmt->targetList->head);
    ColumnRef *cref0 = (ColumnRef *)t0;
    String *f0 = (String *)lfirst(cref0->fields->head);
    if (strcmp(f0->str, "*") == 0) {
        if (decl_cols == NULL || ndecl != td->natts) {
            SetQueryResultError(result,
                "SELECT: catalog 列数与行描述符不一致");
            return NULL;
        }
        int *proj = (int *)calloc((size_t)ndecl, sizeof(int));
        if (proj == NULL) {
            SetQueryResultError(result, "内存分配失败");
            return NULL;
        }
        for (int i = 0; i < ndecl; i++) {
            int attno = driver_find_attno(td, decl_cols[i].name);
            if (attno < 0) {
                SetQueryResultError(result,
                    "SELECT: catalog 列 '%s' 不在行描述符中",
                    decl_cols[i].name);
                free(proj);
                return NULL;
            }
            proj[i] = attno;
        }
        *out_ncols = ndecl;
        return proj;
    }

    /* 显式列名列表 */
    int *proj = (int *)calloc((size_t)ntargets, sizeof(int));
    if (proj == NULL) {
        SetQueryResultError(result, "内存分配失败");
        return NULL;
    }
    int i = 0;
    foreach (lc, stmt->targetList) {
        ColumnRef *cref = (ColumnRef *)lfirst(lc);
        String *f = (String *)lfirst(cref->fields->head);
        int attno = driver_find_attno(td, f->str);
        if (attno < 0) {
            SetQueryResultError(result, "SELECT: 列 '%s' 不存在", f->str);
            free(proj);
            return NULL;
        }
        for (int j = 0; j < i; j++) {
            if (proj[j] == attno) {
                SetQueryResultError(result, "SELECT: 列 '%s' 重复", f->str);
                free(proj);
                return NULL;
            }
        }
        proj[i++] = attno;
    }
    *out_ncols = ntargets;
    return proj;
}

/**
 * @brief SELECT：planner 链 → 执行器四阶段链 → 物化进 QueryResult
 */
static QueryResult *execute_select(QueryResult *result, SelectStmt *stmt) {
    /* ---- 1. 语法子集校验 ---- */
    if (!select_validate_subset(result, stmt)) {
        return result;
    }
    RangeVar *rv = (RangeVar *)lfirst(stmt->fromClause->head);
    const char *tname = rv->relname;

    Oid table_oid = catalog_lookup_table(tname);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "表 '%s' 不存在", tname);
        return result;
    }

    /* ---- 2. planner 链（真实运行，校验物理计划形状） ---- */
    PlannerContext *planner_ctx = planner_create();
    if (planner_ctx == NULL) {
        SetQueryResultError(result, "计划器创建失败");
        return result;
    }
    LogicalPlan *logical_plan = planner_logical_plan(planner_ctx, stmt);
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
    if (phys_plan->type != PHYS_SEQ_SCAN ||
        phys_plan->lefttree != NULL || phys_plan->righttree != NULL) {
        SetQueryResultError(result,
            "SELECT: 物理计划超出单节点 SeqScan 范围（算子 %s）",
            planner_physical_op_name(phys_plan->type));
        planner_free_physical_plan(phys_plan);
        planner_free_logical_plan(logical_plan);
        planner_destroy(planner_ctx);
        return result;
    }
    planner_free_physical_plan(phys_plan);
    planner_free_logical_plan(logical_plan);
    planner_destroy(planner_ctx);

    /* ---- 3. 执行器链：CreateQueryDesc → ExecutorStart ---- */
    MemoryContext old_ctx = NULL;
    MemoryContext mcxt = driver_exec_ctx_begin(&old_ctx);
    if (mcxt == NULL) {
        SetQueryResultError(result, "执行内存上下文创建失败");
        return result;
    }

    /* 物理计划（新阵营 SeqScan 节点）：在驱动上下文中分配，
     * 生命周期覆盖整个执行过程 */
    SeqScan *scan = (SeqScan *)palloc0(mcxt, sizeof(SeqScan));
    if (scan == NULL) {
        driver_exec_ctx_end(mcxt, old_ctx);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    scan->plan.type = T_SeqScan;
    scan->scanrelid = table_oid;

    QueryDesc *qd = CreateQueryDesc((Plan *)scan, NULL);
    if (qd == NULL) {
        driver_exec_ctx_end(mcxt, old_ctx);
        SetQueryResultError(result, "CreateQueryDesc 失败");
        return result;
    }

    ExecutorStart(qd, 0);
    if (qd->estate == NULL || qd->planstate == NULL) {
        ExecutorEnd(qd);
        driver_exec_ctx_end(mcxt, old_ctx);
        SetQueryResultError(result, "ExecutorStart 失败（表 '%s' 无法打开）", tname);
        return result;
    }

    /* ---- 4. 投影（列映射 + 列名），取自扫描槽的真实描述符 ---- */
    SeqScanState *ss = (SeqScanState *)qd->planstate;
    TupleDesc td = ss->ss_ScanTupleSlot->tts_tupleDescriptor;
    int ndecl = 0;
    column_info_t *decl_cols = driver_decl_columns(table_oid, &ndecl);
    int ncols = 0;
    int *proj = select_build_projection(result, stmt, td,
                                        decl_cols, ndecl, &ncols);
    if (decl_cols != NULL) {
        catalog_free_columns(decl_cols);
    }
    if (proj == NULL) {
        ExecutorFinish(qd);
        ExecutorEnd(qd);
        driver_exec_ctx_end(mcxt, old_ctx);
        return result;  /* error_msg 已填 */
    }

    result->ncols = ncols;
    result->col_names = (char **)calloc((size_t)ncols, sizeof(char *));
    if (result->col_names == NULL) {
        free(proj);
        ExecutorFinish(qd);
        ExecutorEnd(qd);
        driver_exec_ctx_end(mcxt, old_ctx);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    for (int j = 0; j < ncols; j++) {
        result->col_names[j] = strdup(td->attrs[proj[j]].attname);
    }

    /* ---- 5. 拉取循环（驱动充当 DestReceiver）+ 逐行物化 ---- */
    TupleTableSlot *slot;
    while ((slot = ExecProcNode(qd->planstate)) != NULL) {
        char ***new_rows = (char ***)realloc(
            result->rows, ((size_t)result->nrows + 1) * sizeof(char *));
        if (new_rows == NULL) {
            SetQueryResultError(result, "内存分配失败");
            break;
        }
        result->rows = new_rows;
        char **row = (char **)calloc((size_t)ncols, sizeof(char *));
        if (row == NULL) {
            SetQueryResultError(result, "内存分配失败");
            break;
        }
        result->rows[result->nrows] = row;
        result->nrows++;
        for (int j = 0; j < ncols; j++) {
            int attno = proj[j];
            row[j] = driver_format_datum(td->attrs[attno].atttypid,
                                         slot->tts_values[attno],
                                         slot->tts_isnull[attno]);
        }
    }

    qd->estate->es_processed = (uint64_t)result->nrows;

    /* ---- 6. ExecutorFinish → ExecutorEnd ---- */
    free(proj);
    ExecutorFinish(qd);
    ExecutorEnd(qd);
    driver_exec_ctx_end(mcxt, old_ctx);
    return result;
}

/* ========================================================================
 * DML 直通道：INSERT / UPDATE / DELETE
 *
 * 与 DDL 直通道同先例：解析 → 语义检查 → catalog/存储层真实写入。
 * 行格式为 nodeSeqscan.h 的自描述编码，与 SeqScan 解码严格对应。
 * ======================================================================== */

/**
 * @brief A_Const → 按列类型的 Datum 转换
 *
 * @return 0 成功；-1 失败（errbuf 已填）
 */
static int driver_aconst_to_datum(const A_Const *c, Oid coltype,
                                  const char *colname,
                                  Datum *out, bool *isnull,
                                  char *errbuf, size_t errlen) {
    if (c->isnull || c->val.type == T_Null) {
        *out = (Datum)0;
        *isnull = true;
        return 0;
    }
    *isnull = false;
    switch (coltype) {
    case DRIVER_OID_INT2:
    case DRIVER_OID_INT4:
    case DRIVER_OID_INT8:
        if (c->val.type != T_Integer) {
            snprintf(errbuf, errlen, "列 '%s' 需要整数常量", colname);
            return -1;
        }
        *out = (Datum)(uint64_t)(int64_t)c->val.val.ival;
        return 0;
    default:
        if (driver_oid_is_text(coltype)) {
            if (c->val.type != T_String || c->val.val.str == NULL) {
                snprintf(errbuf, errlen, "列 '%s' 需要字符串常量", colname);
                return -1;
            }
            *out = (Datum)(uintptr_t)c->val.val.str;
            return 0;
        }
        snprintf(errbuf, errlen,
                 "列 '%s' 的类型 OID %u 暂不支持 DML 写入", colname,
                 (unsigned)coltype);
        return -1;
    }
}

/**
 * @brief 从当前扫描位置重建 6 字节 TID（blocknum u32 + LP 字节偏移 u16）
 *
 * heap_getnext 返回前已自增 rs_cindex，故当前元组 lp_index = rs_cindex-1；
 * 字节布局与 heap_delete/heap_update 的解析（native memcpy）严格对应。
 */
static void driver_scan_current_tid(const TableScanDesc scan, uint8_t tid[6]) {
    uint32_t blk = (uint32_t)scan->rs_cblock;
    uint16_t off = (uint16_t)(SizeOfPageHeaderData +
                              (scan->rs_cindex - 1) * SizeOfHeapLinePointer);
    memcpy(tid, &blk, sizeof(blk));
    memcpy(tid + sizeof(blk), &off, sizeof(off));
}

/**
 * @brief INSERT：VALUES 常量行 → 类型化编码 → heap_insert
 */
static QueryResult *execute_insert(QueryResult *result, InsertStmt *stmt) {
    char errbuf[160];

    /* ---- 语义检查 ---- */
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "INSERT 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;
    if (stmt->selectStmt != NULL) {
        SetQueryResultError(result, "INSERT ... SELECT 暂不支持（仅支持 VALUES）");
        return result;
    }
    if (stmt->returningList != NULL || stmt->withClause != NULL) {
        SetQueryResultError(result, "INSERT: RETURNING/WITH 暂不支持");
        return result;
    }
    if (stmt->valuesLists == NULL || list_length(stmt->valuesLists) == 0) {
        SetQueryResultError(result, "INSERT: VALUES 列表为空");
        return result;
    }

    Oid table_oid = catalog_lookup_table(tname);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "表 '%s' 不存在", tname);
        return result;
    }

    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (rel == NULL) {
        SetQueryResultError(result, "INSERT %s: relation_open 失败", tname);
        return result;
    }
    TupleDesc td = relation_getdesc(rel);
    int natts = (td != NULL) ? td->natts : 0;
    if (td == NULL || natts <= 0) {
        relation_close(rel, 0);
        SetQueryResultError(result, "INSERT %s: 表缺少行描述符", tname);
        return result;
    }

    /* ---- 列映射 ----
     * 无列名列表：VALUES 位置按 SQL 语义对应声明顺序（attnum 升序），
     * 再经列名映射到行描述符 attno（描述符列序不保证等于声明顺序）。
     * 有列名列表：直接按列名映射；未列出列写 NULL。 */
    int nvalue_cols = natts;
    int *col_attno = (int *)calloc((size_t)natts, sizeof(int));
    if (col_attno == NULL) {
        relation_close(rel, 0);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    for (int i = 0; i < natts; i++) {
        col_attno[i] = i;
    }
    if (stmt->cols == NULL) {
        int ndecl = 0;
        column_info_t *decl_cols = driver_decl_columns(table_oid, &ndecl);
        if (decl_cols == NULL || ndecl != natts) {
            if (decl_cols != NULL) {
                catalog_free_columns(decl_cols);
            }
            free(col_attno);
            relation_close(rel, 0);
            SetQueryResultError(result,
                "INSERT %s: catalog 列数与行描述符不一致", tname);
            return result;
        }
        for (int i = 0; i < ndecl; i++) {
            int attno = driver_find_attno(td, decl_cols[i].name);
            if (attno < 0) {
                catalog_free_columns(decl_cols);
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result,
                    "INSERT %s: catalog 列 '%s' 不在行描述符中",
                    tname, decl_cols[i].name);
                return result;
            }
            col_attno[i] = attno;
        }
        catalog_free_columns(decl_cols);
    } else {
        nvalue_cols = list_length(stmt->cols);
        if (nvalue_cols <= 0 || nvalue_cols > natts) {
            free(col_attno);
            relation_close(rel, 0);
            SetQueryResultError(result, "INSERT %s: 列名列表长度非法", tname);
            return result;
        }
        int i = 0;
        ListCell *lc;
        foreach (lc, stmt->cols) {
            Node *n = (Node *)lfirst(lc);
            if (n == NULL || n->type != T_ColumnRef) {
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result, "INSERT %s: 非法的列名", tname);
                return result;
            }
            ColumnRef *cref = (ColumnRef *)n;
            if (cref->fields == NULL || list_length(cref->fields) != 1) {
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result, "INSERT %s: 表名.列名 限定写法暂不支持", tname);
                return result;
            }
            String *f = (String *)lfirst(cref->fields->head);
            int attno = driver_find_attno(td, f->str);
            if (attno < 0) {
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result, "INSERT %s: 列 '%s' 不存在", tname, f->str);
                return result;
            }
            for (int j = 0; j < i; j++) {
                if (col_attno[j] == attno) {
                    free(col_attno);
                    relation_close(rel, 0);
                    SetQueryResultError(result, "INSERT %s: 列 '%s' 重复", tname, f->str);
                    return result;
                }
            }
            col_attno[i++] = attno;
        }
    }

    /* 列类型数组（按 attnum 顺序） */
    Oid *coltypes = (Oid *)calloc((size_t)natts, sizeof(Oid));
    Datum *values = (Datum *)calloc((size_t)natts, sizeof(Datum));
    bool *isnull = (bool *)calloc((size_t)natts, sizeof(bool));
    if (coltypes == NULL || values == NULL || isnull == NULL) {
        free(col_attno); free(coltypes); free(values); free(isnull);
        relation_close(rel, 0);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    for (int i = 0; i < natts; i++) {
        coltypes[i] = td->attrs[i].atttypid;
    }

    /* ---- 逐行编码 + heap_insert ---- */
    int inserted = 0;
    ListCell *rowlc;
    foreach (rowlc, stmt->valuesLists) {
        List *exprs = (List *)lfirst(rowlc);
        if (exprs == NULL || list_length(exprs) != nvalue_cols) {
            SetQueryResultError(result,
                "INSERT %s: VALUES 列数 %d 与目标列数 %d 不匹配",
                tname, exprs ? list_length(exprs) : 0, nvalue_cols);
            goto insert_done;
        }
        for (int i = 0; i < natts; i++) {
            values[i] = (Datum)0;
            isnull[i] = true;  /* 未列出列默认 NULL */
        }
        int i = 0;
        ListCell *vlc;
        foreach (vlc, exprs) {
            Node *n = (Node *)lfirst(vlc);
            if (n == NULL || n->type != T_A_Const) {
                SetQueryResultError(result,
                    "INSERT %s: VALUES 仅支持常量（表达式暂不支持）", tname);
                goto insert_done;
            }
            int attno = col_attno[i++];
            errbuf[0] = '\0';
            if (driver_aconst_to_datum((const A_Const *)n, coltypes[attno],
                                       td->attrs[attno].attname,
                                       &values[attno], &isnull[attno],
                                       errbuf, sizeof(errbuf)) != 0) {
                SetQueryResultError(result, "INSERT %s: %s", tname, errbuf);
                goto insert_done;
            }
        }
        size_t need = sql_row_encoded_size(natts, coltypes, values, isnull);
        if (need == 0) {
            SetQueryResultError(result,
                "INSERT %s: 存在不支持编码的列类型", tname);
            goto insert_done;
        }
        void *buf = malloc(need);
        if (buf == NULL) {
            SetQueryResultError(result, "内存分配失败");
            goto insert_done;
        }
        size_t out_len = 0;
        int enc_rc = sql_row_encode(natts, coltypes, values, isnull,
                                    buf, need, &out_len);
        int ins_rc = (enc_rc == 0)
            ? heap_insert(rel, buf, out_len, 0, 0, NULL, NULL)
            : -1;
        free(buf);
        if (enc_rc != 0 || ins_rc != 0) {
            SetQueryResultError(result, "INSERT %s: heap_insert 失败", tname);
            goto insert_done;
        }
        inserted++;
    }

insert_done:
    free(col_attno);
    free(coltypes);
    free(values);
    free(isnull);
    relation_close(rel, 0);
    if (result->error_msg == NULL) {
        result->nrows = inserted;  /* 受影响行数 */
    }
    return result;
}

/**
 * @brief UPDATE：两阶段（扫描收集 → 应用 SET → heap_update）
 *
 * heap_update 实现为 delete+append，扫描进行中写入会扰乱游标，
 * 故先收集全部旧行 TID 与新行编码，结束扫描后再统一应用。
 */
static QueryResult *execute_update(QueryResult *result, UpdateStmt *stmt) {
    char errbuf[160];

    /* ---- 语义检查 ---- */
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "UPDATE 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;
    if (stmt->whereClause != NULL) {
        SetQueryResultError(result, "UPDATE: WHERE 暂不支持（执行器过滤未接入）");
        return result;
    }
    if (stmt->fromClause != NULL || stmt->returningList != NULL ||
        stmt->withClause != NULL) {
        SetQueryResultError(result, "UPDATE: FROM/RETURNING/WITH 暂不支持");
        return result;
    }
    if (stmt->targetList == NULL || list_length(stmt->targetList) == 0) {
        SetQueryResultError(result, "UPDATE: SET 子句为空");
        return result;
    }

    Oid table_oid = catalog_lookup_table(tname);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "表 '%s' 不存在", tname);
        return result;
    }

    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (rel == NULL) {
        SetQueryResultError(result, "UPDATE %s: relation_open 失败", tname);
        return result;
    }
    TupleDesc td = relation_getdesc(rel);
    int natts = (td != NULL) ? td->natts : 0;
    if (td == NULL || natts <= 0) {
        relation_close(rel, 0);
        SetQueryResultError(result, "UPDATE %s: 表缺少行描述符", tname);
        return result;
    }

    /* ---- SET 子句 → 按 attno 的新值数组 ---- */
    Oid *coltypes = (Oid *)calloc((size_t)natts, sizeof(Oid));
    Datum *set_val = (Datum *)calloc((size_t)natts, sizeof(Datum));
    bool *set_isnull = (bool *)calloc((size_t)natts, sizeof(bool));
    bool *set_flag = (bool *)calloc((size_t)natts, sizeof(bool));
    if (coltypes == NULL || set_val == NULL || set_isnull == NULL || set_flag == NULL) {
        free(coltypes); free(set_val); free(set_isnull); free(set_flag);
        relation_close(rel, 0);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    for (int i = 0; i < natts; i++) {
        coltypes[i] = td->attrs[i].atttypid;
    }
    {
        ListCell *lc;
        foreach (lc, stmt->targetList) {
            Node *n = (Node *)lfirst(lc);
            if (n == NULL || n->type != T_ResTarget) {
                SetQueryResultError(result, "UPDATE %s: 非法的 SET 项", tname);
                goto update_cleanup;
            }
            ResTarget *rt = (ResTarget *)n;
            if (rt->name == NULL || rt->val == NULL ||
                rt->val->type != T_A_Const) {
                SetQueryResultError(result,
                    "UPDATE %s: SET 仅支持 列=常量（表达式暂不支持）", tname);
                goto update_cleanup;
            }
            int attno = driver_find_attno(td, rt->name);
            if (attno < 0) {
                SetQueryResultError(result, "UPDATE %s: 列 '%s' 不存在",
                                    tname, rt->name);
                goto update_cleanup;
            }
            if (set_flag[attno]) {
                SetQueryResultError(result, "UPDATE %s: 列 '%s' 重复赋值",
                                    tname, rt->name);
                goto update_cleanup;
            }
            errbuf[0] = '\0';
            if (driver_aconst_to_datum((const A_Const *)rt->val, coltypes[attno],
                                       rt->name, &set_val[attno],
                                       &set_isnull[attno],
                                       errbuf, sizeof(errbuf)) != 0) {
                SetQueryResultError(result, "UPDATE %s: %s", tname, errbuf);
                goto update_cleanup;
            }
            set_flag[attno] = true;
        }
    }

    /* ---- 两阶段执行（驱动私有上下文承载解码出的 text 字符串） ---- */
    {
        MemoryContext old_ctx = NULL;
        MemoryContext mcxt = driver_exec_ctx_begin(&old_ctx);
        if (mcxt == NULL) {
            SetQueryResultError(result, "执行内存上下文创建失败");
            goto update_cleanup;
        }

        Datum *values = (Datum *)calloc((size_t)natts, sizeof(Datum));
        bool *isnull = (bool *)calloc((size_t)natts, sizeof(bool));
        /* 待应用更新：动态数组 {tid[6], buf, len} */
        typedef struct PendingUpdate { uint8_t tid[6]; void *buf; size_t len; } PendingUpdate;
        PendingUpdate *pending = NULL;
        int npending = 0;
        if (values == NULL || isnull == NULL) {
            free(values); free(isnull);
            driver_exec_ctx_end(mcxt, old_ctx);
            SetQueryResultError(result, "内存分配失败");
            goto update_cleanup;
        }

        /* 阶段 1：扫描收集 */
        TableScanDesc scan = table_beginscan(rel, 0, NULL);
        if (scan == NULL) {
            free(values); free(isnull);
            driver_exec_ctx_end(mcxt, old_ctx);
            SetQueryResultError(result, "UPDATE %s: table_beginscan 失败", tname);
            goto update_cleanup;
        }
        void *blob;
        while ((blob = table_getnext(scan)) != NULL) {
            if (sql_row_decode(natts, coltypes, blob, values, isnull, mcxt) != 0) {
                SetQueryResultError(result,
                    "UPDATE %s: 行解码失败（类型不支持或数据损坏）", tname);
                break;
            }
            for (int i = 0; i < natts; i++) {
                if (set_flag[i]) {
                    values[i] = set_val[i];
                    isnull[i] = set_isnull[i];
                }
            }
            size_t need = sql_row_encoded_size(natts, coltypes, values, isnull);
            if (need == 0) {
                SetQueryResultError(result,
                    "UPDATE %s: 存在不支持编码的列类型", tname);
                break;
            }
            PendingUpdate *np = (PendingUpdate *)realloc(
                pending, ((size_t)npending + 1) * sizeof(PendingUpdate));
            if (np == NULL) {
                SetQueryResultError(result, "内存分配失败");
                break;
            }
            pending = np;
            pending[npending].buf = malloc(need);
            if (pending[npending].buf == NULL) {
                SetQueryResultError(result, "内存分配失败");
                break;
            }
            size_t out_len = 0;
            if (sql_row_encode(natts, coltypes, values, isnull,
                               pending[npending].buf, need, &out_len) != 0) {
                free(pending[npending].buf);
                SetQueryResultError(result,
                    "UPDATE %s: 行编码失败", tname);
                break;
            }
            pending[npending].len = out_len;
            driver_scan_current_tid(scan, pending[npending].tid);
            npending++;
        }
        table_endscan(scan);
        free(values);
        free(isnull);
        driver_exec_ctx_end(mcxt, old_ctx);

        /* 阶段 2：应用更新（仅当收集无错） */
        int updated = 0;
        if (result->error_msg == NULL) {
            for (int i = 0; i < npending; i++) {
                if (heap_update(rel, pending[i].tid, pending[i].buf,
                                pending[i].len, 0, 0, NULL, 0) != 0) {
                    SetQueryResultError(result,
                        "UPDATE %s: heap_update 失败（已应用 %d 行）",
                        tname, updated);
                    break;
                }
                updated++;
            }
            if (result->error_msg == NULL) {
                result->nrows = updated;  /* 受影响行数 */
            }
        }
        for (int i = 0; i < npending; i++) {
            free(pending[i].buf);
        }
        free(pending);
    }

update_cleanup:
    free(coltypes);
    free(set_val);
    free(set_isnull);
    free(set_flag);
    relation_close(rel, 0);
    return result;
}

/**
 * @brief DELETE：两阶段（扫描收集 TID → heap_delete）
 */
static QueryResult *execute_delete(QueryResult *result, DeleteStmt *stmt) {
    /* ---- 语义检查 ---- */
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "DELETE 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;
    if (stmt->whereClause != NULL) {
        SetQueryResultError(result, "DELETE: WHERE 暂不支持（执行器过滤未接入）");
        return result;
    }
    if (stmt->usingClause != NULL || stmt->returningList != NULL ||
        stmt->withClause != NULL) {
        SetQueryResultError(result, "DELETE: USING/RETURNING/WITH 暂不支持");
        return result;
    }

    Oid table_oid = catalog_lookup_table(tname);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "表 '%s' 不存在", tname);
        return result;
    }

    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (rel == NULL) {
        SetQueryResultError(result, "DELETE %s: relation_open 失败", tname);
        return result;
    }

    /* 阶段 1：收集全部 TID（扫描中删除会扰乱游标，与 UPDATE 同理） */
    typedef struct PendingTid { uint8_t tid[6]; } PendingTid;
    PendingTid *pending = NULL;
    int npending = 0;
    TableScanDesc scan = table_beginscan(rel, 0, NULL);
    if (scan == NULL) {
        relation_close(rel, 0);
        SetQueryResultError(result, "DELETE %s: table_beginscan 失败", tname);
        return result;
    }
    while (table_getnext(scan) != NULL) {
        PendingTid *np = (PendingTid *)realloc(
            pending, ((size_t)npending + 1) * sizeof(PendingTid));
        if (np == NULL) {
            free(pending);
            table_endscan(scan);
            relation_close(rel, 0);
            SetQueryResultError(result, "内存分配失败");
            return result;
        }
        pending = np;
        driver_scan_current_tid(scan, pending[npending].tid);
        npending++;
    }
    table_endscan(scan);

    /* 阶段 2：删除 */
    int deleted = 0;
    for (int i = 0; i < npending; i++) {
        if (heap_delete(rel, pending[i].tid, 0, false, false) != 0) {
            SetQueryResultError(result,
                "DELETE %s: heap_delete 失败（已删除 %d 行）", tname, deleted);
            break;
        }
        deleted++;
    }
    free(pending);
    relation_close(rel, 0);
    if (result->error_msg == NULL) {
        result->nrows = deleted;  /* 受影响行数 */
    }
    return result;
}

/* ========================================================================
 * 核心执行 API
 * ======================================================================== */

/**
 * @brief 执行 SQL 语句
 *
 * SQL 执行流程（T8 版本）：
 * 1. sql_parse() 解析为 canonical parsenodes AST（静态存储，无需释放）
 * 2. 按 NodeTag 路由：
 *    - T_CreateStmt / T_DropStmt：DDL 直通道（catalog + 存储对象）
 *    - T_SelectStmt：planner 链 + 执行器四阶段链 + 结果物化
 *    - T_InsertStmt / T_UpdateStmt / T_DeleteStmt：DML 直通道（heapam）
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
        return execute_select(result, (SelectStmt *)ast);
    case T_InsertStmt:
        return execute_insert(result, (InsertStmt *)ast);
    case T_UpdateStmt:
        return execute_update(result, (UpdateStmt *)ast);
    case T_DeleteStmt:
        return execute_delete(result, (DeleteStmt *)ast);
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
