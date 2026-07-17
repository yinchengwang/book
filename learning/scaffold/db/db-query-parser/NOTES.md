# NOTES.md - SQL 解析器工程对照

## 工程源码位置

`engineering/src/db/sql/sql_parser.c` (2068 行)

## 核心组件

### 1. 词法分析器 (Lexer)

```c
// 关键字表：约 180 个 SQL 关键字
static const SqlKeyword g_keywords[] = {
    {"SELECT", TOKEN_SELECT, 6},
    {"FROM", TOKEN_FROM, 4},
    // ...
};

// 二分查找关键字
static SqlTokenType keyword_lookup(const char *text, int len);

// 词法分析器状态
typedef struct SqlLexer_s {
    const char *input;
    int pos, line, col;
    SqlToken current_token;
    bool has_current;
} SqlLexer;
```

### 2. Token 类型

约 100 种 Token 类型：
- 字面量：INTEGER, FLOAT, STRING, NULL, BOOL
- 关键字：SELECT, FROM, WHERE, JOIN, INSERT, UPDATE...
- 操作符：+, -, *, /, =, <>, <, >, <=, >=
- 分隔符：(, ), [, ], {, }, ,, ;, .

### 3. 语法分析器 (Parser)

递归下降解析器：
- `parse_select()` - SELECT 语句
- `parse_insert()` - INSERT 语句
- `parse_update()` - UPDATE 语句
- `parse_delete()` - DELETE 语句
- `parse_expression()` - 表达式（二元操作符优先级）
- `parse_binary()` - 二元表达式

### 4. AST 节点类型

```c
typedef enum SqlAstType_e {
    AST_SELECTStmt,
    AST_InsertStmt,
    AST_UpdateStmt,
    AST_DeleteStmt,
    AST_CreateTableStmt,
    AST_ColumnRef,
    AST_ConstValue,
    AST_AExpr,        // 操作符表达式
    AST_FuncCall,     // 函数调用
    AST_CaseExpr,     // CASE 表达式
    // ...
} SqlAstType;
```

### 5. 语义分析器

```c
typedef struct SqlSemContext_s {
    void *state;
} SqlSemContext;

SqlSemContext *sql_sem_create(void);
void *sql_sem_analyze(SqlSemContext *ctx, void *stmt);
void *sql_sem_resolve_column(void *ctx, const char *schema,
                            const char *table, const char *column);
```

## 关键设计

### 优先级解析

```c
// 操作符优先级表
switch (op) {
    case TOKEN_STAR: prec = 7; break;  // *, /, %
    case TOKEN_PLUS: prec = 6; break;  // +, -
    case TOKEN_EQ:   prec = 5; break;  // =, <>, <, >, <=, >=
    case TOKEN_AND:  prec = 4; break;  // AND
    case TOKEN_OR:   prec = 3; break;  // OR
}
```

### 错误恢复模式

```c
typedef enum SqlErrorRecoveryMode_e {
    ERROR_RECOVERY_NONE = 0,
    ERROR_RECOVERY_SKIP,    // 跳过错误 Token
    ERROR_RECOVERY_INFER    // 推断并修复
} SqlErrorRecoveryMode;
```

## 学习要点

1. **词法分析**：正则匹配 → Token 流
2. **语法分析**：递归下降 → AST
3. **语义分析**：符号表查询、类型检查
4. **优先级**： Pratt parser 或 优先级表法

## 面试高频题

1. 词法分析和语法分析的区别
2. 递归下降解析器的优缺点
3. 如何处理操作符优先级
4. AST 和操作数栈的区别
5. 手写一个表达式解析器