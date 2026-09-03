# SQL 引擎实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个完全对标 PostgreSQL 的 SQL 引擎，支持 SQL:2016 标准核心功能，实现 Parser → Semantic → Planner → Executor 完整链路。

**Architecture:** 采用 PostgreSQL 风格的分层架构（Parser + Semantic + Planner + Executor），使用 Flex/Bison 生成词法和语法分析器，Volcano 执行模型，代价驱动的优化器。

**Tech Stack:** C11、Flex 2.6+、Bison 3.0+、CMake 3.20+、GoogleTest

## Global Constraints

- 代码规范：C11 标准，所有注释使用中文
- 构建系统：CMake 3.20+，输出到 `build/engineering/`
- 测试框架：GoogleTest（vendored），测试文件使用 C++ `.cpp`
- Commit Message：使用中文，遵循 Conventional Commits
- 现有代码保留：`sql_lexer.c`/`sql_parser.c`（简化版）保留并作为参考
- 头文件冲突：使用 `db/parser/sql/sql.h` 路径避免系统 sql.h 冲突

---

## 现有实现状态

| 组件 | 现有文件 | 行数 | 状态 |
|------|---------|------|------|
| 简化版词法分析器 | `sql_lexer.c` | 270 | 可用，支持基础 SQL |
| 简化版语法分析器 | `sql_parser.c` | 935 | 可用，支持 SELECT/INSERT/UPDATE/DELETE/CREATE/DROP |
| 完整版解析器 | `sql/sql_parser.c` | 2067 | 框架，支持更多语法 |
| 执行器框架 | `sql_executor.c` | 752 | 框架，函数体为 TODO |
| 计划器头文件 | `sql_planner.h` | 664 | 定义完成，无实现 |
| 测试 | `sql_parser.cpp` | 615 | 覆盖简化版解析器 |

**设计文档**: `docs/superpowers/specs/2026-07-14-sql-engine-design.md`（~4100 行）

---

## Phase 1: Parser + Semantic Analyzer

> 目标：实现完整的 SQL:2016 词法分析、语法分析和语义分析，生成 PostgreSQL 风格的 Query 结构。

### Task 1: Flex 词法分析器（scan.l）

**Files:**
- Create: `engineering/src/db/parser/sql/scan.l`
- Create: `engineering/include/db/parser/sql/parsenodes.h`（部分，Token 定义）
- Modify: `engineering/src/db/parser/sql/CMakeLists.txt`

**Interfaces:**
- Produces: `sql_yylex()` 函数，`yylval` 语义值，`yylloc` 位置信息

**Step 1: 编写 Flex 词法规则**

```flex
/* scan.l - SQL 词法分析器 */

%{
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/gram.h"
#include <string.h>

#define YYLLOC_DEFAULT(Current, Rhs, N) \
    do { \
        if (N) { \
            (Current).line = YYRHSLOC(Rhs, 1).line; \
            (Current).column = YYRHSLOC(Rhs, 1).column; \
        } else { \
            (Current).line = YYRHSLOC(Rhs, 0).line; \
            (Current).column = YYRHSLOC(Rhs, 0).column; \
        } \
    } while (0)

%}

%option prefix="sql_yy"
%option noyywrap
%option batch
%option pointer
%option yylineno

/* 字符集 */
UCN    \\u[0-9a-fA-F]{4}
IDI    [A-Za-z_ -￿]
IDC    [A-Za-z_0-9 -￿]
IDENT  {IDI}{IDC}*

/* 数字 */
DIGIT  [0-9]
HEX    [0-9a-fA-F]
INT    {DIGIT}+
HEXNUM 0[xX]{HEX}+

/* 字符串 */
STRING '([^'\\\n]|\\.|'')*'

%%

/* 空白和注释 */
[ \t\r\n]+    { /* 忽略 */ }
"--"[^\n]*    { /* 行注释 */ }
"/*"([^*]|\*+[^*/])*\*+"/"  { /* 块注释 */ }

/* 关键字 */
"SELECT"      { return SELECT; }
"FROM"        { return FROM; }
"WHERE"       { return WHERE; }
"INSERT"      { return INSERT; }
"INTO"        { return INTO; }
"VALUES"      { return VALUES; }
"UPDATE"      { return UPDATE; }
"SET"         { return SET; }
"DELETE"      { return DELETE; }
"CREATE"      { return CREATE; }
"DROP"        { return DROP; }
"TABLE"       { return TABLE; }
"INDEX"       { return INDEX; }
"VIEW"        { return VIEW; }
"AND"         { return AND; }
"OR"          { return OR; }
"NOT"         { return NOT; }
"NULL"        { return NULL_P; }
"IS"          { return IS; }
"IN"          { return IN; }
"LIKE"        { return LIKE; }
"BETWEEN"     { return BETWEEN; }
"CASE"        { return CASE; }
"WHEN"        { return WHEN; }
"THEN"        { return THEN; }
"ELSE"        { return ELSE; }
"END"         { return END; }
"GROUP"       { return GROUP; }
"BY"          { return BY; }
"HAVING"      { return HAVING; }
"ORDER"       { return ORDER; }
"ASC"         { return ASC; }
"DESC"        { return DESC; }
"LIMIT"       { return LIMIT; }
"OFFSET"      { return OFFSET; }
"JOIN"        { return JOIN; }
"LEFT"        { return LEFT; }
"RIGHT"       { return RIGHT; }
"FULL"        { return FULL; }
"INNER"       { return INNER; }
"OUTER"       { return OUTER; }
"CROSS"       { return CROSS; }
"ON"          { return ON; }
"AS"          { return AS; }
"DISTINCT"    { return DISTINCT; }
"ALL"         { return ALL; }
"UNION"       { return UNION; }
"INTERSECT"   { return INTERSECT; }
"EXCEPT"      { return EXCEPT; }
"WITH"        { return WITH; }
"RECURSIVE"   { return RECURSIVE; }

/* 数据类型 */
"INT"         { return INT; }
"INTEGER"     { return INT; }
"BIGINT"      { return BIGINT; }
"SMALLINT"    { return SMALLINT; }
"REAL"        { return REAL; }
"DOUBLE"      { return DOUBLE; }
"FLOAT"       { return FLOAT; }
"DECIMAL"     { return DECIMAL; }
"NUMERIC"     { return NUMERIC; }
"VARCHAR"     { return VARCHAR; }
"CHAR"        { return CHAR; }
"TEXT"        { return TEXT; }
"BOOLEAN"     { return BOOLEAN; }
"BOOL"        { return BOOLEAN; }
"DATE"        { return DATE; }
"TIME"        { return TIME; }
"TIMESTAMP"   { return TIMESTAMP; }

/* 操作符 */
"="           { return '='; }
"<>"          { yylval.ival = NE; return Op; }
"!="          { yylval.ival = NE; return Op; }
"<="          { yylval.ival = LE; return Op; }
">="          { yylval.ival = GE; return Op; }
"<"           { return '<'; }
">"           { return '>'; }
"+"           { return '+'; }
"-"           { return '-'; }
"*"           { return '*'; }
"/"           { return '/'; }
"%"           { return '%'; }
"||"          { return Op; }
"->"          { return Op; }
"->>"         { return Op; }

/* 分隔符 */
"("           { return '('; }
")"           { return ')'; }
","           { return ','; }
";"           { return ';'; }
"."           { return '.'; }

/* 标识符 */
{IDENT}       {
                  yylval.str = yytext;
                  return IDENT;
              }

/* 数字 */
{INT}         {
                  yylval.ival = atoi(yytext);
                  return ICONST;
              }

{HEXNUM}      {
                  yylval.str = yytext;
                  return FCONST;
              }

/* 字符串 */
{STRING}      {
                  /* 移除引号 */
                  int len = strlen(yytext);
                  yylval.str = malloc(len - 1);
                  strncpy(yylval.str, yytext + 1, len - 2);
                  yylval.str[len - 2] = '\0';
                  return SCONST;
              }

.             { return yytext[0]; }

%%

int yywrap(void) {
    return 1;
}
```

- [ ] **Step 2: 更新 CMakeLists.txt 添加 Flex 构建**

```cmake
# engineering/src/db/parser/sql/CMakeLists.txt

# Flex 词法分析器
FLEX_TARGET(sql_lexer scan.l ${CMAKE_CURRENT_BINARY_DIR}/lex.sql.c)

# 原有源文件（保留简化版）
set(SQL_PARSER_SIMPLIFIED_SOURCES
    sql_lexer.c
    sql_parser.c
)

# 完整版源文件（新增）
set(SQL_PARSER_FULL_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/lex.sql.c
    parser.c
    keywords.c
    kwlookup.c
)

# 创建库
add_library(db_parser_simplified STATIC ${SQL_PARSER_SIMPLIFIED_SOURCES})
target_link_libraries(db_parser_simplified project_includes)

add_library(db_parser_full STATIC ${SQL_PARSER_FULL_SOURCES})
target_link_libraries(db_parser_full project_includes)
```

- [ ] **Step 3: 编写 Token 定义头文件（部分）**

```c
/* engineering/include/db/parser/sql/parsenodes.h */

#ifndef DB_PARSER_SQL_PARSENODES_H
#define DB_PARSER_SQL_PARSENODES_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Token 类型定义（Bison 生成）
 * ============================================================ */

/* 操作符常量 */
#define NE  1
#define LE  2
#define GE  3

/* ============================================================
 * AST 节点标签
 * ============================================================ */

typedef enum NodeTag {
    T_Node = 0,
    T_List,
    T_IntList,
    T_OidList,
    T_Value,
    T_Integer,
    T_Float,
    T_String,
    T_Null,
    
    /* DML */
    T_SelectStmt,
    T_InsertStmt,
    T_UpdateStmt,
    T_DeleteStmt,
    T_MergeStmt,
    
    /* DDL */
    T_CreateStmt,
    T_CreateIndexStmt,
    T_CreateViewStmt,
    T_DropStmt,
    T_AlterStmt,
    
    /* 表达式 */
    T_A_Expr,
    T_ColumnRef,
    T_A_Const,
    T_FuncCall,
    T_A_Indirection,
    T_TypeCast,
    T_CaseExpr,
    T_SubLink,
    
    /* 子句 */
    T_ResTarget,
    T_RangeVar,
    T_JoinExpr,
    T_SortBy,
    T_GroupClause,
    T_WindowDef,
    
    /* 其他 */
    T_RawStmt,
    T_CommentStmt,
    T_VariableSetStmt,
} NodeTag;

/* ============================================================
 * 基础节点结构
 * ============================================================ */

typedef struct Node {
    NodeTag type;
} Node;

typedef struct Value {
    NodeTag type;
    union {
        int ival;
        double fval;
        char *str;
    } val;
} Value;

typedef struct Integer {
    NodeTag type;
    int ival;
} Integer;

typedef struct Float {
    NodeTag type;
    double fval;
} Float;

typedef struct String {
    NodeTag type;
    char *str;
} String;

typedef struct Null {
    NodeTag type;
} Null;

/* ============================================================
 * 列表结构
 * ============================================================ */

typedef struct List {
    NodeTag type;
    int length;
    ListCell *head;
} List;

typedef struct ListCell {
    void *data;
    struct ListCell *next;
} ListCell;

/* 列表操作宏 */
#define lfirst(lc)      ((lc)->data)
#define lnext(lc)       ((lc)->next)
#define list_length(l)  ((l)->length)

/* ============================================================
 * 位置信息
 * ============================================================ */

typedef struct YYLTYPE {
    int line;
    int column;
} YYLTYPE;

#endif /* DB_PARSER_SQL_PARSENODES_H */
```

- [ ] **Step 4: 验证 Flex 编译**

```bash
cd build/engineering
cmake --build . --target db_parser_full
```

Expected: Flex 生成 `lex.sql.c`，编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/parser/sql/scan.l
git add engineering/include/db/parser/sql/parsenodes.h
git add engineering/src/db/parser/sql/CMakeLists.txt
git commit -m "feat(sql): 添加 Flex 词法分析器基础结构"
```

---

### Task 2: Bison 语法分析器（gram.y）- 基础框架

**Files:**
- Create: `engineering/src/db/parser/sql/gram.y`

**Interfaces:**
- Produces: `sql_yyparse()` 函数，返回 `SelectStmt`/`InsertStmt` 等 AST 根节点

**Step 1: 编写 Bison 语法规则框架**

```yacc
/* gram.y - SQL 语法分析器 */

%{
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/parser.h"
#include <stdio.h>
#include <stdlib.h>

extern int sql_yylex(void);
extern char *sql_yytext;
extern int sql_yylloc;

static Node *parsetree;

void yyerror(const char *msg) {
    fprintf(stderr, "语法错误: %s\n", msg);
}

%}

%name-prefix="sql_yy"
%pure-parser
%locations

/* 语义值类型 */
%union {
    Node *node;
    List *list;
    char *str;
    int ival;
    double fval;
}

/* Token 声明 */
%token <str> IDENT SCONST
%token <ival> ICONST
%token <fval> FCONST

/* 关键字 */
%token SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE
%token CREATE DROP TABLE INDEX VIEW
%token AND OR NOT NULL_P IS IN LIKE BETWEEN
%token GROUP BY HAVING ORDER ASC DESC LIMIT OFFSET
%token JOIN LEFT RIGHT FULL INNER OUTER CROSS ON AS
%token DISTINCT ALL UNION INTERSECT EXCEPT
%token WITH RECURSIVE CASE WHEN THEN ELSE END
%token INT BIGINT SMALLINT REAL DOUBLE FLOAT DECIMAL NUMERIC VARCHAR CHAR TEXT BOOLEAN DATE TIME TIMESTAMP

/* 操作符优先级 */
%right '='
%left OR
%left AND
%right NOT
%left '<' '>' '=' Op
%left '+' '-'
%left '*' '/' '%'
%left UMINUS

/* 非终结符类型 */
%type <node> stmt select_stmt insert_stmt update_stmt delete_stmt
%type <node> create_stmt drop_stmt
%type <node> table_ref where_clause group_clause having_clause order_clause limit_clause
%type <node> expr column_ref const_val func_call case_expr
%type <list> target_list from_clause join_clause column_list value_list expr_list
%type <str> table_name column_name

/* 起始符号 */
%start stmt

%%

/* ============================================================
 * 语句规则
 * ============================================================ */

stmt:
      select_stmt
    | insert_stmt
    | update_stmt
    | delete_stmt
    | create_stmt
    | drop_stmt
    ;

stmt_list:
      stmt
    | stmt_list ';' stmt
    ;

/* ============================================================
 * SELECT 语句
 * ============================================================ */

select_stmt:
      SELECT target_list FROM from_clause
        {
            SelectStmt *n = makeNode(SelectStmt);
            n->targetList = $2;
            n->fromClause = $4;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause
        {
            SelectStmt *n = makeNode(SelectStmt);
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause group_clause having_clause
        {
            SelectStmt *n = makeNode(SelectStmt);
            n->targetList = $2;
            n->fromClause = $4;
            n->whereClause = $5;
            n->groupClause = $6;
            n->havingClause = $7;
            $$ = (Node *)n;
        }
    | SELECT target_list FROM from_clause where_clause group_clause having_clause order_clause
        {
            SelectStmt *n = makeNode(SelectStmt);
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
            SelectStmt *n = makeNode(SelectStmt);
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

target_list:
      '*'
        {
            $$ = makeStarTarget();
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

table_ref:
      table_name
        {
            RangeVar *n = makeNode(RangeVar);
            n->relname = $1;
            $$ = (Node *)n;
        }
    | table_name AS IDENT
        {
            RangeVar *n = makeNode(RangeVar);
            n->relname = $1;
            n->alias = $3;
            $$ = (Node *)n;
        }
    | table_ref join_clause table_ref ON expr
        {
            JoinExpr *n = makeNode(JoinExpr);
            n->jointype = JOIN_INNER;
            n->larg = $1;
            n->rarg = $3;
            n->quals = $5;
            $$ = (Node *)n;
        }
    | table_ref LEFT JOIN table_ref ON expr
        {
            JoinExpr *n = makeNode(JoinExpr);
            n->jointype = JOIN_LEFT;
            n->larg = $1;
            n->rarg = $4;
            n->quals = $6;
            $$ = (Node *)n;
        }
    | '(' select_stmt ')' AS IDENT
        {
            RangeSubselect *n = makeNode(RangeSubselect);
            n->subquery = $2;
            n->alias = $5;
            $$ = (Node *)n;
        }
    ;

join_clause:
      JOIN
    | INNER JOIN
    | CROSS JOIN
    ;

where_clause:
      WHERE expr
        {
            $$ = $2;
        }
    ;

group_clause:
      GROUP BY expr_list
        {
            $$ = $3;
        }
    ;

having_clause:
      HAVING expr
        {
            $$ = $2;
        }
    ;

order_clause:
      ORDER BY sort_list
        {
            $$ = $3;
        }
    ;

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

sort_item:
      column_ref
        {
            SortBy *n = makeNode(SortBy);
            n->node = $1;
            n->sortby_dir = SORTBY_DEFAULT;
            $$ = (Node *)n;
        }
    | column_ref ASC
        {
            SortBy *n = makeNode(SortBy);
            n->node = $1;
            n->sortby_dir = SORTBY_ASC;
            $$ = (Node *)n;
        }
    | column_ref DESC
        {
            SortBy *n = makeNode(SortBy);
            n->node = $1;
            n->sortby_dir = SORTBY_DESC;
            $$ = (Node *)n;
        }
    ;

limit_clause:
      LIMIT ICONST
        {
            $$ = makeIntConst($2);
        }
    | LIMIT ICONST OFFSET ICONST
        {
            $$ = makeLimitOffset($2, $4);
        }
    ;

/* ============================================================
 * INSERT 语句
 * ============================================================ */

insert_stmt:
      INSERT INTO table_name VALUES value_list
        {
            InsertStmt *n = makeNode(InsertStmt);
            n->relation = makeRangeVar(NULL, $3);
            n->valuesLists = $5;
            $$ = (Node *)n;
        }
    | INSERT INTO table_name '(' column_list ')' VALUES value_list
        {
            InsertStmt *n = makeNode(InsertStmt);
            n->relation = makeRangeVar(NULL, $3);
            n->cols = $5;
            n->valuesLists = $8;
            $$ = (Node *)n;
        }
    ;

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
            UpdateStmt *n = makeNode(UpdateStmt);
            n->relation = makeRangeVar(NULL, $2);
            n->targetList = $4;
            $$ = (Node *)n;
        }
    | UPDATE table_name SET set_clause_list where_clause
        {
            UpdateStmt *n = makeNode(UpdateStmt);
            n->relation = makeRangeVar(NULL, $2);
            n->targetList = $4;
            n->whereClause = $5;
            $$ = (Node *)n;
        }
    ;

set_clause_list:
      set_clause
        {
            $$ = list_make1($1);
        }
    | set_clause_list ',' set_clause
        {
            $$ = lappend($1, $3);
        }
    ;

set_clause:
      column_name '=' expr
        {
            ResTarget *n = makeNode(ResTarget);
            n->name = $1;
            n->val = $3;
            $$ = (Node *)n;
        }
    ;

/* ============================================================
 * DELETE 语句
 * ============================================================ */

delete_stmt:
      DELETE FROM table_name
        {
            DeleteStmt *n = makeNode(DeleteStmt);
            n->relation = makeRangeVar(NULL, $3);
            $$ = (Node *)n;
        }
    | DELETE FROM table_name where_clause
        {
            DeleteStmt *n = makeNode(DeleteStmt);
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
            CreateStmt *n = makeNode(CreateStmt);
            n->relation = makeRangeVar(NULL, $3);
            n->tableElts = $5;
            $$ = (Node *)n;
        }
    ;

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

column_def:
      column_name data_type
        {
            ColumnDef *n = makeNode(ColumnDef);
            n->colname = $1;
            n->typeName = $2;
            $$ = (Node *)n;
        }
    | column_name data_type NOT NULL_P
        {
            ColumnDef *n = makeNode(ColumnDef);
            n->colname = $1;
            n->typeName = $2;
            n->is_not_null = true;
            $$ = (Node *)n;
        }
    | column_name data_type PRIMARY KEY
        {
            ColumnDef *n = makeNode(ColumnDef);
            n->colname = $1;
            n->typeName = $2;
            n->is_not_null = true;
            n->is_primary_key = true;
            $$ = (Node *)n;
        }
    ;

data_type:
      INT
        {
            $$ = makeTypeName("int4");
        }
    | BIGINT
        {
            $$ = makeTypeName("int8");
        }
    | VARCHAR
        {
            $$ = makeTypeName("varchar");
        }
    | TEXT
        {
            $$ = makeTypeName("text");
        }
    ;

/* ============================================================
 * DROP 语句
 * ============================================================ */

drop_stmt:
      DROP TABLE table_name
        {
            DropStmt *n = makeNode(DropStmt);
            n->removeType = OBJECT_TABLE;
            n->objects = list_make1(makeString($3));
            $$ = (Node *)n;
        }
    | DROP TABLE IF EXISTS table_name
        {
            DropStmt *n = makeNode(DropStmt);
            n->removeType = OBJECT_TABLE;
            n->objects = list_make1(makeString($5));
            n->missing_ok = true;
            $$ = (Node *)n;
        }
    ;

/* ============================================================
 * 表达式
 * ============================================================ */

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

expr:
      column_ref
    | const_val
    | func_call
    | case_expr
    | '(' expr ')'
        {
            $$ = $2;
        }
    | expr '+' expr
        {
            $$ = makeA_Expr(AEXPR_OP, "+", $1, $3);
        }
    | expr '-' expr
        {
            $$ = makeA_Expr(AEXPR_OP, "-", $1, $3);
        }
    | expr '*' expr
        {
            $$ = makeA_Expr(AEXPR_OP, "*", $1, $3);
        }
    | expr '/' expr
        {
            $$ = makeA_Expr(AEXPR_OP, "/", $1, $3);
        }
    | expr '<' expr
        {
            $$ = makeA_Expr(AEXPR_OP, "<", $1, $3);
        }
    | expr '>' expr
        {
            $$ = makeA_Expr(AEXPR_OP, ">", $1, $3);
        }
    | expr '=' expr
        {
            $$ = makeA_Expr(AEXPR_OP, "=", $1, $3);
        }
    | expr AND expr
        {
            $$ = makeBoolExpr(AND_EXPR, list_make2($1, $3));
        }
    | expr OR expr
        {
            $$ = makeBoolExpr(OR_EXPR, list_make2($1, $3));
        }
    | NOT expr
        {
            $$ = makeBoolExpr(NOT_EXPR, list_make1($2));
        }
    | expr IS NULL_P
        {
            $$ = makeNullTest(IS_NULL, $1);
        }
    | expr IS NOT NULL_P
        {
            $$ = makeNullTest(IS_NOT_NULL, $1);
        }
    | expr IN '(' expr_list ')'
        {
            $$ = makeScalarArrayOp("=", true, $1, $4);
        }
    | expr NOT IN '(' expr_list ')'
        {
            $$ = makeScalarArrayOp("<>", true, $1, $5);
        }
    | expr BETWEEN expr AND expr
        {
            $$ = makeBetweenExpr($1, $3, $5);
        }
    | expr LIKE expr
        {
            $$ = makeA_Expr(AEXPR_OP, "~~", $1, $3);
        }
    ;

column_ref:
      IDENT
        {
            $$ = makeColumnRef($1);
        }
    | IDENT '.' IDENT
        {
            $$ = makeColumnRef2($1, $3);
        }
    ;

const_val:
      ICONST
        {
            $$ = makeIntConst($1);
        }
    | FCONST
        {
            $$ = makeFloatConst($1);
        }
    | SCONST
        {
            $$ = makeStringConst($1);
        }
    | NULL_P
        {
            $$ = makeNullConst();
        }
    ;

func_call:
      IDENT '(' ')'
        {
            $$ = makeFuncCall($1, NIL);
        }
    | IDENT '(' expr_list ')'
        {
            $$ = makeFuncCall($1, $3);
        }
    | IDENT '(' DISTINCT expr ')'
        {
            $$ = makeAggRef($1, $4, true);
        }
    ;

case_expr:
      CASE case_when_list END
        {
            CaseExpr *n = makeNode(CaseExpr);
            n->args = $2;
            $$ = (Node *)n;
        }
    | CASE case_when_list ELSE expr END
        {
            CaseExpr *n = makeNode(CaseExpr);
            n->args = $2;
            n->defresult = $4;
            $$ = (Node *)n;
        }
    ;

case_when_list:
      WHEN expr THEN expr
        {
            CaseWhen *n = makeNode(CaseWhen);
            n->expr = $2;
            n->result = $4;
            $$ = list_make1(n);
        }
    | case_when_list WHEN expr THEN expr
        {
            CaseWhen *n = makeNode(CaseWhen);
            n->expr = $3;
            n->result = $5;
            $$ = lappend($1, n);
        }
    ;

/* ============================================================
 * 辅助规则
 * ============================================================ */

table_name:
      IDENT
        {
            $$ = $1;
        }
    ;

column_name:
      IDENT
        {
            $$ = $1;
        }
    ;

%%
```

- [ ] **Step 2: 更新 CMakeLists.txt 添加 Bison 构建**

```cmake
# 在 engineering/src/db/parser/sql/CMakeLists.txt 中添加

# Bison 语法分析器
BISON_TARGET(sql_parser gram.y ${CMAKE_CURRENT_BINARY_DIR}/gram.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/gram.h)

# 更新完整版源文件列表
set(SQL_PARSER_FULL_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/lex.sql.c
    ${CMAKE_CURRENT_BINARY_DIR}/gram.c
    parser.c
    keywords.c
    kwlookup.c
    makefuncs.c
    value.c
)
```

- [ ] **Step 3: 编写节点构造函数**

```c
/* engineering/src/db/parser/sql/makefuncs.c */

#include "db/parser/sql/parsenodes.h"
#include <stdlib.h>
#include <string.h>

Node *makeNode(NodeTag type) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    if (n) n->type = type;
    return n;
}

SelectStmt *makeSelectStmt(void) {
    SelectStmt *n = (SelectStmt *)calloc(1, sizeof(SelectStmt));
    if (n) n->type = T_SelectStmt;
    return n;
}

InsertStmt *makeInsertStmt(void) {
    InsertStmt *n = (InsertStmt *)calloc(1, sizeof(InsertStmt));
    if (n) n->type = T_InsertStmt;
    return n;
}

UpdateStmt *makeUpdateStmt(void) {
    UpdateStmt *n = (UpdateStmt *)calloc(1, sizeof(UpdateStmt));
    if (n) n->type = T_UpdateStmt;
    return n;
}

DeleteStmt *makeDeleteStmt(void) {
    DeleteStmt *n = (DeleteStmt *)calloc(1, sizeof(DeleteStmt));
    if (n) n->type = T_DeleteStmt;
    return n;
}

CreateStmt *makeCreateStmt(void) {
    CreateStmt *n = (CreateStmt *)calloc(1, sizeof(CreateStmt));
    if (n) n->type = T_CreateStmt;
    return n;
}

DropStmt *makeDropStmt(void) {
    DropStmt *n = (DropStmt *)calloc(1, sizeof(DropStmt));
    if (n) n->type = T_DropStmt;
    return n;
}

RangeVar *makeRangeVar(const char *schemaname, const char *relname) {
    RangeVar *n = (RangeVar *)calloc(1, sizeof(RangeVar));
    if (n) {
        n->type = T_RangeVar;
        n->schemaname = schemaname ? strdup(schemaname) : NULL;
        n->relname = strdup(relname);
    }
    return n;
}

ColumnRef *makeColumnRef(const char *colname) {
    ColumnRef *n = (ColumnRef *)calloc(1, sizeof(ColumnRef));
    if (n) {
        n->type = T_ColumnRef;
        n->fields = list_make1(makeString(colname));
    }
    return n;
}

ColumnRef *makeColumnRef2(const char *tablename, const char *colname) {
    ColumnRef *n = (ColumnRef *)calloc(1, sizeof(ColumnRef));
    if (n) {
        n->type = T_ColumnRef;
        n->fields = list_make2(makeString(tablename), makeString(colname));
    }
    return n;
}

A_Expr *makeA_Expr(A_Expr_Kind kind, const char *name, Node *lexpr, Node *rexpr) {
    A_Expr *n = (A_Expr *)calloc(1, sizeof(A_Expr));
    if (n) {
        n->type = T_A_Expr;
        n->kind = kind;
        n->name = list_make1(makeString(name));
        n->lexpr = lexpr;
        n->rexpr = rexpr;
    }
    return n;
}

A_Const *makeIntConst(int val) {
    A_Const *n = (A_Const *)calloc(1, sizeof(A_Const));
    if (n) {
        n->type = T_A_Const;
        n->val.val.ival = val;
    }
    return n;
}

A_Const *makeFloatConst(double val) {
    A_Const *n = (A_Const *)calloc(1, sizeof(A_Const));
    if (n) {
        n->type = T_A_Const;
        n->val.val.fval = val;
    }
    return n;
}

A_Const *makeStringConst(const char *val) {
    A_Const *n = (A_Const *)calloc(1, sizeof(A_Const));
    if (n) {
        n->type = T_A_Const;
        n->val.val.str = strdup(val);
    }
    return n;
}

A_Const *makeNullConst(void) {
    A_Const *n = (A_Const *)calloc(1, sizeof(A_Const));
    if (n) {
        n->type = T_A_Const;
        n->isnull = true;
    }
    return n;
}

FuncCall *makeFuncCall(const char *funcname, List *args) {
    FuncCall *n = (FuncCall *)calloc(1, sizeof(FuncCall));
    if (n) {
        n->type = T_FuncCall;
        n->funcname = list_make1(makeString(funcname));
        n->args = args;
    }
    return n;
}

List *list_make1(void *data) {
    List *l = (List *)malloc(sizeof(List));
    ListCell *lc = (ListCell *)malloc(sizeof(ListCell));
    if (l && lc) {
        l->type = T_List;
        l->length = 1;
        lc->data = data;
        lc->next = NULL;
        l->head = lc;
    }
    return l;
}

List *list_make2(void *d1, void *d2) {
    List *l = list_make1(d1);
    return lappend(l, d2);
}

List *lappend(List *l, void *data) {
    if (!l) return list_make1(data);
    
    ListCell *lc = (ListCell *)malloc(sizeof(ListCell));
    if (lc) {
        lc->data = data;
        lc->next = NULL;
        
        ListCell *tail = l->head;
        while (tail->next) tail = tail->next;
        tail->next = lc;
        l->length++;
    }
    return l;
}

String *makeString(const char *str) {
    String *s = (String *)malloc(sizeof(String));
    if (s) {
        s->type = T_String;
        s->str = strdup(str);
    }
    return s;
}
```

- [ ] **Step 4: 验证 Bison 编译**

```bash
cd build/engineering
cmake --build . --target db_parser_full
```

Expected: Bison 生成 `gram.c`/`gram.h`，编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/parser/sql/gram.y
git add engineering/src/db/parser/sql/makefuncs.c
git commit -m "feat(sql): 添加 Bison 语法分析器基础框架"
```

---

### Task 3: AST 节点完整定义（parsenodes.h）

**Files:**
- Modify: `engineering/include/db/parser/sql/parsenodes.h`

**Interfaces:**
- Produces: 所有 AST 节点结构（SelectStmt、InsertStmt、UpdateStmt、DeleteStmt、CreateStmt、DropStmt、A_Expr、ColumnRef、FuncCall 等）

**Step 1: 扩展 AST 节点定义**

```c
/* 在 engineering/include/db/parser/sql/parsenodes.h 中继续添加 */

/* ============================================================
 * DML 语句节点
 * ============================================================ */

typedef struct SelectStmt {
    NodeTag type;
    
    /* SELECT 子句 */
    List *targetList;       /* 目标列列表 */
    List *intoClause;       /* INTO 子句（SELECT INTO） */
    
    /* FROM 子句 */
    List *fromClause;       /* FROM 表引用列表 */
    
    /* WHERE 子句 */
    Node *whereClause;      /* WHERE 条件 */
    
    /* GROUP BY 子句 */
    List *groupClause;      /* GROUP BY 列表 */
    Node *havingClause;     /* HAVING 条件 */
    
    /* WINDOW 子句 */
    List *windowClause;     /* WINDOW 定义 */
    
    /* ORDER BY 子句 */
    List *sortClause;       /* ORDER BY 列表 */
    
    /* LIMIT/OFFSET */
    Node *limitCount;       /* LIMIT 值 */
    Node *limitOffset;      /* OFFSET 值 */
    
    /* 集合操作 */
    List *withClause;       /* WITH 子句 */
    SelectStmt *larg;       /* 左操作数（UNION/INTERSECT/EXCEPT） */
    SelectStmt *rarg;       /* 右操作数 */
    SetOperation op;        /* 集合操作类型 */
    bool all;               /* 是否 ALL */
    
    /* 其他 */
    bool distinctClause;    /* DISTINCT */
} SelectStmt;

typedef struct InsertStmt {
    NodeTag type;
    RangeVar *relation;     /* 目标表 */
    List *cols;             /* 列名列表 */
    List *valuesLists;      /* VALUES 列表 */
    SelectStmt *selectStmt; /* SELECT 子查询 */
    List *returningList;    /* RETURNING 列表 */
    List *withClause;       /* WITH 子句 */
} InsertStmt;

typedef struct UpdateStmt {
    NodeTag type;
    RangeVar *relation;     /* 目标表 */
    List *targetList;       /* SET 列表 */
    List *fromClause;       /* FROM 子句 */
    Node *whereClause;      /* WHERE 条件 */
    List *returningList;    /* RETURNING 列表 */
    List *withClause;       /* WITH 子句 */
} UpdateStmt;

typedef struct DeleteStmt {
    NodeTag type;
    RangeVar *relation;     /* 目标表 */
    List *usingClause;      /* USING 子句 */
    Node *whereClause;      /* WHERE 条件 */
    List *returningList;    /* RETURNING 列表 */
    List *withClause;       /* WITH 子句 */
} DeleteStmt;

/* ============================================================
 * DDL 语句节点
 * ============================================================ */

typedef struct CreateStmt {
    NodeTag type;
    RangeVar *relation;     /* 表名 */
    List *tableElts;        /* 列定义/约束列表 */
    List *ofTypename;       /* OF 类型名（typed table） */
    List *constraints;      /* 约束列表 */
    List *options;          /* 表选项 */
    char *accessMethod;     /* 访问方法 */
    char *tablespacename;   /* 表空间名 */
    char *relpersistence;   /* 持久性类型 */
    bool if_not_exists;     /* IF NOT EXISTS */
} CreateStmt;

typedef struct DropStmt {
    NodeTag type;
    ObjectType removeType;  /* 对象类型 */
    List *objects;          /* 对象名列表 */
    bool missing_ok;        /* IF EXISTS */
    bool concurrent;        /* CONCURRENT（索引） */
} DropStmt;

/* ============================================================
 * 表引用节点
 * ============================================================ */

typedef struct RangeVar {
    NodeTag type;
    char *catalogname;      /* 数据库名 */
    char *schemaname;       /* schema 名 */
    char *relname;          /* 表名 */
    char *alias;            /* 别名 */
    bool inh;               /* 是否继承子表 */
    bool relIsExpr;         /* 是否为表达式 */
    char relPersistence;    /* 持久性：'p'=永久,'t'=临时,'u'=非日志 */
} RangeVar;

typedef struct JoinExpr {
    NodeTag type;
    JoinType jointype;      /* 连接类型 */
    Node *larg;             /* 左表 */
    Node *rarg;             /* 右表 */
    Node *quals;            /* 连接条件 */
    List *usingClause;      /* USING 列表 */
    char *alias;            /* 别名 */
} JoinExpr;

/* ============================================================
 * 表达式节点
 * ============================================================ */

typedef enum A_Expr_Kind {
    AEXPR_OP,               /* 普通操作符 */
    AEXPR_OP_ANY,           /* ANY 操作符 */
    AEXPR_IN,               /* IN 表达式 */
    AEXPR_LIKE,             /* LIKE 表达式 */
    AEXPR_ILIKE,            /* ILIKE 表达式 */
    AEXPR_SIMILAR,          /* SIMILAR TO */
    AEXPR_BETWEEN,          /* BETWEEN */
    AEXPR_NOT_BETWEEN,      /* NOT BETWEEN */
    AEXPR_NULLIF,           /* NULLIF */
    AEXPR_COALESCE,         /* COALESCE */
} A_Expr_Kind;

typedef struct A_Expr {
    NodeTag type;
    A_Expr_Kind kind;       /* 表达式类型 */
    List *name;             /* 操作符名 */
    Node *lexpr;            /* 左操作数 */
    Node *rexpr;            /* 右操作数 */
    int location;           /* 位置 */
} A_Expr;

typedef struct ColumnRef {
    NodeTag type;
    List *fields;           /* 列名列表（可能含表名前缀） */
    int location;
} ColumnRef;

typedef struct A_Const {
    NodeTag type;
    Value val;              /* 值 */
    bool isnull;            /* 是否为 NULL */
    int location;
} A_Const;

typedef struct FuncCall {
    NodeTag type;
    List *funcname;         /* 函数名 */
    List *args;             /* 参数列表 */
    List *agg_order;        /* 聚合内 ORDER BY */
    List *agg_filter;       /* FILTER 子句 */
    List *over;             /* OVER 子句 */
    bool agg_distinct;      /* DISTINCT */
    bool func_variadic;     /* VARIADIC */
    struct WindowDef *over; /* 窗口定义 */
    int location;
} FuncCall;

typedef struct TypeCast {
    NodeTag type;
    Node *arg;              /* 待转换表达式 */
    TypeName *typeName;     /* 目标类型 */
    int location;
} TypeCast;

typedef struct CaseExpr {
    NodeTag type;
    Node *arg;              /* CASE 表达式（简单 CASE） */
    List *args;             /* WHEN 列表 */
    Node *defresult;        /* ELSE 结果 */
    int location;
} CaseExpr;

typedef struct CaseWhen {
    NodeTag type;
    Node *expr;             /* WHEN 条件 */
    Node *result;           /* THEN 结果 */
    int location;
} CaseWhen;

/* ============================================================
 * 目标列节点
 * ============================================================ */

typedef struct ResTarget {
    NodeTag type;
    char *name;             /* 列别名 */
    List *indirection;      /* 下标/字段访问 */
    Node *val;              /* 表达式值 */
    int location;
} ResTarget;

typedef struct SortBy {
    NodeTag type;
    Node *node;             /* 排序表达式 */
    SortByDir sortby_dir;   /* ASC/DESC/DEFAULT */
    SortByNulls sortby_nulls;/* NULLS FIRST/LAST */
    List *useOp;            /* 排序操作符 */
    int location;
} SortBy;

/* ============================================================
 * 类型名节点
 * ============================================================ */

typedef struct TypeName {
    NodeTag type;
    List *names;            /* 类型名列表 */
    List *typmods;          /* 类型修饰符 */
    int typemod;            /* 类型修饰符值 */
    bool array_bounds;      /* 是否为数组 */
    bool setof;             /* 是否为集合 */
    bool pct_type;          /* %TYPE 语法 */
    int location;
} TypeName;

typedef struct ColumnDef {
    NodeTag type;
    char *colname;          /* 列名 */
    TypeName *typeName;     /* 类型 */
    char *colnamespace;     /* 列命名空间 */
    List *constraints;      /* 列约束 */
    bool is_not_null;       /* NOT NULL */
    bool is_primary_key;    /* PRIMARY KEY */
    bool is_unique;         /* UNIQUE */
    Node *default_expr;     /* DEFAULT 表达式 */
    int location;
} ColumnDef;

/* ============================================================
 * 枚举类型
 * ============================================================ */

typedef enum JoinType {
    JOIN_INNER = 0,
    JOIN_LEFT,
    JOIN_RIGHT,
    JOIN_FULL,
    JOIN_CROSS,
    JOIN_SEMI,
    JOIN_ANTI,
} JoinType;

typedef enum SetOperation {
    SETOP_NONE = 0,
    SETOP_UNION,
    SETOP_INTERSECT,
    SETOP_EXCEPT,
} SetOperation;

typedef enum ObjectType {
    OBJECT_TABLE,
    OBJECT_INDEX,
    OBJECT_VIEW,
    OBJECT_FUNCTION,
    OBJECT_SCHEMA,
} ObjectType;

typedef enum SortByDir {
    SORTBY_DEFAULT,
    SORTBY_ASC,
    SORTBY_DESC,
} SortByDir;

typedef enum SortByNulls {
    SORTBY_NULLS_DEFAULT,
    SORTBY_NULLS_FIRST,
    SORTBY_NULLS_LAST,
} SortByNulls;

typedef enum BoolExprType {
    AND_EXPR,
    OR_EXPR,
    NOT_EXPR,
} BoolExprType;

typedef enum NullTestType {
    IS_NULL,
    IS_NOT_NULL,
} NullTestType;
```

- [ ] **Step 2: 验证编译**

```bash
cd build/engineering
cmake --build . --target db_parser_full
```

- [ ] **Step 3: 提交**

```bash
git add engineering/include/db/parser/sql/parsenodes.h
git commit -m "feat(sql): 完善 AST 节点定义"
```

---

### Task 4: 语义分析器框架

**Files:**
- Create: `engineering/src/db/parser/sql/parse_analyze.c`
- Create: `engineering/include/db/parser/sql/parse_node.h`
- Create: `engineering/src/db/parser/sql/parse_node.c`

**Interfaces:**
- Consumes: AST 根节点（SelectStmt/InsertStmt 等）
- Produces: Query 结构（已解析的语义表示）

**Step 1: 定义 ParseState 结构**

```c
/* engineering/include/db/parser/sql/parse_node.h */

#ifndef DB_PARSER_SQL_PARSE_NODE_H
#define DB_PARSER_SQL_PARSE_NODE_H

#include "db/parser/sql/parsenodes.h"
#include "db/catalog.h"

/* ============================================================
 * ParseState - 解析状态
 * ============================================================ */

typedef struct ParseState {
    ParseState *parentParseState;   /* 父解析状态（子查询） */
    
    /* 查询环境 */
    List *p_rtable;                 /* 范围表列表 */
    List *p_joinexprs;              /* 连接表达式列表 */
    List *p_namespace;              /* 命名空间（可见列） */
    
    /* 目标列表 */
    List *p_targetlist;             /* 目标列列表 */
    
    /* 参数 */
    List *p_paramtypes;             /* 参数类型列表 */
    int p_numparams;                /* 参数数量 */
    
    /* CTE */
    List *p_ctenamespace;           /* CTE 命名空间 */
    
    /* 聚合和窗口函数 */
    List *p_aggs;                   /* 聚合函数列表 */
    List *p_groupClause;            /* GROUP BY 列表 */
    bool p_hasAggs;                 /* 是否有聚合函数 */
    bool p_hasWindowFuncs;          /* 是否有窗口函数 */
    
    /* 子链接 */
    List *p_sublinks;               /* 子链接列表 */
    
    /* 错误处理 */
    char *p_error_msg;              /* 错误消息 */
    int p_error_location;           /* 错误位置 */
    
    /* Catalog 引用 */
    catalog_t *p_catalog;           /* Catalog 句柄 */
} ParseState;

/* ============================================================
 * Query - 已解析的查询结构
 * ============================================================ */

typedef struct Query {
    NodeTag type;
    
    /* 命令类型 */
    CmdType commandType;            /* CMD_SELECT/INSERT/UPDATE/DELETE/UTILITY */
    
    /* 查询 ID */
    uint64_t queryId;               /* 查询标识符 */
    
    /* 范围表 */
    List *rtable;                   /* RangeTblEntry 列表 */
    
    /* 目标列表 */
    List *targetList;               /* TargetEntry 列表 */
    
    /* 连接信息 */
    List *jointree;                 /* 连接树 */
    
    /* WHERE 条件 */
    Node *jointree->quals;          /* WHERE 条件表达式 */
    
    /* GROUP BY */
    List *groupClause;              /* GROUP BY 列表 */
    Node *havingQual;               /* HAVING 条件 */
    
    /* WINDOW */
    List *windowClause;             /* WINDOW 定义 */
    
    /* ORDER BY */
    List *sortClause;               /* ORDER BY 列表 */
    
    /* LIMIT/OFFSET */
    Node *limitOffset;              /* OFFSET 值 */
    Node *limitCount;               /* LIMIT 值 */
    
    /* 集合操作 */
    SetOperation setOperations;     /* UNION/INTERSECT/EXCEPT */
    
    /* 结果关系（DML） */
    Index resultRelation;           /* 结果表的 rtable 索引 */
    List *returningList;            /* RETURNING 列表 */
    
    /* CTE */
    List *cteList;                  /* WITH 子句 */
    
    /* 其他 */
    List *constraintDeps;           /* 依赖的约束 */
} Query;

/* ============================================================
 * RangeTblEntry - 范围表项
 * ============================================================ */

typedef struct RangeTblEntry {
    NodeTag type;
    
    /* 基本信息 */
    Index rtekind;                  /* RTE 类型 */
    Oid relid;                      /* 表 OID（RTE_RELATION） */
    char *relname;                  /* 表名 */
    char *refname;                  /* 别名 */
    
    /* 列信息 */
    List *eref;                     /* 列别名列表 */
    List *lateral_vars;             /* LATERAL 变量 */
    bool lateral;                   /* 是否 LATERAL */
    
    /* 权限 */
    int32_t rellockmode;            /* 锁模式 */
    
    /* 子查询信息 */
    Query *subquery;                /* 子查询（RTE_SUBQUERY） */
    
    /* 函数信息 */
    Node *funcexpr;                 /* 函数表达式（RTE_FUNCTION） */
    
    /* 连接信息 */
    JoinType jointype;              /* 连接类型（RTE_JOIN） */
    List *joinaliasvars;            /* 连接列别名 */
    
    /* CTE 信息 */
    char *ctename;                  /* CTE 名称（RTE_CTE） */
    Index ctelevelsup;              /* CTE 层级 */
    
    /* 位置 */
    Index rtepermissions;           /* 权限索引 */
    Index perminfoindex;            /* 权限信息索引 */
} RangeTblEntry;

/* ============================================================
 * TargetEntry - 目标列项
 * ============================================================ */

typedef struct TargetEntry {
    NodeTag type;
    
    Expr *expr;                     /* 表达式 */
    AttrNumber resno;               /* 结果列编号 */
    char *resname;                  /* 结果列名 */
    Index ressortgroupref;          /* 排序/分组引用 */
    Oid resorigtbl;                 /* 原始表 OID */
    AttrNumber resorigcol;          /* 原始列编号 */
    bool resjunk;                   /* 是否为隐藏列 */
} TargetEntry;

/* ============================================================
 * 枚举类型
 * ============================================================ */

typedef enum CmdType {
    CMD_UNKNOWN,
    CMD_SELECT,
    CMD_INSERT,
    CMD_UPDATE,
    CMD_DELETE,
    CMD_UTILITY,
} CmdType;

typedef enum RTEKind {
    RTE_RELATION,                   /* 表 */
    RTE_SUBQUERY,                   /* 子查询 */
    RTE_JOIN,                       /* 连接 */
    RTE_FUNCTION,                   /* 函数 */
    RTE_VALUES,                     /* VALUES 子句 */
    RTE_CTE,                        /* CTE */
    RTE_NAMEDTUPLESTORE,            /* 命名元组存储 */
} RTEKind;

/* ============================================================
 * 函数声明
 * ============================================================ */

/* 创建/销毁 ParseState */
ParseState *make_parsestate(ParseState *parentParseState);
void free_parsestate(ParseState *pstate);

/* 语义分析主入口 */
Query *parse_analyze(Node *parseTree, const char *sourceText, 
                     Oid *paramTypes, int numParams);

/* 语句分析 */
Query *transformStmt(ParseState *pstate, Node *stmt);
Query *transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
Query *transformInsertStmt(ParseState *pstate, InsertStmt *stmt);
Query *transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
Query *transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
Query *transformCreateStmt(ParseState *pstate, CreateStmt *stmt);

/* 表达式分析 */
Node *transformExpr(ParseState *pstate, Node *expr, ParseExprKind exprKind);

/* 子句分析 */
List *transformFromClause(ParseState *pstate, List *frmList);
List *transformTargetList(ParseState *pstate, List *targetlist);
Node *transformWhereClause(ParseState *pstate, Node *whereClause);

/* RangeTblEntry 操作 */
RangeTblEntry *addRangeTableEntry(ParseState *pstate, RangeVar *relation,
                                   Alias *alias, bool inh, bool inFromCl);
RangeTblEntry *addRangeTableEntryForSubquery(ParseState *pstate, Query *subquery,
                                              Alias *alias, bool lateral, bool inFromCl);

/* 列解析 */
void markVarForSelectPriv(ParseState *pstate, Var *var);
Node *scanNameSpaceForRefname(ParseState *pstate, const char *refname, int location);

#endif /* DB_PARSER_SQL_PARSE_NODE_H */
```

- [ ] **Step 2: 实现语义分析主入口**

```c
/* engineering/src/db/parser/sql/parse_analyze.c */

#include "db/parser/sql/parse_node.h"
#include "db/parser/sql/parsenodes.h"
#include <stdlib.h>
#include <string.h>

Query *parse_analyze(Node *parseTree, const char *sourceText,
                     Oid *paramTypes, int numParams) {
    ParseState *pstate = make_parsestate(NULL);
    Query *query;
    
    pstate->p_paramtypes = paramTypes;
    pstate->p_numparams = numParams;
    
    query = transformStmt(pstate, parseTree);
    
    free_parsestate(pstate);
    return query;
}

Query *transformStmt(ParseState *pstate, Node *stmt) {
    Query *result = NULL;
    
    switch (nodeTag(stmt)) {
        case T_SelectStmt:
            result = transformSelectStmt(pstate, (SelectStmt *)stmt);
            break;
        case T_InsertStmt:
            result = transformInsertStmt(pstate, (InsertStmt *)stmt);
            break;
        case T_UpdateStmt:
            result = transformUpdateStmt(pstate, (UpdateStmt *)stmt);
            break;
        case T_DeleteStmt:
            result = transformDeleteStmt(pstate, (DeleteStmt *)stmt);
            break;
        case T_CreateStmt:
            result = transformCreateStmt(pstate, (CreateStmt *)stmt);
            break;
        case T_DropStmt:
            result = transformDropStmt(pstate, (DropStmt *)stmt);
            break;
        default:
            elog(ERROR, "不支持的语句类型: %d", nodeTag(stmt));
    }
    
    return result;
}

Query *transformSelectStmt(ParseState *pstate, SelectStmt *stmt) {
    Query *qry = makeNode(Query);
    
    qry->commandType = CMD_SELECT;
    
    /* 处理 WITH 子句 */
    if (stmt->withClause) {
        qry->cteList = transformWithClause(pstate, stmt->withClause);
    }
    
    /* 处理 FROM 子句 */
    if (stmt->fromClause) {
        qry->rtable = transformFromClause(pstate, stmt->fromClause);
    }
    
    /* 处理目标列表 */
    qry->targetList = transformTargetList(pstate, stmt->targetList);
    
    /* 处理 WHERE 子句 */
    if (stmt->whereClause) {
        qry->jointree->quals = transformWhereClause(pstate, stmt->whereClause);
    }
    
    /* 处理 GROUP BY 子句 */
    if (stmt->groupClause) {
        qry->groupClause = transformGroupClause(pstate, stmt->groupClause);
    }
    
    /* 处理 HAVING 子句 */
    if (stmt->havingClause) {
        qry->havingQual = transformWhereClause(pstate, stmt->havingClause);
    }
    
    /* 处理 WINDOW 子句 */
    if (stmt->windowClause) {
        qry->windowClause = transformWindowClause(pstate, stmt->windowClause);
    }
    
    /* 处理 ORDER BY 子句 */
    if (stmt->sortClause) {
        qry->sortClause = transformSortClause(pstate, stmt->sortClause);
    }
    
    /* 处理 LIMIT/OFFSET */
    qry->limitOffset = stmt->limitOffset;
    qry->limitCount = stmt->limitCount;
    
    return qry;
}

Query *transformInsertStmt(ParseState *pstate, InsertStmt *stmt) {
    Query *qry = makeNode(Query);
    
    qry->commandType = CMD_INSERT;
    
    /* 添加目标表到范围表 */
    RangeTblEntry *rte = addRangeTableEntry(pstate, stmt->relation,
                                             NULL, false, true);
    qry->rtable = list_make1(rte);
    qry->resultRelation = 1;
    
    /* 处理列列表 */
    List *columns = NIL;
    if (stmt->cols) {
        columns = transformColumnList(pstate, stmt->cols);
    }
    
    /* 处理 VALUES 子句 */
    if (stmt->valuesLists) {
        List *valuesLists = transformValuesClause(pstate, stmt->valuesLists);
        qry->targetList = valuesLists;
    }
    
    /* 处理 SELECT 子查询 */
    if (stmt->selectStmt) {
        Query *subquery = transformSelectStmt(pstate, stmt->selectStmt);
        /* 合并子查询到 INSERT */
    }
    
    return qry;
}

Query *transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt) {
    Query *qry = makeNode(Query);
    
    qry->commandType = CMD_UPDATE;
    
    /* 添加目标表到范围表 */
    RangeTblEntry *rte = addRangeTableEntry(pstate, stmt->relation,
                                             NULL, false, true);
    qry->rtable = list_make1(rte);
    qry->resultRelation = 1;
    
    /* 处理 SET 子句 */
    qry->targetList = transformTargetList(pstate, stmt->targetList);
    
    /* 处理 WHERE 子句 */
    if (stmt->whereClause) {
        qry->jointree->quals = transformWhereClause(pstate, stmt->whereClause);
    }
    
    return qry;
}

Query *transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt) {
    Query *qry = makeNode(Query);
    
    qry->commandType = CMD_DELETE;
    
    /* 添加目标表到范围表 */
    RangeTblEntry *rte = addRangeTableEntry(pstate, stmt->relation,
                                             NULL, false, true);
    qry->rtable = list_make1(rte);
    qry->resultRelation = 1;
    
    /* 处理 WHERE 子句 */
    if (stmt->whereClause) {
        qry->jointree->quals = transformWhereClause(pstate, stmt->whereClause);
    }
    
    return qry;
}

Query *transformCreateStmt(ParseState *pstate, CreateStmt *stmt) {
    Query *qry = makeNode(Query);
    
    qry->commandType = CMD_UTILITY;
    
    /* 创建 DDL 命令 */
    qry->utilityStmt = (Node *)stmt;
    
    return qry;
}
```

- [ ] **Step 3: 实现 ParseState 辅助函数**

```c
/* engineering/src/db/parser/sql/parse_node.c */

#include "db/parser/sql/parse_node.h"
#include <stdlib.h>
#include <string.h>

ParseState *make_parsestate(ParseState *parentParseState) {
    ParseState *pstate = (ParseState *)calloc(1, sizeof(ParseState));
    
    if (pstate) {
        pstate->parentParseState = parentParseState;
        pstate->p_rtable = NIL;
        pstate->p_joinexprs = NIL;
        pstate->p_namespace = NIL;
        pstate->p_targetlist = NIL;
        pstate->p_paramtypes = NULL;
        pstate->p_numparams = 0;
        pstate->p_ctenamespace = NIL;
        pstate->p_aggs = NIL;
        pstate->p_groupClause = NIL;
        pstate->p_hasAggs = false;
        pstate->p_hasWindowFuncs = false;
        pstate->p_sublinks = NIL;
        pstate->p_error_msg = NULL;
        pstate->p_error_location = 0;
        
        if (parentParseState) {
            pstate->p_catalog = parentParseState->p_catalog;
        }
    }
    
    return pstate;
}

void free_parsestate(ParseState *pstate) {
    if (pstate) {
        if (pstate->p_error_msg) {
            free(pstate->p_error_msg);
        }
        free(pstate);
    }
}

RangeTblEntry *addRangeTableEntry(ParseState *pstate, RangeVar *relation,
                                   Alias *alias, bool inh, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));
    
    if (rte) {
        rte->type = T_RangeTblEntry;
        rte->rtekind = RTE_RELATION;
        
        /* 查找表的 OID */
        if (pstate->p_catalog) {
            rte->relid = catalog_relation_oid(pstate->p_catalog,
                                              relation->relname);
        } else {
            rte->relid = InvalidOid;
        }
        
        rte->relname = relation->relname ? strdup(relation->relname) : NULL;
        rte->refname = alias ? strdup(alias->aliasname) : 
                       relation->relname ? strdup(relation->relname) : NULL;
        rte->inh = inh;
        
        /* 获取列信息 */
        if (OidIsValid(rte->relid)) {
            rte->eref = buildRelationAliases(rte->relid);
        }
        
        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }
    
    return rte;
}

RangeTblEntry *addRangeTableEntryForSubquery(ParseState *pstate, Query *subquery,
                                              Alias *alias, bool lateral, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));
    
    if (rte) {
        rte->type = T_RangeTblEntry;
        rte->rtekind = RTE_SUBQUERY;
        rte->subquery = subquery;
        rte->refname = alias ? strdup(alias->aliasname) : NULL;
        rte->lateral = lateral;
        
        /* 构建列别名列表 */
        rte->eref = buildSubqueryAliases(subquery);
        
        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }
    
    return rte;
}
```

- [ ] **Step 4: 验证编译**

```bash
cd build/engineering
cmake --build . --target db_parser_full
```

- [ ] **Step 5: 提交**

```bash
git add engineering/include/db/parser/sql/parse_node.h
git add engineering/src/db/parser/sql/parse_analyze.c
git add engineering/src/db/parser/sql/parse_node.c
git commit -m "feat(sql): 添加语义分析器框架"
```

---

### Task 5: 表达式语义分析

**Files:**
- Create: `engineering/src/db/parser/sql/parse_expr.c`

**Interfaces:**
- Consumes: 原始表达式节点（A_Expr/ColumnRef/FuncCall/A_Const）
- Produces: 已解析表达式（OpExpr/Var/Const/FuncExpr）

**Step 1: 实现表达式转换**

```c
/* engineering/src/db/parser/sql/parse_expr.c */

#include "db/parser/sql/parse_node.h"
#include "db/parser/sql/parsenodes.h"
#include <stdlib.h>
#include <string.h>

Node *transformExpr(ParseState *pstate, Node *expr, ParseExprKind exprKind) {
    if (!expr) return NULL;
    
    switch (nodeTag(expr)) {
        case T_ColumnRef:
            return transformColumnRef(pstate, (ColumnRef *)expr);
        case T_A_Const:
            return transformConst(pstate, (A_Const *)expr);
        case T_A_Expr:
            return transformA_Expr(pstate, (A_Expr *)expr);
        case T_FuncCall:
            return transformFuncCall(pstate, (FuncCall *)expr);
        case T_TypeCast:
            return transformTypeCast(pstate, (TypeCast *)expr);
        case T_CaseExpr:
            return transformCaseExpr(pstate, (CaseExpr *)expr);
        case T_SubLink:
            return transformSubLink(pstate, (SubLink *)expr);
        default:
            elog(ERROR, "不支持的表达式类型: %d", nodeTag(expr));
    }
    
    return NULL;
}

Node *transformColumnRef(ParseState *pstate, ColumnRef *cref) {
    Var *var = makeNode(Var);
    
    /* 解析列名 */
    ListCell *lc;
    char *nspname = NULL;
    char *relname = NULL;
    char *colname = NULL;
    
    int nnames = list_length(cref->fields);
    switch (nnames) {
        case 1:
            colname = strVal(linitial(cref->fields));
            break;
        case 2:
            relname = strVal(linitial(cref->fields));
            colname = strVal(lsecond(cref->fields));
            break;
        case 3:
            nspname = strVal(linitial(cref->fields));
            relname = strVal(lsecond(cref->fields));
            colname = strVal(lthird(cref->fields));
            break;
        default:
            elog(ERROR, "无效的列引用");
    }
    
    /* 在范围表中查找 */
    RangeTblEntry *rte = NULL;
    int varno = 0;
    
    if (relname) {
        /* 有表名前缀 */
        rte = findRTEByName(pstate, relname);
        if (!rte) {
            elog(ERROR, "表 \"%s\" 不存在", relname);
        }
        varno = getRTEIndex(pstate, rte);
    } else {
        /* 无表名前缀，在可见列中查找 */
        rte = findRTEByColname(pstate, colname);
        if (!rte) {
            elog(ERROR, "列 \"%s\" 不存在", colname);
        }
        varno = getRTEIndex(pstate, rte);
    }
    
    /* 查找列编号 */
    int varattno = findColumnInRTE(rte, colname);
    if (varattno == 0) {
        elog(ERROR, "列 \"%s\" 不存在", colname);
    }
    
    var->varno = varno;
    var->varattno = varattno;
    var->vartype = getColumnType(rte, varattno);
    var->varnoold = varno;
    var->varoattno = varattno;
    
    return (Node *)var;
}

Node *transformConst(ParseState *pstate, A_Const *con) {
    Const *c = makeNode(Const);
    
    c->constisnull = con->isnull;
    
    if (!con->isnull) {
        switch (con->val.type) {
            case T_Integer:
                c->constvalue = Int32GetDatum(con->val.val.ival);
                c->consttype = INT4OID;
                c->constlen = 4;
                break;
            case T_Float:
                c->constvalue = Float8GetDatum(con->val.val.fval);
                c->consttype = FLOAT8OID;
                c->constlen = 8;
                break;
            case T_String:
                c->constvalue = CStringGetDatum(con->val.val.str);
                c->consttype = TEXTOID;
                c->constlen = -1;
                break;
            default:
                elog(ERROR, "不支持的常量类型");
        }
    } else {
        c->consttype = UNKNOWNOID;
        c->constlen = 0;
    }
    
    return (Node *)c;
}

Node *transformA_Expr(ParseState *pstate, A_Expr *a) {
    Node *lexpr = transformExpr(pstate, a->lexpr, EXPR_KIND_WHERE);
    Node *rexpr = transformExpr(pstate, a->rexpr, EXPR_KIND_WHERE);
    
    /* 查找操作符 OID */
    char *opname = strVal(linitial(a->name));
    Oid opno = findOperator(opname, exprType(lexpr), exprType(rexpr));
    
    OpExpr *result = makeNode(OpExpr);
    result->opno = opno;
    result->opfuncid = get_opcode(opno);
    result->args = list_make2(lexpr, rexpr);
    result->location = a->location;
    
    return (Node *)result;
}

Node *transformFuncCall(ParseState *pstate, FuncCall *fn) {
    /* 解析函数名 */
    char *funcname = strVal(linitial(fn->funcname));
    
    /* 转换参数 */
    List *args = NIL;
    ListCell *lc;
    foreach (lc, fn->args) {
        Node *arg = transformExpr(pstate, lfirst(lc), EXPR_KIND_FUNCTION);
        args = lappend(args, arg);
    }
    
    /* 查找函数 OID */
    Oid funcid = findFunction(funcname, args);
    
    /* 判断是否为聚合函数 */
    if (fn->agg_distinct || fn->agg_order || fn->over) {
        /* 聚合函数 */
        Aggref *agg = makeNode(Aggref);
        agg->aggfnoid = funcid;
        agg->args = args;
        agg->aggdistinct = fn->agg_distinct;
        agg->aggorder = fn->agg_order;
        agg->aggfilter = fn->agg_filter;
        
        pstate->p_hasAggs = true;
        pstate->p_aggs = lappend(pstate->p_aggs, agg);
        
        return (Node *)agg;
    } else {
        /* 普通函数 */
        FuncExpr *func = makeNode(FuncExpr);
        func->funcid = funcid;
        func->args = args;
        
        return (Node *)func;
    }
}

Node *transformCaseExpr(ParseState *pstate, CaseExpr *c) {
    CaseExpr *caseExpr = makeNode(CaseExpr);
    
    caseExpr->arg = transformExpr(pstate, c->arg, EXPR_KIND_CASE);
    
    /* 转换 WHEN 列表 */
    List *args = NIL;
    ListCell *lc;
    foreach (lc, c->args) {
        CaseWhen *w = (CaseWhen *)lfirst(lc);
        CaseWhen *neww = makeNode(CaseWhen);
        neww->expr = transformExpr(pstate, w->expr, EXPR_KIND_CASE);
        neww->result = transformExpr(pstate, w->result, EXPR_KIND_CASE);
        args = lappend(args, neww);
    }
    caseExpr->args = args;
    
    /* 转换 ELSE */
    caseExpr->defresult = transformExpr(pstate, c->defresult, EXPR_KIND_CASE);
    
    return (Node *)caseExpr;
}
```

- [ ] **Step 2: 验证编译**

```bash
cd build/engineering
cmake --build . --target db_parser_full
```

- [ ] **Step 3: 提交**

```bash
git add engineering/src/db/parser/sql/parse_expr.c
git commit -m "feat(sql): 添加表达式语义分析"
```

---

### Task 6: 解析器测试

**Files:**
- Create: `engineering/test/db/parser/sql/CMakeLists.txt`
- Create: `engineering/test/db/parser/sql/test_parser.cpp`

**Interfaces:**
- Tests: 词法分析器 Token 生成
- Tests: 语法分析器 AST 结构
- Tests: 语义分析器 Query 结构

**Step 1: 编写测试**

```cpp
/* engineering/test/db/parser/sql/test_parser.cpp */

#include <gtest/gtest.h>
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/parse_node.h"

extern "C" {
    #include "db/parser/sql/parser.h"
}

class SQLParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================
 * 词法分析器测试
 * ============================================================ */

TEST_F(SQLParserTest, Lexer_SelectKeyword) {
    const char *sql = "SELECT";
    /* 调用词法分析器 */
    /* 验证返回 SELECT token */
}

TEST_F(SQLParserTest, Lexer_Identifier) {
    const char *sql = "users";
    /* 验证返回 IDENT token，lexeme = "users" */
}

TEST_F(SQLParserTest, Lexer_Integer) {
    const char *sql = "12345";
    /* 验证返回 ICONST token，value = 12345 */
}

TEST_F(SQLParserTest, Lexer_String) {
    const char *sql = "'hello world'";
    /* 验证返回 SCONST token，value = "hello world" */
}

/* ============================================================
 * 语法分析器测试
 * ============================================================ */

TEST_F(SQLParserTest, Parse_SelectStar) {
    const char *sql = "SELECT * FROM users";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_SelectStmt);
    
    SelectStmt *sel = (SelectStmt *)node;
    EXPECT_EQ(list_length(sel->targetList), 1);
    EXPECT_NE(sel->fromClause, nullptr);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_SelectColumns) {
    const char *sql = "SELECT id, name FROM users";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_SelectStmt);
    
    SelectStmt *sel = (SelectStmt *)node;
    EXPECT_EQ(list_length(sel->targetList), 2);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_SelectWhere) {
    const char *sql = "SELECT * FROM users WHERE id = 1";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    SelectStmt *sel = (SelectStmt *)node;
    EXPECT_NE(sel->whereClause, nullptr);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_Insert) {
    const char *sql = "INSERT INTO users VALUES (1, 'Alice')";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_InsertStmt);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_Update) {
    const char *sql = "UPDATE users SET name = 'Bob' WHERE id = 1";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_UpdateStmt);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_Delete) {
    const char *sql = "DELETE FROM users WHERE id = 1";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_DeleteStmt);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_CreateTable) {
    const char *sql = "CREATE TABLE users (id INT, name VARCHAR(100))";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_CreateStmt);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_DropTable) {
    const char *sql = "DROP TABLE users";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, T_DropStmt);
    
    free(node);
}

/* ============================================================
 * 语义分析器测试
 * ============================================================ */

TEST_F(SQLParserTest, Semantic_SelectQuery) {
    const char *sql = "SELECT id, name FROM users WHERE id > 10";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    
    Query *query = parse_analyze(node, sql, NULL, 0);
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->commandType, CMD_SELECT);
    EXPECT_NE(query->rtable, nullptr);
    EXPECT_NE(query->targetList, nullptr);
    
    free(node);
    free(query);
}

/* ============================================================
 * 复杂查询测试
 * ============================================================ */

TEST_F(SQLParserTest, Parse_Join) {
    const char *sql = "SELECT * FROM orders JOIN users ON orders.user_id = users.id";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    SelectStmt *sel = (SelectStmt *)node;
    EXPECT_NE(sel->fromClause, nullptr);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_GroupBy) {
    const char *sql = "SELECT COUNT(*) FROM users GROUP BY status";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    SelectStmt *sel = (SelectStmt *)node;
    EXPECT_NE(sel->groupClause, nullptr);
    
    free(node);
}

TEST_F(SQLParserTest, Parse_OrderBy) {
    const char *sql = "SELECT * FROM users ORDER BY id DESC LIMIT 10";
    Node *node = sql_parse_one(sql);
    
    ASSERT_NE(node, nullptr);
    SelectStmt *sel = (SelectStmt *)node;
    EXPECT_NE(sel->sortClause, nullptr);
    EXPECT_NE(sel->limitCount, nullptr);
    
    free(node);
}
```

- [ ] **Step 2: 创建 CMakeLists.txt**

```cmake
# engineering/test/db/parser/sql/CMakeLists.txt

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser
    db_parser_full
    project_includes
    gtest
    gtest_main
)

add_test(NAME test_parser COMMAND test_parser)
```

- [ ] **Step 3: 运行测试**

```bash
cd build/engineering
cmake --build . --target test_parser
ctest --test-dir build/engineering -R test_parser --output-on-failure
```

Expected: 所有测试通过

- [ ] **Step 4: 提交**

```bash
git add engineering/test/db/parser/sql/
git commit -m "test(sql): 添加解析器单元测试"
```

---

## Phase 2: Planner + Optimizer

> 目标：实现代价驱动的查询优化器，生成最优执行计划。

### Task 7: 计划器基础结构

**Files:**
- Create: `engineering/src/db/planner/planner.c`
- Create: `engineering/include/db/planner/planner.h`
- Create: `engineering/src/db/planner/CMakeLists.txt`

**Interfaces:**
- Consumes: Query 结构（来自语义分析）
- Produces: Plan 树（SeqScan/IndexScan/HashJoin 等）

**Step 1: 定义 PlannerInfo 结构**

```c
/* engineering/include/db/planner/planner.h */

#ifndef DB_PLANNER_PLANNER_H
#define DB_PLANNER_PLANNER_H

#include "db/parser/sql/parse_node.h"
#include "db/catalog.h"

/* ============================================================
 * PlannerInfo - 计划器全局信息
 * ============================================================ */

typedef struct PlannerInfo {
    NodeTag type;
    
    /* 输入查询 */
    Query *parse;                   /* Query 结构 */
    
    /* 范围表信息 */
    List *base_rtes;                /* 基础范围表项 */
    int simple_rel_array_size;      /* 关系数组大小 */
    struct RelOptInfo **simple_rel_array; /* RelOptInfo 数组 */
    
    /* 连接信息 */
    List *join_info_list;           /* SpecialJoinInfo 列表 */
    List *eq_classes;               /* EquivalenceClass 列表 */
    
    /* 代价参数 */
    struct CostParams *cost_params; /* 代价参数 */
    
    /* 配置 */
    bool enable_seqscan;
    bool enable_indexscan;
    bool enable_hashjoin;
    bool enable_nestloop;
    bool enable_mergejoin;
    bool enable_hashagg;
    bool enable_sort;
    
    /* 统计信息 */
    double tuple_fraction;          /* 预期返回元组比例 */
    double limit_tuples;            /* LIMIT 元组数 */
} PlannerInfo;

/* ============================================================
 * RelOptInfo - 关系优化信息
 * ============================================================ */

typedef struct RelOptInfo {
    NodeTag type;
    
    /* 基本信息 */
    Index relid;                    /* 范围表索引 */
    RelOptKind reloptkind;          /* 关系类型 */
    
    /* 统计信息 */
    double rows;                    /* 估计行数 */
    int rel pages;                  /* 页面数 */
    int reltuples;                  /* 元组数 */
    int width;                      /* 平均宽度 */
    
    /* 限制条件 */
    List *baserestrictinfo;         /* 基础限制条件 */
    List *joininfo;                 /* 连接条件 */
    
    /* 路径列表 */
    List *pathlist;                 /* Path 列表 */
    Path *cheapest_startup_path;    /* 启动代价最小路径 */
    Path *cheapest_total_path;      /* 总代价最小路径 */
    Path *cheapest_unique_path;     /* 唯一代价最小路径 */
    
    /* 索引信息 */
    List *indexlist;                /* IndexOptInfo 列表 */
    
    /* 连接信息 */
    List *lateral_relids;           /* LATERAL 关系 */
    List *direct_lateral_relids;    /* 直接 LATERAL 关系 */
} RelOptInfo;

/* ============================================================
 * Path - 执行路径
 * ============================================================ */

typedef struct Path {
    NodeTag type;
    
    /* 代价 */
    Cost startup_cost;              /* 启动代价 */
    Cost total_cost;                /* 总代价 */
    
    /* 行数 */
    double rows;                    /* 估计行数 */
    int width;                      /* 平均宽度 */
    
    /* 关系 */
    RelOptInfo *parent;             /* 父关系 */
    
    /* 路径键 */
    List *pathkeys;                 /* 排序键 */
} Path;

typedef struct SeqPath {
    Path path;
} SeqPath;

typedef struct IndexPath {
    Path path;
    IndexOptInfo *indexinfo;        /* 索引信息 */
    List *indexclauses;             /* 索引条件 */
    List *indexquals;               /* 索引限定条件 */
    List *indexorderbys;            /* 索引排序条件 */
} IndexPath;

typedef struct BitmapHeapPath {
    Path path;
    Path *bitmapqual;               /* 位图条件 */
} BitmapHeapPath;

typedef struct JoinPath {
    Path path;
    JoinType jointype;              /* 连接类型 */
    Path *outerjoinpath;            /* 外表路径 */
    Path *innerjoinpath;            /* 内表路径 */
    List *joinrestrictinfo;         /* 连接限制条件 */
} JoinPath;

typedef struct NestLoopPath {
    JoinPath jpath;
} NestLoopPath;

typedef struct HashPath {
    JoinPath jpath;
    List *path_hashclauses;         /* Hash 条件 */
    int num_batches;                /* 批次数 */
    int num_buckets;                /* 桶数 */
} HashPath;

typedef struct MergePath {
    JoinPath jpath;
    List *path_mergeclauses;        /* Merge 条件 */
    List *outersortkeys;            /* 外表排序键 */
    List *innersortkeys;            /* 内表排序键 */
} MergePath;

typedef struct AggPath {
    Path path;
    Path *subpath;                  /* 子路径 */
    List *groupClause;              /* GROUP BY */
    List *targetList;               /* 目标列表 */
    AggStrategy aggstrategy;        /* 聚合策略 */
} AggPath;

typedef struct SortPath {
    Path path;
    Path *subpath;                  /* 子路径 */
    List *sortClause;               /* ORDER BY */
} SortPath;

typedef struct LimitPath {
    Path path;
    Path *subpath;                  /* 子路径 */
    Node *limitOffset;              /* OFFSET */
    Node *limitCount;               /* LIMIT */
} LimitPath;

/* ============================================================
 * 代价参数
 * ============================================================ */

typedef struct CostParams {
    double seq_page_cost;           /* 顺序页面代价 */
    double random_page_cost;        /* 随机页面代价 */
    double cpu_tuple_cost;          /* CPU 元组代价 */
    double cpu_index_tuple_cost;    /* CPU 索引元组代价 */
    double cpu_operator_cost;       /* CPU 操作符代价 */
    double parallel_tuple_cost;     /* 并行元组代价 */
    double parallel_setup_cost;     /* 并行设置代价 */
} CostParams;

#define DEFAULT_COST_PARAMS { \
    .seq_page_cost = 1.0, \
    .random_page_cost = 4.0, \
    .cpu_tuple_cost = 0.01, \
    .cpu_index_tuple_cost = 0.005, \
    .cpu_operator_cost = 0.0025, \
    .parallel_tuple_cost = 0.1, \
    .parallel_setup_cost = 1000.0 \
}

/* ============================================================
 * 函数声明
 * ============================================================ */

PlannerInfo *planner(Query *parse, int cursorOptions, struct Plan *boundParams);
Plan *subquery_planner(PlannerInfo *root, Query *parse, bool hasRecursion);
RelOptInfo *build_simple_rel(PlannerInfo *root, Index relid, RelOptInfo *parent);
void generate_paths(PlannerInfo *root, RelOptInfo *rel);
Plan *create_plan(PlannerInfo *root, Path *best_path);

/* 代价计算 */
void cost_seqscan(Path *path, PlannerInfo *root, RelOptInfo *rel);
void cost_index(IndexPath *path, PlannerInfo *root, RelOptInfo *rel);
void cost_hashjoin(HashPath *path, PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
void cost_nestloop(NestLoopPath *path, PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
void cost_mergejoin(MergePath *path, PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
void cost_agg(AggPath *path, PlannerInfo *root);
void cost_sort(SortPath *path, PlannerInfo *root);

#endif /* DB_PLANNER_PLANNER_H */
```

- [ ] **Step 2: 实现计划器主入口**

```c
/* engineering/src/db/planner/planner.c */

#include "db/planner/planner.h"
#include <stdlib.h>

PlannerInfo *planner(Query *parse, int cursorOptions, Plan *boundParams) {
    PlannerInfo *root = makeNode(PlannerInfo);
    
    root->parse = parse;
    root->cost_params = DEFAULT_COST_PARAMS;
    
    /* 初始化配置 */
    root->enable_seqscan = true;
    root->enable_indexscan = true;
    root->enable_hashjoin = true;
    root->enable_nestloop = true;
    root->enable_mergejoin = true;
    root->enable_hashagg = true;
    root->enable_sort = true;
    
    /* 构建范围表信息 */
    root->base_rtes = parse->rtable;
    root->simple_rel_array_size = list_length(parse->rtable) + 1;
    root->simple_rel_array = (RelOptInfo **)calloc(
        root->simple_rel_array_size, sizeof(RelOptInfo *));
    
    /* 构建每个关系的 RelOptInfo */
    ListCell *lc;
    int idx = 1;
    foreach (lc, parse->rtable) {
        RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);
        root->simple_rel_array[idx] = build_simple_rel(root, idx, NULL);
        idx++;
    }
    
    return root;
}

RelOptInfo *build_simple_rel(PlannerInfo *root, Index relid, RelOptInfo *parent) {
    RelOptInfo *rel = makeNode(RelOptInfo);
    
    rel->relid = relid;
    rel->reloptkind = RELOPT_BASEREL;
    
    /* 获取统计信息 */
    RangeTblEntry *rte = root->base_rtes[relid - 1];
    if (rte->rtekind == RTE_RELATION) {
        /* 从 Catalog 获取统计信息 */
        rel->pages = get_relation_pages(rte->relid);
        rel->reltuples = get_relation_tuples(rte->relid);
        rel->rows = rel->reltuples;
        rel->width = get_relation_width(rte->relid);
        
        /* 获取索引列表 */
        rel->indexlist = get_relation_indexes(rte->relid);
    }
    
    return rel;
}

void generate_paths(PlannerInfo *root, RelOptInfo *rel) {
    /* 生成顺序扫描路径 */
    if (root->enable_seqscan) {
        Path *path = create_seqscan_path(root, rel);
        cost_seqscan(path, root, rel);
        add_path(rel, path);
    }
    
    /* 生成索引扫描路径 */
    if (root->enable_indexscan && rel->indexlist) {
        ListCell *lc;
        foreach (lc, rel->indexlist) {
            IndexOptInfo *index = (IndexOptInfo *)lfirst(lc);
            Path *path = create_indexscan_path(root, rel, index);
            cost_index((IndexPath *)path, root, rel);
            add_path(rel, path);
        }
    }
    
    /* 选择最优路径 */
    set_cheapest(rel);
}
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
# engineering/src/db/planner/CMakeLists.txt

file(GLOB_RECURSE PLANNER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(db_planner STATIC ${PLANNER_SOURCE_FILES})
target_link_libraries(db_planner project_includes db_catalog db_storage)
```

- [ ] **Step 4: 验证编译**

```bash
cd build/engineering
cmake --build . --target db_planner
```

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/planner/
git add engineering/include/db/planner/
git commit -m "feat(sql): 添加计划器基础结构"
```

---

## Phase 3: Executor

> 目标：实现 Volcano 执行模型，执行 Plan 树生成查询结果。

### Task 8: 执行器基础结构

**Files:**
- Modify: `engineering/src/db/sql/sql_executor.c`
- Modify: `engineering/include/db/sql/sql_executor.h`

**Interfaces:**
- Consumes: Plan 树（来自计划器）
- Produces: TupleTableSlot（结果元组）

**Step 1: 完善 Executor 结构**

（见设计文档 Phase 3 详细定义）

**Step 2: 实现 ExecProcNode 接口**

**Step 3: 实现执行器节点**

**Step 4: 编写执行器测试**

**Step 5: 提交**

---

## 总结

本计划涵盖 SQL 引擎的三个核心阶段：

| Phase | 任务数 | 预估代码行数 | 关键交付物 |
|-------|--------|-------------|-----------|
| Phase 1: Parser + Semantic | 6 任务 | ~5,000 行 | Flex/Bison 解析器、AST 节点、语义分析器 |
| Phase 2: Planner + Optimizer | 1+ 任务 | ~5,000 行 | 计划器、代价模型、优化规则 |
| Phase 3: Executor | 1+ 任务 | ~5,000 行 | Volcano 执行器、所有算子实现 |

**建议执行方式**：
- **Subagent-Driven（推荐）**：每个任务分配独立子代理，主会话审查每个任务的输出
- **Incremental Delivery**：Phase 1 完成后再启动 Phase 2，确保每个阶段可独立测试

**下一步**：执行 Task 1（Flex 词法分析器），开始 Phase 1 实现。