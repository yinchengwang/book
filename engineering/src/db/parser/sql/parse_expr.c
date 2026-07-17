/**
 * @file parse_expr.c
 * @brief 表达式语义分析实现
 *
 * 将原始 AST 表达式（ColumnRef、A_Const、A_Expr 等）
 * 转换为语义化的表达式节点（Var、Const、OpExpr 等）。
 */

#include "db/parser/sql/parse_node.h"
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/makefuncs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>  /* strcasecmp */

/* makefuncs.h 中已声明 makeBoolExpr，parse_expr.c 不重复定义 */
/* makeBoolExpr 完全由 makefuncs.c 提供 */

/* NIL 常量（空列表） */
#define NIL ((List *)NULL)

/* ============================================================
 * 表达式转换主入口
 * ============================================================ */

/**
 * @brief 表达式转换主入口（内部实现）
 *
 * @param pstate 解析状态
 * @param expr 原始表达式节点
 * @param exprKind 表达式上下文
 * @return 转换后的表达式节点
 */
Node *transformExprImpl(ParseState *pstate, Node *expr, ParseExprKind exprKind) {
    Node *result = NULL;

    if (!expr) {
        return NULL;
    }

    /* 保存表达式上下文 */
    ParseExprKind save_expr_kind = pstate->p_expr_kind;
    pstate->p_expr_kind = exprKind;

    switch (expr->type) {
        case T_ColumnRef:
            result = transformColumnRef(pstate, (ColumnRef *)expr);
            break;

        case T_A_Const:
            result = transformConst(pstate, (A_Const *)expr);
            break;

        case T_A_Expr:
            result = transformA_Expr(pstate, (A_Expr *)expr);
            break;

        case T_FuncCall:
            result = transformFuncCall(pstate, (FuncCall *)expr);
            break;

        case T_BoolExpr:
            result = transformBoolExpr(pstate, (BoolExpr *)expr);
            break;

        case T_NullTest:
            result = transformNullTest(pstate, (NullTest *)expr);
            break;

        case T_TypeCast:
            result = transformTypeCast(pstate, (TypeCast *)expr);
            break;

        case T_CaseExpr:
            result = transformCaseExpr(pstate, (CaseExpr *)expr);
            break;

        case T_SubLink:
            result = transformSubLink(pstate, (SubLink *)expr);
            break;

        case T_List:
            /* 列表表达式（如 IN 列表） */
            result = transformListExpr(pstate, (List *)expr, exprKind);
            break;

        default:
            /* 未知节点类型，保留原样 */
            fprintf(stderr, "警告: 未实现的表达式类型: %d\n", expr->type);
            result = expr;
            break;
    }

    /* 恢复表达式上下文 */
    pstate->p_expr_kind = save_expr_kind;

    return result;
}

/**
 * @brief 表达式转换公共接口
 */
Node *transformExpr(ParseState *pstate, Node *expr, ParseExprKind exprKind) {
    return transformExprImpl(pstate, expr, exprKind);
}

/* ============================================================
 * 列引用转换
 * ============================================================ */

/**
 * @brief 转换列引用
 *
 * 解析列名，查找范围表，生成 Var 节点。
 *
 * 列引用格式：
 * - column_name
 * - table.column_name
 * - schema.table.column_name
 */
Node *transformColumnRef(ParseState *pstate, ColumnRef *cref) {
    Var *var;
    ListCell *lc;
    char *colname = NULL;
    char *relname = NULL;
    char *schemaname = NULL;
    int nfields;
    int i;

    if (!cref || !cref->fields) {
        return NULL;
    }

    /* 解析字段列表 */
    nfields = list_length(cref->fields);
    lc = cref->fields->head;

    if (nfields == 1) {
        /* 单字段：column_name */
        Value *v = (Value *)lfirst(lc);
        colname = v->val.str;
    } else if (nfields == 2) {
        /* 双字段：table.column_name */
        Value *v1 = (Value *)lfirst(lc);
        Value *v2 = (Value *)lfirst(lnext(lc));
        relname = v1->val.str;
        colname = v2->val.str;
    } else if (nfields == 3) {
        /* 三字段：schema.table.column_name */
        Value *v1 = (Value *)lfirst(lc);
        Value *v2 = (Value *)lfirst(lnext(lc));
        Value *v3 = (Value *)lfirst(lnext(lnext(lc)));
        schemaname = v1->val.str;
        relname = v2->val.str;
        colname = v3->val.str;
    } else {
        /* 不支持的格式 */
        fprintf(stderr, "错误: 不支持的列引用格式（字段数: %d）\n", nfields);
        return NULL;
    }

    /* 在范围表中查找列 */
    /* 简化实现：假设列存在，生成占位 Var */
    var = makeVar(1, 1, 0, 0, 0, cref->location);

    if (!var) {
        return NULL;
    }

    /* 设置列名（临时存储在 Var 节点中） */
    /* 注意：实际实现需要从范围表获取列信息 */

    return (Node *)var;
}

/**
 * @brief 创建 Var 节点
 */
Var *makeVar(Index varno, AttrNumber varattno, Oid vartype,
             int32_t vartypmod, Oid varcollid, int location) {
    Var *var = (Var *)calloc(1, sizeof(Var));

    if (var) {
        var->expr.type = T_ColumnRef;  /* 临时标记 */
        var->varno = varno;
        var->varattno = varattno;
        var->vartype = vartype;
        var->vartypmod = vartypmod;
        var->varcollid = varcollid;
        var->varnoold = varno;
        var->varoattno = varattno;
        var->location = location;
    }

    return var;
}

/* ============================================================
 * 常量转换
 * ============================================================ */

/**
 * @brief 转换常量值
 */
Node *transformConst(ParseState *pstate, A_Const *aconst) {
    Const *con;

    if (!aconst) {
        return NULL;
    }

    con = (Const *)calloc(1, sizeof(Const));
    if (!con) {
        return NULL;
    }

    con->expr.type = T_A_Const;  /* 临时标记 */

    if (aconst->isnull) {
        /* NULL 常量 */
        con->constisnull = true;
        con->consttype = 0;  /* unknown type */
        con->constlen = 0;
        con->constbyval = false;
        con->constvalue = 0;
    } else {
        /* 非空常量 */
        con->constisnull = false;

        switch (aconst->val.type) {
            case T_Integer:
                /* 整数常量 */
                con->consttype = 23;  /* int4 OID（简化） */
                con->constlen = 4;
                con->constbyval = true;
                con->constvalue = (uintptr_t)aconst->val.val.ival;
                break;

            case T_Float:
                /* 浮点常量 */
                con->consttype = 701;  /* float8 OID（简化） */
                con->constlen = 8;
                con->constbyval = true;
                /* 注意：需要正确的浮点值转换，这里简化处理 */
                con->constvalue = (uintptr_t)&aconst->val.val.fval;
                break;

            case T_String:
                /* 字符串常量 */
                con->consttype = 25;  /* text OID（简化） */
                con->constlen = -1;  /* varlena */
                con->constbyval = false;
                con->constvalue = (uintptr_t)aconst->val.val.str;
                break;

            default:
                /* 未知类型 */
                con->consttype = 0;
                con->constlen = 0;
                con->constbyval = false;
                con->constvalue = 0;
                break;
        }
    }

    return (Node *)con;
}

/**
 * @brief 创建常量节点
 */
Const *makeConst(Oid consttype, int32_t consttypmod, Oid constcollid,
                 int constlen, uintptr_t constvalue, bool constisnull,
                 bool constbyval) {
    Const *con = (Const *)calloc(1, sizeof(Const));

    if (con) {
        con->expr.type = T_A_Const;
        con->consttype = consttype;
        con->consttypmod = consttypmod;
        con->constcollid = constcollid;
        con->constlen = constlen;
        con->constvalue = constvalue;
        con->constisnull = constisnull;
        con->constbyval = constbyval;
    }

    return con;
}

/* ============================================================
 * 操作符表达式转换
 * ============================================================ */

/**
 * @brief 转换操作符表达式
 */
Node *transformA_Expr(ParseState *pstate, A_Expr *a) {
    Node *lexpr;
    Node *rexpr;
    OpExpr *op;

    if (!a) {
        return NULL;
    }

    /* 转换左右表达式 */
    lexpr = transformExpr(pstate, a->lexpr, pstate->p_expr_kind);
    rexpr = transformExpr(pstate, a->rexpr, pstate->p_expr_kind);

    /* 根据表达式类型处理 */
    switch (a->kind) {
        case AEXPR_OP:
            /* 普通操作符 */
            op = (OpExpr *)calloc(1, sizeof(OpExpr));
            if (!op) {
                return NULL;
            }

            op->expr.type = T_A_Expr;
            op->opno = 0;  /* 需要从操作符名解析 */
            op->opfuncid = 0;
            op->opresulttype = 16;  /* bool OID（简化） */
            op->opretset = false;
            op->opcollid = 0;
            op->inputcollid = 0;
            op->args = list_make2(lexpr, rexpr);
            op->location = a->location;

            return (Node *)op;

        case AEXPR_OP_ANY:
            /* ANY 操作符：expr = ANY(array) */
            /* 简化处理：生成 OpExpr */
            op = (OpExpr *)calloc(1, sizeof(OpExpr));
            if (!op) {
                return NULL;
            }

            op->expr.type = T_A_Expr;
            op->opno = 0;
            op->opresulttype = 16;
            op->args = list_make2(lexpr, rexpr);
            op->location = a->location;

            return (Node *)op;

        case AEXPR_IN:
            /* IN 表达式：expr IN (val1, val2, ...) */
            /* 简化处理：生成 ScalarArrayOpExpr 的简化版本 */
            op = (OpExpr *)calloc(1, sizeof(OpExpr));
            if (!op) {
                return NULL;
            }

            op->expr.type = T_A_Expr;
            op->opno = 0;  /* = 操作符 */
            op->opresulttype = 16;
            op->args = list_make2(lexpr, rexpr);
            op->location = a->location;

            return (Node *)op;

        case AEXPR_LIKE:
            /* LIKE 表达式 */
            op = (OpExpr *)calloc(1, sizeof(OpExpr));
            if (!op) {
                return NULL;
            }

            op->expr.type = T_A_Expr;
            op->opno = 0;  /* LIKE 操作符 OID */
            op->opresulttype = 16;
            op->args = list_make2(lexpr, rexpr);
            op->location = a->location;

            return (Node *)op;

        case AEXPR_BETWEEN:
        case AEXPR_NOT_BETWEEN:
            /* BETWEEN 表达式：expr BETWEEN low AND high */
            /* 简化处理：生成 AND 表达式 */
            {
                BoolExpr *bexpr = (BoolExpr *)calloc(1, sizeof(BoolExpr));
                if (!bexpr) {
                    return NULL;
                }

                bexpr->type = T_BoolExpr;
                bexpr->boolop = AND_EXPR;
                /* 简化：生成 expr >= low AND expr <= high */
                /* 实际需要正确处理 BETWEEN 语义 */
                bexpr->args = list_make2(lexpr, rexpr);

                return (Node *)bexpr;
            }

        default:
            /* 未知类型，返回空 */
            return NULL;
    }
}

/* ============================================================
 * 函数调用转换
 * ============================================================ */

/**
 * @brief 转换函数调用
 *
 * 判断是否为聚合函数，生成 FuncExpr 或 Aggref。
 */
Node *transformFuncCall(ParseState *pstate, FuncCall *fc) {
    List *args = NIL;
    ListCell *lc;
    bool is_agg = false;
    char *funcname = NULL;

    if (!fc) {
        return NULL;
    }

    /* 获取函数名 */
    if (fc->funcname && fc->funcname->head) {
        Value *v = (Value *)lfirst(fc->funcname->head);
        funcname = v->val.str;
    }

    /* 判断是否为聚合函数 */
    if (isAggregateFunction(funcname)) {
        is_agg = true;
        pstate->p_hasAggs = true;
    }

    /* 转换参数列表 */
    if (fc->args) {
        foreach (lc, fc->args) {
            Node *arg = (Node *)lfirst(lc);
            Node *newarg = transformExpr(pstate, arg, pstate->p_expr_kind);
            args = lappend(args, newarg);
        }
    }

    if (is_agg) {
        /* 生成 Aggref */
        Aggref *agg = (Aggref *)calloc(1, sizeof(Aggref));
        if (!agg) {
            return NULL;
        }

        agg->expr.type = T_FuncCall;
        agg->aggfnoid = 0;  /* 需要从函数名解析 */
        agg->aggtype = 0;  /* 需要从函数信息解析 */
        agg->args = args;
        agg->aggorder = fc->agg_order;
        agg->aggdistinct = (List *)(intptr_t)fc->agg_distinct;
        agg->aggstar = fc->agg_star;
        agg->aggfilter = fc->agg_filter ? transformExpr(pstate, (Node *)fc->agg_filter,
                                                          pstate->p_expr_kind) : NULL;
        agg->location = fc->location;

        /* 添加到聚合函数列表 */
        pstate->p_aggs = lappend(pstate->p_aggs, agg);

        return (Node *)agg;
    } else {
        /* 生成 FuncExpr */
        FuncExpr *func = (FuncExpr *)calloc(1, sizeof(FuncExpr));
        if (!func) {
            return NULL;
        }

        func->expr.type = T_FuncCall;
        func->funcid = 0;  /* 需要从函数名解析 */
        func->funcresulttype = 0;  /* 需要从函数信息解析 */
        func->funcretset = false;
        func->funcvariadic = fc->func_variadic;
        func->args = args;
        func->location = fc->location;

        return (Node *)func;
    }
}

/**
 * @brief 判断是否为聚合函数
 */
bool isAggregateFunction(const char *funcname) {
    if (!funcname) {
        return false;
    }

    /* 常见聚合函数列表 */
    static const char *agg_funcs[] = {
        "count", "sum", "avg", "min", "max",
        "array_agg", "string_agg", "json_agg",
        "grouping", "every", "any", "some",
        NULL
    };

    for (int i = 0; agg_funcs[i]; i++) {
        if (strcasecmp(funcname, agg_funcs[i]) == 0) {
            return true;
        }
    }

    return false;
}

/* ============================================================
 * 布尔表达式转换
 * ============================================================ */

/**
 * @brief 转换布尔表达式
 */
Node *transformBoolExpr(ParseState *pstate, BoolExpr *bexpr) {
    List *args = NIL;
    ListCell *lc;
    BoolExpr *result;

    if (!bexpr) {
        return NULL;
    }

    /* 转换参数列表 */
    foreach (lc, bexpr->args) {
        Node *arg = (Node *)lfirst(lc);
        Node *newarg = transformExpr(pstate, arg, pstate->p_expr_kind);
        args = lappend(args, newarg);
    }

    /* 创建新的 BoolExpr */
    result = (BoolExpr *)calloc(1, sizeof(BoolExpr));
    if (!result) {
        return NULL;
    }

    result->type = T_BoolExpr;
    result->boolop = bexpr->boolop;
    result->args = args;

    return (Node *)result;
}

/* ============================================================
 * NULL 测试转换
 * ============================================================ */

/**
 * @brief 转换 NULL 测试
 */
Node *transformNullTest(ParseState *pstate, NullTest *ntest) {
    NullTest *result;
    Node *arg;

    if (!ntest) {
        return NULL;
    }

    /* 转换参数 */
    arg = transformExpr(pstate, ntest->arg, pstate->p_expr_kind);

    result = (NullTest *)calloc(1, sizeof(NullTest));
    if (!result) {
        return NULL;
    }

    result->type = T_NullTest;
    result->nulltesttype = ntest->nulltesttype;
    result->arg = arg;

    return (Node *)result;
}

/* ============================================================
 * 类型转换
 * ============================================================ */

/**
 * @brief 转换类型转换表达式
 */
Node *transformTypeCast(ParseState *pstate, TypeCast *tc) {
    Node *arg;
    TypeCast *result;

    if (!tc) {
        return NULL;
    }

    /* 转换参数表达式 */
    arg = transformExpr(pstate, tc->arg, pstate->p_expr_kind);

    result = (TypeCast *)calloc(1, sizeof(TypeCast));
    if (!result) {
        return NULL;
    }

    result->type = T_TypeCast;
    result->arg = arg;
    result->typeName = tc->typeName;
    result->location = tc->location;

    return (Node *)result;
}

/* ============================================================
 * CASE 表达式转换
 * ============================================================ */

/**
 * @brief 转换 CASE 表达式
 */
Node *transformCaseExpr(ParseState *pstate, CaseExpr *cexpr) {
    CaseExpr *result;
    Node *arg;
    List *args = NIL;
    Node *defresult;
    ListCell *lc;

    if (!cexpr) {
        return NULL;
    }

    /* 转换测试表达式（简单 CASE） */
    arg = transformExpr(pstate, cexpr->arg, pstate->p_expr_kind);

    /* 转换 WHEN 子句 */
    foreach (lc, cexpr->args) {
        CaseWhen *when = (CaseWhen *)lfirst(lc);
        CaseWhen *new_when = (CaseWhen *)calloc(1, sizeof(CaseWhen));
        if (!new_when) {
            continue;
        }

        new_when->type = T_CaseWhen;
        new_when->expr = transformExpr(pstate, when->expr, pstate->p_expr_kind);
        new_when->result = transformExpr(pstate, when->result, pstate->p_expr_kind);
        new_when->location = when->location;

        args = lappend(args, new_when);
    }

    /* 转换 ELSE 子句 */
    defresult = transformExpr(pstate, cexpr->defresult, pstate->p_expr_kind);

    result = (CaseExpr *)calloc(1, sizeof(CaseExpr));
    if (!result) {
        return NULL;
    }

    result->type = T_CaseExpr;
    result->arg = arg;
    result->args = args;
    result->defresult = defresult;
    result->location = cexpr->location;

    return (Node *)result;
}

/* ============================================================
 * 子链接转换
 * ============================================================ */

/**
 * @brief 转换子链接
 */
Node *transformSubLink(ParseState *pstate, SubLink *sublink) {
    SubLink *result;

    if (!sublink) {
        return NULL;
    }

    /* 记录子链接 */
    pstate->p_sublinks = lappend(pstate->p_sublinks, sublink);

    /* 简化处理：保留原始节点 */
    /* TODO: 转换子查询 */

    result = (SubLink *)calloc(1, sizeof(SubLink));
    if (!result) {
        return NULL;
    }

    result->expr.type = T_SubLink;
    result->subLinkType = sublink->subLinkType;
    result->subLinkId = sublink->subLinkId;
    result->testexpr = sublink->testexpr;
    result->operName = sublink->operName;
    result->subselect = sublink->subselect;
    result->location = sublink->location;

    return (Node *)result;
}

/* ============================================================
 * 列表表达式转换
 * ============================================================ */

/**
 * @brief 转换列表表达式
 */
Node *transformListExpr(ParseState *pstate, List *list, ParseExprKind exprKind) {
    List *result = NIL;
    ListCell *lc;

    if (!list) {
        return NULL;
    }

    foreach (lc, list) {
        Node *node = (Node *)lfirst(lc);
        Node *newnode = transformExpr(pstate, node, exprKind);
        result = lappend(result, newnode);
    }

    return (Node *)result;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 创建 OpExpr 节点
 */
OpExpr *makeOpExpr(Oid opno, Oid opresulttype, List *args, int location) {
    OpExpr *op = (OpExpr *)calloc(1, sizeof(OpExpr));

    if (op) {
        op->expr.type = T_A_Expr;
        op->opno = opno;
        op->opfuncid = 0;
        op->opresulttype = opresulttype;
        op->opretset = false;
        op->opcollid = 0;
        op->inputcollid = 0;
        op->args = args;
        op->location = location;
    }

    return op;
}

/**
 * @brief 创建 BoolExpr 节点（委托至 makefuncs.c 实现）
 */
extern BoolExpr *makeBoolExpr(BoolType boolop, List *args);

