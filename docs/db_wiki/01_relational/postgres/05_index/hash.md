# Hash 索引

## 学习目标

- 理解 PostgreSQL Hash 索引的 Linear Hashing 实现
- 掌握 Hash 索引的碰撞处理、扩展、收缩机制
- 熟悉 Hash 索引的适用场景与限制

## 核心概念

- **Hash 索引**：基于哈希表的索引，O(1) 等值查询
- **Linear Hashing**：动态哈希，渐进式扩展
- **Bucket**：哈希桶，存储键值对
- **Overflow Page**：桶溢出页，桶满时链式扩展
- **Split**：桶分裂，哈希表扩展
- **Hash Code**：32 位哈希值，用于定位桶

## Hash 索引结构

PG 的 Hash 索引采用 Linear Hashing（线性哈希），支持渐进式扩展：

```mermaid
graph TD
    A[Meta 页] --> B[Bucket 目录]
    B --> C[Bucket 0]
    B --> D[Bucket 1]
    B --> E[Bucket N-1]

    C --> F[Primary Page]
    F --> G[Overflow Page 1]
    G --> H[Overflow Page 2]

    D --> I[Primary Page]
```

## Linear Hashing 原理

Linear Hashing 允许哈希表渐进式扩展，避免一次性重哈希：

```mermaid
flowchart TD
    A[插入键值对] --> B[计算 hash]
    B --> C[映射到桶]
    C --> D{桶满?}
    D -->|是| E[添加 Overflow 页]
    D -->|否| F[插入]

    E --> G{负载因子超阈值?}
    G -->|是| H[触发 Split]
    G -->|否| F

    H --> I[分裂当前桶]
    I --> J[分配新桶]
    J --> K[重分布键值对]
```

**桶编号计算**：

```c
// 当前桶数 = 2^level * buckets_per_level
bucket = hashcode % (2^level)
// 如果 bucket < splitpoint，可能已分裂
if (bucket < current_splitpoint) {
    bucket = hashcode % (2^(level+1))
}
```

## 页面类型

Hash 索引有 4 种页面类型：

```mermaid
graph TD
    A[Hash Page 类型] --> B[Meta Page<br/>元数据页]
    A --> C[Bucket Page<br/>桶页]
    A --> D[Overflow Page<br/>溢出页]
    A --> E[Bitmap Page<br/>位图页]

    B --> F[包含: level, maxbucket,<br/>highmask, lowmask]

    C --> G[存储索引项<br/>Primary Page]

    D --> H[桶满时的溢出链]

    E --> I[跟踪已分配页]
```

## 索引项结构

```mermaid
graph LR
    A[Hash Index Entry] --> B[Hash Key 4B]
    A --> C[TID 6B<br/>表行指针]

    D[页面结构] --> E[HashPageOpaqueData 16B]
    E --> F[hasho_prevblkno]
    E --> G[hasho_nextblkno]
    E --> H[hasho_bucket]
    E --> I[hasho_flag]
```

## 插入流程

```mermaid
sequenceDiagram
    participant Insert as 插入者
    participant Meta as Meta 页
    participant Bucket as Bucket 页
    participant Overflow as Overflow 页

    Insert->>Meta: 读取 meta 信息
    Insert->>Insert: 计算 hash(key)
    Insert->>Insert: 定位桶号
    Insert->>Bucket: 锁定桶页
    Bucket-->>Insert: 页面满?
    Insert->>Overflow: 创建溢出页
    Overflow-->>Insert: 插入成功
```

**详细流程**：

```mermaid
flowchart TD
    A[hash_insert] --> B[计算 hash_code]
    B --> C[计算 bucket_no]
    C --> D[锁定 bucket 页]
    D --> E{页空间够?}
    E -->|是| F[插入项]
    E -->|否| G[找/创建 overflow 页]
    G --> H[插入到 overflow]
    H --> I{触发 split?}
    I -->|是| J[_hash_splitbucket]
    I -->|否| K[完成]
    J --> K
    F --> K
```

## 扩展（Split）

当负载因子超过阈值时，触发桶分裂：

```mermaid
flowchart TD
    A[_hash_splitbucket] --> B[分配新桶 N]
    B --> C[锁定旧桶 S 和 新桶 N]
    C --> D[遍历旧桶所有页]
    D --> E{hashcode 属于 N?}
    E -->|是| F[移动到新桶]
    E -->|否| G[留在旧桶]
    F --> H[更新元数据]
    G --> H
    H --> I[更新 splitpoint]
```

**分裂示例**：

```mermaid
graph LR
    subgraph "分裂前"
        A[Bucket 0<br/>hash%2=0]
        B[Bucket 1<br/>hash%2=1]
    end

    subgraph "分裂后"
        C[Bucket 0<br/>hash%4=0]
        D[Bucket 2<br/>hash%4=2]
        E[Bucket 1<br/>hash%4=1]
    end

    A --> C
    A --> D
    B --> E
```

## 收缩

VACUUM 可以收缩 Hash 索引：

```mermaid
flowchart TD
    A[VACUUM Hash] --> B[扫描桶页]
    B --> C[清理 LP_DEAD 项]
    C --> D{overflow 页空?}
    D -->|是| E[释放 overflow 页]
    D -->|否| F[保留]

    G[桶过空] --> H{可以合并?}
    H -->|是| I[合并桶]
    H -->|否| J[不操作]
```

## 查询流程

```mermaid
flowchart TD
    A[hash_search] --> B[计算 hash_code]
    B --> C[定位 bucket]
    C --> D[锁定 bucket 页]
    D --> E[扫描 primary 页]
    E --> F{找到 key?}
    F -->|是| G[返回 TID]
    F -->|否| H{有 overflow?}
    H -->|是| I[扫描 overflow 页]
    I --> F
    H -->|否| J[返回 NOT FOUND]
```

## 死元组处理

```mermaid
sequenceDiagram
    participant Delete as DELETE 操作
    participant Bucket as Bucket 页
    participant VACUUM as VACUUM

    Delete->>Bucket: 标记 LP_DEAD
    Note over Bucket: 索引项物理保留

    VACUUM->>Bucket: 扫描 hash 索引
    VACUUM->>Bucket: 清理 LP_DEAD 项
    VACUUM->>Bucket: 更新 FSM
```

## Hash 索引限制

PG 10 之前 Hash 索引不支持 WAL，不推荐用于生产环境。PG 10+ 已支持 WAL：

```mermaid
graph LR
    A[Hash 索引限制] --> B[只支持等值查询<br/>= 操作符]
    A --> C[不支持范围查询<br/>> < BETWEEN]
    A --> D[不支持排序<br/>ORDER BY]
    A --> E[不支持多列<br/>组合键]
    A --> F[不支持 UNIQUE 约束<br/>PG 11+ 支持]
```

## 性能特点

```mermaid
graph TD
    A[Hash 索引性能] --> B[查询: O 平均<br/>等值查询快]
    A --> C[插入: O 平均]
    A --> D[空间: 较大<br/>overflow 页]

    E[优势] --> F[等值查询极快]
    E --> G[无需排序]
    E --> H[适合主键查询]

    I[劣势] --> J[不支持范围]
    I --> K[空间放大]
    I --> L[不适合低选择性列]
```

## 与 BTree 对比

| 维度 | Hash 索引 | BTree 索引 |
|------|-----------|------------|
| 查询类型 | 仅等值 | 等值 + 范围 + 排序 |
| 查询复杂度 | O(1) 平均 | O(log n) |
| 空间效率 | 较低（overflow） | 较高 |
| 范围查询 | 不支持 | 支持 |
| 排序输出 | 不支持 | 支持 |
| 多列索引 | 不支持 | 支持 |
| UNIQUE | PG 11+ 支持 | 支持 |
| WAL 支持 | PG 10+ | 一直支持 |

## 使用建议

```mermaid
flowchart TD
    A[选择索引类型] --> B{查询模式?}
    B -->|仅等值查询| C{PG 版本 ≥ 10?}
    B -->|范围/排序| D[使用 BTree]

    C -->|是| E{列选择性高?}
    C -->|否| D

    E -->|是| F[可考虑 Hash]
    E -->|否| G[使用 BTree]

    F --> H{频繁范围查询?}
    H -->|是| D
    H -->|否| I[使用 Hash]
```

## 配置参数

| 参数 | 说明 |
|------|------|
| `hash_mem_utilization` | Hash 内存利用率（内部） |

## 要点总结

- Hash 索引使用 Linear Hashing，支持渐进式扩展
- 桶满时使用 Overflow 页，负载因子超阈值时触发 Split
- PG 10+ 支持 WAL，生产可用
- 仅支持等值查询，不支持范围/排序/多列
- 空间效率低于 BTree，适合纯等值查询场景

## 思考题

1. 为什么 PG 长期不推荐使用 Hash 索引（直到 PG 10）？WAL 支持对 Hash 索引有什么特殊挑战？
2. Linear Hashing 相比传统可扩展哈希（Extendible Hashing）有什么优势？
3. 什么情况下 Hash 索引比 BTree 索引更适合？给出一个具体场景。