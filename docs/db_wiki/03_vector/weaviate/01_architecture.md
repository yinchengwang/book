# Weaviate 整体架构

## 学习目标

- 理解 Weaviate 的模块化架构设计
- 掌握 GraphQL API 层与存储引擎的协作
- 了解 Schema 自动推理机制

## 核心概念

- **Class**：类似表的集合，定义数据类型和向量化配置
- **Object**：单个数据记录，自动生成向量
- **Module**：向量化/生成式等可插拔模块
- **Schema**：定义 Class 结构和模块配置
- **Inverted Index**：BM25 全文搜索索引
- **Vector Index**：HNSW 向量索引

## 架构总览

```mermaid
graph TB
    subgraph "接入层"
        GQL[GraphQL API] --> AUTH[认证/授权]
        REST[REST API] --> AUTH
    end

    subgraph "模块层"
        MOD[Module System] --> T2V[text2vec<br/>向量化模块]
        MOD --> GEN[generative<br/>生成式模块]
        MOD --> QNA[qna<br/>问答模块]
        MOD --> NER[ner<br/>实体识别]
    end

    subgraph "核心层"
        SCHEMA[Schema Manager<br/>类型定义/推理]
        OBJ[Object Manager<br/>数据 CRUD]
        SEARCH[Search Engine<br/>向量/混合搜索]
    end

    subgraph "存储层"
        STOR[Storage Engine<br/>LSM Tree]
        VEC[Vector Index<br/>HNSW]
        INV[Inverted Index<br/>BM25]
    end

    AUTH --> SCHEMA
    SCHEMA --> OBJ
    OBJ --> SEARCH
    SEARCH --> VEC
    SEARCH --> INV
    OBJ --> STOR
    T2V --> OBJ
```

## 模块化系统架构

```mermaid
graph LR
    subgraph "向量化模块"
        T2V1[text2vec-openai]
        T2V2[text2vec-huggingface]
        T2V3[text2vec-cohere]
        T2V4[text2vec-contextionary]
    end

    subgraph "生成式模块"
        GEN1[generative-openai]
        GEN2[generative-cohere]
        GEN3[generative-palm]
    end

    subgraph "问答模块"
        QNA1[qna-openai]
        QNA2[qna-transformers]
    end

    subgraph "其他模块"
        NER[ner-transformers]
        SUM[sum-transformers]
        IMG[img2vec-neural]
    end

    WEAV[Weaviate Core] --> T2V1
    WEAV --> GEN1
    WEAV --> QNA1
    WEAV --> NER
```

### 模块配置示例

```json
{
  "class": "Article",
  "moduleConfig": {
    "text2vec-openai": {
      "vectorizeClassName": true,
      "model": "ada",
      "type": "text"
    },
    "qna-openai": {
      "model": "text-davinci-003",
      "maxTokens": 100
    },
    "generative-openai": {
      "model": "gpt-3.5-turbo"
    }
  }
}
```

## Schema 自动推理机制

```mermaid
flowchart TD
    A[用户插入数据] --> B{Schema 是否存在?}
    B -->|否| C[自动创建 Class]
    B -->|是| D[验证字段类型]
    C --> E[推理字段类型]
    E --> F[自动配置向量化模块]
    D --> G[调用向量化模块]
    F --> G
    G --> H[生成向量并存储]
```

### 自动类型推理规则

| 输入值示例 | 推理类型 | 说明 |
|-----------|---------|------|
| `"hello world"` | `text` | 文本类型，自动向量化 |
| `123` | `int` | 整数类型 |
| `3.14` | `number` | 浮点类型 |
| `true` | `boolean` | 布尔类型 |
| `"2024-01-01"` | `date` | 日期类型（自动识别格式） |
| `{"key": "value"}` | `object` | 嵌套对象 |
| `["a", "b", "c"]` | `text[]` | 文本数组 |

## GraphQL API 层

```mermaid
graph TB
    CLIENT[客户端] --> GQL[GraphQL Query]
    GQL --> PARSER[查询解析器]
    PARSER --> VALID[Schema 验证]
    VALID --> EXEC[执行引擎]
    EXEC --> AGG[聚合层]
    AGG --> SEARCH[搜索模块]
    SEARCH --> VEC[向量搜索]
    SEARCH --> HYBRID[混合搜索]
    SEARCH --> BM25[全文搜索]
```

### GraphQL 查询类型

```graphql
# 查询操作
type Query {
  Get { ... }      # 获取对象
  Aggregate { ... } # 聚合查询
  Explore { ... }   # 向量探索
}

# 变更操作
type Mutation {
  Add { ... }      # 添加对象
  Update { ... }   # 更新对象
  Delete { ... }   # 删除对象
}
```

## 分布式部署架构

```mermaid
graph TB
    subgraph "客户端"
        CL[Client SDK]
    end

    subgraph "Weaviate 集群"
        N1[Node 1<br/>Primary]
        N2[Node 2<br/>Replica]
        N3[Node 3<br/>Replica]
    end

    subgraph "外部依赖"
        ETCD[etcd<br/>元数据存储]
        S3[S3/MinIO<br/>对象存储（可选）]
    end

    CL --> N1
    N1 <--> N2
    N2 <--> N3
    N1 --> ETCD
    N2 --> ETCD
    N3 --> ETCD
    N1 -.-> S3
```

### 分片策略

```mermaid
graph LR
    CLASS[Class: Article] --> SHARD1[Shard 1<br/>A-M]
    CLASS --> SHARD2[Shard 2<br/>N-Z]
    SHARD1 --> N1[Node 1]
    SHARD2 --> N2[Node 2]
```

## 存储引擎

```mermaid
graph TB
    subgraph "存储组件"
        OBJ[Object Store<br/>LSM Tree]
        VEC_IDX[Vector Index<br/>HNSW]
        INV_IDX[Inverted Index<br/>BM25]
    end

    subgraph "LSM Tree 结构"
        MEM[MemTable<br/>内存写入]
        IMMU[Immutable Table<br/>不可变内存表]
        SST[SSTable<br/>磁盘文件]
    end

    OBJ --> MEM
    MEM --> IMMU
    IMMU --> SST
```

### 写入流程

```mermaid
sequenceDiagram
    participant C as Client
    participant W as Weaviate
    participant MOD as Module
    participant LSM as LSM Tree
    participant HNSW as HNSW Index

    C->>W: 插入对象（无向量）
    W->>MOD: 调用向量化模块
    MOD-->>W: 返回向量
    W->>LSM: 写入对象数据
    W->>HNSW: 添加向量到索引
    W-->>C: 返回对象 ID
```

### 混合搜索流程

```mermaid
flowchart TD
    Q[查询请求] --> PARSE[解析查询参数]
    PARSE --> ALPHA{alpha 值}

    ALPHA -->|alpha=0| BM25[纯 BM25 搜索]
    ALPHA -->|alpha=1| VEC[纯向量搜索]
    ALPHA -->|0<alpha<1| HYBRID[混合搜索]

    BM25 --> SCORE[分数归一化]
    VEC --> SCORE
    HYBRID --> BM25_SCORE[BM25 分数]
    HYBRID --> VEC_SCORE[向量分数]
    BM25_SCORE --> FUSION[分数融合<br/>alpha 加权]
    VEC_SCORE --> FUSION
    FUSION --> SCORE

    SCORE --> RANK[排序返回]
```

## 要点总结

- Weaviate 采用模块化架构，向量化/生成式模块可插拔
- Schema 自动推理机制降低使用门槛
- GraphQL API 提供灵活的查询能力
- 存储层采用 LSM Tree + HNSW + Inverted Index 三重索引
- 混合搜索通过 alpha 参数控制 BM25 与向量的权重

## 思考题

1. Schema 自动推理在什么场景下可能出错？如何手动干预？
2. 模块化设计如何保证不同向量化模块的向量空间兼容性？
3. 混合搜索中 alpha 参数如何影响搜索结果的相关性和多样性？
4. LSM Tree 在 Weaviate 中如何保证向量索引与对象数据的一致性？
