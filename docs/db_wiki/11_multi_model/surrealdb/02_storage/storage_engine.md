# 存储引擎

## 学习目标

- 理解 SurrealDB 的核心存储架构和多层抽象设计
- 掌握多存储后端的适配机制与数据持久化策略
- 了解读写路径的关键设计决策
- 对比项目 storage/ 模块的异同

## 核心概念

- **Storage Engine Interface**：统一的存储抽象接口，屏蔽底层差异
- **KV Backend**：底层键值存储层，支持 RocksDB、TiKV、PostgreSQL、内存等
- **Document Store**：文档存储层，Record ID 作为主键
- **Graph Store**：图存储层，顶点和边的独立存储与索引
- **Transaction Layer**：事务层，ACID 保证与隔离级别
- **Index Layer**：索引层，BTree、Full-Text、Vector 索引

## 存储架构

SurrealDB 采用分层存储架构，上层通过统一接口访问底层多种存储后端：

```mermaid
graph TB
    subgraph "查询层"
        Q[SurrealQL 查询]
    end

    subgraph "事务层"
        TXN[Transaction Manager<br/>ACID 支持]
    end

    subgraph "数据模型层"
        DOC[Document Store<br/>文档存储]
        GRAPH[Graph Store<br/>图存储]
        KV[KV Store<br/>键值存储]
    end

    subgraph "索引层"
        BT[BTree Index]
        FT[Full-Text Index]
        VEC[Vector Index]
    end

    subgraph "存储引擎接口"
        SEI[Storage Engine Interface<br/>统一抽象]
    end

    subgraph "存储后端"
        ROCKS[RocksDB<br/>嵌入式 KV]
        TIKV[TiKV<br/>分布式 KV]
        PG[PostgreSQL<br/>关系存储]
        MEM[Memory<br/>内存存储]
        TB[Turso/libSQL<br/>SQLite 兼容]
    end

    Q --> TXN
    TXN --> DOC
    TXN --> GRAPH
    TXN --> KV
    DOC --> SEI
    GRAPH --> SEI
    KV --> SEI
    DOC --> BT
    DOC --> FT
    DOC --> VEC
    SEI --> ROCKS
    SEI --> TIKV
    SEI --> PG
    SEI --> MEM
    SEI --> TB
```

## 存储后端对比

| 后端 | 类型 | 适用场景 | 事务支持 | 分布式 |
|------|------|----------|----------|--------|
| RocksDB | 嵌入式 LSM-Tree | 单机高吞吐 | ACID | 否 |
| TiKV | 分布式 KV | 大规模分布式 | ACID | 是 |
| PostgreSQL | 关系数据库 | 熟悉的运维体系 | ACID | 是（流复制） |
| Memory | 内存存储 | 测试/开发 | 无持久化 | 否 |
| Turso/libSQL | SQLite 兼容 | 边缘计算 | ACID | 否 |

## 数据持久化机制

### Record 存储格式

SurrealDB 使用 Record ID 作为主键，格式为 `table:id`：

```mermaid
graph LR
    A[Record ID<br/>table:id] --> B[Keyspace<br/>命名空间]
    B --> C[Storage Key<br/>编码后的键]
    C --> D[Storage Value<br/>序列化后的值]

    E[编码流程] --> F[表名哈希]
    F --> G[ID 序列化]
    G --> H[组合 Key]
```

**存储 Key 编码示例**：

```
Namespace: test_ns
Database: test_db
Table: person
Record ID: person:john

最终存储 Key（伪代码）:
/test_ns/test_db/person/john
```

### 图数据存储

图的顶点和边分开存储，边通过特殊表 `__edge__` 管理：

```mermaid
graph TB
    subgraph "顶点存储"
        V1[person:1<br/>name: 张三]
        V2[person:2<br/>name: 李四]
    end

    subgraph "边存储"
        E1[friend:edge1<br/>in: person:1<br/>out: person:2<br/>since: 2024]
    end

    subgraph "图索引"
        IDX_IN[入边索引<br/>person:2 -> friend:edge1]
        IDX_OUT[出边索引<br/>person:1 -> friend:edge1]
    end

    V1 --> E1
    V2 --> E1
    E1 --> IDX_IN
    E1 --> IDX_OUT
```

### 事务与 WAL

RocksDB 后端使用 Write-Ahead Log 保证持久性：

```mermaid
sequenceDiagram
    participant C as 客户端
    participant TXN as 事务管理器
    participant WAL as Write-Ahead Log
    participant MEM as MemTable
    participant DISK as SST Files

    C->>TXN: BEGIN TRANSACTION
    TXN->>WAL: 记录 BEGIN 标记
    C->>TXN: 写入操作
    TXN->>WAL: 追加写 WAL
    TXN->>MEM: 写入 MemTable
    C->>TXN: COMMIT
    TXN->>WAL: 记录 COMMIT 标记
    TXN->>WAL: fsync
    Note over MEM: 后台 Flush
    MEM->>DISK: MemTable -> SST
```

## 读写路径

### 写入路径

```mermaid
flowchart TD
    A[SurrealQL 写入语句] --> B[解析与验证]
    B --> C[开启事务]
    C --> D[生成 Record ID]
    D --> E[序列化数据]
    E --> F[写入 WAL]
    F --> G[更新 MemTable]
    G --> H[更新索引]
    H --> I[提交事务]
    I --> J[返回结果]

    subgraph "索引更新"
        H --> BT[BTree 索引]
        H --> FT[全文索引]
        H --> VEC[向量索引]
    end
```

### 读取路径

```mermaid
flowchart TD
    A[SurrealQL 查询语句] --> B[解析与优化]
    B --> C{走索引?}
    C -->|是| D[索引扫描]
    C -->|否| E[全表扫描]
    D --> F[获取 Record IDs]
    E --> F
    F --> G[批量读取数据]
    G --> H[反序列化]
    H --> I[过滤与投影]
    I --> J[返回结果]

    subgraph "数据读取"
        G --> MEM[MemTable]
        G --> BLK[Block Cache]
        G --> SST[SST Files]
    end
```

### 图遍历路径

```mermaid
flowchart TD
    A[图遍历查询<br/>SELECT ->friend->person] --> B[解析起点]
    B --> C[查找出边索引]
    C --> D[获取目标 Record IDs]
    D --> E[批量读取顶点数据]
    E --> F{继续遍历?}
    F -->|是| C
    F -->|否| G[合并结果]
    G --> H[返回结果]

    subgraph "索引辅助"
        C --> OUT_IDX[出边索引<br/>in -> out -> edge]
    end
```

## 索引结构

### BTree 索引

用于等值查询和范围查询：

```mermaid
graph TB
    subgraph "BTree 索引结构"
        ROOT[Root Node]
        L1[Level 1]
        L2[Leaf Nodes]

        ROOT --> L1
        L1 --> L2

        L2 --> LN1[Key: age=25<br/>Record IDs: p1, p3]
        L2 --> LN2[Key: age=30<br/>Record IDs: p2]
        L2 --> LN3[Key: age=35<br/>Record IDs: p4, p5]
    end
```

### 全文索引

支持文本搜索：

```mermaid
graph TB
    subgraph "全文索引结构"
        TK[Tokenizer<br/>分词器]
        INV[倒排索引]

        TK --> W1[张三 -> person:1]
        TK --> W2[李四 -> person:2]
        TK --> W3[工程师 -> person:1, person:3]

        W1 --> INV
        W2 --> INV
        W3 --> INV
    end
```

### 向量索引

支持相似度搜索：

```mermaid
graph TB
    subgraph "向量索引结构"
        EMB[Embedding 提取]
        IVF[IVF/HNSW 索引]

        EMB --> V1[向量 [0.1, 0.2, ...]<br/>Record: article:1]
        EMB --> V2[向量 [0.3, 0.1, ...]<br/>Record: article:2]

        V1 --> IVF
        V2 --> IVF
    end
```

## 与项目 storage/ 模块对比

| 维度 | SurrealDB | 项目 storage/ |
|------|-----------|---------------|
| 存储后端 | RocksDB/TiKV/PG/Memory | 自研 Buffer Pool + WAL |
| 数据模型 | 文档+图+KV | KV+Vector+Timeseries+Document+Spatial+Graph |
| 事务支持 | ACID，支持隔离级别 | 2PC 分布式事务 |
| 索引类型 | BTree/全文/向量 | BTree/Hash/向量索引 |
| 嵌入式支持 | 是（Rust/JS SDK） | 否（服务端模式） |
| 分布式支持 | TiKV 后端 | Raft + 分片路由 |

### 可借鉴的设计点

| 借鉴点 | SurrealDB 实现 | 项目应用建议 |
|--------|---------------|--------------|
| 存储后端抽象 | Storage Engine Interface | 统一 storage_engine.h 接口 |
| 图存储分离 | 顶点/边独立表 + 入出边索引 | Graph 引擎添加边索引 |
| 多索引支持 | BTree/全文/向量统一管理 | 扩展 index/ 模块支持多类型 |
| 嵌入式模式 | Rust/JS SDK 嵌入 | 提供 C SDK 嵌入式 API |

## 要点总结

- **分层架构**：事务层 → 数据模型层 → 存储引擎接口 → 存储后端
- **多后端支持**：通过 Storage Engine Interface 抽象，支持 RocksDB、TiKV、PostgreSQL 等
- **图存储设计**：顶点和边分开存储，通过入边/出边索引加速遍历
- **索引体系**：BTree、全文、向量索引统一管理
- **与项目对比**：项目自研存储引擎，SurrealDB 采用成熟后端；图存储思路可借鉴

## 思考题

1. SurrealDB 如何保证跨存储后端的事务一致性？
2. 图遍历查询在 RocksDB 后端的性能瓶颈在哪里？如何优化？
3. 项目 storage/ 模块如果要支持多种存储后端，应该如何设计抽象接口？
4. SurrealDB 的嵌入式部署模式有哪些优势和劣势？
