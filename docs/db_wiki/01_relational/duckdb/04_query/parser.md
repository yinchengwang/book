# Parser SQL 解析器

## 学习目标

- 理解 DuckDB 手写递归下降解析器的设计理念与实现细节
- 掌握 Tokenizer → Parser → AST 三阶段解析流程
- 对比 DuckDB 与 PostgreSQL/SQLite 的解析器架构差异

## 核心概念

- **Tokenizer（词法分析器）**：将 SQL 文本切分成 Token 流
- **Parser（语法分析器）**：根据 Token 流构建抽象语法树（AST）
- **AST（抽象语法树）**：表示 SQL 语句结构的数据结构
- **递归下降解析器**：手写的、自顶向下的解析器实现
- **SelectStatement、InsertStatement、CreateStatement**：AST 节点类型

## 解析流程总览

```mermaid
graph TD
    A[SQL 文本] --> B[Tokenizer<br/>词法分析]
    B --> C[Token 流]
    C --> D[Parser<br/>语法分析]
    D --> E[AST<br/>抽象语法树]
    E --> F[Binder<br/>名称解析/类型检查]

    style A fill:#e1f5ff
    style B fill:#fff4e1
    style C fill:#fff4e1
    style D fill:#fff4e1
    style E fill:#e8f5e9
    style F fill:#e8f5e9
```

## Tokenizer 词法分析器

DuckDB 的 Tokenizer 负责将 SQL 文本切分成 Token：

```mermaid
graph LR
    A[SQL 文本] --> B[Tokenizer]
    B --> C[Token 流]

    subgraph "Token 类型"
        D1[关键字: SELECT/FROM/WHERE]
        D2[标识符: table_name/col]
        D3[常量: 123/'abc']
        D4[运算符: + - * /]
        D5[分隔符: , ; ( )]
    end

    C --> D1
    C --> D2
    C --> D3
    C --> D4
    C --> D5
```

**Token 结构**：

```mermaid
graph TD
    A[Token] --> B[type: TokenType<br/>Token 类型]
    A --> C[start: idx_t<br/>起始位置]
    A --> D[length: idx_t<br/>长度]

    E[TokenType 枚举] --> F[关键字类<br/>SELECT/INSERT/CREATE]
    E --> G[标识符类<br/>IDENTIFIER]
    E --> H[常量类<br/>NUMERIC/STRING]
    E --> I[运算符类<br/>PLUS/MINUS/STAR]
```

**Tokenizer 特性**：

| 特性 | 说明 |
|------|------|
| **流式读取** | 不一次性加载全部 SQL，按需读取字符 |
| **位置追踪** | 记录每个 Token 的行列位置，用于错误报告 |
| **关键字识别** | 维护关键字哈希表，快速识别 |
| **Unicode 支持** | 正确处理 UTF-8 编码的标识符和字符串 |

## Parser 语法分析器

DuckDB 使用**手写递归下降解析器**，而非 YACC/Bison 等解析器生成工具：

```mermaid
flowchart TD
    A[Parser 入口] --> B[parseStatement]
    B --> C{Token 类型?}

    C -->|SELECT| D[parseSelect]
    C -->|INSERT| E[parseInsert]
    C -->|CREATE| F[parseCreate]
    C -->|UPDATE| G[parseUpdate]
    C -->|DELETE| H[parseDelete]

    D --> I[SelectStatement]
    E --> J[InsertStatement]
    F --> K[CreateStatement]
    G --> L[UpdateStatement]
    H --> M[DeleteStatement]

    I --> N[返回 AST]
    J --> N
    K --> N
    L --> N
    M --> N
```

### 递归下降解析示例

```mermaid
flowchart TD
    A[parseSelect] --> B[期望 SELECT 关键字]
    B --> C[parseSelectList<br/>解析列列表]
    C --> D[期望 FROM 关键字]
    D --> E[parseFromClause<br/>解析表引用]
    E --> F{有 WHERE?}
    F -->|是| G[parseWhereClause<br/>解析条件]
    F -->|否| H[parseGroupBy<br/>解析分组]
    G --> H
    H --> I[返回 SelectStatement]

    style A fill:#fff4e1
    style I fill:#e8f5e9
```

**核心设计理念**：

1. **手写而非生成**：相比 YACC/Bison 自动生成，手写解析器更易维护和调试
2. **错误恢复友好**：可以精确定位错误位置，提供更好的错误提示
3. **扩展性强**：添加新语法只需新增解析函数，无需修改语法规则文件
4. **性能可控**：避免解析器生成工具带来的额外开销

## AST 节点结构

DuckDB 的 AST 节点定义在 `src/include/parser/parsed_data/` 目录：

```mermaid
graph TD
    A[AST 节点基类] --> B[ParsedExpression<br/>表达式基类]
    A --> C[TableRef<br/>表引用基类]
    A --> D[Statement<br/>语句基类]

    D --> E[SelectStatement]
    D --> F[InsertStatement]
    D --> G[CreateStatement]
    D --> H[UpdateStatement]
    D --> I[DeleteStatement]

    E --> J[select_distinct: bool]
    E --> K[select_list: vector<br/>列列表]
    E --> L[from_table: TableRef<br/>FROM 子句]
    E --> M[where_clause: Expression<br/>WHERE 条件]
    E --> N[group_by: vector<br/>GROUP BY]
    E --> O[having: Expression<br/>HAVING 条件]
    E --> P[order_by: vector<br/>ORDER BY]
    E --> Q[limit: Expression<br/>LIMIT]
    E --> R[offset: Expression<br/>OFFSET]
```

### SelectStatement 示例

```sql
SELECT id, name FROM users WHERE id > 100 ORDER BY name LIMIT 10;
```

```mermaid
graph TD
    A[SelectStatement] --> B[select_list]
    A --> C[from_table]
    A --> D[where_clause]
    A --> E[order_by]
    A --> F[limit]

    B --> B1[ColumnRef: id]
    B --> B2[ColumnRef: name]

    C --> C1[BaseTableRef<br/>table: users]

    D --> D1[ComparisonExpression<br/>id > 100]

    E --> E1[OrderByNode<br/>name ASC]

    F --> F1[ConstantExpression<br/>10]
```

## 表达式解析

DuckDB 的表达式解析支持运算符优先级：

```mermaid
flowchart TD
    A[parseExpression] --> B[parseOrExpression<br/>OR 优先级最低]
    B --> C[parseAndExpression<br/>AND 次低]
    C --> D[parseNotExpression<br/>NOT]
    D --> E[parseComparisonExpression<br/>比较运算]
    E --> F[parseAddSubExpression<br/>加减]
    F --> G[parseMulDivExpression<br/>乘除]
    G --> H[parsePrimaryExpression<br/>原子表达式]

    H --> I[常量]
    H --> J[列引用]
    H --> K[函数调用]
    H --> L[子查询]
    H --> M[括号表达式]
```

**运算符优先级表**：

| 优先级 | 运算符 | 结合性 |
|--------|--------|--------|
| 1（最低） | OR | 左结合 |
| 2 | AND | 左结合 |
| 3 | NOT | 右结合 |
| 4 | =, <>, <, >, <=, >=, LIKE, IN | 左结合 |
| 5 | +, - | 左结合 |
| 6 | *, /, % | 左结合 |
| 7 | 一元运算符 -, NOT | 右结合 |
| 8（最高） | 括号、函数调用 | - |

## SQL 语法支持

DuckDB 支持 SQL 标准语法 + PostgreSQL 兼容语法：

```mermaid
graph TD
    A[DuckDB SQL 支持] --> B[标准 SQL]
    A --> C[PostgreSQL 兼容]
    A --> D[DuckDB 扩展]

    B --> E[SELECT/INSERT/UPDATE/DELETE]
    B --> F[CREATE TABLE/INDEX]
    B --> G[JOIN/子查询]
    B --> H[聚合函数]

    C --> I[ARRAY 类型]
    C --> J[GENERATED 列]
    C --> K[窗口函数]
    C --> L[LATERAL JOIN]

    D --> M[USING 子句简化]
    D --> N[LIST 结构]
    D --> O[STRUCT 类型]
    D --> P[MAP 类型]
```

## 与 PostgreSQL/SQLite 解析器对比

```mermaid
graph TD
    subgraph "DuckDB 解析器"
        A1[手写递归下降解析器] --> A2[优点: 灵活/易调试]
        A1 --> A3[缺点: 手写工作量大]
    end

    subgraph "PostgreSQL 解析器"
        B1[Flex + Bison<br/>scan.l + gram.y] --> B2[优点: 声明式/自动化]
        B1 --> B3[缺点: 规则冲突难调试]
    end

    subgraph "SQLite 解析器"
        C1[Lemon Parser<br/>parse.y] --> C2[优点: 无全局状态]
        C1 --> C3[缺点: 非标准工具]
    end
```

| 维度 | DuckDB | PostgreSQL | SQLite |
|------|--------|------------|--------|
| **解析器类型** | 手写递归下降 | Flex + Bison | Lemon Parser |
| **语法规则文件** | C++ 代码 | gram.y | parse.y |
| **词法分析** | 手写 Tokenizer | Flex (scan.l) | 内置 Tokenizer |
| **扩展性** | 高（直接修改代码） | 中（修改 gram.y） | 中（修改 parse.y） |
| **错误提示** | 精确（手写控制） | 中等 | 中等 |
| **性能** | 高 | 中（Bison 开销） | 高 |
| **维护性** | 中（需理解代码流程） | 高（声明式规则） | 中 |

## 错误处理

DuckDB 提供详细的错误报告：

```mermaid
graph TD
    A[Parser 错误] --> B[语法错误]
    A --> C[语义错误]

    B --> D[unexpected token]
    B --> E[expected X but got Y]
    B --> F[语法规则不匹配]

    C --> G[列不存在]
    C --> H[类型不匹配]
    C --> I[函数参数错误]

    J[错误信息结构] --> K[位置: 行/列]
    J --> L[错误类型]
    J --> M[上下文: 附近 SQL 片段]
    J --> N[修复提示]
```

**错误报告示例**：

```sql
SELECT id FORM users;
-- Error: Parser error: Expected FROM but got FORM
--        SELECT id FORM users
--                    ^
```

## 解析器扩展点

DuckDB 提供扩展机制允许自定义语法：

```mermaid
graph TD
    A[扩展点] --> B[自定义函数<br/>CREATE FUNCTION]
    A --> C[自定义类型<br/>CREATE TYPE]
    A --> D[自定义聚合<br/>CREATE AGGREGATE]
    A --> E[自定义宏<br/>CREATE MACRO]

    F[扩展流程] --> G[注册扩展]
    G --> H[解析时识别]
    H --> I[调用扩展回调]
```

## 要点总结

- DuckDB 使用**手写递归下降解析器**，避免依赖 YACC/Bison，提高灵活性和可维护性
- 解析流程：Tokenizer → Parser → AST，Token 流是中间表示
- AST 节点类型丰富：SelectStatement、InsertStatement、CreateStatement 等
- 运算符优先级通过解析函数调用栈实现，而非外部声明
- 错误处理精确，提供位置信息和修复提示
- 与 PG（Flex/Bison）和 SQLite（Lemon）相比，DuckDB 选择手写方案，强调灵活性和可调试性

## 思考题

1. 为什么 DuckDB 选择手写递归下降解析器而不是使用 YACC/Bison？这种选择在什么情况下是优势，什么情况下是劣势？
2. DuckDB 的 Tokenizer 如何处理 SQL 注入？解析阶段是否需要考虑安全性？
3. 如果要扩展 DuckDB 支持自定义 SQL 语法（如 `SELECT ... SAMPLE 10%`），应该修改哪些模块？
4. 手写解析器 vs 声明式语法规则（gram.y），哪个更容易支持复杂的 SQL 方言（如 PL/pgSQL 的控制流）？
