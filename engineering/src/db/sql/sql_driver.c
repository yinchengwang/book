/**
 * @file sql_driver.c
 * @brief SQL 执行驱动实现 - 端到端 SQL 执行入口
 *
 * T7（SQL 栈全收敛）：复活于 canonical Bison AST（sql_parse, T6）。
 * T8：SELECT 链路接通——新阵营执行器四阶段链 + 结果物化；
 *     INSERT/UPDATE/DELETE 经 DML 直通道真实落存储并上报受影响行数。
 * T9：主路径算子落地——ORDER BY/LIMIT、GROUP BY+聚合、INNER JOIN、
 *     FROM 子查询；驱动直接构造执行器 Plan 树，planner 仅作形状校验。
 *
 * 实现 execute_sql() 入口函数，整合：
 * 1. sql_parse        - canonical SQL 文本解析（parsenodes AST）
 * 2. DDL 直通道       - CREATE/DROP TABLE：语义检查 + catalog + 存储对象
 * 3. SELECT 直构造    - 从 AST 递归构造 Plan 树 → 执行器四阶段链 → 物化
 * 4. DML 直通道       - INSERT/UPDATE/DELETE：行编码 + heapam 写路径
 * 5. QueryResult      - 结果/错误返回
 */

#include "db/sql/sql_driver.h"
#include "db/parser/sql/sql_parse.h"
#include "db/catalog.h"
#include "db/rel.h"
#include "db/heapam.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodeSort.h"
#include "db/sql/nodeLimit.h"
#include "db/sql/nodeAgg.h"
#include "db/sql/nodeHashjoin.h"
#include "db/sql/executor.h"
#include "db/sql/sql_planner.h"
#include "db/sql/memctx.h"
#include "db/parser/sql/makefuncs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>

/* ========================================================================
 * QueryResult 管理
 * ======================================================================== */

QueryResult *CreateQueryResult(void) {
    QueryResult *result = (QueryResult *)calloc(1, sizeof(QueryResult));
    if (result == NULL) return NULL;
    result->nrows = 0;
    result->ncols = 0;
    result->col_names = NULL;
    result->rows = NULL;
    result->error_msg = NULL;
    return result;
}

void SetQueryResultError(QueryResult *result, const char *fmt, ...) {
    if (result == NULL || fmt == NULL) return;
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    result->error_msg = strdup(buffer);
}

void FreeQueryResult(QueryResult *result) {
    if (result == NULL) return;
    if (result->col_names != NULL) {
        for (int i = 0; i < result->ncols; i++) {
            if (result->col_names[i] != NULL) free(result->col_names[i]);
        }
        free(result->col_names);
    }
    if (result->rows != NULL) {
        for (int i = 0; i < result->nrows; i++) {
            if (result->rows[i] != NULL) {
                for (int j = 0; j < result->ncols; j++) {
                    if (result->rows[i][j] != NULL) free(result->rows[i][j]);
                }
                free(result->rows[i]);
            }
        }
        free(result->rows);
    }
    if (result->error_msg != NULL) free(result->error_msg);
    free(result);
}

void PrintQueryResult(const QueryResult *result) {
    if (result == NULL) { printf("QueryResult: (null)\n"); return; }
    if (result->error_msg != NULL) { printf("Error: %s\n", result->error_msg); return; }
    printf("QueryResult: %d rows, %d cols\n", result->nrows, result->ncols);
    if (result->col_names != NULL) {
        printf("Columns: ");
        for (int i = 0; i < result->ncols; i++) {
            printf("%s%s", result->col_names[i], (i < result->ncols - 1) ? ", " : "");
        }
        printf("\n");
    }
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
 * DDL 直通道
 * ======================================================================== */

static Oid driver_type_oid(const TypeName *tn, char *errbuf, size_t errlen) {
    const char *name = NULL;
    if (tn != NULL && tn->names != NULL && tn->names->head != NULL) {
        String *s = (String *)lfirst(tn->names->head);
        if (s != NULL) name = s->str;
    }
    if (name == NULL) { snprintf(errbuf, errlen, "列类型缺失"); return InvalidOid; }
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

static QueryResult *execute_create_table(QueryResult *result, CreateStmt *stmt) {
    char errbuf[128];
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
    if (catalog_lookup_table(tname) != InvalidOid) {
        if (stmt->if_not_exists) return result;
        SetQueryResultError(result, "表 '%s' 已存在", tname);
        return result;
    }
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
            SetQueryResultError(result, "CREATE TABLE %s: 仅支持列定义", tname);
            free(cols);
            return result;
        }
        ColumnDef *cd = (ColumnDef *)elt;
        if (cd->colname == NULL || cd->typeName == NULL) {
            SetQueryResultError(result, "CREATE TABLE %s: 第 %d 列定义不完整", tname, i + 1);
            free(cols);
            return result;
        }
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
    Oid table_oid = catalog_create_table(tname, cols, ncols);
    free(cols);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "CREATE TABLE %s: catalog_create_table 失败", tname);
        return result;
    }
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (rel != NULL) relation_close(rel, 0);
    result->nrows = 0;
    return result;
}

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
        if (stmt->missing_ok) return result;
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
 * SELECT 工具
 * ======================================================================== */

#define DRIVER_OID_INT2    21
#define DRIVER_OID_INT4    23
#define DRIVER_OID_INT8    20
#define DRIVER_OID_TEXT    25
#define DRIVER_OID_CHAR    1042
#define DRIVER_OID_VARCHAR 1043

static bool driver_oid_is_text(Oid t) {
    return t == DRIVER_OID_TEXT || t == DRIVER_OID_CHAR || t == DRIVER_OID_VARCHAR;
}

static char *driver_format_datum(Oid coltype, Datum v, bool isnull) {
    if (isnull) return strdup("NULL");
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

static int driver_cmp_column_attnum(const void *a, const void *b) {
    return (int)((const column_info_t *)a)->attnum -
           (int)((const column_info_t *)b)->attnum;
}

static column_info_t *driver_decl_columns(Oid table_oid, int *out_n) {
    int n = 0;
    column_info_t *cols = catalog_get_columns(table_oid, &n);
    if (cols == NULL || n <= 0) {
        if (cols != NULL) catalog_free_columns(cols);
        return NULL;
    }
    qsort(cols, (size_t)n, sizeof(column_info_t), driver_cmp_column_attnum);
    *out_n = n;
    return cols;
}

static MemoryContext driver_exec_ctx_begin(MemoryContext *old_out) {
    MemoryContext mcxt = AllocSetContextCreate(
        NULL, "SqlDriverExec", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    if (mcxt == NULL) return NULL;
    *old_out = MemoryContextSwitchTo(mcxt);
    return mcxt;
}

static void driver_exec_ctx_end(MemoryContext mcxt, MemoryContext old) {
    MemoryContextSwitchTo(old);
    delete_memory(mcxt);
}

/* mcxt 内字符串复制（pstrdup 替代） */
static char *driver_mcxt_strdup(MemoryContext mcxt, const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s);
    char *p = (char *)palloc(mcxt, len + 1);
    if (p == NULL) return NULL;
    memcpy(p, s, len + 1);
    return p;
}

/* ========================================================================
 * SELECT 计划构造
 * ======================================================================== */

typedef struct FromRef {
    char *alias;
    char *relname;       /**< 原表名（用于 j_a.col 形式：未声明别名时按表名匹配） */
    Oid   table_oid;
    int   base_attno;
} FromRef;

typedef struct PlanBuildResult {
    Plan    *root;
    char   **col_names;
    Oid     *col_types;
    int      natts;
    FromRef *from_refs;
    int      nfrom_refs;
    MemoryContext mcxt;
} PlanBuildResult;

static PlanBuildResult driver_build_select_plan(QueryResult *result,
                                                const SelectStmt *stmt);

static int driver_extract_colref(const ColumnRef *cref,
                                 const char **out_alias,
                                 const char **out_colname) {
    if (cref == NULL || cref->fields == NULL) return -1;
    int nf = list_length(cref->fields);
    if (nf == 1) {
        Node *f = (Node *)lfirst(cref->fields->head);
        if (f == NULL || f->type != T_String) return -1;
        *out_alias = NULL;
        *out_colname = ((String *)f)->str;
        return 0;
    } else if (nf == 2) {
        Node *fa = (Node *)lfirst(cref->fields->head);
        Node *fb = (Node *)lfirst(cref->fields->head->next);
        if (fa == NULL || fa->type != T_String ||
            fb == NULL || fb->type != T_String) return -1;
        *out_alias = ((String *)fa)->str;
        *out_colname = ((String *)fb)->str;
        return 0;
    }
    return -1;
}

/* ---------- SeqScan ---------- */

static PlanBuildResult driver_build_seqscan(QueryResult *result,
                                            MemoryContext mcxt,
                                            const char *tname,
                                            const char *alias) {
    PlanBuildResult pr = {0};
    pr.mcxt = mcxt;
    Oid table_oid = catalog_lookup_table(tname);
    if (table_oid == InvalidOid) {
        SetQueryResultError(result, "表 '%s' 不存在", tname);
        return pr;
    }
    int ndecl = 0;
    column_info_t *cols = driver_decl_columns(table_oid, &ndecl);
    if (cols == NULL) {
        SetQueryResultError(result, "表 '%s' catalog 列空", tname);
        return pr;
    }
    SeqScan *scan = (SeqScan *)palloc0(mcxt, sizeof(SeqScan));
    scan->plan.type = T_SeqScan;
    scan->scanrelid = table_oid;
    pr.root = (Plan *)scan;
    pr.natts = ndecl;
    pr.col_names = (char **)palloc(mcxt, sizeof(char *) * ndecl);
    pr.col_types = (Oid *)palloc(mcxt, sizeof(Oid) * ndecl);
    for (int i = 0; i < ndecl; i++) {
        pr.col_names[i] = driver_mcxt_strdup(mcxt, cols[i].name);
        pr.col_types[i] = cols[i].type_oid;
    }
    pr.nfrom_refs = 1;
    pr.from_refs = (FromRef *)palloc(mcxt, sizeof(FromRef));
    pr.from_refs[0].alias = (alias != NULL) ? driver_mcxt_strdup(mcxt, alias) : NULL;
    pr.from_refs[0].relname = driver_mcxt_strdup(mcxt, tname);
    pr.from_refs[0].table_oid = table_oid;
    pr.from_refs[0].base_attno = 0;
    catalog_free_columns(cols);
    return pr;
}

/* ---------- Sort ---------- */

static bool driver_build_sort(QueryResult *result, const List *sortClause,
                              PlanBuildResult child, PlanBuildResult *out) {
    if (sortClause == NULL || list_length(sortClause) == 0) {
        *out = child;
        return true;
    }
    int nkeys = list_length(sortClause);
    int *sortColIdx = (int *)palloc(child.mcxt, sizeof(int) * nkeys);
    bool *sortDesc = (bool *)palloc(child.mcxt, sizeof(bool) * nkeys);
    Oid *sortColTypes = (Oid *)palloc(child.mcxt, sizeof(Oid) * nkeys);
    int k = 0;
    ListCell *lc;
    foreach (lc, sortClause) {
        Node *n = (Node *)lfirst(lc);
        if (n == NULL || n->type != T_SortBy) {
            SetQueryResultError(result, "ORDER BY 仅支持 SortBy");
            return false;
        }
        SortBy *sb = (SortBy *)n;
        if (sb->node == NULL || sb->node->type != T_ColumnRef) {
            SetQueryResultError(result, "ORDER BY 仅支持列名");
            return false;
        }
        const char *alias = NULL;
        const char *colname = NULL;
        if (driver_extract_colref((ColumnRef *)sb->node, &alias, &colname) != 0) {
            SetQueryResultError(result, "ORDER BY 列引用非法");
            return false;
        }
        int attno = -1;
        for (int i = 0; i < child.natts; i++) {
            if (strcmp(child.col_names[i], colname) == 0) { attno = i; break; }
        }
        if (attno < 0) {
            SetQueryResultError(result, "ORDER BY 列 '%s' 不在 FROM 输出中", colname);
            return false;
        }
        (void)alias;
        sortColIdx[k] = attno;
        sortColTypes[k] = child.col_types[attno];
        sortDesc[k] = (sb->sortby_dir == SORTBY_DESC);
        k++;
    }
    Sort *sp = (Sort *)palloc0(child.mcxt, sizeof(Sort));
    sp->plan.type = T_Sort;
    sp->numCols = nkeys;
    sp->sortColIdx = sortColIdx;
    sp->sortDesc = sortDesc;
    sp->sortColTypes = sortColTypes;
    sp->plan.lefttree = child.root;
    sp->plan.righttree = NULL;
    *out = child;
    out->root = (Plan *)sp;
    return true;
}

/* ---------- Limit ---------- */

static bool driver_extract_int_const(Node *n, int64_t *out) {
    if (n == NULL || n->type != T_A_Const) return false;
    A_Const *c = (A_Const *)n;
    if (c->val.type != T_Integer) return false;
    *out = (int64_t)c->val.val.ival;
    return true;
}

static bool driver_build_limit(QueryResult *result, const Node *limitCount,
                               const Node *limitOffset,
                               PlanBuildResult child, PlanBuildResult *out) {
    if (limitCount == NULL && limitOffset == NULL) { *out = child; return true; }
    int64_t count = -1;
    int64_t offset = 0;
    if (limitCount != NULL) {
        if (!driver_extract_int_const((Node *)limitCount, &count)) {
            SetQueryResultError(result, "LIMIT 必须是整数常量");
            return false;
        }
        if (count < 0) {
            SetQueryResultError(result, "LIMIT 不支持负数");
            return false;
        }
    }
    if (limitOffset != NULL) {
        if (!driver_extract_int_const((Node *)limitOffset, &offset)) {
            SetQueryResultError(result, "OFFSET 必须是整数常量");
            return false;
        }
        if (offset < 0) {
            SetQueryResultError(result, "OFFSET 不支持负数");
            return false;
        }
    }
    Limit *lp = (Limit *)palloc0(child.mcxt, sizeof(Limit));
    lp->plan.type = T_Limit;
    lp->limitCount = (long)count;
    lp->limitOffset = (long)offset;
    lp->plan.lefttree = child.root;
    lp->plan.righttree = NULL;
    *out = child;
    out->root = (Plan *)lp;
    return true;
}

/* ---------- Agg ---------- */

static AggFuncKind driver_agg_func_kind(const char *name) {
    if (name == NULL) return AGG_FUNC_COUNT;
    if (strcasecmp(name, "count") == 0) return AGG_FUNC_COUNT;
    if (strcasecmp(name, "sum") == 0)   return AGG_FUNC_SUM;
    if (strcasecmp(name, "min") == 0)   return AGG_FUNC_MIN;
    if (strcasecmp(name, "max") == 0)   return AGG_FUNC_MAX;
    if (strcasecmp(name, "avg") == 0)   return AGG_FUNC_AVG;
    return AGG_FUNC_COUNT;
}

static bool driver_build_agg(QueryResult *result, const SelectStmt *stmt,
                             PlanBuildResult child, PlanBuildResult *out) {
    List *aggrefs = NULL;
    int nagg = 0;
    if (stmt->targetList != NULL) {
        ListCell *lc;
        foreach (lc, stmt->targetList) {
            Node *n = (Node *)lfirst(lc);
            if (n == NULL || n->type != T_ResTarget) continue;
            ResTarget *rt = (ResTarget *)n;
            if (rt->val == NULL || rt->val->type != T_FuncCall) continue;
            FuncCall *fc = (FuncCall *)rt->val;
            if (fc->funcname == NULL || list_length(fc->funcname) != 1) {
                SetQueryResultError(result, "聚合函数必须是无模式限定的简单名");
                return false;
            }
            Node *fn = (Node *)lfirst(fc->funcname->head);
            if (fn == NULL || fn->type != T_String) {
                SetQueryResultError(result, "聚合函数名非法");
                return false;
            }
            AggFuncKind kind = driver_agg_func_kind(((String *)fn)->str);
            if (kind == AGG_FUNC_AVG) {
                SetQueryResultError(result, "AVG 暂不支持（无浮点类型）");
                return false;
            }
            AggrefDef *ad = (AggrefDef *)palloc0(child.mcxt, sizeof(AggrefDef));
            ad->fn = kind;
            ad->arg_attno = -1;
            ad->arg_type = InvalidOid;
            ad->out_type = DRIVER_OID_INT8;
            if (rt->name != NULL) {
                ad->out_name = driver_mcxt_strdup(child.mcxt, rt->name);
            } else {
                char buf[32];
                snprintf(buf, sizeof(buf), "%s", ((String *)fn)->str);
                for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
                ad->out_name = driver_mcxt_strdup(child.mcxt, buf);
            }
            if (kind == AGG_FUNC_COUNT && fc->agg_star) {
                ad->arg_attno = -1;
                ad->arg_type = InvalidOid;
            } else {
                if (fc->args == NULL || list_length(fc->args) != 1) {
                    SetQueryResultError(result, "聚合函数仅支持单参数");
                    return false;
                }
                Node *arg = (Node *)lfirst(fc->args->head);
                if (arg == NULL || arg->type != T_ColumnRef) {
                    SetQueryResultError(result, "聚合函数参数必须是列名");
                    return false;
                }
                const char *alias = NULL;
                const char *colname = NULL;
                if (driver_extract_colref((ColumnRef *)arg, &alias, &colname) != 0) {
                    SetQueryResultError(result, "聚合函数参数列引用非法");
                    return false;
                }
                int attno = -1;
                for (int i = 0; i < child.natts; i++) {
                    if (strcmp(child.col_names[i], colname) == 0) { attno = i; break; }
                }
                if (attno < 0) {
                    SetQueryResultError(result, "聚合函数参数列 '%s' 不存在", colname);
                    return false;
                }
                ad->arg_attno = attno;
                ad->arg_type = child.col_types[attno];
                ad->out_type = (kind == AGG_FUNC_COUNT) ? DRIVER_OID_INT8
                                : ad->arg_type;
            }
            aggrefs = lappend(aggrefs, ad);
            nagg++;
        }
    }
    if (stmt->havingClause != NULL) {
        SetQueryResultError(result, "HAVING 暂不支持");
        return false;
    }
    int numGroupCols = 0;
    int *grpColIdx = NULL;
    Oid *grpColTypes = NULL;
    if (stmt->groupClause != NULL) {
        numGroupCols = list_length(stmt->groupClause);
        if (numGroupCols > 0) {
            grpColIdx = (int *)palloc(child.mcxt, sizeof(int) * numGroupCols);
            grpColTypes = (Oid *)palloc(child.mcxt, sizeof(Oid) * numGroupCols);
            int g = 0;
            ListCell *lc;
            foreach (lc, stmt->groupClause) {
                Node *n = (Node *)lfirst(lc);
                if (n == NULL || n->type != T_ColumnRef) {
                    SetQueryResultError(result, "GROUP BY 仅支持列名");
                    return false;
                }
                const char *alias = NULL;
                const char *colname = NULL;
                if (driver_extract_colref((ColumnRef *)n, &alias, &colname) != 0) {
                    SetQueryResultError(result, "GROUP BY 列引用非法");
                    return false;
                }
                int attno = -1;
                for (int i = 0; i < child.natts; i++) {
                    if (strcmp(child.col_names[i], colname) == 0) { attno = i; break; }
                }
                if (attno < 0) {
                    SetQueryResultError(result, "GROUP BY 列 '%s' 不在 FROM 输出中",
                                        colname);
                    return false;
                }
                grpColIdx[g] = attno;
                grpColTypes[g] = child.col_types[attno];
                g++;
            }
        }
    }
    AggStrategy strat = (numGroupCols > 0) ? AGG_HASHED : AGG_PLAIN;
    Agg *ap = (Agg *)palloc0(child.mcxt, sizeof(Agg));
    ap->plan.type = T_Agg;
    ap->aggstrategy = strat;
    ap->numCols = numGroupCols;
    ap->grpColIdx = grpColIdx;
    ap->grpColTypes = grpColTypes;
    ap->aggrefs = aggrefs;
    ap->out_natts = numGroupCols + nagg;
    ap->out_desc = NULL;
    ap->plan.lefttree = child.root;
    ap->plan.righttree = NULL;
    *out = child;
    out->root = (Plan *)ap;
    out->natts = numGroupCols + nagg;
    out->col_names = (char **)palloc(child.mcxt, sizeof(char *) * out->natts);
    out->col_types = (Oid *)palloc(child.mcxt, sizeof(Oid) * out->natts);
    for (int i = 0; i < numGroupCols; i++) {
        out->col_names[i] = driver_mcxt_strdup(child.mcxt, child.col_names[grpColIdx[i]]);
        out->col_types[i] = grpColTypes[i];
    }
    ListCell *lc;
    int idx = 0;
    foreach (lc, aggrefs) {
        AggrefDef *ad = (AggrefDef *)lfirst(lc);
        out->col_names[numGroupCols + idx] = driver_mcxt_strdup(child.mcxt, ad->out_name);
        out->col_types[numGroupCols + idx] = ad->out_type;
        idx++;
    }
    return true;
}

/* ---------- HashJoin ---------- */

static bool driver_build_hashjoin(QueryResult *result, const JoinExpr *je,
                                  PlanBuildResult *out_left,
                                  PlanBuildResult *out_right) {
    if (je->jointype != JOIN_INNER) {
        SetQueryResultError(result, "JOIN 仅支持 INNER");
        return false;
    }
    if (je->usingClause != NULL) {
        SetQueryResultError(result, "JOIN USING 暂不支持");
        return false;
    }
    if (je->quals == NULL || je->quals->type != T_A_Expr) {
        SetQueryResultError(result, "JOIN ON 必须是单等值");
        return false;
    }
    A_Expr *ax = (A_Expr *)je->quals;
    if (ax->kind != AEXPR_OP || ax->lexpr == NULL || ax->rexpr == NULL ||
        ax->lexpr->type != T_ColumnRef || ax->rexpr->type != T_ColumnRef) {
        SetQueryResultError(result, "JOIN ON 仅支持 col = col");
        return false;
    }
    if (ax->name == NULL || list_length(ax->name) != 1) {
        SetQueryResultError(result, "JOIN ON 操作符非法");
        return false;
    }
    Node *opnode = (Node *)lfirst(ax->name->head);
    if (opnode == NULL || opnode->type != T_String ||
        strcmp(((String *)opnode)->str, "=") != 0) {
        SetQueryResultError(result, "JOIN ON 仅支持 = 操作符");
        return false;
    }
    const char *l_alias = NULL, *l_col = NULL;
    const char *r_alias = NULL, *r_col = NULL;
    if (driver_extract_colref((ColumnRef *)ax->lexpr, &l_alias, &l_col) != 0 ||
        driver_extract_colref((ColumnRef *)ax->rexpr, &r_alias, &r_col) != 0) {
        SetQueryResultError(result, "JOIN ON 列引用非法");
        return false;
    }
    int l_attno = -1, r_attno = -1;
    for (int i = 0; i < out_left->natts; i++) {
        if (strcmp(out_left->col_names[i], l_col) == 0) { l_attno = i; break; }
    }
    for (int i = 0; i < out_right->natts; i++) {
        if (strcmp(out_right->col_names[i], r_col) == 0) { r_attno = i; break; }
    }
    if (l_attno < 0) {
        SetQueryResultError(result, "JOIN ON 左列 '%s' 不存在", l_col);
        return false;
    }
    if (r_attno < 0) {
        SetQueryResultError(result, "JOIN ON 右列 '%s' 不存在", r_col);
        return false;
    }
    HJClause *cl = (HJClause *)palloc(out_left->mcxt, sizeof(HJClause));
    cl->outer_attno = l_attno;
    cl->inner_attno = r_attno;
    cl->key_type = out_left->col_types[l_attno];
    HashJoin *hp = (HashJoin *)palloc0(out_left->mcxt, sizeof(HashJoin));
    hp->join.jointype = je->jointype;
    hp->join.joinqual = NULL;
    hp->join.plan.type = T_HashJoin;
    hp->join.plan.lefttree = out_left->root;
    hp->join.plan.righttree = out_right->root;
    hp->hashclauses = list_make1(cl);
    hp->hashoperators = NULL;
    hp->hashnullrecheck = false;
    hp->out_desc = NULL;
    hp->out_natts = out_left->natts + out_right->natts;
    hp->outer_natts = out_left->natts;
    hp->inner_natts = out_right->natts;
    PlanBuildResult orig = *out_left;
    out_left->root = (Plan *)hp;
    int new_natts = orig.natts + out_right->natts;
    char **new_names = (char **)palloc(orig.mcxt, sizeof(char *) * new_natts);
    Oid *new_types = (Oid *)palloc(orig.mcxt, sizeof(Oid) * new_natts);
    for (int i = 0; i < orig.natts; i++) {
        new_names[i] = driver_mcxt_strdup(orig.mcxt, orig.col_names[i]);
        new_types[i] = orig.col_types[i];
    }
    for (int i = 0; i < out_right->natts; i++) {
        new_names[orig.natts + i] = driver_mcxt_strdup(orig.mcxt, out_right->col_names[i]);
        new_types[orig.natts + i] = out_right->col_types[i];
    }
    out_left->col_names = new_names;
    out_left->col_types = new_types;
    out_left->natts = new_natts;
    int old_n = orig.nfrom_refs;
    int new_n = old_n + out_right->nfrom_refs;
    FromRef *merged = (FromRef *)palloc(orig.mcxt, sizeof(FromRef) * new_n);
    memcpy(merged, orig.from_refs, sizeof(FromRef) * old_n);
    for (int i = 0; i < out_right->nfrom_refs; i++) {
        merged[old_n + i] = out_right->from_refs[i];
        merged[old_n + i].base_attno = orig.natts + out_right->from_refs[i].base_attno;
    }
    out_left->from_refs = merged;
    out_left->nfrom_refs = new_n;
    return true;
}

/* ---------- 递归 FROM ---------- */

static PlanBuildResult driver_build_from(QueryResult *result,
                                         const List *fromClause,
                                         MemoryContext mcxt) {
    PlanBuildResult pr = {0};
    pr.mcxt = mcxt;
    if (fromClause == NULL || list_length(fromClause) == 0) {
        SetQueryResultError(result, "SELECT 必须有 FROM 子句");
        return pr;
    }
    if (list_length(fromClause) > 1) {
        SetQueryResultError(result, "SELECT FROM 仅支持单个范围");
        return pr;
    }
    Node *from = (Node *)lfirst(fromClause->head);
    if (from == NULL) {
        SetQueryResultError(result, "FROM 子项为空");
        return pr;
    }
    if (from->type == T_RangeVar) {
        RangeVar *rv = (RangeVar *)from;
        if (rv->relname == NULL) {
            SetQueryResultError(result, "FROM 表名缺失");
            return pr;
        }
        pr = driver_build_seqscan(result, mcxt, rv->relname, rv->alias);
        return pr;
    } else if (from->type == T_RangeSubselect) {
        RangeSubselect *rs = (RangeSubselect *)from;
        if (rs->subquery == NULL || rs->subquery->type != T_SelectStmt) {
            SetQueryResultError(result, "FROM 子查询必须是 SELECT");
            return pr;
        }
        pr = driver_build_select_plan(result, (SelectStmt *)rs->subquery);
        if (result->error_msg != NULL) return pr;
        pr.mcxt = mcxt;  /* 折叠子查询：复用外层 mcxt */
        /* 把子查询的 from_refs 合并进外层（折叠为同一 Slot 域），
         * 并把别名作为新增 from_ref 暴露，使 s.col 形式可解析 */
        if (rs->alias != NULL) {
            int old_n = pr.nfrom_refs;
            int new_n = old_n + 1;
            FromRef *merged = (FromRef *)palloc(mcxt, sizeof(FromRef) * new_n);
            if (old_n > 0 && pr.from_refs != NULL) {
                memcpy(merged, pr.from_refs, sizeof(FromRef) * old_n);
            }
            /* 折叠：子查询整体作为一项暴露给外层选择 */
            int base_attno = 0;
            for (int i = 0; i < old_n; i++) {
                int next = (i + 1 < old_n) ? merged[i + 1].base_attno : pr.natts;
                if (merged[i].base_attno >= base_attno) {
                    base_attno = merged[i].base_attno;
                }
                (void)next;
            }
            merged[old_n].alias = driver_mcxt_strdup(mcxt, rs->alias);
            merged[old_n].relname = driver_mcxt_strdup(mcxt, rs->alias);
            merged[old_n].table_oid = InvalidOid;
            merged[old_n].base_attno = 0;
            pr.from_refs = merged;
            pr.nfrom_refs = new_n;
        }
        return pr;
    } else if (from->type == T_JoinExpr) {
        JoinExpr *je = (JoinExpr *)from;
        if (je->larg == NULL || je->rarg == NULL) {
            SetQueryResultError(result, "JOIN 两侧缺失");
            return pr;
        }
        if (je->larg->type != T_RangeVar || je->rarg->type != T_RangeVar) {
            SetQueryResultError(result, "JOIN 仅支持表直接连接");
            return pr;
        }
        RangeVar *lr = (RangeVar *)je->larg;
        RangeVar *rr = (RangeVar *)je->rarg;
        PlanBuildResult l = driver_build_seqscan(result, mcxt, lr->relname, lr->alias);
        if (result->error_msg != NULL) return pr;
        PlanBuildResult r = driver_build_seqscan(result, mcxt, rr->relname, rr->alias);
        if (result->error_msg != NULL) return pr;
        if (!driver_build_hashjoin(result, je, &l, &r)) return pr;
        return l;
    }
    SetQueryResultError(result, "FROM 仅支持表名 / 子查询 / INNER JOIN");
    return pr;
}

/* ---------- 顶层 SELECT 构造 ---------- */

static bool driver_target_has_agg(const List *targetList) {
    if (targetList == NULL) return false;
    ListCell *lc;
    foreach (lc, targetList) {
        Node *n = (Node *)lfirst(lc);
        if (n != NULL && n->type == T_ResTarget) {
            ResTarget *rt = (ResTarget *)n;
            if (rt->val != NULL && rt->val->type == T_FuncCall) return true;
        }
    }
    return false;
}

static PlanBuildResult driver_build_select_plan(QueryResult *result,
                                                const SelectStmt *stmt) {
    PlanBuildResult pr = {0};
    if (stmt->larg != NULL || stmt->rarg != NULL) {
        SetQueryResultError(result, "SELECT: 集合操作暂不支持");
        return pr;
    }
    if (stmt->withClause != NULL) {
        SetQueryResultError(result, "SELECT: WITH 暂不支持");
        return pr;
    }
    if (stmt->intoClause != NULL) {
        SetQueryResultError(result, "SELECT INTO 暂不支持");
        return pr;
    }
    if (stmt->distinctClause) {
        SetQueryResultError(result, "SELECT: DISTINCT 暂不支持");
        return pr;
    }
    if (stmt->windowClause != NULL) {
        SetQueryResultError(result, "SELECT: WINDOW 暂不支持");
        return pr;
    }
    if (stmt->whereClause != NULL) {
        SetQueryResultError(result, "SELECT: WHERE 暂不支持（T9 主路径未涵盖）");
        return pr;
    }
    if (stmt->targetList == NULL || list_length(stmt->targetList) == 0) {
        SetQueryResultError(result, "SELECT: 目标列列表为空");
        return pr;
    }

    MemoryContext old_ctx = NULL;
    MemoryContext mcxt = driver_exec_ctx_begin(&old_ctx);
    if (mcxt == NULL) {
        SetQueryResultError(result, "执行内存上下文创建失败");
        return pr;
    }
    pr.mcxt = mcxt;

    pr = driver_build_from(result, stmt->fromClause, mcxt);
    if (result->error_msg != NULL) return pr;

    bool need_agg = (stmt->groupClause != NULL && list_length(stmt->groupClause) > 0)
                    || driver_target_has_agg(stmt->targetList);
    if (need_agg) {
        PlanBuildResult agg_pr = {0};
        if (!driver_build_agg(result, stmt, pr, &agg_pr)) return pr;
        pr = agg_pr;
    }

    if (stmt->sortClause != NULL && list_length(stmt->sortClause) > 0) {
        PlanBuildResult sort_pr = {0};
        if (!driver_build_sort(result, stmt->sortClause, pr, &sort_pr)) return pr;
        pr = sort_pr;
    }

    if (stmt->limitCount != NULL || stmt->limitOffset != NULL) {
        PlanBuildResult lim_pr = {0};
        if (!driver_build_limit(result, stmt->limitCount, stmt->limitOffset,
                                pr, &lim_pr)) return pr;
        pr = lim_pr;
    }

    return pr;
}

/* ========================================================================
 * 投影
 * ======================================================================== */

static int driver_resolve_aggref(const FuncCall *fc, const PlanBuildResult *pr) {
    if (fc->funcname == NULL || list_length(fc->funcname) != 1) return -1;
    Node *fn = (Node *)lfirst(fc->funcname->head);
    if (fn == NULL || fn->type != T_String) return -1;
    const char *fname = ((String *)fn)->str;
    char lower[32];
    snprintf(lower, sizeof(lower), "%s", fname);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (int i = 0; i < pr->natts; i++) {
        if (strcmp(pr->col_names[i], lower) == 0) return i;
    }
    /* 也尝试 COUNT(*)/SUM(col)/MIN(col)/MAX(col) 精确匹配 arg_attno */
    if (fc->agg_star && strcmp(lower, "count") == 0) {
        /* 查找名为 "count" 的 AggrefDef */
        for (int i = 0; i < pr->natts; i++) {
            if (strcmp(pr->col_names[i], "count") == 0) return i;
        }
    }
    return -1;
}

static bool driver_resolve_target(QueryResult *result, const ResTarget *rt,
                                  const PlanBuildResult *pr,
                                  int *out_attno, char **out_colname) {
    if (rt == NULL || rt->val == NULL) return false;
    *out_attno = -1;
    *out_colname = NULL;

    if (rt->val->type == T_ColumnRef) {
        ColumnRef *cref = (ColumnRef *)rt->val;
        const char *alias = NULL;
        const char *colname = NULL;
        if (driver_extract_colref(cref, &alias, &colname) != 0) {
            SetQueryResultError(result, "目标列引用非法");
            return false;
        }
        if (alias != NULL) {
            int found_from = -1;
            for (int i = 0; i < pr->nfrom_refs; i++) {
                /* 优先匹配 alias；未声明别名时回退到 relname（j_a.col 形式） */
                if (pr->from_refs[i].alias != NULL &&
                    strcmp(pr->from_refs[i].alias, alias) == 0) {
                    found_from = i;
                    break;
                }
                if (pr->from_refs[i].relname != NULL &&
                    strcmp(pr->from_refs[i].relname, alias) == 0) {
                    found_from = i;
                    break;
                }
            }
            if (found_from < 0) {
                SetQueryResultError(result, "别名 '%s' 不存在", alias);
                return false;
            }
            int base = pr->from_refs[found_from].base_attno;
            int end = (found_from + 1 < pr->nfrom_refs)
                       ? pr->from_refs[found_from + 1].base_attno
                       : pr->natts;
            for (int i = base; i < end; i++) {
                if (strcmp(pr->col_names[i], colname) == 0) {
                    *out_attno = i;
                    *out_colname = (rt->name != NULL) ? rt->name : pr->col_names[i];
                    return true;
                }
            }
            SetQueryResultError(result, "别名 '%s' 下找不到列 '%s'", alias, colname);
            return false;
        }
        for (int i = 0; i < pr->natts; i++) {
            if (strcmp(pr->col_names[i], colname) == 0) {
                *out_attno = i;
                *out_colname = (rt->name != NULL) ? rt->name : pr->col_names[i];
                return true;
            }
        }
        SetQueryResultError(result, "列 '%s' 不在 SELECT 输出中", colname);
        return false;
    }

    if (rt->val->type == T_FuncCall) {
        int agg_attno = driver_resolve_aggref((FuncCall *)rt->val, pr);
        if (agg_attno < 0) {
            SetQueryResultError(result, "聚合函数未在 SELECT 中声明或无 Agg 层");
            return false;
        }
        *out_attno = agg_attno;
        *out_colname = (rt->name != NULL) ? rt->name : pr->col_names[agg_attno];
        return true;
    }

    SetQueryResultError(result, "SELECT 目标仅支持列名或聚合函数");
    return false;
}

/* ========================================================================
 * execute_select（驱动入口）
 * ======================================================================== */

static QueryResult *execute_select(QueryResult *result, SelectStmt *stmt) {
    PlanBuildResult pr = driver_build_select_plan(result, stmt);
    if (result->error_msg != NULL) return result;

    int ntargets = list_length(stmt->targetList);
    int *proj_attnos = (int *)calloc((size_t)ntargets, sizeof(int));
    char **proj_names = (char **)calloc((size_t)ntargets, sizeof(char *));
    if (proj_attnos == NULL || proj_names == NULL) {
        free(proj_attnos); free(proj_names);
        MemoryContext old = MemoryContextCurrent();
        MemoryContextSwitchTo(pr.mcxt);
        MemoryContextSwitchTo(old);
        delete_memory(pr.mcxt);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    int col = 0;
    ListCell *lc;
    foreach (lc, stmt->targetList) {
        Node *n = (Node *)lfirst(lc);
        if (n == NULL || n->type != T_ResTarget) {
            free(proj_attnos); free(proj_names);
            delete_memory(pr.mcxt);
            SetQueryResultError(result, "目标项必须是 ResTarget");
            return result;
        }
        ResTarget *rt = (ResTarget *)n;
        int attno = -1;
        char *cname = NULL;
        if (!driver_resolve_target(result, rt, &pr, &attno, &cname)) {
            free(proj_attnos); free(proj_names);
            delete_memory(pr.mcxt);
            return result;
        }
        proj_attnos[col] = attno;
        proj_names[col] = cname ? strdup(cname) : NULL;
        col++;
    }

    QueryDesc *qd = CreateQueryDesc(pr.root, NULL);
    if (qd == NULL) {
        free(proj_attnos); free(proj_names);
        delete_memory(pr.mcxt);
        SetQueryResultError(result, "CreateQueryDesc 失败");
        return result;
    }
    ExecutorStart(qd, 0);
    if (qd->estate == NULL || qd->planstate == NULL) {
        ExecutorEnd(qd);
        free(proj_attnos); free(proj_names);
        delete_memory(pr.mcxt);
        SetQueryResultError(result, "ExecutorStart 失败");
        return result;
    }

    result->ncols = ntargets;
    result->col_names = proj_names;

    TupleTableSlot *slot;
    while ((slot = ExecProcNode(qd->planstate)) != NULL) {
        char ***new_rows = (char ***)realloc(
            result->rows, ((size_t)result->nrows + 1) * sizeof(char *));
        if (new_rows == NULL) { SetQueryResultError(result, "内存分配失败"); break; }
        result->rows = new_rows;
        char **row = (char **)calloc((size_t)ntargets, sizeof(char *));
        if (row == NULL) { SetQueryResultError(result, "内存分配失败"); break; }
        result->rows[result->nrows] = row;
        result->nrows++;
        for (int j = 0; j < ntargets; j++) {
            int attno = proj_attnos[j];
            Datum v = (slot->tts_values != NULL && attno < slot->tts_nvalid)
                       ? slot->tts_values[attno] : (Datum)0;
            bool in = (slot->tts_isnull != NULL && attno < slot->tts_nvalid)
                       ? slot->tts_isnull[attno] : true;
            Oid coltype = (attno < pr.natts) ? pr.col_types[attno] : InvalidOid;
            row[j] = driver_format_datum(coltype, v, in);
        }
    }

    qd->estate->es_processed = (uint64_t)result->nrows;
    free(proj_attnos);
    ExecutorFinish(qd);
    ExecutorEnd(qd);
    delete_memory(pr.mcxt);
    return result;
}

/* ========================================================================
 * DML 直通道（与 T8 一致）
 * ======================================================================== */

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
        snprintf(errbuf, errlen, "列 '%s' 的类型 OID %u 暂不支持 DML 写入", colname,
                 (unsigned)coltype);
        return -1;
    }
}

static void driver_scan_current_tid(const TableScanDesc scan, uint8_t tid[6]) {
    uint32_t blk = (uint32_t)scan->rs_cblock;
    uint16_t off = (uint16_t)(SizeOfPageHeaderData +
                              (scan->rs_cindex - 1) * SizeOfHeapLinePointer);
    memcpy(tid, &blk, sizeof(blk));
    memcpy(tid + sizeof(blk), &off, sizeof(off));
}

static QueryResult *execute_insert(QueryResult *result, InsertStmt *stmt) {
    char errbuf[160];
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "INSERT 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;
    if (stmt->selectStmt != NULL) {
        SetQueryResultError(result, "INSERT ... SELECT 暂不支持");
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
    int nvalue_cols = natts;
    int *col_attno = (int *)calloc((size_t)natts, sizeof(int));
    if (col_attno == NULL) {
        relation_close(rel, 0);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    for (int i = 0; i < natts; i++) col_attno[i] = i;
    if (stmt->cols == NULL) {
        int ndecl = 0;
        column_info_t *decl_cols = driver_decl_columns(table_oid, &ndecl);
        if (decl_cols == NULL || ndecl != natts) {
            if (decl_cols != NULL) catalog_free_columns(decl_cols);
            free(col_attno);
            relation_close(rel, 0);
            SetQueryResultError(result, "INSERT %s: catalog 列数与行描述符不一致", tname);
            return result;
        }
        for (int i = 0; i < ndecl; i++) {
            int attno = -1;
            for (int j = 0; j < natts; j++) {
                if (strcmp(td->attrs[j].attname, decl_cols[i].name) == 0) {
                    attno = j;
                    break;
                }
            }
            if (attno < 0) {
                catalog_free_columns(decl_cols);
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result, "INSERT %s: catalog 列 '%s' 不在行描述符中",
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
            ColumnDef *cref = (ColumnDef *)n;
            if (cref->colname == NULL) {
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result, "INSERT %s: 非法的列名", tname);
                return result;
            }
            int attno = -1;
            for (int j = 0; j < natts; j++) {
                if (strcmp(td->attrs[j].attname, cref->colname) == 0) {
                    attno = j;
                    break;
                }
            }
            if (attno < 0) {
                free(col_attno);
                relation_close(rel, 0);
                SetQueryResultError(result, "INSERT %s: 列 '%s' 不存在", tname, cref->colname);
                return result;
            }
            for (int j = 0; j < i; j++) {
                if (col_attno[j] == attno) {
                    free(col_attno);
                    relation_close(rel, 0);
                    SetQueryResultError(result, "INSERT %s: 列 '%s' 重复", tname, cref->colname);
                    return result;
                }
            }
            col_attno[i++] = attno;
        }
    }
    Oid *coltypes = (Oid *)calloc((size_t)natts, sizeof(Oid));
    Datum *values = (Datum *)calloc((size_t)natts, sizeof(Datum));
    bool *isnull = (bool *)calloc((size_t)natts, sizeof(bool));
    if (coltypes == NULL || values == NULL || isnull == NULL) {
        free(col_attno); free(coltypes); free(values); free(isnull);
        relation_close(rel, 0);
        SetQueryResultError(result, "内存分配失败");
        return result;
    }
    for (int i = 0; i < natts; i++) coltypes[i] = td->attrs[i].atttypid;
    int inserted = 0;
    ListCell *rowlc;
    foreach (rowlc, stmt->valuesLists) {
        List *exprs = (List *)lfirst(rowlc);
        if (exprs == NULL || list_length(exprs) != nvalue_cols) {
            SetQueryResultError(result, "INSERT %s: VALUES 列数 %d 与目标列数 %d 不匹配",
                                tname, exprs ? list_length(exprs) : 0, nvalue_cols);
            goto insert_done;
        }
        for (int i = 0; i < natts; i++) { values[i] = (Datum)0; isnull[i] = true; }
        int i = 0;
        ListCell *vlc;
        foreach (vlc, exprs) {
            Node *n = (Node *)lfirst(vlc);
            if (n == NULL || n->type != T_A_Const) {
                SetQueryResultError(result, "INSERT %s: VALUES 仅支持常量", tname);
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
            SetQueryResultError(result, "INSERT %s: 存在不支持编码的列类型", tname);
            goto insert_done;
        }
        void *buf = malloc(need);
        if (buf == NULL) {
            SetQueryResultError(result, "内存分配失败");
            goto insert_done;
        }
        size_t out_len = 0;
        int enc_rc = sql_row_encode(natts, coltypes, values, isnull, buf, need, &out_len);
        int ins_rc = (enc_rc == 0) ? heap_insert(rel, buf, out_len, 0, 0, NULL, NULL) : -1;
        free(buf);
        if (enc_rc != 0 || ins_rc != 0) {
            SetQueryResultError(result, "INSERT %s: heap_insert 失败", tname);
            goto insert_done;
        }
        inserted++;
    }
insert_done:
    free(col_attno); free(coltypes); free(values); free(isnull);
    relation_close(rel, 0);
    if (result->error_msg == NULL) result->nrows = inserted;
    return result;
}

static QueryResult *execute_update(QueryResult *result, UpdateStmt *stmt) {
    char errbuf[160];
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "UPDATE 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;
    if (stmt->whereClause != NULL) {
        SetQueryResultError(result, "UPDATE: WHERE 暂不支持");
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
    for (int i = 0; i < natts; i++) coltypes[i] = td->attrs[i].atttypid;
    {
        ListCell *lc;
        foreach (lc, stmt->targetList) {
            Node *n = (Node *)lfirst(lc);
            if (n == NULL || n->type != T_ResTarget) {
                SetQueryResultError(result, "UPDATE %s: 非法的 SET 项", tname);
                goto update_cleanup;
            }
            ResTarget *rt = (ResTarget *)n;
            if (rt->name == NULL || rt->val == NULL || rt->val->type != T_A_Const) {
                SetQueryResultError(result, "UPDATE %s: SET 仅支持 列=常量", tname);
                goto update_cleanup;
            }
            int attno = -1;
            for (int j = 0; j < natts; j++) {
                if (strcmp(td->attrs[j].attname, rt->name) == 0) { attno = j; break; }
            }
            if (attno < 0) {
                SetQueryResultError(result, "UPDATE %s: 列 '%s' 不存在", tname, rt->name);
                goto update_cleanup;
            }
            if (set_flag[attno]) {
                SetQueryResultError(result, "UPDATE %s: 列 '%s' 重复赋值", tname, rt->name);
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
    {
        MemoryContext old_ctx = NULL;
        MemoryContext mcxt = driver_exec_ctx_begin(&old_ctx);
        if (mcxt == NULL) {
            SetQueryResultError(result, "执行内存上下文创建失败");
            goto update_cleanup;
        }
        Datum *values = (Datum *)calloc((size_t)natts, sizeof(Datum));
        bool *isnull = (bool *)calloc((size_t)natts, sizeof(bool));
        typedef struct PendingUpdate { uint8_t tid[6]; void *buf; size_t len; } PendingUpdate;
        PendingUpdate *pending = NULL;
        int npending = 0;
        if (values == NULL || isnull == NULL) {
            free(values); free(isnull);
            driver_exec_ctx_end(mcxt, old_ctx);
            SetQueryResultError(result, "内存分配失败");
            goto update_cleanup;
        }
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
                SetQueryResultError(result, "UPDATE %s: 行解码失败", tname);
                break;
            }
            for (int i = 0; i < natts; i++) {
                if (set_flag[i]) { values[i] = set_val[i]; isnull[i] = set_isnull[i]; }
            }
            size_t need = sql_row_encoded_size(natts, coltypes, values, isnull);
            if (need == 0) {
                SetQueryResultError(result, "UPDATE %s: 存在不支持编码的列类型", tname);
                break;
            }
            PendingUpdate *np = (PendingUpdate *)realloc(
                pending, ((size_t)npending + 1) * sizeof(PendingUpdate));
            if (np == NULL) { SetQueryResultError(result, "内存分配失败"); break; }
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
                SetQueryResultError(result, "UPDATE %s: 行编码失败", tname);
                break;
            }
            pending[npending].len = out_len;
            driver_scan_current_tid(scan, pending[npending].tid);
            npending++;
        }
        table_endscan(scan);
        free(values); free(isnull);
        driver_exec_ctx_end(mcxt, old_ctx);
        int updated = 0;
        if (result->error_msg == NULL) {
            for (int i = 0; i < npending; i++) {
                if (heap_update(rel, pending[i].tid, pending[i].buf,
                                pending[i].len, 0, 0, NULL, 0) != 0) {
                    SetQueryResultError(result, "UPDATE %s: heap_update 失败", tname);
                    break;
                }
                updated++;
            }
            if (result->error_msg == NULL) result->nrows = updated;
        }
        for (int i = 0; i < npending; i++) free(pending[i].buf);
        free(pending);
    }
update_cleanup:
    free(coltypes); free(set_val); free(set_isnull); free(set_flag);
    relation_close(rel, 0);
    return result;
}

static QueryResult *execute_delete(QueryResult *result, DeleteStmt *stmt) {
    if (stmt->relation == NULL || stmt->relation->relname == NULL) {
        SetQueryResultError(result, "DELETE 缺少表名");
        return result;
    }
    const char *tname = stmt->relation->relname;
    if (stmt->whereClause != NULL) {
        SetQueryResultError(result, "DELETE: WHERE 暂不支持");
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
    int deleted = 0;
    for (int i = 0; i < npending; i++) {
        if (heap_delete(rel, pending[i].tid, 0, false, false) != 0) {
            SetQueryResultError(result, "DELETE %s: heap_delete 失败", tname);
            break;
        }
        deleted++;
    }
    free(pending);
    relation_close(rel, 0);
    if (result->error_msg == NULL) result->nrows = deleted;
    return result;
}

/* ========================================================================
 * 核心执行 API
 * ======================================================================== */

QueryResult *execute_sql(const char *sql, void *db) {
    QueryResult *result = CreateQueryResult();
    if (result == NULL) return NULL;
    (void)db;
    if (sql == NULL) {
        SetQueryResultError(result, "SQL 语句为空");
        return result;
    }
    Node *ast = sql_parse(sql);
    if (ast == NULL) {
        SetQueryResultError(result, "SQL 解析失败: %s", sql_parse_last_error());
        return result;
    }
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

int execute_ddl(const char *sql, void *db) {
    if (sql == NULL) return -1;
    QueryResult *result = execute_sql(sql, db);
    if (result == NULL) return -1;
    int ret = 0;
    if (result->error_msg != NULL) ret = -1;
    FreeQueryResult(result);
    return ret;
}

int execute_dml(const char *sql, void *db) {
    if (sql == NULL) return -1;
    QueryResult *result = execute_sql(sql, db);
    if (result == NULL) return -1;
    int rows_affected = -1;
    if (result->error_msg == NULL) rows_affected = result->nrows;
    FreeQueryResult(result);
    return rows_affected;
}