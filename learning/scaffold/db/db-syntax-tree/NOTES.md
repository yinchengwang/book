# NOTES.md - 语法树工程对照

## 工程源码位置

`engineering/include/db/sql/sql_parser.h` (AST 定义)

## AST 节点类型

```c
typedef enum SqlAstType_e {
    /* 语句 */
    AST_SELECTStmt,
    AST_InsertStmt,
    AST_UpdateStmt,
    AST_DeleteStmt,
    AST_CreateTableStmt,

    /* 表达式 */
    AST_ColumnRef,
    AST_ParamRef,
    AST_ConstValue,
    AST_AExpr,        // 二元操作符
    AST_FuncCall,     // 函数调用
    AST_CaseExpr,     // CASE 表达式

    /* 表引用 */
    AST_RangeVar,
    AST_JoinExpr,

    /* 列表 */
    AST_List,
    AST_TargetEntry
} SqlAstType;
```

## 遍历模式

1. **Pre-order**: 父节点 → 子节点
2. **Post-order**: 子节点 → 父节点
3. **In-order**: 左子树 → 根 → 右子树（用于二元表达式）

## 面试高频题

1. AST 和语法树的区别
2. Visitor 模式在 AST 中的应用
3. 如何用 AST 实现 SQL 验证