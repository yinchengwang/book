# SQL 解析

## 学习目标
- 理解 SQL 语句的解析流程
- 掌握词法分析和语法分析的作用

## 核心概念

- **词法分析**：将 SQL 字符串拆分为 Token
- **语法分析**：根据语法规则构建语法树
- **AST**：抽象语法树，SQL 的结构化表示

## 解析流程

```mermaid
flowchart LR
    A[SQL 字符串] --> B[词法分析]
    B --> C[Token 流]
    C --> D[语法分析]
    D --> E[语法树 AST]
    E --> F[语义分析]
    F --> G[查询树]
```

## 词法分析示例

```mermaid
graph TD
    A["SELECT * FROM users WHERE id = 1"]
    A --> B["SELECT: 关键字"]
    A --> C["*: 通配符"]
    A --> D["FROM: 关键字"]
    A --> E["users: 标识符"]
    A --> F["WHERE: 关键字"]
    A --> G["id: 标识符"]
    A --> H["=: 操作符"]
    A --> I["1: 整数常量"]
```

## AST 结构

```mermaid
graph TD
    SelectStmt --> TargetList
    SelectStmt --> FromClause
    SelectStmt --> WhereClause
    TargetList --> ColumnRef
    FromClause --> TableName
    WhereClause --> OpExpr
    OpExpr --> ColumnRef2
    OpExpr --> Const
```

## 要点总结

- 解析分为词法分析、语法分析和语义分析
- AST 是后续优化和执行的基础

## 思考题

1. 如何处理 SQL 语法错误？
2. AST 如何支持复杂的嵌套查询？