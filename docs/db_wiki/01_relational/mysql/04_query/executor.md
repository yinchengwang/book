# Executor 执行器

## 学习目标

- 理解 MySQL 执行器的迭代器模型架构与 PostgreSQL 火山模型的异同
- 掌握 MySQL 执行器的核心组件：Table Scan Iterator、Index Scan Iterator、Sort Iterator、Join Iterator、Aggregation Iterator
- 熟悉 `handler` 接口的调用方式与存储引擎交互细节
- 掌握 Using filesort、Using temporary、ICP、MRR 的原理与应用场景

## 核心概念

- **Iterator Model**：执行器采用迭代器模型，每个节点实现 `Read()` 接口返回一行
- **handler 接口**：Server 层与存储引擎的抽象接口，屏蔽不同存储引擎的差异
- **Using filesort**：MySQL 排序操作的实现，使用 sort_buffer 或临时文件
- **Using temporary**：使用临时表的情况（GROUP BY、DISTINCT、UNION、子查询）
- **ICP（Index Condition Pushdown）**：将 WHERE 条件中与索引相关的部分下推到存储引擎
- **MRR（Multi-Range Read）**：对二级索引范围扫描，先排序主键再回表，将随机 IO 转为顺序 IO

## 执行器组件架构

```mermaid
graph TD
    A[Server 执行器<br/>Iterator Model] --> B[Table Scan Iterator]
    A --> C[Index Scan Iterator]
    A --> D[Sort Iterator]
    A --> E[Join Iterator]
    A --> F[Aggregation Iterator]
    A --> G[Limit Iterator]
    A --> H[Subquery Iterator]

    B --> I[handler::ha_rnd_init<br/>handler::ha_rnd_next]
    C --> J[handler::ha_index_init<br/>handler::ha_index_read]

    I --> K[handler 接口]
    J --> K
    K --> L[InnoDB 存储引擎<br/>ha_innodb.cc]
    L --> M[Buffer Pool]
    L --> N[B+Tree 索引]
    L --> O[聚簇索引]
    L --> P[二级索引]
```

### handler 接口

`handler` 是 Server 层与存储引擎的抽象接口，所有存储引擎（InnoDB、MyISAM、Memory）都实现此接口。

```mermaid
graph LR
    A[Server 执行器] --> B[handler 接口]
    
    subgraph "handler 核心方法"
        C[表操作]
        D[全表扫描]
        E[索引扫描]
        F[行操作]
    end
    
    C --> C1[ha_open<br/>打开表]
    C --> C2[ha_close<br/>关闭表]
    
    D --> D1[ha_rnd_init<br/>初始化全表扫描]
    D --> D2[ha_rnd_next<br/>读取下一行]
    D --> D3[ha_rnd_end<br/>结束扫描]
    
    E --> E1[ha_index_init<br/>初始化索引扫描]
    E --> E2[ha_index_read<br/>按索引读取]
    E --> E3[ha_index_next<br/>索引下一行]
    E --> E4[ha_index_end<br/>结束索引扫描]
    
    F --> F1[ha_write_row<br/>插入行]
    F --> F2[ha_update_row<br/>更新行]
    F --> F3[ha_delete_row<br/>删除行]
    
    B --> C
    B --> D
    B --> E
    B --> F
```

### handler 接口调用示例

```mermaid
sequenceDiagram
    participant Executor as 执行器
    participant Handler as handler 接口
    participant InnoDB as InnoDB 引擎
    participant Buffer as Buffer Pool
    participant BTree as B+Tree 索引

    Note over Executor: 全表扫描场景
    Executor->>Handler: ha_rnd_init()
    Handler->>InnoDB: innobase_init()
    InnoDB-->>Handler: 准备完成
    
    loop 每一行
        Executor->>Handler: ha_rnd_next()
        Handler->>InnoDB: innobase_rnd_next()
        InnoDB->>Buffer: 读取页面
        Buffer->>BTree: 扫描聚簇索引
        BTree-->>Buffer: 返回行数据
        Buffer-->>InnoDB: 返回行
        InnoDB-->>Handler: 返回行
        Handler-->>Executor: 返回一行
    end
    
    Executor->>Handler: ha_rnd_end()
```

## Table Scan Iterator

全表扫描是最简单的执行方式，遍历聚簇索引的所有叶子节点。

```mermaid
flowchart TD
    A[Table Scan Iterator] --> B[初始化<br/>ha_rnd_init]
    B --> C[循环读取<br/>ha_rnd_next]
    C --> D{还有数据?}
    D -->|是| E[返回一行]
    E --> F[检查 WHERE 条件]
    F --> G{满足条件?}
    G -->|是| H[返回给父节点]
    G -->|否| C
    D -->|否| I[结束扫描<br/>ha_rnd_end]
```

**全表扫描的代价**：

- IO 代价：读取聚簇索引的所有叶子页面（顺序 IO）
- CPU 代价：处理每一行数据（检查 WHERE 条件）
- 无回表代价：聚簇索引叶子节点包含完整行数据

## Index Scan Iterator

索引扫描分为两种：**覆盖索引扫描**和**非覆盖索引扫描（需要回表）**。

```mermaid
flowchart TD
    A[Index Scan Iterator] --> B{覆盖索引?}
    B -->|是| C[仅扫描二级索引]
    B -->|否| D[扫描二级索引<br/>+ 回表聚簇索引]
    
    C --> E[ha_index_init<br/>初始化索引扫描]
    E --> F[ha_index_next<br/>遍历索引叶子]
    F --> G[返回索引列数据<br/>无需回表]
    
    D --> H[ha_index_init]
    H --> I[ha_index_next<br/>获取主键值]
    I --> J[ha_index_read<br/>回表聚簇索引]
    J --> K[返回完整行数据]
```

### 覆盖索引 vs 非覆盖索引的查询路径对比

```mermaid
graph TD
    subgraph "覆盖索引扫描（Using index）"
        A1[SELECT id, name FROM users<br/>WHERE name = 'Alice'<br/>索引: idx_name]
        A1 --> B1[二级索引 idx_name<br/>叶子节点: name + id]
        B1 --> C1[直接返回 id, name<br/>无需回表]
    end
    
    subgraph "非覆盖索引扫描（需要回表）"
        A2[SELECT * FROM users<br/>WHERE name = 'Alice'<br/>索引: idx_name]
        A2 --> B2[二级索引 idx_name<br/>获取主键 id]
        B2 --> C2[回表聚簇索引<br/>通过 id 查找完整行]
        C2 --> D2[返回完整行数据<br/>包含 age, email 等]
    end
```

### 回表的代价

```mermaid
graph LR
    A[回表代价分析] --> B[随机 IO<br/>二级索引到聚簇索引]
    A --> C[每次回表<br/>一次 B+Tree 查找<br/>2-4 层]
    
    D[减少回表的策略] --> E[覆盖索引<br/>索引包含所有查询列]
    D --> F[多列索引<br/>减少回表次数]
    D --> G[ICP 下推<br/>减少回表的行数]
```

## Sort Iterator

MySQL 的排序操作使用 `sort_buffer`（内存缓冲区），当内存不足时使用临时文件。

### Using filesort 的两种排序策略

```mermaid
graph TD
    A[Using filesort] --> B[全字段排序<br/>Full Field Sort]
    A --> C[Rowid 排序<br/>Rowid Sort]
    
    B --> D[sort_buffer 存储所有字段]
    B --> E[内存排序后<br/>直接返回结果]
    B --> F[优点: 无需回表]
    B --> G[缺点: 占用内存大]
    
    C --> H[sort_buffer 只存储<br/>排序字段 + 主键]
    C --> I[排序后根据主键<br/>回表读取完整行]
    C --> J[优点: 节省内存]
    C --> K[缺点: 需要额外回表]
```

### 全字段排序流程

```mermaid
flowchart TD
    A[全字段排序] --> B[初始化 sort_buffer<br/>大小: sort_buffer_size]
    B --> C[读取行到 sort_buffer]
    C --> D{buffer 满?}
    D -->|是| E[排序 buffer 中数据]
    E --> F[写入临时文件]
    F --> G[清空 buffer]
    G --> C
    D -->|否| H{读取完成?}
    H -->|否| C
    H -->|是| I{有临时文件?}
    I -->|是| J[多路归并排序<br/>merge sort]
    I -->|否| K[内存排序<br/>quicksort]
    J --> L[返回有序行]
    K --> L
```

### Rowid 排序流程

```mermaid
flowchart TD
    A[Rowid 排序] --> B[初始化 sort_buffer]
    B --> C[读取排序字段 + 主键<br/>到 sort_buffer]
    C --> D{buffer 满?}
    D -->|是| E[排序后写入临时文件]
    E --> C
    D -->|否| F{读取完成?}
    F -->|否| C
    F -->|是| G[内存排序或文件归并]
    G --> H[遍历排序结果]
    H --> I[根据主键回表<br/>读取完整行]
    I --> J[返回有序行]
```

### 两种排序策略的选择

```mermaid
flowchart TD
    A[排序策略选择] --> B{查询字段总长度<br/>< max_length_for_sort_data?}
    B -->|是| C[全字段排序<br/>避免回表]
    B -->|否| D[Rowid 排序<br/>节省内存]
    
    C --> E[适用于: 查询列少]
    D --> F[适用于: 查询列多<br/>或列宽大]
```

**关键参数**：

- `sort_buffer_size`：排序缓冲区大小（默认 256KB）
- `max_length_for_sort_data`：超过此值时使用 Rowid 排序（默认 1024 字节）

## Join Iterator

MySQL 的 Join 执行依赖 Join Buffer，主要实现 Block Nested Loop Join 和 Batched Key Access Join。

### Join 执行流程

```mermaid
sequenceDiagram
    participant Outer as 外表 Iterator
    participant Buffer as Join Buffer
    participant Inner as 内表 Iterator
    participant Result as 结果集

    Note over Outer: Build 阶段
    loop 外表每批
        Outer->>Buffer: 读取一批外表行
        Buffer->>Buffer: 存储外表行
    end
    
    Note over Inner: Probe 阶段
    loop 内表每行
        Inner->>Buffer: 读取内表当前行
        Buffer->>Buffer: 与 Buffer 中外表行匹配
        Buffer-->>Result: 匹配行加入结果
    end
```

### Join Buffer 管理

```mermaid
graph TD
    A[Join Buffer] --> B[join_buffer_size<br/>默认 256KB]
    B --> C[每批次大小 = join_buffer_size]
    
    D[Join Buffer 生命周期] --> E[每个 Join 分配一个 Buffer]
    E --> F[查询结束后释放]
    
    G[优化建议] --> H[增大 join_buffer_size<br/>减少内表扫描次数]
    G --> I[确保 Join 列有索引<br/>使用 INLJ 代替 BNLJ]
```

## Aggregation Iterator

聚合操作分为 `Stream Aggregation`（流式聚合）和 `Hash Aggregation`（哈希聚合）。

```mermaid
graph TD
    A[聚合方式] --> B[Stream Aggregation<br/>流式聚合]
    A --> C[Hash Aggregation<br/>哈希聚合]
    
    B --> D[输入已排序<br/>按组遍历]
    D --> E[无需临时表<br/>内存开销小]
    E --> F[条件: GROUP BY 列有索引]
    
    C --> G[构建 Hash Table<br/>分组存储]
    G --> H[可能使用临时表<br/>内存开销大]
    H --> I[条件: GROUP BY 列无索引]
```

### Stream Aggregation 流程

```mermaid
flowchart TD
    A[Stream Aggregation] --> B[输入已按 GROUP BY 排序]
    B --> C[读取第一组数据]
    C --> D[初始化聚合值<br/>COUNT/SUM/AVG]
    D --> E{同组下一行?}
    E -->|是| F[累积聚合值]
    F --> E
    E -->|否| G[输出当前组结果]
    G --> H{还有数据?}
    H -->|是| C
    H -->|否| I[结束]
```

### Hash Aggregation 流程

```mermaid
flowchart TD
    A[Hash Aggregation] --> B[初始化 Hash Table]
    B --> C[读取行数据]
    C --> D[计算分组 Hash]
    D --> E{Hash Table 有此组?}
    E -->|是| F[更新聚合值]
    E -->|否| G[插入新组]
    F --> H{还有数据?}
    G --> H
    H -->|是| C
    H -->|否| I[遍历 Hash Table]
    I --> J[输出每组结果]
```

## Using temporary 临时表

MySQL 在以下情况使用临时表：

```mermaid
graph TD
    A[Using temporary 触发条件] --> B[GROUP BY + 聚合函数<br/>无索引]
    A --> C[DISTINCT<br/>无索引]
    A --> D[UNION<br/>合并结果]
    A --> E[子查询<br/>需要物化]
    A --> F[ORDER BY + GROUP BY<br/>列不一致]
    
    B --> G[创建内存临时表<br/>MEMORY 引擎]
    C --> G
    D --> G
    
    G --> H{内存临时表满?}
    H -->|是| I[转为磁盘临时表<br/>InnoDB 引擎]
    H -->|否| J[继续使用内存表]
    
    I --> K[代价: 额外 IO]
```

### 临时表的类型

| 类型 | 存储位置 | 适用场景 | 限制 |
|------|----------|----------|------|
| 内存临时表 | MEMORY 引擎 | 小数据量临时结果 | 不支持 BLOB/TEXT |
| 磁盘临时表 | InnoDB 引擎 | 大数据量或 BLOB/TEXT | 额外 IO 开销 |

## ICP（Index Condition Pushdown）

ICP 将 WHERE 条件中与索引相关的部分下推到存储引擎，减少回表次数。

### ICP 下推前后的执行流程对比

```mermaid
graph TD
    subgraph "无 ICP（传统方式）"
        A1[二级索引扫描<br/>获取主键] --> B1[回表聚簇索引<br/>读取完整行]
        B1 --> C1[Server 层<br/>检查 WHERE 条件]
        C1 --> D1{满足条件?}
        D1 -->|是| E1[返回结果]
        D1 -->|否| F1[丢弃行]
    end
    
    subgraph "有 ICP（下推优化）"
        A2[二级索引扫描] --> B2[存储引擎层<br/>检查索引条件]
        B2 --> C2{满足索引条件?}
        C2 -->|是| D2[回表聚簇索引]
        C2 -->|否| E2[跳过回表<br/>直接读取下一行]
        D2 --> F2[Server 层<br/>检查其他条件]
        F2 --> G2{满足条件?}
        G2 -->|是| H2[返回结果]
        G2 -->|否| I2[丢弃行]
    end
```

### ICP 适用场景

```sql
-- 场景：复合索引 (zipcode, lastname, firstname)
SELECT * FROM people
WHERE zipcode = '95054' AND lastname LIKE '%etrunia%' AND address LIKE '%Main Street%';

-- 无 ICP：zipcode 索引扫描后，每一行都回表，再在 Server 层过滤
-- 有 ICP：zipcode 索引扫描时，在存储引擎层过滤 lastname LIKE '%etrunia%'
--         只有满足条件的行才回表，减少回表次数
```

**ICP 的限制**：

1. 只适用于二级索引，不适用于聚簇索引（聚簇索引已包含完整行，无需下推）
2. 只适用于等值条件、范围条件、LIKE（非前缀匹配除外）
3. 子查询条件不下推
4. 存储函数不下推

## MRR（Multi-Range Read）

MRR 针对二级索引的范围扫描，通过先排序主键再回表，将随机 IO 转为顺序 IO。

### MRR 的回表优化流程

```mermaid
graph TD
    subgraph "无 MRR（随机回表）"
        A1[二级索引范围扫描] --> B1[获取主键: 5, 2, 8, 1, 9]
        B1 --> C1[按索引顺序回表<br/>随机 IO]
        C1 --> D1[读取页面: 页5 → 页2 → 页8 → 页1 → 页9]
    end
    
    subgraph "有 MRR（顺序回表）"
        A2[二级索引范围扫描] --> B2[获取主键: 5, 2, 8, 1, 9]
        B2 --> C2[排序主键: 1, 2, 5, 8, 9]
        C2 --> D2[按主键顺序回表<br/>顺序 IO]
        D2 --> E2[读取页面: 页1 → 页2 → 页5 → 页8 → 页9]
    end
```

### MRR 执行流程

```mermaid
flowchart TD
    A[MRR 执行] --> B[二级索引范围扫描]
    B --> C[收集主键值<br/>到 read_rnd_buffer]
    C --> D{buffer 满<br/>或扫描结束?}
    D -->|否| C
    D -->|是| E[排序主键值]
    E --> F[按主键顺序<br/>批量回表]
    F --> G[返回行数据]
```

**MRR 的收益**：

1. 减少随机 IO：将离散的主键排序后顺序回表
2. 减少页面访问次数：同一页面只读取一次
3. 提升 Buffer Pool 命中率：顺序访问更易命中缓存

**MRR 的限制**：

1. 只适用于二级索引范围扫描
2. 需要额外的 `read_rnd_buffer` 内存
3. 不适用于主键范围扫描（主键范围扫描本身就是顺序的）

### MRR 配置参数

```sql
-- 启用 MRR（默认开启）
SET optimizer_switch = 'mrr=on';

-- 启用 MRR + 排序（默认关闭，需要显式开启）
SET optimizer_switch = 'mrr=on,mrr_cost_based=off';

-- 查看 MRR 使用情况
EXPLAIN SELECT * FROM orders WHERE user_id > 1000;
-- Extra: Using MRR
```

## Batched Key Access（BKA）

BKA 是 MRR 的扩展，用于 Join 场景，将 Join Buffer 中的主键排序后批量回表。

```mermaid
flowchart TD
    A[BKA Join] --> B[读取 Join Buffer<br/>中所有外表主键]
    B --> C[排序主键]
    C --> D[批量回表内表<br/>顺序 IO]
    D --> E[匹配 Join 条件]
    E --> F[返回结果行]
```

## 执行器与 PG 的对比

| 维度 | MySQL | PostgreSQL |
|------|-------|------------|
| 执行模型 | Iterator 模型（Read 接口） | Volcano 火山模型（ExecProcNode） |
| 存储引擎接口 | handler 接口（多引擎支持） | AM 接口（表访问方法） |
| 排序实现 | sort_buffer + filesort | tuplesort（内存/磁盘混合） |
| Join 实现 | BNLJ/INLJ/Hash Join | NLJ/Hash Join/Merge Join |
| 聚合实现 | Stream/Hash 聚合 | Hash/Sort 聚合 |
| 条件下推 | ICP（Index Condition Pushdown） | 无直接对应 |
| 顺序化回表 | MRR（Multi-Range Read） | 无直接对应 |
| 批量访问 | BKA（Batched Key Access） | 无直接对应 |

## 要点总结

- MySQL 执行器采用**迭代器模型**，通过 `handler` 接口与存储引擎交互
- **Using filesort** 有两种策略：**全字段排序**（省回表）和 **Rowid 排序**（省内存）
- **Using temporary** 在 GROUP BY、DISTINCT、UNION、子查询时使用，内存临时表满后转为磁盘临时表
- **ICP（Index Condition Pushdown）** 将 WHERE 条件中与索引相关的部分下推到存储引擎，减少回表次数
- **MRR（Multi-Range Read）** 对二级索引范围扫描，先排序主键再回表，将随机 IO 转为顺序 IO
- MySQL 执行器特有的优化（ICP、MRR、BKA）是 PG 没有的，这些优化针对二级索引回表场景

## 思考题

1. MySQL 的 `sort_buffer_size` 设置过大或过小各有什么问题？如何选择合适的大小？
2. ICP 和 MRR 都是为了减少回表开销，但它们的优化思路不同。分析两者的差异与适用场景。
3. MySQL 为什么选择在 Server 层实现 ICP，而不是在存储引擎层直接执行全部 WHERE 条件？
4. 对于大表排序（超过 `sort_buffer_size`），MySQL 使用临时文件排序。如何避免大表排序导致的性能问题？