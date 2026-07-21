# Dolt 存储引擎

## 学习目标

- 理解 Prolly-Tree 的核心原理与实现细节
- 掌握内容可寻址存储（CAS）机制
- 了解 Noms 数据模型与 Chunk Store 架构
- 对比分析 Dolt 存储引擎与本项目 storage 模块的差异

## 核心存储架构

```mermaid
graph TB
    subgraph "应用层"
        SQL["SQL 查询层"]
        DOLT["dolt CLI"]
    end

    subgraph "版本控制层"
        COMMIT["Commit 链管理"]
        BRANCH["分支管理"]
        MERGE["合并引擎"]
    end

    subgraph "存储引擎层"
        PROLLY["Prolly-Tree<br/>内容可寻址 B-Tree"]
        CHUNK_STORE["Chunk Store<br/>块存储接口"]
        NOMS["Noms 数据模型<br/>结构化数据抽象"]
    end

    subgraph "持久化层"
        LOCAL_FS["本地文件系统<br/>.dolt/noms/"]
        S3["远程存储<br/>DoltHub S3"]
        MANIFEST["Manifest 文件<br/>根哈希索引"]
    end

    SQL --> PROLLY
    DOLT --> BRANCH
    COMMIT --> PROLLY
    BRANCH --> COMMIT
    MERGE --> PROLLY
    PROLLY --> CHUNK_STORE
    CHUNK_STORE --> NOMS
    NOMS --> LOCAL_FS
    NOMS --> S3
    LOCAL_FS --> MANIFEST
```

## Prolly-Tree 结构详解

### 核心概念

Prolly-Tree 是 Dolt 存储引擎的核心创新，全称 "Probabilistic B-Tree"，是一种内容可寻址的 B-Tree 变体。

```mermaid
graph TB
    subgraph "Prolly-Tree 节点结构"
        ROOT["根节点<br/>Chunk Hash: h1<br/>包含: [k1, k2, ptr1, ptr2]"]
        
        INT1["内部节点<br/>Chunk Hash: h2<br/>包含: [k3, ptr3, ptr4]"]
        INT2["内部节点<br/>Chunk Hash: h3<br/>包含: [k4, ptr5, ptr6]"]
        
        LEAF1["叶子节点<br/>Chunk Hash: h4<br/>包含: [(k1,v1), (k2,v2)]"]
        LEAF2["叶子节点<br/>Chunk Hash: h5<br/>包含: [(k3,v3), (k4,v4)]"]
        LEAF3["叶子节点<br/>Chunk Hash: h6<br/>包含: [(k5,v5), (k6,v6)]"]
    end

    ROOT --> INT1
    ROOT --> INT2
    INT1 --> LEAF1
    INT1 --> LEAF2
    INT2 --> LEAF3
    
    style ROOT fill:#f9f,stroke:#333
    style INT1 fill:#bbf,stroke:#333
    style INT2 fill:#bbf,stroke:#333
    style LEAF1 fill:#bfb,stroke:#333
    style LEAF2 fill:#bfb,stroke:#333
    style LEAF3 fill:#bfb,stroke:#333
```

### 关键特性

| 特性 | 标准B-Tree | Prolly-Tree |
|------|-----------|-------------|
| 节点标识 | 内存指针/页面号 | 内容哈希（Chunk Hash） |
| 更新方式 | 原地修改 | 写时复制（Copy-on-Write） |
| 一致性保证 | WAL + Checkpoint | 哈希校验天然保证 |
| 版本控制 | 需额外实现 | 天然支持快照 |
| 数据去重 | 无 | 相同内容共享存储 |

### Chunk 结构

```c
// 伪代码：Chunk 数据结构
struct Chunk {
    string hash;        // 内容哈希（SHA-256 或类似）
    bytes  data;        // 序列化后的数据
    int    height;      // 树高度（叶子为 0）
    int    num_entries; // 条目数量
    bytes  boundary;    // 边界键（内部节点）
};
```

### 写入路径

```mermaid
sequenceDiagram
    participant C as 客户端
    participant S as SQL 层
    participant P as Prolly-Tree
    participant CS as Chunk Store
    participant D as 磁盘

    C->>S: INSERT INTO users VALUES (...)
    S->>P: 写入新行
    P->>P: 找到目标叶子节点
    P->>P: 创建新叶子节点（CoW）
    P->>P: 计算新叶子哈希 h'
    P->>P: 更新父节点引用（CoW）
    P->>P: 向上传播至根节点
    P->>CS: 写入新 Chunks
    CS->>D: 持久化到 .dolt/noms/
    P-->>S: 返回新根哈希
    S-->>C: 写入成功
```

### 读取路径

```mermaid
sequenceDiagram
    participant C as 客户端
    participant S as SQL 层
    participant P as Prolly-Tree
    participant CS as Chunk Store
    participant D as 磁盘
    participant M as 缓存

    C->>S: SELECT * FROM users WHERE id=1
    S->>P: 查询键 id=1
    P->>M: 检查根节点缓存
    alt 缓存命中
        M-->>P: 返回根节点
    else 缓存未命中
        P->>CS: 读取根 Chunk
        CS->>D: 读取 .dolt/noms/
        D-->>CS: 返回 Chunk 数据
        CS-->>P: 解析为节点
        P->>M: 缓存节点
    end
    P->>P: 遍历到叶子节点
    P-->>S: 返回目标行
    S-->>C: 返回查询结果
```

## Noms 数据模型

### 核心类型

```mermaid
graph TB
    subgraph "Noms 类型系统"
        PRIMITIVE["原始类型<br/>Bool/String/Number/Blob"]
        STRUCT["Struct<br/>结构化记录"]
        MAP["Map<br/>键值集合"]
        SET["Set<br/>唯一值集合"]
        LIST["List<br/>有序列表"]
        REF["Ref<br/>引用类型"]
    end

    PRIMITIVE --> STRUCT
    STRUCT --> MAP
    MAP --> SET
    SET --> LIST
    LIST --> REF
```

### Chunk 序列化

```go
// Go 伪代码：Noms Chunk 序列化
type Value interface {
    Kind() NomsKind
    WriteTo(w io.Writer) error
    Hash() hash.Hash
}

// Struct 示例
type User struct {
    ID    uint64 `noms:"id"`
    Name  string `noms:"name"`
    Email string `noms:"email"`
}

// 序列化为 Chunk
func (u *User) ToChunk() *Chunk {
    data := serialize(u)  // Noms 序列化格式
    hash := sha256(data)  // 计算哈希
    return &Chunk{Hash: hash, Data: data}
}
```

## 数据持久化机制

### 存储文件布局

```
.dolt/
├── noms/                    # Noms Chunk 存储
│   ├── chunks/              # Chunk 数据文件
│   │   ├── 00/              # 按哈希前缀分片
│   │   │   ├── 00abc123...  # Chunk 文件
│   │   │   └── 00def456...
│   │   ├── 01/
│   │   └── ...
│   └── manifest             # 根哈希索引
├── working/                 # 工作目录状态
├── heads/                   # 分支指针
└── commit-graph/            # Commit 图结构
```

### 持久化流程

```mermaid
flowchart TB
    START[数据变更] --> COW[Copy-on-Write 创建新节点]
    COW --> HASH[计算新节点哈希]
    HASH --> CHUNK[生成 Chunk]
    CHUNK --> CACHE[写入 Chunk 缓存]
    CACHE --> DISK[异步刷盘]
    DISK --> MANIFEST[更新 Manifest]
    MANIFEST --> COMMIT[创建 Commit 记录]
    COMMIT --> HEADS[更新分支指针]
    HEADS --> END[完成]
```

## 与本项目 storage 模块对比

### 架构差异

| 维度 | Dolt | 本项目 storage 模块 |
|------|------|---------------------|
| 核心结构 | Prolly-Tree | 标准 B-Tree + 堆表 |
| 版本控制 | 原生支持（Commit 链） | 无（需 WAL 恢复） |
| 内容寻址 | 是（Chunk Hash） | 否（页面号） |
| 并发控制 | MVCC + 分支隔离 | 锁 + Buffer Pool |
| 数据模型 | Noms 多态类型 | 关系模型为主 |

### 功能对比

```mermaid
graph LR
    subgraph "Dolt 存储能力"
        D1[内容可寻址]
        D2[版本控制]
        D3[分支隔离]
        D4[数据去重]
        D5[历史快照]
    end

    subgraph "本项目存储能力"
        P1[Buffer Pool]
        P2[WAL 恢复]
        P3[索引结构]
        P4[多模型支持]
        P5[事务处理]
    end

    D1 -- 可借鉴 --> P1
    D2 -- 可借鉴 --> P5
    D3 -- 可借鉴 --> P5
```

### 可借鉴的设计点

1. **内容可寻址存储**
   - 在本项目 BTree 中添加 Chunk Hash 校验
   - 实现数据完整性自动验证
   - 支持增量同步

2. **版本控制集成**
   - 在 Commit 记录中保存根哈希
   - 实现 `AS OF` 时间旅行查询
   - 支持数据审计追踪

3. **分支管理**
   - 实现沙箱隔离机制
   - 支持测试环境快速切换
   - 数据变更可回滚

## 要点总结

- **Prolly-Tree 核心**：内容可寻址 + 写时复制 + 天然版本控制
- **Chunk Store**：所有数据以 Chunk 形式存储，通过哈希索引
- **Noms 模型**：结构化数据抽象，支持多种数据类型
- **持久化**：Manifest 记录根哈希，Chunk 按哈希分片存储
- **对比本项目**：Dolt 侧重版本控制，本项目侧重事务处理

## 思考题

1. Prolly-Tree 的写时复制机制对写密集场景的性能影响如何？有哪些优化策略？
2. 如何在本项目 BTree 中引入内容可寻址机制？需要修改哪些接口？
3. Dolt 的 Chunk Store 与传统 Buffer Pool 各自的优缺点是什么？
4. 如果要为项目添加版本控制功能，应该选择在存储层还是 SQL 层实现？为什么？
5. Noms 数据模型的多态类型系统对查询优化有何挑战？Dolt 是如何解决的？

## 参考资源

- [Dolt Storage Architecture](https://docs.dolthub.com/architecture/storage)
- [Prolly-Tree 论文](https://www.dolthub.com/blog/2021-11-12-prolly-tree/)
- [Noms 数据模型](https://github.com/attic-labs/noms)
- 本项目: `engineering/include/db/storage_engine.h`