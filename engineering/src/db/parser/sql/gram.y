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

/* 错误处理 */
void yyerror(const char *msg) {
    fprintf(stderr, "SQL 语法错误: %s (位置: %d:%d)\n",
            msg, sql_yylloc.line, sql_yylloc.column);
}

/* NIL 常量（空列表） */
#define NIL ((List *)NULL)

%}

/* Bison 配置 */
%name-prefix="sql_yy"
%pure-parser
%locations
%defines
%error-verbose

/* ============================================================
 * 语义值类型
 * ============================================================ */

%union {
    Node *node;               /* 通用节点 */
    List *list;               /* 列表 */
    char *str;                /* 字符串 */
    int ival;                 /* 整数 */
    double fval;              /* 浮点数 */
    bool boolval;             /* 布尔值 */
}

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

/* ============================================================
 * 操作符优先级（从低到高）
 * ============================================================ */

/* 赋值和比较 */
%right '='
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

/* 子句 */
%type <list> target_list from_clause where_clause group_clause
%type <list> having_clause order_clause limit_clause
%type <list> column_list value_list set_clause_list
%type <list> column_def_list

/* 表引用 */
%type <node> table_ref join_clause

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

select_stmt:
      SELECT target_list FROM from_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause group_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            n->groupClause = $6;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause group_clause having_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            n->groupClause = $6;
            n->havingClause = $7;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause group_clause having_clause order_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            n->groupClause = $6;
            n->havingClause = $7;
            n->sortClause = $8;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause group_clause having_clause order_clause limit_clause
        {
            SelectStmt *n = makeSelectStmt();
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            n->groupClause = $6;
            n->havingClause = $7;
            n->sortClause = $8;
            n->limitCount = $9;
            $$ = (Node *)n;
        }
    ;

/* 目标列列表 */
target_list:
      '*'
        {
            ColumnRef *n = makeColumnRef("*");
            $$ = list_make1(n);
        }
    | target_list ',' column_ref
        {
            $$ = lappend($1, $3);
        }
    | column_ref
        {
            $$ = list_make1($1);
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
    ;

/* 连接子句 */
join_clause:
      JOIN
    | INNER JOIN
    | OUTER JOIN
    ;

/* WHERE 子句 */
where_clause:
      WHERE expr
        {
            $$ = $2;
        }
    ;

/* GROUP BY 子句 */
group_clause:
      GROUP BY expr_list
        {
            $$ = $3;
        }
    ;

/* HAVING 子句 */
having_clause:
      HAVING expr
        {
            $$ = $2;
        }
    ;

/* ORDER BY 子句 */
order_clause:
      ORDER BY sort_list
        {
            $$ = $3;
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
            $$ = makeSortBy($1, SORTBY_DEFAULT);
        }
    | column_ref ASC
        {
            $$ = makeSortBy($1, SORTBY_ASC);
        }
    | column_ref DESC
        {
            $$ = makeSortBy($1, SORTBY_DESC);
        }
    ;

/* LIMIT 子句 */
limit_clause:
      LIMIT ICONST
        {
            $$ = makeIntConst($2);
        }
    | LIMIT ICONST OFFSET ICONST
        {
            /* 简化处理：只返回 limit 值 */
            $$ = makeIntConst($2);
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
      UPDATE table_name SET set_clause_list
        {
            UpdateStmt *n = makeUpdateStmt();
            n->relation = makeRangeVar(NULL, $2);
            n->targetList = $4;
            $$ = (Node *)n;
        }
    | UPDATE table_name SET set_clause_list where_clause
        {
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
      DELETE FROM table_name
        {
            DeleteStmt *n = makeDeleteStmt();
            n->relation = makeRangeVar(NULL, $3);
            $$ = (Node *)n;
        }
    | DELETE FROM table_name where_clause
        {
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
            $$ = makeColumnDef($1, $2);
        }
    | column_name data_type NOT NULL_P
        {
            ColumnDef *n = makeColumnDef($1, $2);
            n->is_not_null = true;
            $$ = (Node *)n;
        }
    | column_name data_type PRIMARY KEY
        {
            ColumnDef *n = makeColumnDef($1, $2);
            n->is_not_null = true;
            n->is_primary_key = true;
            $$ = (Node *)n;
        }
    ;

/* 数据类型 */
data_type:
      INT_P
        {
            $$ = makeTypeName("int4");
        }
    | BIGINT
        {
            $$ = makeTypeName("int8");
        }
    | SMALLINT
        {
            $$ = makeTypeName("int2");
        }
    | REAL
        {
            $$ = makeTypeName("float4");
        }
    | DOUBLE_P
        {
            $$ = makeTypeName("float8");
        }
    | FLOAT_P
        {
            $$ = makeTypeName("float4");
        }
    | VARCHAR
        {
            $$ = makeTypeName("varchar");
        }
    | CHAR_P
        {
            $$ = makeTypeName("char");
        }
    | TEXT_P
        {
            $$ = makeTypeName("text");
        }
    | BOOLEAN_P
        {
            $$ = makeTypeName("bool");
        }
    | DATE
        {
            $$ = makeTypeName("date");
        }
    | TIME
        {
            $$ = makeTypeName("time");
        }
    | TIMESTAMP
        {
            $$ = makeTypeName("timestamp");
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