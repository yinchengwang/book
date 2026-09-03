# RAG Phase 2: 检索增强实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> 
> **目标硬件:** RTX 4060 Laptop (8GB VRAM), 16GB RAM
> **仓库:** D:\code\book\engineering\rag
> **基线:** Phase 1 完成后的所有新增模块

## Global Constraints

- C++17 标准
- 遵循现有 RAG 项目代码风格
- 模块独立可测试
- 与 Phase 1 模块集成

---

## Task 1: Query Expansion 模块

### 概述
实现查询扩展功能，包括 HyDE、同义词扩展、多视角查询三种策略。

### 文件
- Create: `include/rag/query_expander.h` - QueryExpander 基类、HyDEExpander、SynonymExpander、MultiViewExpander
- Create: `src/rag/expander/CMakeLists.txt`
- Create: `src/rag/expander/query_expander.cpp` - 基类实现
- Create: `src/rag/expander/hyde_expander.cpp` - HyDE 实现
- Create: `src/rag/expander/synonym_expander.cpp` - 同义词扩展实现
- Create: `src/rag/expander/multi_view_expander.cpp` - 多视角扩展实现
- Create: `test/rag/test_expander.cpp` - 单元测试

### 接口
- `QueryExpander::expand(const std::string& query) -> std::vector<std::string>`
- `HyDEExpander` 需要 LLM 服务注入
- `SynonymExpander` 需要 Embedding 服务和同义词表

### 配置
```cpp
struct QueryExpansionConfig {
    bool enable = true;
    std::vector<std::string> methods = {"hyde", "synonym"};
    int num_generations = 2;
    float temperature = 0.7f;
    int synonym_top_k = 3;
    float synonym_threshold = 0.85f;
};
```

### 测试用例
1. HyDEExpand - HyDE 扩展测试（mock LLM）
2. SynonymExpand - 同义词扩展测试
3. MultiViewExpand - 多视角扩展测试
4. CombinedExpand - 组合扩展测试
5. QueryExpansionConfig - 配置测试

---

## Task 2: 动态 RRF 权重融合

### 概述
实现查询类型自适应权重和置信度加权融合。

### 文件
- Create: `include/rag/adaptive_rrf.h` - AdaptiveRRF、ConfidenceWeightedFusion
- Create: `src/rag/fusion/CMakeLists.txt`
- Create: `src/rag/fusion/adaptive_rrf.cpp` - 动态权重 RRF 实现
- Create: `test/rag/test_rrf.cpp` - 单元测试

### 接口
```cpp
enum class QueryType { FACTUAL, ANALYTICAL, MULTI_HOP, COMPARATIVE, CHAT };

class AdaptiveRRF {
public:
    Weights get_weights(QueryType query_type);
    std::vector<RetrievalResult> fuse(
        const std::vector<RetrievalResult>& hnsw,
        const std::vector<RetrievalResult>& bm25,
        QueryType query_type, int top_k);
};

class ConfidenceWeightedFusion {
public:
    std::vector<RetrievalResult> fuse(
        const std::vector<RetrievalResult>& hnsw,
        const std::vector<RetrievalResult>& bm25,
        float hnsw_confidence, float bm25_confidence, int top_k);
};
```

### 权重配置
- FACTUAL: hnsw=0.4, bm25=0.6
- ANALYTICAL: hnsw=0.7, bm25=0.3
- MULTI_HOP: hnsw=0.3, bm25=0.2, graph=0.5
- COMPARATIVE: hnsw=0.5, bm25=0.3, graph=0.2
- CHAT: hnsw=0.6, bm25=0.4

### 测试用例
1. AdaptiveRRFFactual - 事实型查询权重
2. AdaptiveRRFAnalytical - 分析型查询权重
3. AdaptiveRRFMultiHop - 多跳查询权重
4. ConfidenceWeightedFusion - 置信度加权
5. RRFNormalization - RRF 归一化

---

## Task 3: Self-RAG 评估

### 概述
实现 Self-RAG 的检索必要性判断和相关性评分。

### 文件
- Create: `include/rag/self_rag.h` - RetrievalNecessityChecker、SelfRAGEvaluator
- Create: `src/rag/selfrag/CMakeLists.txt`
- Create: `src/rag/selfrag/self_rag.cpp` - Self-RAG 实现
- Create: `test/rag/test_selfrag.cpp` - 单元测试

### 接口
```cpp
class RetrievalNecessityChecker {
public:
    bool should_retrieve(const std::string& query, QueryType type);
};

class SelfRAGEvaluator {
public:
    struct EvaluationResult {
        float relevance;
        float support;
        float coherence;
        bool is_useful;
    };
    
    EvaluationResult evaluate(const std::string& query, const std::string& chunk);
    std::vector<RetrievalResult> filter_irrelevant(
        const std::vector<RetrievalResult>& results,
        const std::string& query, float threshold);
};
```

### 配置
```cpp
struct SelfRAGConfig {
    bool enable = true;
    float relevance_threshold = 0.3f;
    float support_threshold = 0.3f;
    bool check_retrieval_necessity = true;
    std::vector<std::string> knowledge_signals = {
        "是什么", "如何", "怎么", "为什么", "哪个", "哪些"
    };
};
```

### 测试用例
1. ShouldRetrieveKnowledge - 知识型查询需要检索
2. ShouldRetrieveChat - 闲聊但含知识信号
3. ShouldNotRetrieve - 纯闲聊不检索
4. EvaluateRelevance - 相关性评估
5. FilterIrrelevant - 过滤无关结果
6. SelfRAGConfig - 配置测试

---

## Task 4: 语义分块

### 概述
实现基于 Embedding 的语义分块和代码感知分块。

### 文件
- Create: `include/rag/semantic_chunker.h` - SemanticChunker、CodeAwareChunker
- Create: `src/rag/chunker/CMakeLists.txt`
- Create: `src/rag/chunker/semantic_chunker.cpp` - 语义分块实现
- Create: `src/rag/chunker/code_chunker.cpp` - 代码感知分块实现
- Create: `test/rag/test_chunker.cpp` - 单元测试

### 接口
```cpp
class SemanticChunker : public Chunker {
public:
    struct Config {
        float similarity_threshold = 0.7f;
        int min_chunk_size = 100;
        int max_chunk_size = 800;
        bool merge_short = true;
    };
    
    std::vector<Chunk> chunk(const Document& doc) override;
};

class CodeAwareChunker : public Chunker {
public:
    struct Config {
        bool preserve_context = true;
        int min_chunk_lines = 5;
        int merge_threshold = 50;
    };
    
    std::vector<Chunk> chunk(const Document& doc) override;
};
```

### 测试用例
1. SemanticChunkerBasic - 基础语义分块
2. SemanticChunkerMerge - 短块合并
3. SemanticChunkerBreakpoints - 语义断点
4. CodeAwareChunkerFunction - 函数级分块
5. CodeAwareChunkerContext - 上下文保留
6. ChunkerConfig - 配置测试

---

## 实施顺序

1. **Task 1**: Query Expansion（3天）- 基础模块，后续任务依赖
2. **Task 2**: 动态 RRF（2天）- 融合模块
3. **Task 3**: Self-RAG（3天）- 评估模块
4. **Task 4**: 语义分块（3天）- 准确性模块

每个任务独立完成并通过测试后进行下一个。
