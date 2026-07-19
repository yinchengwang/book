# Parser SQL 解析器

## 学习目标

- 理解 PostgreSQL SQL 解析器的实现架构（lex/yacc → parse tree → query tree）
- 掌握 gram.y 与 scan.l 的工作方式与扩展点
- 了解解析器、分析器、重写器的关系与职责划分

## 核心概念

- **Scanner（lex）**：词法分析，把 SQL 文本切分成 Token
- **Parser（yacc）**：语法分析，构建 Parse Tree
- **Parse Tree（原始语法树）**：gram.y 规则生成的结构体（`SelectStmt`、`InsertStmt` 等）
- **Analyze（语义分析）**：绑定列名、类型检查、权限检查
- **Query Tree**：经过 `transformStmt` 转换后的查询结构（`Query` 结构体）
- **Rewrite（重写）**：应用规则系统（视图、规则）

## 解析流程总览

```mermaid
graph TD
    A[SQL 文本] --> B[Scanner scan.l]
    B --> C[Token Stream]
    C --> D[Parser gram.y]
    D --> E[Raw Parse Tree<br/>SelectStmt/InsertStmt]
    E --> F[Analyze<br/>parse_analyze]
    F --> G[Query Tree<br/>Query 结构体]
    G --> H[Rewrite<br/>QueryRewrite]
    H --> I[最终 Query<br/>送给 Planner]
```

## Scanner（词法分析）

`scan.l` 是 Flex 的输入文件，定义 Token：

```mermaid
graph LR
    A[SQL 文本] --> B[Scanner<br/>scan.l]
    B --> C[Token 流]

    subgraph "Token 类型"
        D1[关键字: SELECT/FROM/WHERE]
        D2[标识符: table_name/col]
        D3[常量: 123/'abc']
        D4[运算符: + - * /]
    end

    C --> D1
    C --> D2
    C --> D3
    C --> D4
```

**关键 Token 定义**（简化）：

```c
// scan.l 片段
%{
#include "parser/gramparse.h"
%}

%option 8bit noyywrap
%x xui xus xs xusval

KEYWORD     1100
IDENT       1200

%%

"SELECT"    { return SELECT; }
"FROM"      { return FROM; }
"WHERE"     { return WHERE; }
[0-9]+      { yylval.ival = atoi(yytext); return ICONST; }
[a-zA-Z_][a-zA-Z0-9_]*  {
                yylval.str = pstrdup(yytext);
                return IDENT;
            }
```

## Parser（语法分析）

`gram.y` 是 Bison 的输入文件，定义语法规则：

```mermaid
graph TD
    A[gram.y] --> B[规则定义]
    B --> C[SelectStmt 规则]
    B --> D[InsertStmt 规则]
    B --> E[CreateStmt 规则]

    subgraph "SelectStmt 简化规则"
        F[SelectStmt: SELECT opt_target_list<br/>from_clause where_clause]
        F --> G[→ SelectStmt 节点]
    end
```

**简化规则示例**：

```yacc
// gram.y 片段
SelectStmt:
            select_no_parens                    { $$ = $1; }
        |   select_with_parens                  { $$ = $1; }
        ;

select_no_parens:
            simple_select                       { $$ = $1; }
        |   set_clause                          { $$ = $1; }
        ;

simple_select:
            SELECT opt_distinct target_list
            from_clause where_clause
            group_clause having_clause
            window_clause
            {
                SelectStmt *n = makeNode(SelectStmt);
                n->distinctClause = $2;
                n->targetList = $3;
                n->fromClause = $4;
                n->whereClause = $5;
                n->groupClause = $6;
                n->havingClause = $7;
                n->windowClause = $8;
                $$ = (Node *)n;
            }
        ;
```

**Parse Tree 结构体**：

```mermaid
graph TD
    A[SelectStmt] --> B[distinctClause]
    A --> C[targetList: List of ResTarget]
    A --> D[fromClause: List of RangeVar]
    A --> E[whereClause: Expr]
    A --> F[groupClause: List]
    A --> G[havingClause: Expr]
    A --> H[sortClause: List]
    A --> I[limitOffset: Expr]
    A --> J[limitCount: Expr]
```

## Parse Tree → Query Tree

`parse_analyze` 把 Parse Tree 转换成 Query Tree：

```mermaid
flowchart TD
    A[parse_analyze] --> B[transformStmt]
    B --> C{Stmt 类型?}

    C -->|SELECT| D[transformSelectStmt]
    C -->|INSERT| E[transformInsertStmt]
    C -->|UPDATE| F[transformUpdateStmt]
    C -->|DELETE| G[transformDeleteStmt]

    D --> H[绑定 RangeVar<br/>→ RangeTblEntry]
    D --> I[解析 TargetList<br/>→ TargetEntry]
    D --> J[解析 WHERE<br/>→ qual]
    D --> K[类型检查<br/>表达式类型推断]

    E --> L[绑定目标表]
    E --> M[解析列列表]

    F --> N[解析 SET 子句]

    G --> O[解析条件]
```

**Query 结构体关键字段**：

```mermaid
graph TD
    A[Query] --> B[commandType: SELECT/INSERT/UPDATE/DELETE]
    A --> C[rtable: RangeTblEntry 列表<br/>FROM 子句的表]
    A --> D[jointree: FromExpr<br/>JOIN 结构]
    A --> E[targetList: TargetEntry 列表]
    A --> F[qual: Expr WHERE 条件]
    A --> G[groupClause: List]
    A --> H[sortClause: List]
    A --> I[limitOffset/limitCount]
```

## RangeTblEntry（范围表条目）

每个表、子查询、函数调用都会生成一个 `RangeTblEntry`：

```mermaid
graph LR
    A[FROM 子句] --> B[RangeTblEntry]
    B --> C[alias: 表别名]
    B --> D[relid: 表 OID]
    B --> E[subquery: 子查询 Query]
    B --> F[functions: 函数 RTE]
    B --> G[joinaliasvars: JOIN 列]
```

**绑定过程**：

```mermaid
sequenceDiagram
    participant Parse as Parse Tree
    participant Trans as transformStmt
    participant RTE as RangeTblEntry
    participant Query as Query Tree

    Parse->>Trans: FROM users u, orders o
    Trans->>RTE: 创建 RTE for users<br/>relid = users_oid
    Trans->>RTE: 创建 RTE for orders<br/>relid = orders_oid
    RTE->>Query: rtable = [users_RTE, orders_RTE]
    Note over Query: 后续列引用通过 RTE 查找
```

## 重写器（Rewrite）

Query Tree 生成后进入重写阶段：

```mermaid
flowchart TD
    A[QueryRewrite] --> B[Query查询]
    B --> C{有视图/规则?}
    C -->|是| D[应用重写规则]
    C -->|否| E[返回原 Query]
    D --> F[替换视图为子查询]
    D --> G[应用规则 Rule]
    D --> H[重写完成]
    H --> E

    E --> I[返回 Query 给 Planner]
```

**视图展开示例**：

```sql
CREATE VIEW v_users AS SELECT id, name FROM users WHERE status = 'active';

SELECT * FROM v_users WHERE id > 100;
```

重写后：

```sql
SELECT id, name FROM (SELECT id, name FROM users WHERE status = 'active') v_users WHERE id > 100;
```

## Parser Hook 扩展点

PG 提供多个 Hook 允许扩展解析器：

```mermaid
graph TD
    A[Parser Hook] --> B[process_parse_pre_command_hook]
    A --> C[process_parse_post_command_hook]
    A --> D[planner_hook]
    A --> E[ExecutorStart_hook]

    B --> F[在命令解析前拦截]
    C --> G[在命令解析后处理]
    D --> H[干预查询规划]
    E --> I[干预执行器]
```

**Hook 使用场景**：

- 审计：记录所有 SQL
- 权限增强：实现行级安全
- 查询改写：自动添加 WHERE 条件
- 自定义命令：识别非标准语法

## 解析错误处理

```mermaid
flowchart TD
    A[解析错误] --> B[语法错误]
    A --> C[语义错误]
    A --> D[权限错误]

    B --> E[yyerror 报错<br/>ERROR: syntax error]
    B --> F[位置指示]

    C --> G[列不存在<br/>ERROR: column "xyz" does not exist]
    C --> H[类型不匹配]

    D --> I[ERROR: permission denied]
```

**错误信息结构**：

```c
typedef struct ErrorData {
    int         elevel;         // ERROR / WARNING / NOTICE
    char       *message;        // 错误消息
    char       *detail;         // 详细信息
    char       *hint;           // 提示
    int         cursorpos;      // 光标位置
    const char *funcname;       // 函数名
    const char *filename;       // 文件名
    int         lineno;         // 行号
} ErrorData;
```

## 解析流程与缓存

PG 会缓存解析结果：

```mermaid
graph TD
    A[SQL 语句] --> B{在 Plan Cache?}
    B -->|是| C[直接返回 CachedPlan]
    B -->|否| D[完整解析流程]
    D --> E[Scanner → Parser]
    E --> F[Analyze]
    F --> G[Rewrite]
    G --> H[Cache Plan]
    H --> C
```

**Plan Cache 好处**：

- 避免重复解析（ Prepared Statement）
- 绑定变量参数化执行

## 要点总结

- Scanner（scan.l）负责词法分析，生成 Token 流
- Parser（gram.y）负责语法分析，构建 Parse Tree（SelectStmt 等）
- `parse_analyze` 把 Parse Tree 转成 Query Tree，绑定表名、列名、类型
- Rewrite 阶段展开视图、应用规则
- 解析过程可被 Hook 拦截，用于审计、权限等扩展
- Plan Cache 缓存解析结果，避免重复开销

## 思考题

1. 为什么 PG 把 Scanner 和 Parser 分开实现？直接用 yacc 处理文本有什么问题？
2. Parse Tree 和 Query Tree 的区别是什么？为什么需要两个阶段？
3. 如果要扩展 PG 支持自定义 SQL 语法（如 `SELECT ... USING INDEX hint`），应该修改哪个环节？