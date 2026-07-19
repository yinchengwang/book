# BTree 索引

## 学习目标

- 理解 PostgreSQL BTree 索引的 Lehman & Yao 高并发算法
- 掌握 BTree 页面分裂、合并、扫描的实现细节
- 熟悉 BTree 与 InnoDB B+Tree 的关键差异

## 核心概念

- **BTree（B-Tree）**：自平衡树，保持有序，支持 O(log n) 查找/插入/删除
- **Lehman & Yao 算法**：高并发 BTree，使用"右链接 + 标记删除"实现无锁读
- **Page Split**：页面分裂，节点满时分裂为两个节点
- **Page Merge**：页面合并，节点过空时合并到兄弟
- **Fastpath**：最右路径优化，减少根到叶的遍历层数
- **Vacuum BTree**：清理索引中的死元组

## BTree 结构

PG 的 BTree 是标准的 B-Tree（不是 B+Tree），叶子节点和内部节点结构相同：

```mermaid
graph TD
    A[Root 内部节点] --> B[Leaf 左]
    A --> C[Leaf 中]
    A --> D[Internal 右]
    D --> E[Leaf 右1]
    D --> F[Leaf 右2]

    B -->|right link| C
    C -->|right link| E
    E -->|right link| F

    G[叶子节点] --> H[数据项: key + TID]
    I[内部节点] --> J[分隔键 + 子节点指针]
```

## Lehman & Yao 高并发算法

传统 BTree 的写操作需要锁定整棵树，导致并发性能差。Lehman & Yao 算法通过以下技术实现高并发：

```mermaid
graph TD
    A[Lehman & Yao 核心技术] --> B[右链接<br/>Right Link]
    A --> C[标记删除<br/>Mark Delete]
    A --> D[结转标志<br/>Move Right]
    A --> E[无锁读<br/>无读锁]

    B --> F[每个节点有指向右兄弟的指针]
    C --> G[删除时只标记 不物理删除]
    D --> H[分裂时设置标志 指引读操作]
    E --> I[读操作不阻塞写操作]
```

### 读操作（无锁）

```mermaid
flowchart TD
    A[BTree Search] --> B[从根节点开始]
    B --> C[遍历到目标叶节点]
    C --> D{找到 key?}
    D -->|是| E[返回 TID]
    D -->|否| F{有 rightlink 且 key 溢出?}
    F -->|是| G[移到右兄弟]
    G --> D
    F -->|否| H[返回 NOT FOUND]
```

**关键点**：读操作不持有任何读锁，只靠页面级右链接导航。

### 写操作（分裂）

```mermaid
sequenceDiagram
    participant W as 写入者
    participant P as 待分裂页
    participant R as 右兄弟
    participant Parent as 父节点

    W->>P: 锁定 P
    W->>P: 分配新页 R
    W->>P: 复制一半数据到 R
    W->>R: 设置 rightlink = P.rightlink
    W->>P: 设置 rightlink = R
    W->>P: 设置 SPLIT_END 标志
    W->>Parent: 锁定父节点
    W->>Parent: 插入分隔键 + R 指针
    W->>Parent: 解锁
    W->>P: 解锁
```

**分裂后的状态**：

```mermaid
graph LR
    A[旧页 P] -->|rightlink| B[新页 R]
    B -->|rightlink| C[旧右兄弟]

    A -.->|SPLIT_END 标志| D[读操作移到 R]
```

## 页面结构

BTree 页面在 Heap 页面基础上增加 16 字节 Special：

```mermaid
graph TD
    subgraph "BTree Page"
        H[PageHeader 24B]
        I[ItemId Array]
        T[Tuple Data<br/>Index Entries]
        S[BTPageOpaqueData 16B]
    end

    subgraph "BTPageOpaqueData"
        S1[btpo_prev: 4B<br/>左兄弟]
        S2[btpo_next: 4B<br/>右兄弟]
        S3[btpo_level: 4B<br/>层级]
        S4[btpo_flags: 4B<br/>标志位]
    end

    H --> I
    I --> T
    T --> S
    S --> S1
    S1 --> S2
    S2 --> S3
    S3 --> S4
```

**关键标志**：

| 标志 | 含义 |
|------|------|
| `BTP_LEAF` | 叶节点 |
| `BTP_ROOT` | 根节点 |
| `BTP_DELETED` | 已删除页 |
| `BTP_HALF_DEAD` | 半死页（等待回收） |
| `BTP_SPLIT_END` | 分裂完成（读操作应右移） |

## 索引项结构

BTree 的索引项（Index Tuple）结构：

```mermaid
graph TD
    A[IndexTupleData] --> B[t_info: 2B<br/>信息位]
    A --> C[keydata: 变长<br/>索引键]

    subgraph "t_info 标志"
        D1[INDEX_NULL_MASK]
        D2[INDEX_VAR_MASK]
        D3[INDEX_SIZE_MASK<br/>低 15 位]
    end
```

**键值存储**：

- 固定长度键：直接存储
- 变长长度键：TOAST 外存或内联
- `INCLUDE` 列：存储在叶节点，不参与比较

## 扫描操作

### 唯一键查找

```mermaid
flowchart TD
    A[BTWSearch] --> B[从根到叶遍历]
    B --> C[锁定叶节点]
    C --> D[二分查找 key]
    D --> E{找到?}
    E -->|是| F[返回 TID]
    E -->|否| G[返回 NOT FOUND]
    F --> H[解锁]
    G --> H
```

### 范围扫描

```mermaid
flowchart TD
    A[BTree 扫描] --> B[定位起始键]
    B --> C[锁定叶节点]
    C --> D[读取当前行]
    D --> E{符合条件?}
    E -->|是| F[返回行]
    E -->|否| G{页内还有数据?}
    G -->|是| D
    G -->|否| H{有 rightlink?}
    H -->|是| I[移到右兄弟]
    I --> D
    H -->|否| J[结束]
```

## 插入操作

```mermaid
flowchart TD
    A[BTInsert] --> B[查找目标叶节点]
    B --> C[检查页空间]
    C --> D{空间够?}
    D -->|是| E[插入索引项]
    D -->|否| F[页分裂]

    F --> G[分配新页]
    G --> H[移动一半数据]
    H --> I[更新 rightlink]
    I --> J[插入分隔键到父节点]

    J --> K{父节点满?}
    K -->|是| L[递归分裂]
    K -->|否| M[完成]
    L --> M

    E --> M
```

## 删除操作

PG 不直接物理删除索引项，而是标记删除（等待 VACUUM）：

```mermaid
flowchart TD
    A[BTree Delete] --> B[查找目标项]
    B --> C[标记为 DEAD<br/>设置 LP_DEAD]
    C --> D[写 WAL]
    D --> E[完成]

    F[VACUUM] --> G[扫描 BTree]
    G --> H[回收 LP_DEAD 项]
    H --> I[更新 FSM]
```

**Page Merge（合并）**：

```mermaid
flowchart TD
    A[节点过空] --> B{左兄弟存在且有空?}
    B -->|是| C[合并到左兄弟]
    B -->|否| D{右兄弟存在且有空?}
    D -->|是| E[合并到右兄弟]
    D -->|否| F[不合并]

    C --> G[更新 rightlink]
    E --> G
    G --> H[标记节点为 DELETED]
```

## Vacuum BTree

VACUUM 处理 BTree 索引中的死元组：

```mermaid
sequenceDiagram
    participant Vacuum as VACUUM
    participant Heap as Heap 表
    participant BTree as BTree 索引

    Vacuum->>Heap: 扫描并标记死元组
    Vacuum->>BTree: 扫描索引
    BTree->>BTree: 清理 LP_DEAD 项
    BTree-->>Vacuum: 返回回收空间
    Vacuum->>Heap: 更新 FSM/VM
```

## BTree vs InnoDB B+Tree

| 维度 | PostgreSQL BTree | InnoDB B+Tree |
|------|------------------|---------------|
| 树类型 | B-Tree（叶和内部相同） | B+Tree（只有叶存数据） |
| 叶节点存储 | 键 + TID | 键 + 行数据（聚簇） |
| 兄弟链接 | 右链接（rightlink） | 双向链表（prev/next） |
| 并发控制 | Lehman & Yao 无锁读 | Latch + 范围锁 |
| 分裂策略 | 50% 分裂 | 50% 或 批量分裂 |
| 合并策略 | 惰性（VACUUM 时） | 惰性合并 |

## 性能优化

### Fastpath 优化

最右路径缓存，减少层数遍历：

```mermaid
graph LR
    A[缓存] --> B[最右叶节点]
    B --> C[单调递增插入<br/>直接定位叶节点]
    C --> D[跳过根→中间层]
```

### 批量加载

`CREATE INDEX` 使用排序 + 批量构建：

```mermaid
flowchart TD
    A[CREATE INDEX] --> B[扫描表]
    B --> C[排序索引项]
    C --> D[批量构建 BTree<br/>自底向上]
    D --> E[写入索引文件]
```

## 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `maintenance_work_mem` | 64MB | CREATE INDEX 内存 |
| `max_parallel_maintenance_workers` | 2 | 并行创建索引 |

## 要点总结

- PG BTree 使用 **Lehman & Yao 高并发算法**，实现无锁读、右链接导航
- 页面分裂时设置 SPLIT_END 标志，引导读操作右移
- 删除操作只标记 LP_DEAD，由 VACUUM 回收
- 与 InnoDB B+Tree 相比，PG BTree 是 B-Tree，叶节点存 TID 而非行数据
- Fastpath 优化加速单调递增插入

## 思考题

1. 为什么 PG 选择 B-Tree 而非 B+Tree？两者在存储效率和查询性能上有何差异？
2. Lehman & Yao 算法的"无锁读"在什么情况下会失效？分裂中的页如何保证读正确？
3. 如果一张表的插入模式是随机键（如 UUID），BTree 会有什么性能问题？如何优化？