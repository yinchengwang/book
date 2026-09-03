/**
 * @file makefuncs.h
 * @brief AST 节点构造函数声明
 *
 * 提供创建 SQL AST 节点的辅助函数。
 * PostgreSQL 风格的节点构造接口。
 */

#ifndef DB_PARSER_SQL_MAKEFUNCS_H
#define DB_PARSER_SQL_MAKEFUNCS_H

#include "db/parser/sql/parsenodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 通用节点构造
 * ============================================================ */

/**
 * @brief 创建通用节点
 * @param type 节点类型标签
 * @return 新创建的节点
 */
Node *makeNode(NodeTag type);

/**
 * @brief 创建字符串节点
 * @param str 字符串值
 * @return String 节点
 */
String *makeString(const char *str);

/* ============================================================
 * 列表操作
 * ============================================================ */

/**
 * @brief 创建单元素列表
 * @param data 元素数据
 * @return 新列表
 */
List *list_make1(void *data);

/**
 * @brief 创建双元素列表
 * @param d1 第一个元素
 * @param d2 第二个元素
 * @return 新列表
 */
List *list_make2(void *d1, void *d2);

/**
 * @brief 追加元素到列表末尾
 * @param l 目标列表
 * @param data 新元素
 * @return 原列表（可能为空时返回新列表）
 */
List *lappend(List *l, void *data);

/**
 * @brief 获取列表第 n 个元素
 * @param l 列表
 * @param n 索引（从 0 开始）
 * @return 元素数据
 */
void *list_nth(List *l, int n);

/**
 * @brief 连接两个列表
 * @param l1 第一个列表
 * @param l2 第二个列表
 * @return 连接后的列表
 */
List *list_concat(List *l1, List *l2);

/**
 * @brief 释放列表（不释放元素数据）
 * @param l 列表
 */
void list_free(List *l);

/* ============================================================
 * DML 语句构造
 * ============================================================ */

/**
 * @brief 创建 SELECT 语句节点
 * @return SelectStmt 节点
 */
SelectStmt *makeSelectStmt(void);

/**
 * @brief 创建 INSERT 语句节点
 * @return InsertStmt 节点
 */
InsertStmt *makeInsertStmt(void);

/**
 * @brief 创建 UPDATE 语句节点
 * @return UpdateStmt 节点
 */
UpdateStmt *makeUpdateStmt(void);

/**
 * @brief 创建 DELETE 语句节点
 * @return DeleteStmt 节点
 */
DeleteStmt *makeDeleteStmt(void);

/* ============================================================
 * DDL 语句构造
 * ============================================================ */

/**
 * @brief 创建 CREATE TABLE 语句节点
 * @return CreateStmt 节点
 */
CreateStmt *makeCreateStmt(void);

/**
 * @brief 创建 DROP 语句节点
 * @return DropStmt 节点
 */
DropStmt *makeDropStmt(void);

/* ============================================================
 * 表引用构造
 * ============================================================ */

/**
 * @brief 创建范围变量（表引用）
 * @param schemaname 模式名（可为 NULL）
 * @param relname 关系名（表名）
 * @return RangeVar 节点
 */
RangeVar *makeRangeVar(const char *schemaname, const char *relname);

/* ============================================================
 * 表达式构造
 * ============================================================ */

/**
 * @brief 创建列引用（单列名）
 * @param colname 列名
 * @return ColumnRef 节点
 */
ColumnRef *makeColumnRef(const char *colname);

/**
 * @brief 创建列引用（表名.列名）
 * @param tablename 表名
 * @param colname 列名
 * @return ColumnRef 节点
 */
ColumnRef *makeColumnRef2(const char *tablename, const char *colname);

/**
 * @brief 创建 A_Expr 表达式节点
 * @param kind 表达式类型
 * @param name 操作符名称
 * @param lexpr 左表达式
 * @param rexpr 右表达式
 * @return A_Expr 节点
 */
A_Expr *makeA_Expr(A_Expr_Kind kind, const char *name, Node *lexpr, Node *rexpr);

/**
 * @brief 创建布尔表达式
 * @param boolop 布尔操作类型
 * @param args 参数列表
 * @return BoolExpr 节点
 */
BoolExpr *makeBoolExpr(BoolType boolop, List *args);

/**
 * @brief 创建整数常量
 * @param val 整数值
 * @return A_Const 节点
 */
A_Const *makeIntConst(int val);

/**
 * @brief 创建浮点数常量
 * @param val 浮点数值
 * @return A_Const 节点
 */
A_Const *makeFloatConst(double val);

/**
 * @brief 创建字符串常量
 * @param val 字符串值
 * @return A_Const 节点
 */
A_Const *makeStringConst(const char *val);

/**
 * @brief 创建 NULL 常量
 * @return A_Const 节点
 */
A_Const *makeNullConst(void);

/**
 * @brief 创建函数调用
 * @param funcname 函数名
 * @param args 参数列表
 * @return FuncCall 节点
 */
FuncCall *makeFuncCall(const char *funcname, List *args);

/**
 * @brief创建 NULL 测试表达式
 * @param nulltest 测试类型
 * @param arg 测试表达式
 * @return NullTest 节点
 */
NullTest *makeNullTest(NullTestType nulltest, Node *arg);

/**
 * @brief 创建 TypeName 节点
 * @param name 类型名称字符串
 * @return TypeName 节点
 */
TypeName *makeTypeName(const char *name);

/**
 * @brief 创建 ResTarget 节点（用于 SELECT 目标列表和 UPDATE SET 子句）
 * @param name 目标名称
 * @param val 值表达式
 * @return ResTarget 节点
 */
ResTarget *makeResTarget(const char *name, Node *val);

/**
 * @brief 创建 ColumnDef 节点（列定义）
 * @param colname 列名
 * @param typeName 类型名
 * @return ColumnDef 节点
 */
ColumnDef *makeColumnDef(const char *colname, TypeName *typeName);

/**
 * @brief 创建 SortBy 节点（排序项）
 * @param node 排序表达式
 * @param dir 排序方向
 * @return SortBy 节点
 */
SortBy *makeSortBy(Node *node, SortByDir dir);

/**
 * @brief 创建 JoinExpr 节点
 * @param jointype 连接类型
 * @param larg 左表
 * @param rarg 右表
 * @param quals 连接条件
 * @return JoinExpr 节点
 */
JoinExpr *makeJoinExpr(JoinType jointype, Node *larg, Node *rarg, Node *quals);

#ifdef __cplusplus
}
#endif

#endif /* DB_PARSER_SQL_MAKEFUNCS_H */