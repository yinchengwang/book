# SQL 浮点数支持规格

## Overview

为 SQL 词法分析器和解析器添加浮点数常量识别和处理能力。

## Syntax

```
float_literal ::= digits '.' digits [exponent]
exponent      ::= ('e' | 'E') ['+' | '-'] digits
digits        ::= [0-9]+
```

### Examples

| Input | Parsed Value | Type |
|-------|-------------|------|
| `3.14` | 3.14 | SQL_TYPE_REAL |
| `0.5` | 0.5 | SQL_TYPE_REAL |
| `.25` | 0.25 | SQL_TYPE_REAL |
| `1e10` | 10000000000.0 | SQL_TYPE_REAL |
| `2.5E-3` | 0.0025 | SQL_TYPE_REAL |
| `3.14e+2` | 314.0 | SQL_TYPE_REAL |

## Token Definition

### sql_token_type_t

新增枚举值：

```c
SQL_TOKEN_FLOAT,     /**< 浮点数字面量 */
```

### sql_node_t

新增字段：

```c
struct {
    sql_data_type_t type;   /**< SQL_TYPE_REAL */
    int      int_value;     /**< 整数值（保留） */
    double   float_value;   /**< 浮点数值 */
    char    *str_value;     /**< 字符串值（保留） */
} constant;
```

## Lexer Rules

1. 识别模式: `[0-9]+\.[0-9]*` 或 `[0-9]*\.[0-9]+`
2. 指数部分可选: `(e|E)[\+\-]?[0-9]+`
3. 优先于 IDENT: 首个字符是数字且包含小数点或指数

## Acceptance Criteria

- [ ] `SELECT 3.14` 解析成功，类型为 SQL_TYPE_REAL
- [ ] `SELECT 0.5` 解析成功
- [ ] `SELECT 1e10` 解析成功
- [ ] `SELECT 2.5E-3` 解析成功
- [ ] `SELECT -.5` 解析成功（负浮点数）
- [ ] `SELECT * FROM t WHERE id > 3.14` WHERE 子句中的浮点数正确处理
- [ ] `3.14` 不会误解析为标识符
