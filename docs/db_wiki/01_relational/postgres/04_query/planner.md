# Planner 查询规划器

## 学习目标

- 理解 PostgreSQL Planner 的三阶段架构（逻辑规划 → 物理规划 → 代价估算）
- 掌握路径生成、路径选择、GEQO 遗传算法的工作原理
- 熟悉统计信息（pg_statistic）在代价估算中的作用

## 核心概念

- **Planner**：查询优化器，把 Query Tree 转成 Plan Tree
- **Path**：一种可能的执行方式（顺序扫描、索引扫描、Hash Join 等）
- **Plan**：最终选定的执行计划节点（SeqScan、IndexScan、HashJoin 等）
- **Cost**：执行代价估算（startup_cost + total_cost）
- **Statistics**：表级、列级统计信息，用于估算行数
- **GEQO**：遗传算法优化器，处理多表 JOIN 顺序
- **EquivalenceClass**：等价类，用于推导等值条件

## Planner 三阶段

```mermaid
graph TD
    A[Query Tree] --> B[逻辑规划<br/>生成 Paths]
    B --> C[物理规划<br/>选择最优 Path]
    C --> D[生成 Plan Tree]

    B --> E[生成所有可能的 Path]
    E --> F[RelOptInfo 存储路径]
    F --> G[计算每个 Path 的 Cost]
    G --> H[选择最低 Cost Path]
    H --> D
```

### 阶段 1：逻辑规划

`query_planner` 负责生成所有可能的 Path：

```mermaid
flowchart TD
    A[query_planner] --> B[预处理<br/>pull_up_sublinks]
    B --> C[初始化 RelOptInfo]
    C --> D[生成基础路径<br/>Seq Scan / Index Scan]
    D --> E[生成 Join 路径<br/>尝试所有组合]
    E --> F[存储到 RelOptInfo]
```

### 阶段 2：物理规划

`make_one_rel` 选择最优 Path：

```mermaid
flowchart TD
    A[make_one_rel] --> B[遍历所有 RelOptInfo]
    B --> C[比较 Path 的 total_cost]
    C --> D[选择 cheapest_path]
    D --> E[生成 Plan 节点<br/>create_plan]
```

### 阶段 3：代价估算

`cost_qual` 计算每个操作的开销：

```mermaid
graph LR
    A[Cost 公式] --> B[startup_cost<br/>启动代价]
    A --> C[total_cost<br/>总代价]
    C --> D[= startup_cost + 执行代价]
```

## Path 类型

```mermaid
graph TD
    A[Path 类型] --> B[扫描类]
    A --> C[Join 类]
    A --> D[其他类]

    B --> B1[SeqPath<br/>顺序扫描]
    B --> B2[IndexPath<br/>索引扫描]
    B --> B3[BitmapHeapPath<br/>位图扫描]
    B --> B4[TidPath<br/>TID 扫描]
    B --> B5[ForeignPath<br/>外部表]

    C --> C1[NestLoopPath<br/>嵌套循环]
    C --> C2[HashPath<br/>Hash Join]
    C --> C3[MergePath<br/>Merge Join]

    D --> D1[SortPath<br/>排序]
    D --> D2[AggPath<br/>聚合]
    D --> D3[UniquePath<br/>去重]
    D --> D4[LimitPath<br/>Limit]
```

## 路径生成示例

```mermaid
sequenceDiagram
    participant Q as Query
    participant P as Planner
    participant R as RelOptInfo
    participant S as Statistics

    Q->>P: SELECT * FROM t WHERE id=1
    P->>R: 创建 RelOptInfo(t)
    P->>S: 获取统计信息<br/>pg_statistic
    S-->>P: relpages=100, reltuples=10000
    P->>P: 估算条件选择率
    P->>P: 生成 SeqPath cost=1000
    P->>P: 生成 IndexPath cost=10
    P->>P: 比较 total_cost
    P-->>Q: 返回 IndexScan Plan
```

## 代价估算公式

PG 使用 CPU + IO 双维度估算：

```mermaid
graph TD
    A[总代价] --> B[CPU 代价]
    A --> C[IO 代价]

    B --> D[处理每行的 CPU 时间<br/>cpu_tuple_cost = 0.01]
    B --> E[操作符代价<br/>cpu_operator_cost = 0.0025]

    C --> F[随机 IO<br/>random_page_cost = 4.0]
    C --> G[顺序 IO<br/>seq_page_cost = 1.0]

    D --> H[行数 × cpu_tuple_cost]
    E --> I[操作次数 × cpu_operator_cost]
    F --> J[页面数 × random_page_cost]
    G --> K[页面数 × seq_page_cost]
```

**核心参数**：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `seq_page_cost` | 1.0 | 顺序读取一个页面的代价 |
| `random_page_cost` | 4.0 | 随机读取一个页面的代价 |
| `cpu_tuple_cost` | 0.01 | 处理一行数据的 CPU 代价 |
| `cpu_index_tuple_cost` | 0.005 | 处理一个索引项的代价 |
| `cpu_operator_cost` | 0.0025 | 处理一个操作符的代价 |
| `parallel_tuple_cost` | 0.1 | 并行传递一行的代价 |
| `parallel_setup_cost` | 1000 | 并行初始化代价 |

## Join 顺序选择

PG 对 JOIN 采用动态规划 + 遗传算法（GEQO）：

```mermaid
flowchart TD
    A[Join 规划] --> B{表数量 ≤ 12?}
    B -->|是| C[动态规划<br/>枚举所有组合]
    B -->|否| D[GEQO 遗传算法<br/>近似最优]

    C --> E[生成所有 Join 路径]
    E --> F[计算每条路径 Cost]
    F --> G[选择 cheapest]

    D --> H[初始化种群]
    H --> I[交叉/变异]
    I --> J[选择适应度高的]
    J --> K{满足终止条件?}
    K -->|否| I
    K -->|是| G
```

### GEQO 遗传算法

GEQO 用于处理多表 JOIN（>12 表）：

```mermaid
graph TD
    A[GEQO 流程] --> B[编码: 表序列<br/>如 1-3-2-4]
    B --> C[初始化种群<br/>随机生成多个序列]
    C --> D[评估适应度<br/>计算 Join Cost]
    D --> E[选择<br/>保留优秀个体]
    E --> F[交叉<br/>合并两个序列]
    F --> G[变异<br/>随机交换位置]
    G --> H{迭代次数达阈值?}
    H -->|否| D
    H -->|是| I[返回最优序列]
```

**关键参数**：

- `geqo_threshold`：默认 12，触发 GEQO 的表数量阈值
- `geqo_effort`：默认 5，搜索努力程度（1-10）
- `geqo_pool_size`：种群大小
- `geqo_generations`：迭代代数

## 统计信息

PG 存储表级和列级统计信息：

```mermaid
graph TD
    A[pg_statistic] --> B[stadistinct<br/>不同值数量]
    A --> C[stanullfrac<br/>NULL 比例]
    A --> D[stawidth<br/>平均宽度]
    A --> E[stadistinct<br/>唯一值比例]
    A --> F[most_common_vals<br/>MCV 频繁值]
    A --> G[most_common_freqs<br/>MCV 频率]
    A --> H[histogram_bounds<br/>直方图]

    I[pg_stats 视图] --> A
```

**选择率估算示例**：

```sql
-- 假设 id 列有 100 个唯一值
SELECT * FROM t WHERE id = 1;
-- 选择率 ≈ 1/100 = 0.01（如果 id 均匀分布）
-- 或从 MCV 读取频率（如果分布不均匀）

SELECT * FROM t WHERE id > 50;
-- 选择率 ≈ (100-50)/100 = 0.5（使用直方图估算）
```

```mermaid
flowchart TD
    A[估算选择率] --> B{有 MCV?}
    B -->|是| C[查 MCV 频率]
    B -->|否| D{有 histogram?}
    D -->|是| E[从 histogram 估算]
    D -->|否| F[用默认选择率<br/>如 = 条件: 0.003]

    C --> G[选择率 = MCV_freq]
    E --> H[选择率 = (边界比例)]
```

## 等价类（EquivalenceClass）

等价类用于推导等值条件：

```sql
SELECT * FROM t1 JOIN t2 ON t1.id = t2.id WHERE t1.id = 100;
-- 等价类: {t1.id, t2.id, 100}
-- 推导: t2.id = 100
```

```mermaid
graph LR
    A[等值条件] --> B[EquivalenceClass]
    B --> C[t1.id]
    B --> D[t2.id]
    B --> E[常量 100]

    F[推导] --> G[t2.id = 100<br/>可用于索引扫描]
```

## Plan Tree 结构

选定的 Path 转换成 Plan 节点：

```mermaid
graph TD
    A[Plan 节点] --> B[SeqScan]
    A --> C[IndexScan]
    A --> D[BitmapHeapScan]
    A --> E[NestLoop]
    A --> F[HashJoin]
    A --> G[MergeJoin]
    A --> H[Sort]
    A --> I[Aggregate]
    A --> J[Limit]

    B --> K[targetlist: List<br/>qual: List<br/>scanrelid: Index]
```

**Plan Tree 示例**：

```sql
EXPLAIN SELECT * FROM users WHERE id = 100;

Index Scan using users_pkey on users  (cost=0.28..8.29 rows=1 width=100)
  Index Cond: (id = 100)
```

```mermaid
graph TD
    A[IndexScan] --> B[targetlist: id, name, ...]
    A --> C[indexqual: id=100]
    A --> D[scanrelid: users OID]
```

## 并行查询

PG 9.6+ 支持并行执行：

```mermaid
graph TD
    A[Parallel Seq Scan] --> B[Gather 节点]
    B --> C[Worker 1<br/>扫描部分页面]
    B --> D[Worker 2<br/>扫描部分页面]
    B --> E[Worker N<br/>...]
    C --> F[合并结果]
    D --> F
    E --> F
```

**并行参数**：

- `max_parallel_workers_per_gather`：默认 2
- `parallel_setup_cost`：默认 1000
- `parallel_tuple_cost`：默认 0.1

## Planner Hint

PG 默认不支持 Hint，但可通过 `pg_hint_plan` 扩展：

```sql
/*+ SeqScan(users) IndexScan(users_pkey) */
SELECT * FROM users WHERE id = 100;
```

## 要点总结

- Planner 分三阶段：逻辑规划（生成 Path）→ 物理规划（选 Path）→ 生成 Plan
- Cost = CPU 代价 + IO 代价，基于 `pg_statistic` 统计信息估算
- 多表 JOIN 使用动态规划（≤12 表）或 GEQO 遗传算法（>12 表）
- 等价类用于推导等值条件，传递常量到其他表
- 并行查询通过 Gather 节点协调多个 Worker

## 思考题

1. 为什么 PG 选择基于代价的优化器（CBO）而非基于规则的优化器（RBO）？两种方案各有什么优劣？
2. GEQO 遗传算法是近似算法，不保证全局最优。为什么 PG 选择 GEQO 而不是穷举？
3. `random_page_cost = 4.0` 是默认值，在 SSD 上是否应该调小？为什么？