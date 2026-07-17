# Design: RAG 模块优化

## Context

当前 RAG 模块 (`rag/`) 虽然具备完整的架构，但核心的 Embedding 和 LLM 服务都是占位实现：

- **SimpleEmbeddingService**：使用随机向量生成，无法实现语义相似性
- **SimpleLLMService**：基于关键词硬编码回复，无法处理任意问题

这导致系统无法进行真实的 RAG 效果评估。

## Goals / Non-Goals

**Goals:**
- 集成 Ollama API，实现真实的 Embedding 和 LLM 推理
- 引入 RAGAS 评估框架，建立 RAG 效果评估能力
- 实现向量索引持久化，支持重启后恢复
- 保持向后兼容，Simple 服务仍可用于测试

**Non-Goals:**
- 不实现 llama.cpp 直接集成（使用 Ollama 作为抽象层）
- 不实现 PDF/Word 解析（Phase 2）
- 不实现多租户和访问控制

## Decisions

### 决策 1: Ollama 作为本地推理抽象层

**选择**: Ollama API (HTTP)

**理由**:
1. **零依赖**：无需编译 llama.cpp 或 transformers，直接通过 HTTP 调用
2. **模型管理**：Ollama 自动管理模型下载和版本
3. **跨平台**：Windows/Linux/Mac 一致体验
4. **灵活切换**：可随时更换模型（llama3.2/qwen2.5/deepseek-r1）

**替代方案考虑**:
- ❌ llama.cpp 直接集成：编译复杂，Windows 支持不佳
- ❌ HuggingFace transformers：需要 Python 环境或复杂 C++ 绑定
- ❌ OpenAI API：需要网络和 API Key，有成本

### 决策 2: RAGAS 评估指标实现

**选择**: 在 C++ 中实现核心指标的计算逻辑

```cpp
// rag/include/rag/evaluator.h
struct RAGEvaluation {
    double faithfulness;      // 答案对上下文的忠实度
    double answer_relevance; // 答案与问题的相关性
    double context_relevance;// 上下文与问题的相关性
};

struct RetrievalMetrics {
    double recall_at_k;     // Recall@K
    double mrr;             // MRR (Mean Reciprocal Rank)
    double ndcg_at_k;      // NDCG@K
};
```

**Faithfulness 计算**:
```
1. 将答案拆分为独立的陈述
2. 对每个陈述，检查是否能从上下文中推导
3. Faithfulness = (有效陈述数) / (总陈述数)
```

**Answer Relevance 计算**:
```
1. 使用 LLM 生成 N 个与答案相关的问题
2. 计算生成问题与原始问题的余弦相似度
3. Relevance = avg(similarities)
```

### 决策 3: OllamaEmbeddingService 设计

```cpp
// rag/include/rag/ollama_embedding.h
class OllamaEmbeddingService : public EmbeddingService {
public:
    OllamaEmbeddingService(
        const std::string& base_url = "http://localhost:11434",
        const std::string& model = "nomic-embed-text",
        bool use_cache = true);

    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;

    int dimension() const override { return dimension_; }
    bool is_ready() const override;
    std::string model_name() const override { return model_; }

private:
    std::string base_url_;
    std::string model_;
    int dimension_ = 768;  // nomic-embed-text 默认维度
    bool use_cache_;
    httplib::Client http_client_;
    std::unordered_map<std::string, std::vector<float>> cache_;
    std::mutex cache_mutex_;
};
```

**API 调用格式**:
```bash
POST http://localhost:11434/api/embeddings
{
    "model": "nomic-embed-text",
    "prompt": "要编码的文本"
}
```

### 决策 4: OllamaLLMService 设计

```cpp
// rag/include/rag/ollama_llm.h
class OllamaLLMService : public LLMService {
public:
    OllamaLLMService(
        const std::string& base_url = "http://localhost:11434",
        const std::string& model = "llama3.2");

    void load(const std::string& model_path,
               const LLMConfig& config) override;
    void unload() override;
    bool is_loaded() const override;

    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options) override;
    void generate_stream(const std::string& prompt,
                        const GenerateOptions& options,
                        StreamCallback callback) override;

    int context_window() const override;
    const std::string& model_type() const override;
    const std::string& model_path() const override;

private:
    std::string base_url_;
    std::string model_;
    std::string model_type_;
    int context_window_ = 4096;
    httplib::Client http_client_;
};
```

**同步生成 API**:
```bash
POST http://localhost:11434/api/generate
{
    "model": "llama3.2",
    "prompt": "用户问题...",
    "stream": false
}
```

**流式生成 API**:
```bash
POST http://localhost:11434/api/generate
{
    "model": "llama3.2",
    "prompt": "用户问题...",
    "stream": true
}
```

### 决策 5: 向量索引持久化

```cpp
// rag/include/rag/hnsw_persist.h
class HNSWPersister {
public:
    // 序列化
    int save(HNSWIndex* index, const std::string& path);

    // 反序列化
    int load(HNSWIndex* index, const std::string& path);

    // 元数据
    struct IndexMeta {
        std::string model_name;
        int dimension;
        size_t vector_count;
        int max_level;
        int ef_construction;
        int M;
        int64_t created_at;
    };
};
```

**文件格式**:
```
┌────────────────────────────────────────┐
│ Header (64 bytes)                     │
│   magic: 0x484E5357 "HNSW"            │
│   version: 1                          │
│   dimension: int                       │
│   vector_count: size_t                │
│   ...                                 │
├────────────────────────────────────────┤
│ HNSW Data (可变长)                    │
│   levels, edges, vectors...           │
└────────────────────────────────────────┘
```

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| Ollama 服务不可用 | Embedding/LLM 调用失败 | Simple 服务作为 fallback，添加重试逻辑 |
| 网络延迟影响性能 | 端到端延迟增加 | 添加本地缓存，批量编码减少请求数 |
| 模型质量问题 | 检索/生成效果不佳 | 支持切换模型，评估框架量化效果 |
| 上下文长度限制 | 长文档截断 | 支持文档自动分块 |

## Migration Plan

### Phase 1: Ollama 集成 (Week 1)
1. 创建 `ollama_embedding.cpp` 和 `ollama_llm.cpp`
2. 添加 httplib 依赖
3. 更新 CMakeLists.txt
4. 添加单元测试

### Phase 2: 评估框架 (Week 2)
1. 创建 `evaluator.cpp`
2. 实现 RetrievalMetrics 计算
3. 实现 RAGEvaluation 计算（需要 LLM）
4. 添加基准测试数据集

### Phase 3: 持久化 (Week 3)
1. 实现 HNSW 序列化/反序列化
2. 添加增量索引 API
3. 集成到 RAGEngine

## Open Questions

1. **Ollama 模型选择**: 推荐中文场景使用 qwen2.5，英文场景使用 llama3.2？
2. **评估数据集**: 是否需要预设的 QA 数据集？
3. **缓存策略**: Embedding 缓存大小限制？如何处理 evicted ？
