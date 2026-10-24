/* gram.y - SQL 语法分析器 */
/**
 * @file gram.y
 * @brief Bison 语法规则文件
 *
 * 实现 SQL:2016 核心语法，支持：
 * - DML: SELECT/INSERT/UPDATE/DELETE
 * - DDL: CREATE TABLE/DROP TABLE
 * - 表达式：算术/逻辑/比较/函数调用
 */

%{
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/makefuncs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Flex 生成的词法分析器接口 */
extern int sql_yylex(void);
extern YYSTYPE sql_yylval;
extern YYLTYPE sql_yylloc;
extern char *sql_yytext;

/* 解析结果 */
static Node *parsetree;

/* 解析结果与错误的对外访问器（供 sql_parse.c 使用） */
static char yy_error_buf[512];
Node *sql_yy_get_parsetree(void) { return parsetree; }
const char *sql_yy_get_error(void) { return yy_error_buf; }
void sql_yy_reset_state(void) {
    parsetree = NULL;
    yy_error_buf[0] = '\0';
    sql_yylloc.first_line = 1;
    sql_yylloc.first_column = 1;
    sql_yylloc.last_line = 1;
    sql_yylloc.last_column = 1;
}

/* 错误处理：写入缓冲（经 sql_parse_last_error() 读取），不再打 stderr。
 * 显式使用 sql_yy 前缀名（bison 通过 #define yyerror sql_yyerror 调用）。 */
void sql_yyerror(const char *msg) {
    snprintf(yy_error_buf, sizeof(yy_error_buf),
             "%s (line %d, column %d)", msg,
             sql_yylloc.first_line, sql_yylloc.first_column);
}

/* NIL 常量（空列表） */
#define NIL ((List *)NULL)

%}

/* Bison 配置 */
/* T6：移除 %pure-parser —— scan.l 是非重入 Flex 词法器（全局 yylval/yylloc、
 * yylex(void) 签名），与 pure-parser 的 yylex(&yylval,&yylloc) 调用约定不兼容。
 * YYSTYPE 的唯一定义在 parsenodes.h（%union 会与之重复定义）。 */
%define api.prefix {sql_yy}
%define api.value.type {union YYSTYPE}
%define api.location.type {YYLTYPE}
%locations
%defines
%define parse.error verbose

/* ============================================================
 * Token 声明
 * ============================================================ */

/* 字面量 */
%token <str> IDENT SCONST
%token <ival> ICONST
%token <fval> FCONST
%token <str> XCONST

/* DML 关键字 */
%token SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE
%token GROUP BY HAVING ORDER ASC DESC LIMIT OFFSET

/* DDL 关键字 */
%token CREATE DROP TABLE INDEX VIEW ALTER
%token PRIMARY KEY FOREIGN REFERENCES UNIQUE CHECK CONSTRAINT

/* 连接关键字 */
%token JOIN LEFT RIGHT FULL INNER OUTER CROSS ON AS

/* 集合操作 */
%token DISTINCT ALL UNION INTERSECT EXCEPT

/* CTE */
%token WITH RECURSIVE

/* 逻辑操作 */
%token AND OR NOT

/* NULL 和布尔值 */
%token NULL_P TRUE_P FALSE_P IS IN LIKE BETWEEN

/* CASE 表达式 */
%token CASE WHEN THEN ELSE END

/* 数据类型 */
%token INT_P BIGINT SMALLINT TINYINT
%token REAL DOUBLE_P FLOAT_P DECIMAL_P NUMERIC
%token VARCHAR CHAR_P CHARACTER TEXT_P
%token BOOLEAN_P DATE TIME TIMESTAMP BLOB

/* 其他 */
%token DEFAULT USING CASCADE RESTRICT IF EXISTS

/* 词法器识别但语法暂未使用的关键字（T6：scan.l 会返回它们，必须先声明） */
%token DATABASE SCHEMA

/* ============================================================
 * 操作符优先级（从低到高）
 * ============================================================ */

/* 赋值和比较（'=' 只在 %left 组声明一次；T6 修复重复声明导致的 bison 错误） */
%left '<' '>' '=' Op

/* 逻辑操作符 */
%left OR
%left AND
%right NOT

/* 算术操作符 */
%left '+' '-'
%left '*' '/' '%'
%left UMINUS
%left CONCAT

/* ============================================================
 * 非终结符类型
 * ============================================================ */

/* 语句 */
%type <node> stmt select_stmt insert_stmt update_stmt delete_stmt
%type <node> create_stmt drop_stmt

/* 子句（where/having/limit 语义值为节点；from/group/order/target 为列表——
 * T6 修正：与规则动作及 SelectStmt 字段类型对齐） */
%type <list> target_list from_clause group_clause order_clause
%type <node> where_clause having_clause limit_clause
%type <list> column_list value_list set_clause_list
%type <list> column_def_list

/* 表引用（join_clause 无语义值——其规则无动作，仅作语法占位） */
%type <node> table_ref

/* 表达式 */
%type <node> expr column_ref const_val func_call case_expr
%type <list> expr_list sort_list case_when_list

/* 辅助 */
%type <str> table_name column_name
%type <node> data_type column_def sort_item

/* ============================================================
 * 起始符号
 * ============================================================ */

%start stmt

%%

/* ============================================================
 * 语句规则
 * ============================================================ */

stmt:
      select_stmt      { parsetree = $1; $$ = $1; }
    | insert_stmt      { parsetree = $1; $$ = $1; }
    | update_stmt      { parsetree = $1; $$ = $1; }
    | delete_stmt      { parsetree = $1; $$ = $1; }
    | create_stmt      { parsetree = $1; $$ = $1; }
    | drop_stmt        { parsetree = $1; $$ = $1; }
    ;

/* ============================================================
 * SELECT 语句
 * ============================================================ */

/* T9：五个子句均可独立缺省（各规则含空产生式），
 * ORDER BY/GROUP BY/LIMIT/OFFSET 不再以 WHERE 为前置条件。 */
select_stmt:
      SELECT target_list FROM from_clause where_clause group_clause having_clause order_clause limit_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            n->groupClause = $6;
            n->havingClause = $7;
            n->sortClause = $8;
            if ($9 != NULL) {
                /* limit_clause 语义值为双元素 List：(count, offset) */
                List *lim = (List *)$9;
                n->limitCount = (Node *)lfirst(lim->head);
                n->limitOffset = (lim->head->next != NULL)
                               ? (Node *)lfirst(lim->head->next) : NULL;
            }
            $$ = (Node *)n;
        }
    ;

/* 目标列列表（T6：允许函数调用，如 COUNT(*)；
 * T9：允许 `column_ref AS IDENT` / `func_call AS IDENT` 形成 ResTarget 别名。） */
target_list:
      '*'
        {
            ColumnRef *n = makeColumnRef("*");
            $$ = list_make1(n);
        }
    | target_list ',' column_ref
        {
            /* T9：列引用 → ResTarget（name=NULL, val=ColumnRef） */
            ResTarget *r = (ResTarget *)makeNode(T_ResTarget);
            r->name = NULL;
            r->val = (Node *)$3;
            $$ = lappend($1, (Node *)r);
        }
    | target_list ',' column_ref AS IDENT
        {
            /* T9：列别名 → ResTarget 包装（name=$5, val=$3） */
            ResTarget *r = makeResTarget($5, $3);
            $$ = lappend($1, (Node *)r);
        }
    | target_list ',' func_call
        {
            /* T9：函数调用 → ResTarget（name=NULL, val=FuncCall） */
            ResTarget *r = (ResTarget *)makeNode(T_ResTarget);
            r->name = NULL;
            r->val = (Node *)$3;
            $$ = lappend($1, (Node *)r);
        }
    | target_list ',' func_call AS IDENT
        {
            ResTarget *r = makeResTarget($5, $3);
            $$ = lappend($1, (Node *)r);
        }
    | column_ref
        {
            /* T9：列引用 → ResTarget（name=NULL, val=ColumnRef） */
            ResTarget *r = (ResTarget *)makeNode(T_ResTarget);
            r->name = NULL;
            r->val = (Node *)$1;
            $$ = list_make1((Node *)r);
        }
    | column_ref AS IDENT
        {
            ResTarget *r = makeResTarget($3, $1);
            $$ = list_make1((Node *)r);
        }
    | func_call
        {
            /* T9：函数调用 → ResTarget（name=NULL, val=FuncCall） */
            ResTarget *r = (ResTarget *)makeNode(T_ResTarget);
            r->name = NULL;
            r->val = (Node *)$1;
            $$ = list_make1((Node *)r);
        }
    | func_call AS IDENT
        {
            ResTarget *r = makeResTarget($3, $1);
            $$ = list_make1((Node *)r);
        }
    ;

/* FROM 子句 */
from_clause:
      table_ref
        {
            $$ = list_make1($1);
        }
    | from_clause ',' table_ref
        {
            $$ = lappend($1, $3);
        }
    ;

/* 表引用 */
table_ref:
      table_name
        {
            RangeVar *n = makeRangeVar(NULL, $1);
            $$ = (Node *)n;
        }
    | table_name AS IDENT
        {
            RangeVar *n = makeRangeVar(NULL, $1);
            n->alias = strdup($3);
            $$ = (Node *)n;
        }
    | table_name IDENT
        {
            RangeVar *n = makeRangeVar(NULL, $1);
            n->alias = strdup($2);
            $$ = (Node *)n;
        }
    | table_ref join_clause table_ref ON expr
        {
            JoinExpr *n = makeJoinExpr(JOIN_INNER, $1, $3, $5);
            $$ = (Node *)n;
        }
    | table_ref LEFT JOIN table_ref ON expr
        {
            JoinExpr *n = makeJoinExpr(JOIN_LEFT, $1, $4, $6);
            $$ = (Node *)n;
        }
    | table_ref RIGHT JOIN table_ref ON expr
        {
            JoinExpr *n = makeJoinExpr(JOIN_RIGHT, $1, $4, $6);
            $$ = (Node *)n;
        }
    | table_ref FULL JOIN table_ref ON expr
        {
            JoinExpr *n = makeJoinExpr(JOIN_FULL, $1, $4, $6);
            $$ = (Node *)n;
        }
    | table_ref CROSS JOIN table_ref
        {
            JoinExpr *n = makeJoinExpr(JOIN_CROSS, $1, $4, NULL);
            $$ = (Node *)n;
        }
    | '(' select_stmt ')' AS IDENT
        {
            RangeSubselect *n = (RangeSubselect *)makeNode(T_RangeSubselect);
            n->subquery = $2;
            n->alias = strdup($5);
            $$ = (Node *)n;
        }
    | '(' select_stmt ')' IDENT
        {
            /* T9：FROM 子查询允许省略 AS（与 PG 兼容） */
            RangeSubselect *n = (RangeSubselect *)makeNode(T_RangeSubselect);
            n->subquery = $2;
            n->alias = strdup($4);
            $$ = (Node *)n;
        }
    ;

/* 连接子句 */
join_clause:
      JOIN
    | INNER JOIN
    | OUTER JOIN
    ;

/* WHERE 子句（T9：可缺省） */
where_clause:
      WHERE expr
        {
            $$ = $2;
        }
    | /* empty */
        {
            $$ = NULL;
        }
    ;

/* GROUP BY 子句（T9：可缺省） */
group_clause:
      GROUP BY expr_list
        {
            $$ = $3;
        }
    | /* empty */
        {
            $$ = NULL;
        }
    ;

/* HAVING 子句（T9：可缺省） */
having_clause:
      HAVING expr
        {
            $$ = $2;
        }
    | /* empty */
        {
            $$ = NULL;
        }
    ;

/* ORDER BY 子句（T9：可缺省） */
order_clause:
      ORDER BY sort_list
        {
            $$ = $3;
        }
    | /* empty */
        {
            $$ = NULL;
        }
    ;

/* 排序列表 */
sort_list:
      sort_item
        {
            $$ = list_make1($1);
        }
    | sort_list ',' sort_item
        {
            $$ = lappend($1, $3);
        }
    ;

/* 排序项 */
sort_item:
      column_ref
        {
            $$ = (Node *)makeSortBy($1, SORTBY_DEFAULT);
        }
    | column_ref ASC
        {
            $$ = (Node *)makeSortBy($1, SORTBY_ASC);
        }
    | column_ref DESC
        {
            $$ = (Node *)makeSortBy($1, SORTBY_DESC);
        }
    ;

/* LIMIT 子句（T9：可缺省；保留 OFFSET。
 * 语义值为双元素 List：(count A_Const 或 NULL, offset A_Const 或 NULL)，
 * 由 select_stmt 动作拆入 SelectStmt.limitCount/limitOffset。） */
limit_clause:
      LIMIT ICONST
        {
            $$ = (Node *)list_make2((Node *)makeIntConst($2), NULL);
        }
    | LIMIT ICONST OFFSET ICONST
        {
            $$ = (Node *)list_make2((Node *)makeIntConst($2),
                                    (Node *)makeIntConst($4));
        }
    | OFFSET ICONST
        {
            $$ = (Node *)list_make2(NULL, (Node *)makeIntConst($2));
        }
    | /* empty */
        {
            $$ = NULL;
        }
    ;

/* ============================================================
 * INSERT 语句
 * ============================================================ */

insert_stmt:
      INSERT INTO table_name VALUES value_list
        {
            InsertStmt *n = makeInsertStmt();
            n->relation = makeRangeVar(NULL, $3);
            n->valuesLists = $5;
            $$ = (Node *)n;
        }
    | INSERT INTO table_name '(' column_list ')' VALUES value_list
        {
            InsertStmt *n = makeInsertStmt();
            n->relation = makeRangeVar(NULL, $3);
            n->cols = $5;
            n->valuesLists = $8;
            $$ = (Node *)n;
        }
    ;

/* 列列表 */
column_list:
      column_name
        {
            $$ = list_make1(makeColumnRef($1));
        }
    | column_list ',' column_name
        {
            $$ = lappend($1, makeColumnRef($3));
        }
    ;

/* 值列表 */
value_list:
      '(' expr_list ')'
        {
            $$ = list_make1($2);
        }
    | value_list ',' '(' expr_list ')'
        {
            $$ = lappend($1, $4);
        }
    ;

/* ============================================================
 * UPDATE 语句
 * ============================================================ */

update_stmt:
      UPDATE table_name SET set_clause_list where_clause
        {
            /* T9：where_clause 可空，合并原两个候选式 */
            UpdateStmt *n = makeUpdateStmt();
            n->relation = makeRangeVar(NULL, $2);
            n->targetList = $4;
            n->whereClause = $5;
            $$ = (Node *)n;
        }
    ;

/* SET 子句列表 */
set_clause_list:
      column_name '=' expr
        {
            $$ = list_make1(makeResTarget($1, $3));
        }
    | set_clause_list ',' column_name '=' expr
        {
            $$ = lappend($1, makeResTarget($3, $5));
        }
    ;

/* ============================================================
 * DELETE 语句
 * ============================================================ */

delete_stmt:
      DELETE FROM table_name where_clause
        {
            /* T9：where_clause 可空，合并原两个候选式 */
            DeleteStmt *n = makeDeleteStmt();
            n->relation = makeRangeVar(NULL, $3);
            n->whereClause = $4;
            $$ = (Node *)n;
        }
    ;

/* ============================================================
 * CREATE TABLE 语句
 * ============================================================ */

create_stmt:
      CREATE TABLE table_name '(' column_def_list ')'
        {
            CreateStmt *n = makeCreateStmt();
            n->relation = makeRangeVar(NULL, $3);
            n->tableElts = $5;
            $$ = (Node *)n;
        }
    ;

/* 列定义列表 */
column_def_list:
      column_def
        {
            $$ = list_make1($1);
        }
    | column_def_list ',' column_def
        {
            $$ = lappend($1, $3);
        }
    ;

/* 列定义 */
column_def:
      column_name data_type
        {
            $$ = (Node *)makeColumnDef($1, (TypeName *)$2);
        }
    | column_name data_type NOT NULL_P
        {
            ColumnDef *n = makeColumnDef($1, (TypeName *)$2);
            n->is_not_null = true;
            $$ = (Node *)n;
        }
    | column_name data_type PRIMARY KEY
        {
            ColumnDef *n = makeColumnDef($1, (TypeName *)$2);
            n->is_not_null = true;
            n->is_primary_key = true;
            $$ = (Node *)n;
        }
    ;

/* 数据类型 */
data_type:
      INT_P
        {
            $$ = (Node *)makeTypeName("int4");
        }
    | BIGINT
        {
            $$ = (Node *)makeTypeName("int8");
        }
    | SMALLINT
        {
            $$ = (Node *)makeTypeName("int2");
        }
    | REAL
        {
            $$ = (Node *)makeTypeName("float4");
        }
    | DOUBLE_P
        {
            $$ = (Node *)makeTypeName("float8");
        }
    | FLOAT_P
        {
            $$ = (Node *)makeTypeName("float4");
        }
    | VARCHAR
        {
            $$ = (Node *)makeTypeName("varchar");
        }
    | CHAR_P
        {
            $$ = (Node *)makeTypeName("char");
        }
    | TEXT_P
        {
            $$ = (Node *)makeTypeName("text");
        }
    | BOOLEAN_P
        {
            $$ = (Node *)makeTypeName("bool");
        }
    | DATE
        {
            $$ = (Node *)makeTypeName("date");
        }
    | TIME
        {
            $$ = (Node *)makeTypeName("time");
        }
    | TIMESTAMP
        {
            $$ = (Node *)makeTypeName("timestamp");
        }
    ;

/* ============================================================
 * DROP 语句
 * ============================================================ */

drop_stmt:
      DROP TABLE table_name
        {
            DropStmt *n = makeDropStmt();
            n->removeType = 1;  /* OBJECT_TABLE */
            n->objects = list_make1(makeString($3));
            n->missing_ok = false;
            $$ = (Node *)n;
        }
    | DROP TABLE IF EXISTS table_name
        {
            DropStmt *n = makeDropStmt();
            n->removeType = 1;  /* OBJECT_TABLE */
            n->objects = list_make1(makeString($5));
            n->missing_ok = true;
            $$ = (Node *)n;
        }
    ;

/* ============================================================
 * 表达式
 * ============================================================ */

/* 表达式列表 */
expr_list:
      expr
        {
            $$ = list_make1($1);
        }
    | expr_list ',' expr
        {
            $$ = lappend($1, $3);
        }
    ;

/* 表达式 */
expr:
      column_ref
    | const_val
    | func_call
    | case_expr
    | '(' expr ')'
        {
            $$ = $2;
        }
    /* 算术运算 */
    | expr '+' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "+", $1, $3);
        }
    | expr '-' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "-", $1, $3);
        }
    | expr '*' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "*", $1, $3);
        }
    | expr '/' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "/", $1, $3);
        }
    | expr '%' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "%", $1, $3);
        }
    /* 比较运算 */
    | expr '<' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "<", $1, $3);
        }
    | expr '>' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, ">", $1, $3);
        }
    | expr '=' expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_OP, "=", $1, $3);
        }
    | expr Op expr
        {
            /* 通用操作符（!=, <=, >= 等） */
            $$ = (Node *)makeA_Expr(AEXPR_OP, sql_yylval.str, $1, $3);
        }
    /* 逻辑运算 */
    | expr AND expr
        {
            $$ = (Node *)makeBoolExpr(AND_EXPR, list_make2($1, $3));
        }
    | expr OR expr
        {
            $$ = (Node *)makeBoolExpr(OR_EXPR, list_make2($1, $3));
        }
    | NOT expr
        {
            $$ = (Node *)makeBoolExpr(NOT_EXPR, list_make1($2));
        }
    /* NULL 测试 */
    | expr IS NULL_P
        {
            $$ = (Node *)makeNullTest(IS_NULL, $1);
        }
    | expr IS NOT NULL_P
        {
            $$ = (Node *)makeNullTest(IS_NOT_NULL, $1);
        }
    /* IN 表达式 */
    | expr IN '(' expr_list ')'
        {
            $$ = (Node *)makeA_Expr(AEXPR_IN, "=", $1, (Node *)$4);
        }
    | expr NOT IN '(' expr_list ')'
        {
            $$ = (Node *)makeA_Expr(AEXPR_IN, "<>", $1, (Node *)$5);
        }
    /* BETWEEN */
    | expr BETWEEN expr AND expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_BETWEEN, "BETWEEN", $1, $5);
        }
    /* LIKE */
    | expr LIKE expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_LIKE, "~~", $1, $3);
        }
    | expr NOT LIKE expr
        {
            $$ = (Node *)makeA_Expr(AEXPR_LIKE, "!~~", $1, $4);
        }
    ;

/* 列引用 */
column_ref:
      IDENT
        {
            $$ = (Node *)makeColumnRef($1);
        }
    | IDENT '.' IDENT
        {
            $$ = (Node *)makeColumnRef2($1, $3);
        }
    ;

/* 常量值 */
const_val:
      ICONST
        {
            $$ = (Node *)makeIntConst($1);
        }
    | FCONST
        {
            $$ = (Node *)makeFloatConst($1);
        }
    | SCONST
        {
            $$ = (Node *)makeStringConst($1);
        }
    | NULL_P
        {
            $$ = (Node *)makeNullConst();
        }
    | TRUE_P
        {
            $$ = (Node *)makeIntConst(1);
        }
    | FALSE_P
        {
            $$ = (Node *)makeIntConst(0);
        }
    ;

/* 函数调用 */
func_call:
      IDENT '(' ')'
        {
            $$ = (Node *)makeFuncCall($1, NIL);
        }
    | IDENT '(' expr_list ')'
        {
            $$ = (Node *)makeFuncCall($1, $3);
        }
    | IDENT '(' DISTINCT expr ')'
        {
            FuncCall *n = makeFuncCall($1, list_make1($4));
            n->agg_distinct = true;
            $$ = (Node *)n;
        }
    | IDENT '(' '*' ')'
        {
            FuncCall *n = makeFuncCall($1, NIL);
            n->agg_star = true;
            $$ = (Node *)n;
        }
    ;

/* CASE 表达式 */
case_expr:
      CASE case_when_list END
        {
            CaseExpr *n = (CaseExpr *)makeNode(T_CaseExpr);
            n->args = $2;
            $$ = (Node *)n;
        }
    | CASE case_when_list ELSE expr END
        {
            CaseExpr *n = (CaseExpr *)makeNode(T_CaseExpr);
            n->args = $2;
            n->defresult = $4;
            $$ = (Node *)n;
        }
    ;

/* CASE WHEN 列表 */
case_when_list:
      WHEN expr THEN expr
        {
            CaseWhen *n = (CaseWhen *)makeNode(T_CaseWhen);
            n->expr = $2;
            n->result = $4;
            $$ = list_make1(n);
        }
    | case_when_list WHEN expr THEN expr
        {
            CaseWhen *n = (CaseWhen *)makeNode(T_CaseWhen);
            n->expr = $3;
            n->result = $5;
            $$ = lappend($1, n);
        }
    ;

/* ============================================================
 * 辅助规则
 * ============================================================ */

/* 表名 */
table_name:
      IDENT
        {
            $$ = $1;
        }
    ;

/* 列名 */
column_name:
      IDENT
        {
            $$ = $1;
        }
    ;

%%
