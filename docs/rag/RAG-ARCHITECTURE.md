# 多态 RAG 架构

## 整体架构

```mermaid
graph TB
    subgraph "接入层"
        API[REST API]
        SDK[多语言 SDK]
        CLI[CLI 工具]
    end

    subgraph "RAG 核心"
        subgraph "查询处理"
            Query[查询理解]
            Rewrite[查询改写]
            Expand[查询扩展]
        end

        subgraph "检索引擎"
            BM25[BM25 检索]
            Vector[向量检索]
            Graph[图检索]
            Hybrid[混合检索]
        end

        subgraph "重排序"
            RRF[RRF 重排]
            Reranker[重排模型]
            Score[分数融合]
        end
    end

    subgraph "索引层"
        subgraph "索引类型"
            TextIdx[文本索引]
            VecIdx[向量索引]
            GraphIdx[知识图谱]
            SQLIdx[结构化索引]
        end
    end

    subgraph "存储层"
        DocStore[文档存储]
        VecStore[向量存储]
        GraphStore[图存储]
        MetaStore[元数据存储]
    end

    subgraph "Embedding 模型"
        EmbModel[Embedding 模型]
        OpenAI[OpenAI]
        BGE[BGE]
        M3E[M3E]
        Jina[Jina]
    end

    subgraph "LLM"
        LLM[大语言模型]
        ChatGPT[ChatGPT]
        Claude[Claude]
        LocalLLM[本地 LLM]
    end

    API --> Query
    SDK --> Query
    CLI --> Query
    Query --> Rewrite
    Rewrite --> Expand
    Expand --> Hybrid
    Hybrid --> BM25
    Hybrid --> Vector
    Hybrid --> Graph
    BM25 --> TextIdx
    Vector --> VecIdx
    Graph --> GraphIdx
    Hybrid --> RRF
    RRF --> Reranker
    Reranker --> Score
    Score --> LLM
    LLM --> ChatGPT
    LLM --> Claude
    LLM --> LocalLLM
    Vector --> EmbModel
    EmbModel --> OpenAI
    EmbModel --> BGE
    EmbModel --> M3E
    EmbModel --> Jina
```

## 子模块架构

### 1. 文本索引 (BM25)

```mermaid
graph TB
    subgraph "BM25 检索"
        BM25[BM25Engine]
        subgraph "索引构建"
            Parse[文档解析]
            Token[分词]
            Invert[倒排索引]
            IDF[IDF 计算]
        end
        subgraph "查询处理"
            QueryParse[查询解析]
            QueryToken[查询分词]
            Score[BM25 评分]
        end
    end

    subgraph "存储"
        Dict[词典]
        Postings[倒排表]
        DocLen[文档长度]
    end

    subgraph "优化"
        Cache[缓存]
        Pruning[剪枝]
        BlockMax[BlockMax WAND]
    end

    Parse --> Token
    Token --> Invert
    Invert --> IDF
    IDF --> Dict
    Dict --> Postings
    DocLen --> Score
    QueryParse --> QueryToken
    QueryToken --> Score
    Score --> Cache
    Cache --> BlockMax
    BlockMax --> Pruning
```

### 2. 向量检索

```mermaid
graph TB
    subgraph "向量检索"
        VecSearch[VectorSearch]
        subgraph "索引"
            HNSW[HNSW]
            IVF[IVF-Flat]
            IVF_PQ[IVF-PQ]
            NSW[NSW]
            DiskANN[DiskANN]
        end
        subgraph "量化"
            PQ[Product Quantization]
            SQ[Scalar Quantization]
            OPQ[Optimized PQ]
        end
        subgraph "检索策略"
            ANN[ANN 近似]
            KNN[KNN 精确]
            Range[范围检索]
            Hybrid[混合检索]
        end
    end

    subgraph "Embedding"
        Emb[Embedding]
        Norm[归一化]
        Dim[维度处理]
    end

    subgraph "过滤"
        PreFil[预过滤]
        PostFil[后过滤]
        Label[标签过滤]
    end

    VecSearch --> HNSW
    VecSearch --> IVF
    VecSearch --> IVF_PQ
    VecSearch --> NSW
    VecSearch --> DiskANN
    HNSW --> PQ
    IVF --> PQ
    IVF_PQ --> PQ
    PQ --> OPQ
    PQ --> SQ
    VecSearch --> ANN
    VecSearch --> KNN
    VecSearch --> Range
    ANN --> Hybrid
    KNN --> Hybrid
    Range --> Hybrid
    Emb --> Norm
    Norm --> Dim
    Dim --> HNSW
    Hybrid --> PreFil
    Hybrid --> PostFil
    PreFil --> Label
    PostFil --> Label
```

### 3. 知识图谱检索

```mermaid
graph TB
    subgraph "知识图谱"
        KG[KnowledgeGraph]
        subgraph "存储"
            Triple[三元组存储]
            Graph[图索引]
            Prop[属性存储]
        end
        subgraph "查询"
            SPARQL[SPARQL]
            Cypher[Cypher]
            NL[自然语言]
        end
        subgraph "推理"
            TransE[TransE]
            TransH[TransH]
            Rule[规则推理]
        end
    end

    subgraph "实体链接"
        NER[命名实体识别]
        Linking[实体链接]
        Disambig[消歧]
    end

    subgraph "关系抽取"
        RelEx[关系抽取]
        Coref[共指消解]
        Multi[多关系抽取]
    end

    KG --> Triple
    KG --> Graph
    KG --> Prop
    Graph --> SPARQL
    Graph --> Cypher
    Graph --> NL
    Triple --> TransE
    Triple --> TransH
    Triple --> Rule
    KG --> NER
    NER --> Linking
    Linking --> Disambig
    KG --> RelEx
    RelEx --> Coref
    Coref --> Multi
```

### 4. 混合检索

```mermaid
graph TB
    subgraph "混合检索"
        Hybrid[HybridRetrieval]
        subgraph "多路召回"
            VecRecall[向量召回]
            BM25Recall[BM25 召回]
            KGRecall[知识图谱召回]
            SQLRecall[结构化召回]
        end
        subgraph "分数融合"
            RRF[RRF 融合]
            Weight[加权融合]
            COE[Coef 优化]
        end
        subgraph "重排序"
            Cross[交叉编码]
            Late[晚融合]
            Fine[微调重排]
        end
    end

    subgraph "查询理解"
        Intent[意图识别]
        Entity[实体识别]
        Type[类型判断]
    end

    subgraph "结果去重"
        Dedup[去重]
        Merge[合并]
        Diver[多样性]
    end

    Intent --> VecRecall
    Intent --> BM25Recall
    Intent --> KGRecall
    Intent --> SQLRecall
    VecRecall --> RRF
    BM25Recall --> RRF
    KGRecall --> RRF
    SQLRecall --> RRF
    RRF --> Weight
    Weight --> COE
    COE --> Cross
    Cross --> Late
    Late --> Fine
    Fine --> Dedup
    Dedup --> Merge
    Merge --> Diver
```

### 5. 查询处理

```mermaid
graph TB
    subgraph "查询处理"
        QP[QueryProcessor]
        subgraph "理解"
            Parse[语法解析]
            NLU[意图理解]
            Extr[关键信息抽取]
        end
        subgraph "改写"
            CorQA[纠错]
            Expand[查询扩展]
            Norm[标准化]
            Para[ paraphrase]
        end
        subgraph "分解"
            Decompose[问题分解]
            SubQ[子问题生成]
            Join[连接策略]
        end
    end

    subgraph "上下文"
        History[对话历史]
        Session[会话管理]
        Context[上下文构建]
    end

    subgraph "优化"
        Cache[查询缓存]
        RewriteCache[改写缓存]
        Preview[预览生成]
    end

    Parse --> NLU
    NLU --> Extr
    Extr --> CorQA
    CorQA --> Expand
    Expand --> Norm
    Norm --> Para
    Para --> Decompose
    Decompose --> SubQ
    SubQ --> Join
    History --> Session
    Session --> Context
    Context --> Parse
    QP --> Cache
    Cache --> RewriteCache
    RewriteCache --> Preview
```

### 6. RAG 流程

```mermaid
graph LR
    subgraph "阶段一：检索"
        Q1[用户查询]
        Q2[Embedding]
        Q3[多路召回]
        Q4[结果融合]
    end

    subgraph "阶段二：重排"
        R1[粗排]
        R2[精排]
        R3[多样性]
    end

    subgraph "阶段三：生成"
        G1[上下文构建]
        G2[Prompt 组装]
        G3[LLM 生成]
        G4[后处理]
    end

    Q1 --> Q2 --> Q3 --> Q4
    Q4 --> R1 --> R2 --> R3
    R3 --> G1 --> G2 --> G3 --> G4
```

```mermaid
sequenceDiagram
    participant User as 用户
    participant QP as 查询处理器
    participant Emb as Embedding
    participant Vec as 向量检索
    participant BM25 as BM25 检索
    participant KG as 知识图谱
    participant Fusen as 分数融合
    participant Rerank as 重排序
    participant LLM as LLM
    participant Out as 输出

    User->>QP: 用户查询
    QP->>QP: 查询改写/扩展
    QP-->>Emb: 查询向量
    QP-->>Vec: 关键词
    QP-->>KG: 实体信息

    par 并行检索
        Emb->>Vec: ANN 检索
        QP->>BM25: BM25 检索
        QP->>KG: 图检索
    end

    Vec-->>Fusen: Top-K 向量结果
    BM25-->>Fusen: Top-K BM25 结果
    KG-->>Fusen: 图检索结果

    Fusen->>Fusen: RRF 分数融合
    Fusen-->>Rerank: 融合结果

    Rerank->>Rerank: 交叉编码重排
    Rerank-->>LLM: Top-N 相关文档

    LLM->>LLM: 构建 Prompt
    LLM->>LLM: 生成回答
    LLM-->>Out: 回答

    Out-->>User: RAG 回答
```

## 多态特性

### 检索模式

```mermaid
graph TB
    subgraph "多态检索"
        Poly[PolymorphicRAG]
        subgraph "Naive RAG"
            NR[基础 RAG]
            NR_Q[查询]
            NR_R[检索]
            NR_G[生成]
        end
        subgraph "Advanced RAG"
            AR[高级 RAG]
            AR_Q[查询优化]
            AR_I[索引优化]
            AR_R[检索后处理]
        end
        subgraph "Modular RAG"
            MR[模块化 RAG]
            MR_S[搜索模块]
            MR_M[记忆模块]
            MR_K[知识图谱]
        end
    end

    Poly --> NR
    Poly --> AR
    Poly --> MR
```

### 存储模式

| 模式 | 适用场景 | 索引类型 |
|------|----------|----------|
| 纯向量 | 语义相似 | HNSW, IVF-PQ |
| 纯文本 | 关键词精确 | BM25, Inverted |
| 知识图谱 | 关系推理 | Graph, Triple |
| 混合 | 综合检索 | Vector + BM25 + KG |
| 多模态 | 图文音视频 | CLIP, Audio |

### 部署模式

```mermaid
graph TB
    subgraph "单机部署"
        Standalone[单机版]
        Standalone --> Vec1[向量库]
        Standalone --> Doc1[文档库]
    end

    subgraph "分布式部署"
        Dist[分布式版]
        Dist --> Proxy[代理层]
        Proxy --> Vec2[向量集群]
        Proxy --> Doc2[文档集群]
        Proxy --> KG2[图集群]
    end

    subgraph "云原生"
        Cloud[云原生版]
        Cloud --> K8s[Kubernetes]
        K8s --> Vec3[向量服务]
        K8s --> LLM3[LLM 服务]
        K8s --> Cache3[缓存服务]
    end
```
