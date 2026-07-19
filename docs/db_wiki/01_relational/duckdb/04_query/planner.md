# Planner 查询计划器

## 学习目标

- 理解 DuckDB 查询计划器的四阶段架构（Binder → Logical Planner → Optimizer → Physical Planner）
- 掌握 Cascades 框架的优化器设计原理
- 对比 DuckDB 与 PostgreSQL 的优化器架构差异（Cascades vs System-R）

## 核心概念

- **Binder**：名称解析、类型检查、将 AST 转换为绑定后的逻辑计划
- **Logical Planner**：生成逻辑查询计划（Logical Plan）
- **Optimizer**：基于 Cascades 框架的查询优化器，应用规则优化
- **Physical Planner**：将逻辑计划转换为物理执行计划
- **Cascades Framework**：基于规则的搜索框架，探索等价计划空间
- **Cost Model**：代价模型，估算不同执行计划的代价

## 计划器四阶段架构

```mermaid
graph TD
    A[AST<br/>解析器输出] --> B[Binder<br/>名称解析/类型检查]
    B --> C[Logical Planner<br/>生成逻辑计划]
    C --> D[Optimizer<br/>Cascades 优化]
    D --> E[Physical Planner<br/>逻辑→物理转换]
    E --> F[Physical Plan<br/>执行器输入]

    style A fill:#e1f5ff
    style B fill:#fff4e1
    style C fill:#fff4e1
    style D fill:#fff4e1
    style E fill:#fff4e1
    style F fill:#e8f5e9
```

## Binder 绑定器

Binder 负责 AST 的名称解析和类型检查：

```mermaid
flowchart TD
    A[Binder 入口] --> B[绑定表名]
    B --> C[绑定列名]
    C --> D[类型检查]
    D --> E[解析子查询]
    E --> F[返回绑定后计划]

    B --> G[查 Catalog<br/>表名 → 表对象]
    C --> H[查 Schema<br/>列名 → 列对象]
    D --> I[类型兼容检查<br/>隐式类型转换]
    E --> J[递归绑定子查询]
```

**Binder 职责**：

| 职责 | 说明 |
|------|------|
| **名称解析** | 将表名、列名绑定到 Catalog 中的实际对象 |
| **类型检查** | 检查表达式类型是否合法，必要时进行隐式类型转换 |
| **作用域管理** | 管理子查询、CTE、JOIN 的作用域，解决列引用歧义 |
| **别名处理** | 处理表别名、列别名，建立别名映射表 |
| **参数绑定** | 绑定 Prepared Statement 的参数位置 |

**绑定示例**：

```sql
SELECT u.name, o.total FROM users u JOIN orders o ON u.id = o.user_id;
```

```mermaid
sequenceDiagram
    participant AST as AST
    participant Binder as Binder
    participant Catalog as Catalog
    participant Plan as Bound Plan

    AST->>Binder: SelectStatement
    Binder->>Catalog: 查表 users
    Catalog-->>Binder: 表对象 users, 列: id, name, ...
    Binder->>Catalog: 查表 orders
    Catalog-->>Binder: 表对象 orders, 列: user_id, total, ...
    Binder->>Binder: 绑定列名 u.name → users.name
    Binder->>Binder: 绑定列名 o.total → orders.total
    Binder->>Binder: 类型检查 JOIN 条件
    Binder-->>Plan: 返回 Bound Select
```

## Logical Planner 逻辑计划器

Logical Planner 将绑定后的 AST 转换为逻辑算子树：

```mermaid
graph TD
    A[逻辑算子类型] --> B[LogicalGet<br/>表扫描]
    A --> C[LogicalFilter<br/>过滤]
    A --> D[LogicalProjection<br/>投影]
    A --> E[LogicalJoin<br/>连接]
    A --> F[LogicalAggregate<br/>聚合]
    A --> G[LogicalSort<br/>排序]
    A --> H[LogicalLimit<br/>Limit]
    A --> I[LogicalDistinct<br/>去重]
```

**逻辑计划示例**：

```sql
SELECT name, SUM(amount) FROM orders WHERE amount > 100 GROUP BY name;
```

```mermaid
graph TD
    A[LogicalProjection<br/>name, SUM] --> B[LogicalAggregate<br/>GROUP BY name]
    B --> C[LogicalFilter<br/>amount > 100]
    C --> D[LogicalGet<br/>orders]
```

## Optimizer 优化器

DuckDB 使用**基于 Cascades 框架的优化器**：

```mermaid
flowchart TD
    A[Optimizer 入口] --> B[规则注册]
    B --> C[探索等价计划]
    C --> D[应用优化规则]
    D --> E[代价估算]
    E --> F[选择最优计划]

    G[优化规则类型] --> H[逻辑规则<br/>等价变换]
    G --> I[物理规则<br/>实现选择]

    H --> J[谓词下推]
    H --> K[列裁剪]
    H --> L[子查询展开]
    H --> M[Join 重排序]

    I --> N[Join 物理选择<br/>Hash/Merge/NL]
    I --> O[聚合物理选择<br/>Hash/Sort]
```

### Cascades 框架核心概念

```mermaid
graph TD
    A[Cascades 框架] --> B[Expression<br/>逻辑/物理表达式]
    A --> C[Group<br/>等价表达式集合]
    A --> D[Rule<br/>转换规则]
    A --> E[Memo<br/>记忆化搜索空间]

    B --> F[Logical Expression<br/>逻辑算子]
    B --> G[Physical Expression<br/>物理算子]

    C --> H[同一 Group 内的表达式<br/>等价可互换]

    D --> I[Transformation Rule<br/>逻辑→逻辑]
    D --> J[Implementation Rule<br/>逻辑→物理]
```

**Cascades 工作流程**：

```mermaid
flowchart TD
    A[输入逻辑计划] --> B[初始 Group]
    B --> C[应用 Transformation Rules]
    C --> D[生成等价逻辑计划]
    D --> E[存入 Memo]

    E --> F[应用 Implementation Rules]
    F --> G[生成物理计划候选]
    G --> H[计算代价]

    H --> I[选择最低代价计划]
    I --> J[输出最优物理计划]
```

### 优化规则详解

```mermaid
graph TD
    A[优化规则] --> B[谓词下推<br/>Filter Pushdown]
    A --> C[列裁剪<br/>Column Pruning]
    A --> D[子查询展开<br/>Subquery Unnesting]
    A --> E[Join 重排序<br/>Join Reordering]
    A --> F[常量折叠<br/>Constant Folding]
    A --> G[公共表达式消除<br/>CSE Elimination]

    B --> H[将 Filter 尽可能下推<br/>靠近数据源]

    C --> I[移除未使用的列<br/>减少 I/O]

    D --> J[将子查询转为 Join<br/>提高优化空间]

    E --> K[根据统计信息<br/>选择最优 Join 顺序]

    F --> L[编译时计算常量表达式]

    G --> M[消除重复计算]
```

### 代价模型

DuckDB 的代价模型基于统计信息估算：

```mermaid
graph TD
    A[代价模型] --> B[Cardinality 估算<br/>行数估计]
    A --> C[Cost 估算<br/>执行代价]

    B --> D[基数估计<br/>选择率 × 表行数]
    B --> E[直方图<br/>范围查询估算]
    B --> F[MCV<br/>高频值估算]

    C --> G[CPU 代价<br/>运算开销]
    C --> H[IO 代价<br/>扫描开销]
    C --> I[内存代价<br/>Hash/Sort 内存]

    G --> J[元组处理代价]
    H --> K[页面读取代价]
```

**代价计算公式**：

```
总代价 = 启动代价 + 执行代价
执行代价 = CPU 代价 + IO 代价 + 内存代价
```

| 代价因子 | 说明 |
|----------|------|
| **tuple_cost** | 处理一行数据的代价 |
| **scan_cost** | 顺序扫描一个页面的代价 |
| **index_scan_cost** | 索引扫描的代价 |
| **hash_build_cost** | 构建 Hash Table 的代价 |
| **hash_probe_cost** | 探测 Hash Table 的代价 |
| **sort_cost** | 排序代价 |

### Join 重排序

DuckDB 使用动态规划 + 代价估算选择最优 Join 顺序：

```mermaid
flowchart TD
    A[Join 重排序] --> B[生成所有可能的 Join 顺序]
    B --> C[计算每个顺序的代价]
    C --> D[选择最低代价顺序]

    E[Join 顺序优化] --> F[小表驱动大表]
    E --> G[高选择性条件优先]
    E --> H[Hash Join 优先于 NL]
```

**示例**：

```sql
SELECT * FROM a JOIN b ON a.id = b.a_id JOIN c ON b.id = c.b_id;
```

```mermaid
graph TD
    A[可能的 Join 顺序] --> B[a ⋈ b ⋈ c]
    A --> C[a ⋈ c ⋈ b]
    A --> D[b ⋈ a ⋈ c]
    A --> E[b ⋈ c ⋈ a]
    A --> F[c ⋈ a ⋈ b]
    A --> G[c ⋈ b ⋈ a]

    B --> H[代价计算]
    C --> H
    D --> H
    E --> H
    F --> H
    G --> H

    H --> I[选择最低代价顺序]
```

## Physical Planner 物理计划器

Physical Planner 将逻辑计划转换为物理计划：

```mermaid
graph TD
    A[逻辑算子] --> B[物理算子映射]

    B --> C[LogicalGet → PhysicalTableScan<br/>表扫描]
    B --> D[LogicalFilter → PhysicalFilter<br/>过滤]
    B --> E[LogicalJoin → PhysicalHashJoin<br/>Hash Join]
    B --> F[LogicalAggregate → PhysicalHashAggregate<br/>Hash 聚合]
    B --> G[LogicalSort → PhysicalSort<br/>排序]
    B --> H[LogicalLimit → PhysicalLimit<br/>Limit]

    I[物理选择策略] --> J[根据代价选择<br/>最优物理实现]
    I --> K[考虑数据分布<br/>选择并行策略]
```

**物理算子类型**：

```mermaid
graph TD
    A[物理算子] --> B[扫描类]
    A --> C[连接类]
    A --> D[聚合类]
    A --> E[排序类]

    B --> B1[PhysicalTableScan<br/>顺序扫描]
    B --> B2[PhysicalIndexScan<br/>索引扫描]

    C --> C1[PhysicalHashJoin<br/>Hash Join]
    C --> C2[PhysicalNestedLoopJoin<br/>嵌套循环]
    C --> C3[PhysicalMergeJoin<br/>归并连接]

    D --> D1[PhysicalHashAggregate<br/>Hash 聚合]
    D --> D2[PhysicalSortedAggregate<br/>排序聚合]

    E --> E1[PhysicalSort<br/>内存/磁盘排序]
```

## 与 PostgreSQL 优化器对比

```mermaid
graph TD
    subgraph "DuckDB Optimizer"
        A1[Cascades 框架] --> A2[规则驱动探索]
        A1 --> A3[Memo 记忆化]
        A1 --> A4[统一规则应用]

        A5[优势] --> A6[搜索空间可控]
        A5 --> A7[规则易于扩展]
    end

    subgraph "PostgreSQL Optimizer"
        B1[System-R 风格] --> B2[动态规划]
        B1 --> B3[GEQO 遗传算法]
        B1 --> B4[显式路径生成]

        B5[优势] --> B6[成熟稳定]
        B5 --> B7[OLTP 场景优化]
    end
```

| 维度 | DuckDB (Cascades) | PostgreSQL (System-R) |
|------|-------------------|----------------------|
| **优化框架** | Cascades | System-R + GEQO |
| **搜索策略** | 规则驱动探索 | 动态规划 + 遗传算法 |
| **Join 重排序** | Cascades 自动探索 | 显式路径生成 + GEQO |
| **规则扩展** | 易于添加新规则 | 需修改 planner 代码 |
| **代价模型** | 基于统计信息估算 | 基于统计信息 + 参数 |
| **并行查询** | 自动并行化 | 显式 Parallel Path |
| **适用场景** | OLAP 分析查询 | OLTP 事务查询 |

## 优化器统计信息

DuckDB 维护表级和列级统计信息：

```mermaid
graph TD
    A[统计信息] --> B[表级统计]
    A --> C[列级统计]

    B --> D[行数 card]
    B --> E[字节数 bytes]

    C --> F[最小值 min]
    C --> G[最大值 max]
    C --> H[唯一值数 distinct]
    C --> I[NULL 比例 null_frac]
    C --> J[直方图 histogram]
    C --> K[高频值 MCV]

    L[统计信息收集] --> M[ANALYZE 命令]
    M --> N[采样统计]
    N --> O[更新 Catalog]
```

**选择率估算示例**：

```sql
-- 等值条件
SELECT * FROM t WHERE col = 'value';
-- 选择率 = 1 / distinct_count

-- 范围条件
SELECT * FROM t WHERE col > 100;
-- 选择率 = (max - 100) / (max - min)
```

## 优化示例

### 谓词下推

```sql
SELECT name FROM users WHERE age > 30;
```

```mermaid
graph TD
    subgraph "优化前"
        A1[LogicalProjection] --> B1[LogicalFilter<br/>age > 30]
        B1 --> C1[LogicalGet<br/>users]
    end

    subgraph "优化后"
        A2[LogicalProjection] --> B2[LogicalGet<br/>users WHERE age > 30]
    end
```

### Join 重排序

```sql
SELECT * FROM small_table s JOIN large_table l ON s.id = l.s_id;
```

```mermaid
graph TD
    subgraph "优化前"
        A1[LogicalJoin<br/>small ⋈ large]
    end

    subgraph "优化后"
        B1[最优顺序选择]
        B1 --> C1[Hash Join<br/>small 作为构建表]
    end
```

## 要点总结

- DuckDB 优化器分为四阶段：Binder → Logical Planner → Optimizer → Physical Planner
- 使用**Cascades 框架**进行优化，通过规则探索等价计划空间
- 优化规则包括谓词下推、列裁剪、子查询展开、Join 重排序等
- 代价模型基于统计信息估算行数和执行代价
- 与 PostgreSQL 的 System-R 风格相比，Cascades 更易扩展规则，适合 OLAP 场景
- 统计信息是优化的基础，ANALYZE 命令收集并更新统计

## 思考题

1. Cascades 框架相比传统的动态规划优化器（如 PostgreSQL 的 System-R）有何优势？在什么情况下劣势？
2. 谓词下推为何能显著提升性能？是否存在谓词下推反而降低性能的情况？
3. Join 重排序是一个 NP 问题，DuckDB 如何在合理时间内找到较好的 Join 顺序？
4. 如果统计信息过期，优化器会做出错误的决策。DuckDB 如何保证统计信息的时效性？
