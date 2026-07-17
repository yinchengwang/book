# SQL WHERE 表达式规格

## Overview

扩展 SQL 解析器，支持完整的 WHERE 子句表达式，包括逻辑操作符、比较操作符和特殊操作符。

## Supported Operations

### Logical Operators

| Operator | Syntax | Precedence |
|----------|--------|------------|
| AND | `expr AND expr` | 2 (低) |
| OR | `expr OR expr` | 1 (最低) |
| NOT | `NOT expr` | 3 (高) |

### Comparison Operators

| Operator | Syntax | Example |
|----------|--------|---------|
| = | 等于 | `id = 1` |
| <> 或 != | 不等于 | `status <> 'active'` |
| > | 大于 | `age > 18` |
| >= | 大于等于 | `salary >= 5000` |
| < | 小于 | `score < 60` |
| <= | 小于等于 | `count <= 100` |

### Special Operators

| Operator | Syntax | Example |
|----------|--------|---------|
| LIKE | 模式匹配 | `name LIKE '%test%'` |
| NOT LIKE | 模式不匹配 | `name NOT LIKE '%bad%'` |
| IN | 在列表中 | `id IN (1, 2, 3)` |
| NOT IN | 不在列表中 | `status NOT IN ('x', 'y')` |
| BETWEEN | 范围包含 | `age BETWEEN 18 AND 65` |
| NOT BETWEEN | 范围不包含 | `age NOT BETWEEN 0 AND 17` |
| IS NULL | 为空 | `email IS NULL` |
| IS NOT NULL | 不为空 | `email IS NOT NULL` |
| IS TRUE | 为真 | `active IS TRUE` |
| IS FALSE | 为假 | `deleted IS FALSE` |

## Syntax Extensions

### AST Node Types

新增节点类型：

```c
SQL_NODE_LOGICAL_OP,    /**< AND/OR/NOT 逻辑操作 */
SQL_NODE_IN_LIST,       /**< IN/NOT IN 列表 */
SQL_NODE_BETWEEN,       /**< BETWEEN 范围 */
SQL_NODE_LIKE,          /**< LIKE/NOT LIKE 模式 */
SQL_NODE_IS_NULL        /**< IS NULL/IS NOT NULL */
```

### Expression Structure

```c
struct {
    sql_node_t *left;   /**< 左操作数 */
    sql_node_t *right;  /**< 右操作数（用于二元） */
    sql_node_t *expr;   /**< 单目操作数（用于 NOT/IS_NULL） */
    sql_logical_op_t op; /**< AND/OR/NOT */
} logical_op;

struct {
    sql_node_t *expr;   /**< 被检查的表达式 */
    sql_node_t **list;  /**< IN 列表项 */
    size_t count;       /**< 列表长度 */
    bool negated;       /**< 是否为 NOT IN */
} in_list;

struct {
    sql_node_t *expr;   /**< 被检查的表达式 */
    sql_node_t *min;    /**< 范围下限 */
    sql_node_t *max;    /**< 范围上限 */
    bool negated;       /**< 是否为 NOT BETWEEN */
} between;

struct {
    sql_node_t *expr;   /**< 被匹配的表达式 */
    char *pattern;      /**< LIKE 模式 */
    char escape;        /**< 转义字符 */
    bool negated;       /**< 是否为 NOT LIKE */
} like;

struct {
    sql_node_t *expr;   /**< 被检查的表达式 */
    bool is_null;       /**< true=IS NULL, false=IS NOT NULL */
    bool truth_value;   /**< 用于 IS TRUE/FALSE */
} is_null;
```

## LIKE Pattern Rules

- `%` 匹配任意长度（包括空）的字符序列
- `_` 匹配任意单个字符
- `\%` 匹配字面量 `%`
- `\_` 匹配字面量 `_`
- 默认转义字符: `\`

### Examples

| Pattern | Match | Non-Match |
|---------|-------|-----------|
| `%test%` | `my test`, `testing` | `text` |
| `a%b` | `ab`, `aabb`, `a123b` | `abx` |
| `a_b` | `a1b`, `aAb` | `a12b` |
| `\%abc` | `%abc` | `xabc` |

## Acceptance Criteria

- [ ] `WHERE id = 1 AND name = 'test'` 正确解析
- [ ] `WHERE id > 10 OR status = 'active'` 正确解析
- [ ] `WHERE NOT (id = 1)` 正确解析
- [ ] `WHERE name LIKE '%test%'` 正确解析
- [ ] `WHERE id IN (1, 2, 3)` 正确解析
- [ ] `WHERE age BETWEEN 18 AND 65` 正确解析
- [ ] `WHERE email IS NULL` 正确解析
- [ ] `WHERE active IS TRUE` 正确解析
- [ ] 表达式优先级正确: `A OR B AND C` = `A OR (B AND C)`
- [ ] 括号可改变优先级: `(A OR B) AND C`
