# LanceDB 整体架构

## 学习目标

- 理解 LanceDB 的嵌入式零服务器架构设计
- 掌握 Lance 列存格式的核心设计理念
- 了解数据读写路径和事务模型

## 核心概念

- **嵌入式架构**：类似 SQLite，无需独立服务器进程
- **Lance 格式**：专为向量数据设计的列存格式
- **Table**：数据表，包含多个列（向量列 + 标量列）
- **Version**：数据版本，支持 Time Travel 查询

## 架构总览

```mermaid
graph TB
    APP[应用程序] --> DB[LanceDB]
    DB --> TABLE[Table 表]
    TABLE --> LANCE[Lance 列存文件<br/>.lance]
    
    LANCE --> COL1[向量列<br/>Embedding]
    LANCE --> COL2[标量列<br/>文本/图像/元数据]
    
    DB --> CACHE[内存缓存]
    DB --> IDX[向量索引<br/>IVF_PQ/HNSW]
    
    style APP fill:#8df,stroke:#35c
    style LANCE fill:#fda,stroke:#c60
```

## Lance 列存格式设计

```mermaid
graph TB
    LANCE[Lance 文件格式] --> SCHEMA[Schema 元数据]
    LANCE --> DATA[数据区]
    LANCE --> IDX[索引区]
    
    SCHEMA --> FIELD[字段定义<br/>名称/类型/向量维度]
    SCHEMA --> VERSION[版本信息<br/>版本号/时间戳]
    
    DATA --> ROW_GROUP[行组 Row Group]
    ROW_GROUP --> COL_CHUNK[列块 Column Chunk]
    COL_CHUNK --> PAGE[数据页 Page]
    
    IDX --> VEC_IDX[向量索引<br/>IVF_PQ/HNSW]
    IDX --> SCALAR_IDX[标量索引<br/>BTree/Bitmap]
    
    style DATA fill:#afa,stroke:#393
    style IDX fill:#fda,stroke:#c60
```

### Lance 与 Parquet 对比

| 特性 | Lance | Parquet |
|------|-------|---------|
| 向量列支持 | 原生支持 | 需自定义类型 |
| 随机访问 | O(1) 定位 | 需扫描 Row Group |
| 增量更新 | 支持 Append/Delete | 需重写整个文件 |
| 索引集成 | 向量索引内置 | 无内置索引 |
| 多模态数据 | 原生支持图像/视频 | 仅结构化数据 |

## 嵌入式架构设计

```mermaid
graph LR
    subgraph "传统向量数据库"
        C1[客户端] --> NET[网络]
        NET --> S1[服务器进程]
        S1 --> STORE[存储层]
    end
    
    subgraph "LanceDB 嵌入式"
        C2[客户端] --> LIB[库调用<br/>Python/Rust]
        LIB --> FILE[文件系统<br/>.lance 文件]
    end
    
    style C2 fill:#8df,stroke:#35c
    style LIB fill:#afa,stroke:#393
```

**嵌入式优势**：
- 零运维：无需部署、监控服务器
- 低延迟：无网络开销，直接文件访问
- 简单部署：`pip install lancedb` 即可
- 数据安全：数据文件可随应用分发

## 数据写入路径

```mermaid
sequenceDiagram
    participant C as Client
    participant DB as LanceDB
    participant MEM as 内存 Buffer
    participant FILE as Lance 文件

    C->>DB: add(data)
    DB->>MEM: 写入内存 Buffer
    MEM->>DB: 返回 Row ID
    DB-->>C: 确认写入
    
    Note over DB,FILE: 后台异步刷盘
    
    DB->>FILE: Append 新数据页
    DB->>FILE: 更新 Manifest
    
    Note over FILE: 创建新版本
```

## 数据读取路径

```mermaid
flowchart TD
    Q[查询请求] --> PARSE[解析查询]
    PARSE --> VEC[获取查询向量]
    VEC --> IDX{使用索引?}
    
    IDX -->|是| IDX_SCAN[索引扫描<br/>IVF_PQ/HNSW]
    IDX -->|否| FULL_SCAN[全表扫描]
    
    IDX_SCAN --> CANDIDATES[候选集]
    FULL_SCAN --> CANDIDATES
    
    CANDIDATES --> FILTER[标量过滤]
    FILTER --> READ[读取列数据]
    READ --> RERANK[精确重排]
    RERANK --> RES[返回 Top-K]
```

## 事务模型

```mermaid
graph TB
    TXN[事务模型] --> MVCC[MVCC 多版本控制]
    
    MVCC --> VER1[版本 1<br/>初始数据]
    MVCC --> VER2[版本 2<br/>追加写入]
    MVCC --> VER3[版本 3<br/>删除标记]
    
    VER1 --> SNAP1[快照查询<br/>v1]
    VER2 --> SNAP2[快照查询<br/>v2]
    VER3 --> SNAP3[当前查询<br/>v3]
    
    VER1 --> CLEANUP[版本清理<br/>GC 回收旧版本]
    
    style MVCC fill:#fda,stroke:#c60
```

**事务特性**：
- **快照隔离**：读操作看到某一版本的快照
- **单写多读**：同一时刻只有一个写事务
- **Time Travel**：可查询任意历史版本

## 版本管理

```mermaid
graph LR
    V1[Version 1<br/>创建表] --> V2[Version 2<br/>插入数据]
    V2 --> V3[Version 3<br/>删除数据]
    V3 --> V4[Version 4<br/>更新数据]
    
    QUERY1[查询 v2] --> V2
    QUERY2[查询最新] --> V4
    QUERY3[查询 v1] --> V1
    
    GC[版本清理] -.->|删除| V1
    GC -.->|删除| V2
    
    style V4 fill:#8df,stroke:#35c
```

## 要点总结

- LanceDB 采用嵌入式架构，无服务器进程，类似 SQLite
- Lance 列存格式专为向量和多模态数据设计
- MVCC 多版本控制支持 Time Travel 查询
- 数据以文件形式存储，支持增量更新

## 思考题

1. Lance 列存格式相比 Parquet，在向量数据场景有哪些优势？
2. 嵌入式架构的优缺点是什么？什么场景适合使用？
3. MVCC 版本管理如何平衡存储空间和查询性能？