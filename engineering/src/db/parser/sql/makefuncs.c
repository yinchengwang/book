/**
 * @file makefuncs.c
 * @brief AST 节点构造函数实现
 *
 * 提供 PostgreSQL 风格的 AST 节点创建辅助函数。
 */

#include "db/parser/sql/makefuncs.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 通用节点构造
 * ============================================================ */

/**
 * @brief 创建通用节点（已分配并初始化类型标签）
 */
Node *makeNode(NodeTag type) {
    /* 根据类型分配正确大小的内存 */
    void *node = NULL;

    switch (type) {
        case T_SelectStmt:
            node = calloc(1, sizeof(SelectStmt));
            break;
        case T_InsertStmt:
            node = calloc(1, sizeof(InsertStmt));
            break;
        case T_UpdateStmt:
            node = calloc(1, sizeof(UpdateStmt));
            break;
        case T_DeleteStmt:
            node = calloc(1, sizeof(DeleteStmt));
            break;
        case T_CreateStmt:
            node = calloc(1, sizeof(CreateStmt));
            break;
        case T_DropStmt:
            node = calloc(1, sizeof(DropStmt));
            break;
        case T_RangeVar:
            node = calloc(1, sizeof(RangeVar));
            break;
        case T_ColumnRef:
            node = calloc(1, sizeof(ColumnRef));
            break;
        case T_A_Expr:
            node = calloc(1, sizeof(A_Expr));
            break;
        case T_A_Const:
            node = calloc(1, sizeof(A_Const));
            break;
        case T_FuncCall:
            node = calloc(1, sizeof(FuncCall));
            break;
        case T_BoolExpr:
            node = calloc(1, sizeof(BoolExpr));
            break;
        case T_NullTest:
            node = calloc(1, sizeof(NullTest));
            break;
        case T_ResTarget:
            node = calloc(1, sizeof(ResTarget));
            break;
        case T_SortBy:
            node = calloc(1, sizeof(SortBy));
            break;
        case T_JoinExpr:
            node = calloc(1, sizeof(JoinExpr));
            break;
        case T_RangeSubselect:
            node = calloc(1, sizeof(RangeSubselect));
            break;
        case T_ColumnDef:
            node = calloc(1, sizeof(ColumnDef));
            break;
        case T_CaseExpr:
            node = calloc(1, sizeof(CaseExpr));
            break;
        case T_CaseWhen:
            node = calloc(1, sizeof(CaseWhen));
            break;
        default:
            /* 默认使用 Node 结构大小 */
            node = calloc(1, sizeof(Node));
            break;
    }

    if (node) {
        ((Node *)node)->type = type;
    }

    return (Node *)node;
}

/**
 * @brief 创建字符串节点
 */
String *makeString(const char *str) {
    String *s = (String *)malloc(sizeof(String));
    if (s) {
        s->type = T_String;
        s->str = str ? strdup(str) : NULL;
    }
    return s;
}

/* ============================================================
 * 列表操作
 * ============================================================ */

/**
 * @brief 创建单元素列表
 */
List *list_make1(void *data) {
    List *l = (List *)malloc(sizeof(List));
    ListCell *lc = (ListCell *)malloc(sizeof(ListCell));

    if (!l || !lc) {
        free(l);
        free(lc);
        return NULL;
    }

    l->type = T_List;
    l->length = 1;
    lc->data = data;
    lc->next = NULL;
    l->head = lc;

    return l;
}

/**
 * @brief 创建双元素列表
 */
List *list_make2(void *d1, void *d2) {
    List *l = list_make1(d1);
    if (l) {
        l = lappend(l, d2);
    }
    return l;
}

/**
 * @brief 追加元素到列表末尾
 */
List *lappend(List *l, void *data) {
    if (!l) {
        return list_make1(data);
    }

    ListCell *lc = (ListCell *)malloc(sizeof(ListCell));
    if (!lc) {
        return l;
    }

    lc->data = data;
    lc->next = NULL;

    /* 找到列表尾部 */
    ListCell *tail = l->head;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = lc;
    l->length++;

    return l;
}

/**
 * @brief 获取列表第 n 个元素
 */
void *list_nth(List *l, int n) {
    if (!l || n < 0 || n >= l->length) {
        return NULL;
    }

    ListCell *lc = l->head;
    for (int i = 0; i < n && lc; i++) {
        lc = lc->next;
    }

    return lc ? lc->data : NULL;
}

/**
 * @brief 连接两个列表
 */
List *list_concat(List *l1, List *l2) {
    if (!l1) {
        return l2;
    }
    if (!l2) {
        return l1;
    }

    /* 找到 l1 的尾部 */
    ListCell *tail = l1->head;
    while (tail->next) {
        tail = tail->next;
    }

    /* 将 l2 连接到 l1 尾部 */
    tail->next = l2->head;
    l1->length += l2->length;

    /* 释放 l2 的 List 结构（保留 ListCell） */
    free(l2);

    return l1;
}

/**
 * @brief 释放列表（不释放元素数据）
 */
void list_free(List *l) {
    if (!l) {
        return;
    }

    ListCell *lc = l->head;
    while (lc) {
        ListCell *next = lc->next;
        free(lc);
        lc = next;
    }

    free(l);
}

/* ============================================================
 * DML 语句构造
 * ============================================================ */

/**
 * @brief 创建 SELECT 语句节点
 */
SelectStmt *makeSelectStmt(void) {
    SelectStmt *n = (SelectStmt *)makeNode(T_SelectStmt);
    return n;
}

/**
 * @brief 创建 INSERT 语句节点
 */
InsertStmt *makeInsertStmt(void) {
    InsertStmt *n = (InsertStmt *)makeNode(T_InsertStmt);
    return n;
}

/**
 * @brief 创建 UPDATE 语句节点
 */
UpdateStmt *makeUpdateStmt(void) {
    UpdateStmt *n = (UpdateStmt *)makeNode(T_UpdateStmt);
    return n;
}

/**
 * @brief 创建 DELETE 语句节点
 */
DeleteStmt *makeDeleteStmt(void) {
    DeleteStmt *n = (DeleteStmt *)makeNode(T_DeleteStmt);
    return n;
}

/* ============================================================
 * DDL 语句构造
 * ============================================================ */

/**
 * @brief 创建 CREATE TABLE 语句节点
 */
CreateStmt *makeCreateStmt(void) {
    CreateStmt *n = (CreateStmt *)makeNode(T_CreateStmt);
    return n;
}

/**
 * @brief 创建 DROP 语句节点
 */
DropStmt *makeDropStmt(void) {
    DropStmt *n = (DropStmt *)makeNode(T_DropStmt);
    return n;
}

/* ============================================================
 * 表引用构造
 * ============================================================ */

/**
 * @brief 创建范围变量（表引用）
 */
RangeVar *makeRangeVar(const char *schemaname, const char *relname) {
    RangeVar *n = (RangeVar *)makeNode(T_RangeVar);
    if (n) {
        n->schemaname = schemaname ? strdup(schemaname) : NULL;
        n->relname = relname ? strdup(relname) : NULL;
        n->alias = NULL;
    }
    return n;
}

/* ============================================================
 * 表达式构造
 * ============================================================ */

/**
 * @brief 创建列引用（单列名）
 */
ColumnRef *makeColumnRef(const char *colname) {
    ColumnRef *n = (ColumnRef *)makeNode(T_ColumnRef);
    if (n) {
        n->fields = list_make1(makeString(colname));
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建列引用（表名.列名）
 */
ColumnRef *makeColumnRef2(const char *tablename, const char *colname) {
    ColumnRef *n = (ColumnRef *)makeNode(T_ColumnRef);
    if (n) {
        n->fields = list_make2(makeString(tablename), makeString(colname));
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建 A_Expr 表达式节点
 */
A_Expr *makeA_Expr(A_Expr_Kind kind, const char *name, Node *lexpr, Node *rexpr) {
    A_Expr *n = (A_Expr *)makeNode(T_A_Expr);
    if (n) {
        n->kind = kind;
        n->name = list_make1(makeString(name));
        n->lexpr = lexpr;
        n->rexpr = rexpr;
    }
    return n;
}

/**
 * @brief 创建布尔表达式
 */
BoolExpr *makeBoolExpr(BoolType boolop, List *args) {
    BoolExpr *n = (BoolExpr *)makeNode(T_BoolExpr);
    if (n) {
        n->boolop = boolop;
        n->args = args;
    }
    return n;
}

/**
 * @brief 创建整数常量
 */
A_Const *makeIntConst(int val) {
    A_Const *n = (A_Const *)makeNode(T_A_Const);
    if (n) {
        n->val.type = T_Integer;
        n->val.val.ival = val;
        n->isnull = false;
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建浮点数常量
 */
A_Const *makeFloatConst(double val) {
    A_Const *n = (A_Const *)makeNode(T_A_Const);
    if (n) {
        n->val.type = T_Float;
        n->val.val.fval = val;
        n->isnull = false;
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建字符串常量
 */
A_Const *makeStringConst(const char *val) {
    A_Const *n = (A_Const *)makeNode(T_A_Const);
    if (n) {
        n->val.type = T_String;
        n->val.val.str = val ? strdup(val) : NULL;
        n->isnull = false;
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建 NULL 常量
 */
A_Const *makeNullConst(void) {
    A_Const *n = (A_Const *)makeNode(T_A_Const);
    if (n) {
        n->val.type = T_Null;
        n->isnull = true;
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建函数调用
 */
FuncCall *makeFuncCall(const char *funcname, List *args) {
    FuncCall *n = (FuncCall *)makeNode(T_FuncCall);
    if (n) {
        n->funcname = list_make1(makeString(funcname));
        n->args = args;
        n->agg_distinct = false;
        n->agg_star = false;
        n->location = 0;
    }
    return n;
}

/**
 * @brief 创建 NULL 测试表达式
 */
NullTest *makeNullTest(NullTestType nulltesttype, Node *arg) {
    NullTest *n = (NullTest *)makeNode(T_NullTest);
    if (n) {
        n->nulltesttype = nulltesttype;
        n->arg = arg;
    }
    return n;
}

/**
 * @brief 创建类型名
 */
TypeName *makeTypeName(const char *name) {
    TypeName *n = (TypeName *)malloc(sizeof(TypeName));
    if (n) {
        n->type = T_String;  /* 简化处理，使用 T_String 表示 */
        n->names = list_make1(makeString(name));
    }
    return n;
}

/**
 * @brief 创建 ResTarget 节点
 */
ResTarget *makeResTarget(const char *name, Node *val) {
    ResTarget *n = (ResTarget *)makeNode(T_ResTarget);
    if (n) {
        n->name = name ? strdup(name) : NULL;
        n->val = val;
    }
    return n;
}

/**
 * @brief 创建 ColumnDef 节点
 */
ColumnDef *makeColumnDef(const char *colname, TypeName *typeName) {
    ColumnDef *n = (ColumnDef *)makeNode(T_ColumnDef);
    if (n) {
        n->colname = colname ? strdup(colname) : NULL;
        n->typeName = typeName;
        n->is_not_null = false;
        n->is_primary_key = false;
    }
    return n;
}

/**
 * @brief 创建 SortBy 节点
 */
SortBy *makeSortBy(Node *node, SortByDir dir) {
    SortBy *n = (SortBy *)makeNode(T_SortBy);
    if (n) {
        n->node = node;
        n->sortby_dir = dir;
    }
    return n;
}

/**
 * @brief 创建 JoinExpr 节点
 */
JoinExpr *makeJoinExpr(JoinType jointype, Node *larg, Node *rarg, Node *quals) {
    JoinExpr *n = (JoinExpr *)makeNode(T_JoinExpr);
    if (n) {
        n->jointype = jointype;
        n->larg = larg;
        n->rarg = rarg;
        n->quals = quals;
    }
    return n;
}