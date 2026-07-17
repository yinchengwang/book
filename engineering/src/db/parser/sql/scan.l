/* scan.l - SQL 词法分析器 */
/*
 * PostgreSQL 风格的 Flex 词法分析器
 * 支持 SQL:2016 标准核心词法单元
 */

%{
#include "db/parser/sql/parsenodes.h"
#include "db/parser/sql/gram.h"
#include <string.h>
#include <stdlib.h>

/* 位置信息默认宏 */
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

/* 全局语义值 */
YYSTYPE yylval;
YYLTYPE yylloc;

%}

%option prefix="sql_yy"
%option noyywrap
%option batch
%option pointer
%option yylineno

/* 字符集定义 */
UCN    \\u[0-9a-fA-F]{4}
IDI    [A-Za-z_\x80-\xFF]
IDC    [A-Za-z_0-9\x80-\xFF]
IDENT  {IDI}{IDC}*

/* 数字定义 */
DIGIT  [0-9]
HEX    [0-9a-fA-F]
INT    {DIGIT}+
HEXNUM 0[xX]{HEX}+
FLOAT  {DIGIT}+\.{DIGIT}*([eE][+-]?{DIGIT}+)?|{DIGIT}*\.{DIGIT}+([eE][+-]?{DIGIT}+)?|{DIGIT}+[eE][+-]?{DIGIT}+

/* 字符串定义 */
STRING '([^'\\\n]|\\.|'')*'

%%

/* 空白和注释 */
[ \t\r]+          { /* 忽略空白 */ }
\n                { yylloc.line++; yylloc.column = 1; }
"--"[^\n]*        { /* 行注释 */ }
"/*"([^*]|\*+[^*/])*\*+"/"  { /* 块注释 */ }

/* 关键字 - DML */
"SELECT"          { return SELECT; }
"FROM"            { return FROM; }
"WHERE"           { return WHERE; }
"INSERT"          { return INSERT; }
"INTO"            { return INTO; }
"VALUES"          { return VALUES; }
"UPDATE"          { return UPDATE; }
"SET"             { return SET; }
"DELETE"          { return DELETE; }

/* 关键字 - DDL */
"CREATE"          { return CREATE; }
"DROP"            { return DROP; }
"ALTER"           { return ALTER; }
"TABLE"           { return TABLE; }
"INDEX"           { return INDEX; }
"VIEW"            { return VIEW; }
"DATABASE"        { return DATABASE; }
"SCHEMA"          { return SCHEMA; }

/* 关键字 - 逻辑操作 */
"AND"             { return AND; }
"OR"              { return OR; }
"NOT"             { return NOT; }

/* 关键字 - NULL 和布尔值 */
"NULL"            { return NULL_P; }
"TRUE"            { yylval.ival = 1; return TRUE_P; }
"FALSE"           { yylval.ival = 0; return FALSE_P; }

/* 关键字 - 比较操作 */
"IS"              { return IS; }
"IN"              { return IN; }
"LIKE"            { return LIKE; }
"BETWEEN"         { return BETWEEN; }

/* 关键字 - CASE 表达式 */
"CASE"            { return CASE; }
"WHEN"            { return WHEN; }
"THEN"            { return THEN; }
"ELSE"            { return ELSE; }
"END"             { return END; }

/* 关键字 - 分组 */
"GROUP"           { return GROUP; }
"BY"              { return BY; }
"HAVING"          { return HAVING; }

/* 关键字 - 排序 */
"ORDER"           { return ORDER; }
"ASC"             { return ASC; }
"DESC"            { return DESC; }

/* 关键字 - 限制 */
"LIMIT"           { return LIMIT; }
"OFFSET"          { return OFFSET; }

/* 关键字 - 连接 */
"JOIN"            { return JOIN; }
"LEFT"            { return LEFT; }
"RIGHT"           { return RIGHT; }
"FULL"            { return FULL; }
"INNER"           { return INNER; }
"OUTER"           { return OUTER; }
"CROSS"           { return CROSS; }
"ON"              { return ON; }

/* 关键字 - 别名 */
"AS"              { return AS; }

/* 关键字 - 集合操作 */
"DISTINCT"        { return DISTINCT; }
"ALL"             { return ALL; }
"UNION"           { return UNION; }
"INTERSECT"       { return INTERSECT; }
"EXCEPT"          { return EXCEPT; }

/* 关键字 - CTE */
"WITH"            { return WITH; }
"RECURSIVE"       { return RECURSIVE; }

/* 关键字 - 约束 */
"PRIMARY"         { return PRIMARY; }
"KEY"             { return KEY; }
"FOREIGN"         { return FOREIGN; }
"REFERENCES"      { return REFERENCES; }
"UNIQUE"          { return UNIQUE; }
"CHECK"           { return CHECK; }
"CONSTRAINT"      { return CONSTRAINT; }

/* 关键字 - 数据类型 */
"INT"             { return INT_P; }
"INTEGER"         { return INT_P; }
"BIGINT"          { return BIGINT; }
"SMALLINT"        { return SMALLINT; }
"TINYINT"         { return TINYINT; }
"REAL"            { return REAL; }
"DOUBLE"          { return DOUBLE_P; }
"FLOAT"           { return FLOAT_P; }
"DECIMAL"         { return DECIMAL_P; }
"NUMERIC"         { return NUMERIC; }
"VARCHAR"         { return VARCHAR; }
"CHAR"            { return CHAR_P; }
"CHARACTER"       { return CHARACTER; }
"TEXT"            { return TEXT_P; }
"BOOLEAN"         { return BOOLEAN_P; }
"BOOL"            { return BOOLEAN_P; }
"DATE"            { return DATE; }
"TIME"            { return TIME; }
"TIMESTAMP"       { return TIMESTAMP; }
"BLOB"            { return BLOB; }

/* 关键字 - 其他 */
"DEFAULT"         { return DEFAULT; }
"USING"           { return USING; }
"CASCADE"         { return CASCADE; }
"RESTRICT"        { return RESTRICT; }

/* 操作符 */
"="               { return '='; }
"<>"              { yylval.ival = NE; return Op; }
"!="              { yylval.ival = NE; return Op; }
"<="              { yylval.ival = LE; return Op; }
">="              { yylval.ival = GE; return Op; }
"<"               { return '<'; }
">"               { return '>'; }
"+"               { return '+'; }
"-"               { return '-'; }
"*"               { return '*'; }
"/"               { return '/'; }
"%"               { return '%'; }
"||"              { yylval.ival = CONCAT; return Op; }
"->"              { yylval.ival = JSON_OP; return Op; }
"->>"             { yylval.ival = JSON_TEXT_OP; return Op; }

/* 分隔符 */
"("               { return '('; }
")"               { return ')'; }
","               { return ','; }
";"               { return ';'; }
"."               { return '.'; }

/* 标识符 */
{IDENT}           {
                      int len = strlen(yytext);
                      yylval.str = (char *)malloc(len + 1);
                      if (yylval.str) {
                          strcpy(yylval.str, yytext);
                      }
                      return IDENT;
                  }

/* 整数 */
{INT}             {
                      yylval.ival = atoi(yytext);
                      return ICONST;
                  }

/* 十六进制数 */
{HEXNUM}          {
                      int len = strlen(yytext);
                      yylval.str = (char *)malloc(len + 1);
                      if (yylval.str) {
                          strcpy(yylval.str, yytext);
                      }
                      return XCONST;
                  }

/* 浮点数 */
{FLOAT}           {
                      yylval.fval = atof(yytext);
                      return FCONST;
                  }

/* 字符串 */
{STRING}          {
                      /* 移除引号 */
                      int len = strlen(yytext);
                      yylval.str = (char *)malloc(len - 1);
                      if (yylval.str) {
                          strncpy(yylval.str, yytext + 1, len - 2);
                          yylval.str[len - 2] = '\0';
                      }
                      return SCONST;
                  }

/* 未识别字符 */
.                 { return yytext[0]; }

%%
