# Heap Table 堆表存储

## 学习目标

- 理解 DuckDB 为何无堆表概念，转而采用纯列式存储
- 掌握 ColumnChunk、Row Group、Column Segment 的三层存储结构
- 熟悉 Zone Map 统计信息如何实现高效的谓词下推过滤

## 核心概念

- **无堆表概念**：DuckDB 不使用 PG 风格的"无序堆 + ctid 指针"，每张表按列存储
- **ColumnChunk（列数据块）**：单列的一段连续数据，通常 10000-50000 行
- **Row Group（行组）**：一组行（如 10000 行）的所有列数据，是 DuckDB 的基本存储单元
- **Column Segment（列段）**：单个 Row Group 内的一列数据，对应一个 ColumnChunk
- **Zone Map（区域映射）**：每个 ColumnChunk 维护 min/max/null_count，用于过滤无用数据块
- **无 MVCC 行版本**：DuckDB 不维护 xmin/xmax，更新操作是"删除旧数据 + 追加新数据"

## 整体存储结构

DuckDB 的表存储是**纯列式**的，与 PostgreSQL 的堆表有根本差异：

```mermaid
graph TB
    subgraph "PostgreSQL 堆表"
        PG1[Page 0<br/>row1: col1,col2,col3]
        PG2[Page 1<br/>row2: col1,col2,col3]
        PG3[Page 2<br/>row3: col1,col2,col3]
    end

    subgraph "DuckDB 列式存储"
        DD1[Row Group 0<br/>10000 行]
        DD2[Row Group 1<br/>10000 行]
        DD3[Row Group 2<br/>10000 行]
    end

    DD1 --> RG1_C1[Column Segment: col1]
    DD1 --> RG1_C2[Column Segment: col2]
    DD1 --> RG1_C3[Column Segment: col3]

    DD2 --> RG2_C1[Column Segment: col1]
    DD2 --> RG2_C2[Column Segment: col2]
    DD2 --> RG2_C3[Column Segment: col3]

    style PG1 fill:#ffebee
    style DD1 fill:#e8f5e9
```

**关键差异**：
- PG 的页面包含**完整行**（跨列），查询时需要读取整行再提取所需列
- DuckDB 的 Row Group 包含**分列存储**，查询时只读取需要的列

## Row Group 与 Column Segment

Row Group 是 DuckDB 的基本存储单元，默认包含 122880 行（可配置）。每个 Row Group 内部，每列独立存储为 Column Segment。

```mermaid
graph LR
    subgraph "Row Group 结构"
        A[Row Group Header<br/>元数据]
        A --> B[Column Segment 0<br/>col1 数据 + Zone Map]
        A --> C[Column Segment 1<br/>col2 数据 + Zone Map]
        A --> D[Column Segment 2<br/>col3 数据 + Zone Map]
        A --> E[Column Segment N<br/>colN 数据 + Zone Map]
    end

    subgraph "Column Segment 内部"
        F[数据块<br/>压缩后的列值]
        F --> G[统计信息<br/>min/max/null_count]
        F --> H[压缩元数据<br/>编码类型/大小]
    end

    B --> F
```

**设计原因**：

1. **批量处理效率**：Row Group 大小与向量化执行的批量大小（1024 行）对齐，方便整块加载
2. **压缩效率**：同一列数据类型一致，压缩比高（RLE/Delta/字典编码）
3. **并行扫描**：不同 Row Group 可并行处理，无锁竞争

## ColumnChunk 数据布局

ColumnChunk 是单个 Column Segment 的物理存储单元：

```mermaid
graph TB
    subgraph "ColumnChunk 结构"
        A[Chunk Header<br/>行数/数据类型/压缩类型]
        A --> B[数据区<br/>压缩后的值数组]
        A --> C[NULL 位图<br/>标记 NULL 位置]
        A --> D[Zone Map<br/>min/max/null_count]
    end

    subgraph "Zone Map 示例"
        E[列: age<br/>min=18, max=65<br/>null_count=5]
        F[列: name<br/>min='Alice', max='Zoe'<br/>null_count=0]
        G[列: salary<br/>min=30000, max=120000<br/>null_count=12]
    end

    D --> E
    D --> F
    D --> G
```

**Zone Map 的作用**：

```mermaid
flowchart TD
    A[查询: WHERE age > 70] --> B[扫描 Row Group 0<br/>age 的 Zone Map]
    B --> C{max >= 70?}
    C -->|是| D[读取该 ColumnChunk<br/>解压并过滤]
    C -->|否| E[跳过整个 Chunk<br/>零 I/O]
```

**性能提升**：对于选择性高的查询（如"age > 70"只匹配 5% 数据），Zone Map 可以跳过 95% 的数据块，I/O 减少 20 倍。

## 写入路径

DuckDB 的写入是**批量追加**优化的，不支持高效的随机插入：

```mermaid
flowchart TD
    A[INSERT 批量数据] --> B[数据收集到内存缓冲]
    B --> C{缓冲达到阈值?}
    C -->|否| D[继续收集]
    C -->|是| E[压缩每列数据]
    E --> F[写入新的 Row Group]
    F --> G[更新表元数据<br/>行数/Zone Map]
    G --> H[返回成功]
```

**关键步骤**：

1. **批量收集**：单行插入会先缓存在内存，凑够一个 Row Group 才刷盘
2. **列级压缩**：每列独立选择最优压缩算法（RLE/Delta/字典/FSST）
3. **追加写入**：新数据总是追加到文件末尾，不做随机插入

**性能影响**：
- 批量插入（INSERT INTO ... VALUES (...), (...), ...）性能极高
- 单行插入性能较差，需要等待缓冲凑够 Row Group

## 更新与删除

DuckDB 的更新和删除是**逻辑标记 + 后台合并**模式，与 PG 的 MVCC 不同：

```mermaid
sequenceDiagram
    participant User as 用户
    participant Duck as DuckDB
    participant Storage as 存储层

    User->>Duck: UPDATE table SET col = val WHERE id = 100
    Duck->>Storage: 找到目标 Row Group
    Duck->>Storage: 标记删除旧行（逻辑删除）
    Duck->>Storage: 追加新行到最新 Row Group
    Note over Storage: 旧数据未立即删除

    User->>Duck: 后续查询
    Duck->>Storage: 读取时过滤已删除行
    Note over Duck: 需要合并删除标记

    User->>Duck: VACUUM 或 CHECKPOINT
    Duck->>Storage: 合并数据<br/>回收空间
```

**关键差异**：
- PG 的 UPDATE 会写入新版本，老版本通过 VACUUM 清理
- DuckDB 的 UPDATE 是"逻辑删除 + 追加新行"，需要显式 VACUUM 或等待后台合并

## 与 PostgreSQL 堆表的对比

```mermaid
graph LR
    subgraph "PG 堆表 UPDATE"
        A1[旧 Tuple V1] --> A2[xmax 标记删除]
        A2 --> A3[新 Tuple V2<br/>xmin=当前事务]
        A3 --> A4[VACUUM 清理 V1]
    end

    subgraph "DuckDB 列存 UPDATE"
        B1[旧列数据块] --> B2[删除位图标记]
        B2 --> B3[新列数据块追加]
        B3 --> B4[VACUUM 合并重写]
    end

    style A1 fill:#ffebee
    style B1 fill:#fff4e1
```

| 维度 | PostgreSQL 堆表 | DuckDB 列存 |
|------|----------------|-------------|
| 数据组织 | 无序堆 + ctid 指针 | Row Group + Column Segment |
| 页面大小 | 固定 8KB | 可变（由 Row Group 决定） |
| UPDATE 代价 | 写入新版本，老版本待回收 | 逻辑删除 + 追加新行 + 后台合并 |
| 删除方式 | 标记 xmax，VACUUM 回收空间 | 删除位图，VACUUM 合并重写 |
| 随机插入 | 支持高效随机插入 | 仅优化批量追加 |
| Zone Map | 无（需依赖索引统计） | 每列自动维护 |
| MVCC | 完整行版本链 | 无行版本，逻辑删除 |

## Zone Map 过滤示例

```mermaid
graph TB
    subgraph "表: orders (100 万行)"
        A[Row Group 0<br/>行 0-122879]
        B[Row Group 1<br/>行 122880-245759]
        C[Row Group 2<br/>行 245760-368639]
        D[Row Group 7<br/>行 860160-983039]
    end

    A --> A1[order_date Zone Map<br/>min=2024-01-01, max=2024-03-31]
    B --> B1[order_date Zone Map<br/>min=2024-04-01, max=2024-06-30]
    C --> C1[order_date Zone Map<br/>min=2024-07-01, max=2024-09-30]
    D --> D1[order_date Zone Map<br/>min=2024-10-01, max=2024-12-31]

    Query[WHERE order_date > '2024-11-01']
    Query -.过滤.-> A1
    Query -.过滤.-> B1
    Query -.过滤.-> C1
    Query -.读取.-> D1

    Note1[跳过 87.5% 数据块] -.-> A1
    Note2[跳过 87.5% 数据块] -.-> B1
    Note3[跳过 87.5% 数据块] -.-> C1
```

**查询优化效果**：

```sql
-- 查询: 只需要 2024-11-01 之后的数据
SELECT COUNT(*) FROM orders WHERE order_date > '2024-11-01';

-- DuckDB 执行计划:
-- 1. 读取每个 Row Group 的 Zone Map（元数据，开销极小）
-- 2. 只有 Row Group 7 的 max >= 2024-11-01，需要读取
-- 3. 跳过 87.5% 的数据块，I/O 减少 8 倍
```

## 要点总结

- DuckDB **无堆表概念**，采用纯列式存储，每列独立存储为 Column Segment
- Row Group 是基本存储单元（默认 122880 行），内部按列划分为多个 Column Segment
- Zone Map（min/max/null_count）实现高效的谓词下推，可跳过无关数据块
- UPDATE 是"逻辑删除 + 追加新行"，需要显式 VACUUM 或后台合并
- 与 PG 堆表相比，列存不适合随机插入，但大幅优化了分析查询的 I/O

## 思考题

1. 为什么 DuckDB 选择 122880 行作为 Row Group 大小？这个数字在压缩效率、并行度、I/O 粒度之间如何权衡？
2. Zone Map 只记录 min/max/null_count，如果查询条件是"col BETWEEN min AND max 但实际无匹配"，会发生什么？
3. 假设一张表频繁更新（如状态字段从"待处理"变为"已完成"），DuckDB 的"逻辑删除 + 追加"模式会带来什么问题？如何优化？
4. 与 PG 堆表的 MVCC 行版本相比，DuckDB 的无版本设计在并发事务下有何优劣？
