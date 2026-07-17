# 规格：sql-parser

## ADDED Requirements

### Requirement: SQL 词法分析器

系统 SHALL 实现 SQL 词法分析器（Lexer），能够将输入的 SQL 文本分割为 Token 序列。

#### Scenario: 基础词法分析
- **WHEN** 输入 SQL 文本 `"SELECT id, name FROM users WHERE age > 18"`
- **THEN** 词法分析器 SHALL 输出 Token 序列：[SELECT, IDENT(id), ',', IDENT(name), FROM, IDENT(users), WHERE, IDENT(age), '>', INTEGER(18)]

#### Scenario: 关键字和标识符识别
- **WHEN** 输入包含大小写混合的 SQL
- **THEN** 关键字（如 SELECT, FROM）SHALL 不区分大小写
- **THEN** 标识符 SHALL 区分大小写（除非用双引号包裹）

#### Scenario: 字符串和数字字面量
- **WHEN** 输入包含字符串和数字的 SQL
- **THEN** 字符串字面量（如 `'hello'`）SHALL 被识别为 STRING Token
- **THEN** 数字字面量（如 `123`, `3.14`）SHALL 被识别为 INTEGER 或 FLOAT Token

#### Scenario: 运算符和分隔符
- **WHEN** 输入包含运算符和分隔符的 SQL
- **THEN** `+`, `-`, `*`, `/`, `=`, `<>`, `<`, `>`, `<=`, `>=` SHALL 被识别为 OPERATOR Token
- **THEN** `,`, `(`, `)`, `;`, `.` SHALL 被识别为 PUNCTUATION Token

#### Scenario: 位置信息跟踪
- **WHEN** 词法分析器处理输入
- **THEN** 每个 Token SHALL 记录起始位置（行号、列号）
- **THEN** 错误信息 SHALL 能够指出错误位置

---

### Requirement: SQL 语法分析器

系统 SHALL 实现 SQL 语法分析器（Parser），能够将 Token 序列解析为抽象语法树（AST）。

#### Scenario: SELECT 语句解析
- **WHEN** 输入 SELECT 语句 `"SELECT id, name FROM users WHERE age > 18"`
- **THEN** 语法分析器 SHALL 输出 SelectStmt AST 节点
- **THEN** AST SHALL 包含 target_list、from_clause、where_clause 等子节点

#### Scenario: INSERT 语句解析
- **WHEN** 输入 INSERT 语句
- **THEN** 语法分析器 SHALL 输出 InsertStmt AST 节点
- **THEN** AST SHALL 包含 relation、columns、values 等子节点

#### Scenario: UPDATE 语句解析
- **WHEN** 输入 UPDATE 语句
- **THEN** 语法分析器 SHALL 输出 UpdateStmt AST 节点
- **THEN** AST SHALL 包含 relation、target_list、where_clause 等子节点

#### Scenario: DELETE 语句解析
- **WHEN** 输入 DELETE 语句
- **THEN** 语法分析器 SHALL 输出 DeleteStmt AST 节点
- **THEN** AST SHALL 包含 relation、where_clause 等子节点

#### Scenario: CREATE TABLE 语句解析
- **WHEN** 输入 CREATE TABLE 语句
- **THEN** 语法分析器 SHALL 输出 CreateStmt AST 节点
- **THEN** AST SHALL 包含 relation、column_defs、constraints 等子节点

#### Scenario: JOIN 子句解析
- **WHEN** 输入包含 JOIN 的语句
- **THEN** 语法分析器 SHALL 正确解析 INNER/LEFT/RIGHT/FULL JOIN
- **THEN** ON 条件 SHALL 被正确关联到对应的 JOIN

#### Scenario: 聚合函数解析
- **WHEN** 输入包含聚合函数的语句
- **THEN** COUNT, SUM, AVG, MIN, MAX SHALL 被识别为 AGGREGATE 函数
- **THEN** GROUP BY 和 HAVING 子句 SHALL 被正确解析

#### Scenario: 子查询解析
- **WHEN** 输入包含子查询的语句
- **THEN** 标量子查询 SHALL 被支持
- **THEN** EXISTS/IN 子查询 SHALL 被支持

---

### Requirement: 语义分析器

系统 SHALL 实现语义分析器（Analyzer），能够进行表/列引用解析、类型推导和别名解析。

#### Scenario: 表引用解析
- **WHEN** 解析 SELECT 语句
- **THEN** 表名 SHALL 被解析为对某张表的引用
- **THEN** 别名（如 `FROM users u`）SHALL 被正确处理

#### Scenario: 列引用解析
- **WHEN** 解析包含列引用的语句
- **THEN** 列名 SHALL 被解析为对某张表的某列的引用
- **THEN** `table.column` 格式 SHALL 被支持

#### Scenario: 类型推导
- **WHEN** 表达式包含字面量
- **THEN** 整数 SHALL 被推导为 INTEGER 类型
- **THEN** 字符串 SHALL 被推导为 VARCHAR 类型
- **THEN** 浮点数 SHALL 被推导为 DOUBLE 类型

#### Scenario: 错误检测
- **WHEN** 解析包含不存在表/列的语句
- **THEN** 分析器 SHALL 报告相应的错误信息

---

### Requirement: SQL 语法覆盖

系统 SHALL 支持以下 SQL 语法：

#### Scenario: DDL 支持
- **WHEN** 执行 DDL 语句
- **THEN** CREATE TABLE/DROP TABLE/ALTER TABLE SHALL 被支持
- **THEN** CREATE INDEX/DROP INDEX SHALL 被支持
- **THEN** CREATE VIEW/DROP VIEW SHALL 被支持

#### Scenario: DML 支持
- **WHEN** 执行 DML 语句
- **THEN** SELECT/INSERT/UPDATE/DELETE SHALL 被支持
- **THEN** INSERT ... VALUES 和 INSERT ... SELECT SHALL 被支持

#### Scenario: 表达式支持
- **WHEN** 使用表达式
- **THEN** 算术表达式 SHALL 被支持
- **THEN** 逻辑表达式 SHALL 被支持
- **THEN** CASE WHEN 表达式 SHALL 被支持
- **THEN** NULLIF, COALESCE 函数 SHALL 被支持

#### Scenario: 多模态扩展语法
- **WHEN** 使用多模态扩展
- **THEN** `VECTOR_SEARCH(column, query, k)` 函数 SHALL 被支持
- **THEN** `TIME_SERIES_AGG(ts, window, func)` 函数 SHALL 被支持
- **THEN** `GRAPH_TRAVERSE(graph, start, depth)` 函数 SHALL 被支持
