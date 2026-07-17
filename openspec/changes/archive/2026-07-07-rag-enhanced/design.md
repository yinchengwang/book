# RAG 系统增强设计文档

> 状态: 草稿 | 创建日期: 2026-07-07

## 1. 设计原则

### 1.1 核心原则

1. **向后兼容**: 现有接口保持不变，新增模块独立实现
2. **渐进增强**: Phase 1 不依赖 Phase 2，可独立部署
3. **可选依赖**: 模型推理失败时回退到简化实现
4. **性能优先**: 避免引入显著的性能开销

### 1.2 代码组织

```
rag/
├── include/rag/
│   ├── embedding.h           # Embedding 服务接口 (新增)
│   ├── reranker.h            # 重排序器接口 (新增)
│   ├── enhanced_retriever.h  # 增强检索器 (新增)
│   ├── semantic_chunker.h    # 语义分块器 (新增)
│   ├── knowledge_graph.h     # 知识图谱 (新增)
│   ├── entity_extractor.h    # 实体提取器 (新增)
│   ├── graph_retriever.h     # 图检索器 (新增)
│   └── ...
│
├── src/rag/
│   ├── embedding/            # Embedding 实现 (新增)
│   │   ├── CMakeLists.txt
│   │   └── bge_embedding.cpp
│   │
│   ├── reranker/             # 重排序实现 (新增)
│   │   ├── CMakeLists.txt
│   │   ├── lightweight_reranker.cpp
│   │   └── bge_reranker.cpp
│   │
│   ├── retrieval/
│   │   └── enhanced_retriever.cpp  # 增强检索器 (新增)
│   │
│   ├── chunker/
│   │   └── semantic_chunker.cpp    # 语义分块 (新增)
│   │
│   ├── knowledge_graph/      # 知识图谱 (新增)
│   │   ├── CMakeLists.txt
│   │   ├── knowledge_graph.cpp
│   │   └── graph_storage.cpp
│   │
│   ├── entity_extraction/    # 实体提取 (新增)
│   │   ├── CMakeLists.txt
│   │   ├── llm_extractor.cpp
│   │   └── rule_extractor.cpp
│   │
│   └── graph_retrieval/      # 图检索 (新增)
│       ├── CMakeLists.txt
│       └── graph_retriever.cpp
│
└── test/rag/
    ├── test_embedding.cpp
    ├── test_reranker.cpp
    ├── test_kg.cpp
    └── test_graph_retriever.cpp
```

## 2. 详细设计

### 2.1 Embedding 服务

#### 2.1.1 接口设计

```cpp
// rag/include/rag/embedding.h

namespace rag {

/**
 * @brief Embedding 服务接口
 */
class EmbeddingService {
public:
    virtual ~EmbeddingService() = default;
    
    // 编码单个文本
    virtual std::vector<float> encode(const std::string& text) = 0;
    
    // 批量编码
    virtual std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) = 0;
    
    // 获取向量维度
    virtual int dimension() const = 0;
    
    // 是否就绪
    virtual bool is_ready() const = 0;
    
    // 获取模型名称
    virtual std::string model_name() const = 0;
};

/**
 * @brief BGE-M3 Embedding 服务
 */
class BGEM3EmbeddingService : public EmbeddingService {
public:
    BGEM3EmbeddingService(const std::string& model_path = "",
                          const std::string& device = "cpu",
                          int batch_size = 32);
    
    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;
    
    int dimension() const override { return 1024; }
    bool is_ready() const override { return ready_; }
    std::string model_name() const override { return "bge-m3"; }
    
    // 稀疏向量 (用于混合检索)
    struct SparseVector {
        std::vector<uint32_t> indices;
        std::vector<float> values;
    };
    SparseVector encode_sparse(const std::string& text);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
};

}  // namespace rag
```

#### 2.1.2 实现策略

**Pimpl 模式**: 使用 Pimpl 隐藏 transformers/onnxruntime 依赖，编译时可选择是否包含。

**回退机制**:
```cpp
std::vector<float> BGEM3EmbeddingService::encode(const std::string& text) {
    if (!ready_) {
        // 回退到随机向量
        return std::vector<float>(dimension_, 0.0f);
    }
    // ... 正常逻辑
}
```

**缓存机制**:
```cpp
// LRU 缓存，限制大小避免内存膨胀
std::unordered_map<std::string, std::vector<float>> cache_;
size_t max_cache_size_ = 10000;
```

### 2.2 重排序器

#### 2.2.1 接口设计

```cpp
// rag/include/rag/reranker.h

namespace rag {

struct RerankResult {
    std::string chunk_id;
    std::string content;
    float score;
    std::string reason;  // 重排原因 (可选)
};

/**
 * @brief 重排序器接口
 */
class Reranker {
public:
    virtual ~Reranker() = default;
    
    virtual bool init(const std::string& model_path) = 0;
    
    virtual std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) = 0;
    
    virtual std::string model_name() const = 0;
    virtual bool is_ready() const = 0;
};

/**
 * @brief 轻量级重排序器 (无外部依赖)
 */
class LightweightReranker : public Reranker {
public:
    LightweightReranker();
    
    bool init(const std::string& model_path) override {
        return true;  // 无需初始化
    }
    
    std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) override;
    
    std::string model_name() const override { return "lightweight"; }
    bool is_ready() const override { return true; }
    
    // 注入分数
    void set_bm25_scores(const std::unordered_map<std::string, float>& scores);
    void set_vector_scores(const std::unordered_map<std::string, float>& scores);

private:
    float compute_score(const std::string& query, const Chunk& chunk);
    float keyword_match_score(const std::string& query, const std::string& content);
    
    std::unordered_map<std::string, float> bm25_scores_;
    std::unordered_map<std::string, float> vector_scores_;
};

}  // namespace rag
```

#### 2.2.2 LightweightReranker 算法

```
综合分数 = 关键词匹配 * 0.4 + BM25分数 * 0.3 + 向量分数 * 0.3 + 位置奖励 * 0.05

关键词匹配 = 匹配词数 / 总词数 + (文档以查询开头 ? 0.2 : 0)

位置奖励 = chunk_index == 0 ? 0.05 : 0
```

### 2.3 增强检索器

#### 2.3.1 接口设计

```cpp
// rag/include/rag/enhanced_retriever.h

namespace rag {

struct EnhancedRetrievalConfig : public RetrievalConfig {
    bool enable_rerank = true;
    std::string reranker_type = "lightweight";
    std::string reranker_model;
    
    int recall_top_k = 50;
    int return_top_k = 10;
    
    bool enable_mmr = false;
    float mmr_lambda = 0.7f;
    
    bool enable_query_expansion = false;
    std::string expansion_method = "hyde";
};

/**
 * @brief 增强检索器
 */
class EnhancedRetriever : public Retriever {
public:
    EnhancedRetriever(
        std::shared_ptr<HybridRetriever> base_retriever,
        std::shared_ptr<Reranker> reranker,
        std::shared_ptr<EmbeddingService> embed_service,
        const EnhancedRetrievalConfig& config);
    
    std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) override;
    
    std::string name() const override { return "enhanced"; }
    const RetrievalConfig& config() const override { return config_; }

private:
    std::vector<RetrievalResult> select_with_mmr(
        const std::vector<RetrievalResult>& candidates,
        const std::string& query,
        int top_k);
    
    std::shared_ptr<HybridRetriever> base_retriever_;
    std::shared_ptr<Reranker> reranker_;
    std::shared_ptr<EmbeddingService> embed_service_;
    EnhancedRetrievalConfig config_;
};

}  // namespace rag
```

#### 2.3.2 MMR 算法

```cpp
// Maximum Marginal Relevance 算法
std::vector<RetrievalResult> EnhancedRetriever::select_with_mmr(
    const std::vector<RetrievalResult>& candidates,
    const std::string& query,
    int top_k) {
    
    std::vector<RetrievalResult> selected;
    auto remaining = candidates;
    auto query_vec = embed_service_->encode(query);
    
    while (selected.size() < top_k && !remaining.empty()) {
        float best_mmr = -1.0f;
        size_t best_idx = 0;
        
        for (size_t i = 0; i < remaining.size(); i++) {
            float relevance = remaining[i].score;
            
            // 最大相似度 (多样性惩罚)
            float max_sim = 0.0f;
            for (const auto& sel : selected) {
                auto sel_vec = embed_service_->encode(sel.chunk.content);
                max_sim = std::max(max_sim, cosine_similarity(query_vec, sel_vec));
            }
            
            // MMR = λ * relevance + (1-λ) * (1 - max_sim)
            float mmr = mmr_lambda_ * relevance + 
                       (1 - mmr_lambda_) * (1 - max_sim);
            
            if (mmr > best_mmr) {
                best_mmr = mmr;
                best_idx = i;
            }
        }
        
        selected.push_back(remaining[best_idx]);
        remaining.erase(remaining.begin() + best_idx);
    }
    
    return selected;
}
```

### 2.4 知识图谱

#### 2.4.1 数据结构

```cpp
// rag/include/rag/knowledge_graph.h

namespace rag {

/**
 * @brief 实体
 */
struct KGEntity {
    std::string id;                    // hash(name + type)
    std::string name;                  // 实体名称
    std::string type;                  // PERSON/ORG/LOC/CONCEPT/TECHNOLOGY
    
    std::string description;           // LLM 生成的描述
    std::unordered_map<std::string, std::string> properties;
    
    std::string source_chunk_id;       // 来源
    float confidence = 1.0f;           // 置信度
    
    std::vector<float> embedding;      // 实体向量
    int degree = 0;                    // 连接数
};

/**
 * @brief 关系
 */
struct KGRelation {
    std::string id;
    std::string source_id;
    std::string target_id;
    std::string type;                  // WORKS_FOR/LOCATED_IN/USES/...
    
    std::string description;
    float weight = 1.0f;
    float confidence = 1.0f;
    
    std::string source_chunk_id;
    std::string reverse_type;          // 反向关系类型
};

/**
 * @brief 知识图谱
 */
class KnowledgeGraph {
public:
    // 实体操作
    void add_entity(const KGEntity& entity);
    void update_entity(const KGEntity& entity);
    void remove_entity(const std::string& entity_id);
    std::optional<KGEntity> get_entity(const std::string& entity_id) const;
    
    // 关系操作
    void add_relation(const KGRelation& relation);
    std::vector<KGRelation> get_relations(const std::string& entity_id);
    
    // 图遍历
    std::vector<std::string> get_neighbors(const std::string& entity_id, int depth = 1);
    
    // 持久化
    void save(const std::string& path);
    void load(const std::string& path);

private:
    std::unordered_map<std::string, KGEntity> entities_;
    std::unordered_map<std::string, KGRelation> relations_;
    std::unordered_map<std::string, std::vector<std::string>> adjacency_;
};

}  // namespace rag
```

#### 2.4.2 与 graph_engine 集成

```cpp
// rag/src/rag/knowledge_graph/graph_storage.cpp

class GraphEngineStorage : public KnowledgeGraph {
public:
    GraphEngineStorage(const std::string& data_dir) {
        graph_engine_init(data_dir.c_str());
        graph_ = graph_engine_open("knowledge_graph", ACCESS_MODE_RW);
        
        if (!graph_) {
            graph_engine_create("knowledge_graph", nullptr);
            graph_ = graph_engine_open("knowledge_graph", ACCESS_MODE_RW);
        }
    }
    
    void add_entity(const KGEntity& entity) override {
        // 序列化
        std::string data = serialize_entity(entity);
        
        // 添加到 graph_engine
        graph_vertex_id_t vid = graph_engine_add_vertex(graph_, data.data(), data.size());
        
        entity_id_to_vertex_[entity.id] = vid;
        vertex_to_entity_id_[vid] = entity.id;
    }
    
    void add_relation(const KGRelation& relation) override {
        auto src_vid = entity_id_to_vertex_[relation.source_id];
        auto tgt_vid = entity_id_to_vertex_[relation.target_id];
        
        // 序列化关系
        std::string data = serialize_relation(relation);
        
        // 添加边
        graph_engine_add_edge(graph_, data.data(), data.size());
    }
    
    std::vector<std::string> get_neighbors(
        const std::string& entity_id, int depth) override {
        
        std::vector<std::string> neighbors;
        std::unordered_set<std::string> visited;
        std::queue<std::pair<std::string, int>> queue;
        
        queue.emplace(entity_id, 0);
        visited.insert(entity_id);
        
        while (!queue.empty()) {
            auto [current, current_depth] = queue.front();
            queue.pop();
            
            if (current_depth >= depth) continue;
            
            // 使用 graph_engine 的遍历接口
            auto vid = entity_id_to_vertex_[current];
            auto edge_ids = graph_csr_neighbors(graph_, vid);
            
            for (auto neighbor_vid : edge_ids) {
                auto neighbor_id = vertex_to_entity_id_[neighbor_vid];
                if (visited.insert(neighbor_id).second) {
                    neighbors.push_back(neighbor_id);
                    queue.emplace(neighbor_id, current_depth + 1);
                }
            }
        }
        
        return neighbors;
    }

private:
    void* graph_;  // graph_engine 句柄
    std::unordered_map<std::string, graph_vertex_id_t> entity_id_to_vertex_;
    std::unordered_map<graph_vertex_id_t, std::string> vertex_to_entity_id_;
};
```

### 2.5 实体提取器

#### 2.5.1 接口设计

```cpp
// rag/include/rag/entity_extractor.h

namespace rag {

struct ExtractionResult {
    std::vector<KGEntity> entities;
    std::vector<KGRelation> relations;
    std::string summary;
};

/**
 * @brief 实体提取器接口
 */
class EntityExtractor {
public:
    virtual ~EntityExtractor() = default;
    
    virtual ExtractionResult extract(const std::string& text) = 0;
    virtual ExtractionResult extract_from_chunk(const Chunk& chunk) = 0;
};

/**
 * @brief 基于 LLM 的实体提取
 */
class LLMBasedExtractor : public EntityExtractor {
public:
    LLMBasedExtractor(std::shared_ptr<LLMService> llm_service);
    
    ExtractionResult extract(const std::string& text) override;
    ExtractionResult extract_from_chunk(const Chunk& chunk) override;

private:
    std::string build_extraction_prompt(const std::string& text);
    ExtractionResult parse_extraction_response(const std::string& response);
    
    std::shared_ptr<LLMService> llm_service_;
    std::vector<std::string> entity_types_ = {
        "PERSON", "ORG", "LOC", "CONCEPT", "TECHNOLOGY"
    };
    std::vector<std::string> relation_types_ = {
        "WORKS_FOR", "LOCATED_IN", "USES", "RELATED_TO"
    };
};

/**
 * @brief 基于规则的实体提取 (轻量级)
 */
class RuleBasedExtractor : public EntityExtractor {
public:
    RuleBasedExtractor();
    
    ExtractionResult extract(const std::string& text) override;
    ExtractionResult extract_from_chunk(const Chunk& chunk) override;
    
    void add_entity_pattern(const std::string& type, const std::regex& pattern);

private:
    std::vector<std::pair<std::string, std::regex>> entity_patterns_;
    std::vector<std::pair<std::string, std::regex>> relation_patterns_;
};

}  // namespace rag
```

#### 2.5.2 LLM 提取 Prompt

```cpp
std::string LLMBasedExtractor::build_extraction_prompt(const std::string& text) {
    return R"(
请从以下文本中提取实体和关系，并以 JSON 格式返回。

文本：
""" 
)" + text + R"(
"""

要求：
1. 实体类型: PERSON, ORG, LOC, CONCEPT, TECHNOLOGY
2. 关系类型: WORKS_FOR, LOCATED_IN, USES, RELATED_TO, PART_OF
3. 每个实体: name, type, description
4. 每个关系: source, target, type, description
5. 只提取明确存在的关系

返回格式:
{
  "entities": [{"name": "", "type": "", "description": ""}],
  "relations": [{"source": "", "target": "", "type": "", "description": ""}],
  "summary": "一句话摘要"
}
)";
}
```

### 2.6 图检索器

#### 2.6.1 接口设计

```cpp
// rag/include/rag/graph_retriever.h

namespace rag {

struct GraphRetrievalResult {
    std::string chunk_id;
    std::string content;
    std::string entity_id;
    std::string entity_name;
    float score;
    int hop_distance;
};

/**
 * @brief Graph RAG 检索器
 */
class GraphRetriever {
public:
    GraphRetriever(
        std::shared_ptr<KnowledgeGraph> kg,
        std::shared_ptr<EntityExtractor> extractor,
        std::shared_ptr<EmbeddingService> embed_service);
    
    // 图检索
    std::vector<GraphRetrievalResult> retrieve(
        const std::string& query,
        int top_k,
        int max_hops = 2);
    
    // 获取子图
    struct Subgraph {
        std::vector<KGEntity> entities;
        std::vector<KGRelation> relations;
        std::string summary;
    };
    Subgraph get_subgraph(const std::string& query, int max_hops = 2);

private:
    std::vector<std::string> link_query_entities(const std::string& query);
    std::vector<KGEntity> query_subgraph(
        const std::vector<std::string>& seed_entities,
        int max_hops);
    
    std::shared_ptr<KnowledgeGraph> kg_;
    std::shared_ptr<EntityExtractor> extractor_;
    std::shared_ptr<EmbeddingService> embed_service_;
};

/**
 * @brief 混合 Graph + Vector 检索
 */
class HybridGraphRetriever : public Retriever {
public:
    HybridGraphRetriever(
        std::shared_ptr<GraphRetriever> graph_retriever,
        std::shared_ptr<Retriever> vector_retriever,
        float graph_weight = 0.5f);
    
    std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) override;
    
    std::string name() const override { return "hybrid_graph"; }
    const RetrievalConfig& config() const override { return config_; }

private:
    std::vector<RetrievalResult> fuse_results(
        const std::vector<RetrievalResult>& graph_results,
        const std::vector<RetrievalResult>& vector_results);
    
    std::shared_ptr<GraphRetriever> graph_retriever_;
    std::shared_ptr<Retriever> vector_retriever_;
    float graph_weight_;
    RetrievalConfig config_;
};

}  // namespace rag
```

#### 2.6.2 检索流程

```cpp
std::vector<GraphRetrievalResult> GraphRetriever::retrieve(
    const std::string& query,
    int top_k,
    int max_hops) {
    
    // 1. 实体链接: 找到查询中的实体
    auto query_entities = link_query_entities(query);
    
    // 2. 子图扩展
    auto subgraph_entities = query_subgraph(query_entities, max_hops);
    
    // 3. 排序
    return rank_results(query, subgraph_entities, max_hops);
}

std::vector<std::string> GraphRetriever::link_query_entities(
    const std::string& query) {
    
    // 方法1: 从 KG 搜索相似实体
    auto candidates = kg_->search_entities(query, 10);
    
    // 方法2: LLM 提取后匹配
    if (candidates.empty()) {
        auto extraction = extractor_->extract(query);
        for (const auto& entity : extraction.entities) {
            auto found = kg_->get_entity(entity.name);
            if (found) {
                return {found->id};
            }
        }
    }
    
    return {};
}
```

## 3. 集成策略

### 3.1 RAG 引擎集成

```cpp
// rag/src/rag/engine/engine.cpp 修改

void RAGEngine::init_components() {
    // ... 原有代码 ...
    
    // 根据配置选择检索器类型
    if (config_.retrieval.type == "enhanced") {
        // Phase 1: 增强检索器
        auto reranker = std::make_shared<LightweightReranker>();
        auto enhanced_config = config_.retrieval.enhanced;
        
        enhanced_retriever_ = std::make_shared<EnhancedRetriever>(
            std::dynamic_pointer_cast<HybridRetriever>(retriever_),
            reranker,
            embed_service_,
            enhanced_config);
        
        retriever_ = enhanced_retriever_;
    } else if (config_.retrieval.type == "graph") {
        // Phase 2: Graph RAG
        init_graph_rag();
    }
}

void RAGEngine::init_graph_rag() {
    // 初始化知识图谱
    kg_storage_ = std::make_shared<GraphEngineStorage>(
        data_dir_.string() + "/knowledge_graph");
    
    // 初始化实体提取器
    entity_extractor_ = std::make_shared<RuleBasedExtractor>();
    
    // 初始化图检索器
    graph_retriever_ = std::make_shared<GraphRetriever>(
        kg_storage_,
        entity_extractor_,
        embed_service_);
    
    // 创建混合检索器
    hybrid_graph_retriever_ = std::make_shared<HybridGraphRetriever>(
        graph_retriever_,
        enhanced_retriever_,
        config_.retrieval.graph.graph_weight);
    
    retriever_ = hybrid_graph_retriever_;
}
```

### 3.2 图谱构建流水线

```cpp
// 新增: 图谱构建
void RAGEngine::build_knowledge_graph() {
    auto chunks = chunk_repo_->find_all();
    
    for (const auto& chunk : chunks) {
        // 1. 提取实体和关系
        auto extraction = entity_extractor_->extract_from_chunk(chunk);
        
        // 2. 添加到图谱
        for (const auto& entity : extraction.entities) {
            kg_storage_->add_entity(entity);
        }
        
        for (const auto& relation : extraction.relations) {
            kg_storage_->add_relation(relation);
        }
    }
    
    // 3. 持久化
    kg_storage_->save(data_dir_ / "knowledge_graph.json");
}
```

## 4. 配置设计

### 4.1 完整配置

```yaml
# rag/config.yaml

rag:
  data_dir: "./rag_data"
  index_dir: "./rag_data/index"
  
  # 数据源
  data_sources:
    - path: "./docs"
      file_types: [".md", ".txt"]
    - path: "./src"
      file_types: [".cpp", ".h", ".hpp", ".c"]
  
  # 分块配置
  chunking:
    type: "recursive"
    chunk_size: 512
    overlap: 50
  
  # 检索配置
  retrieval:
    type: "enhanced"  # basic | enhanced | graph
    
    # 基础配置
    top_k: 5
    hybrid_weight: 0.5
    rrf_k: 60
    
    # 增强配置
    enhanced:
      enable_rerank: true
      reranker_type: "lightweight"
      reranker_model: ""
      recall_top_k: 50
      return_top_k: 10
      
      enable_mmr: false
      mmr_lambda: 0.7
      
      enable_query_expansion: false
      expansion_method: "hyde"
    
    # Graph RAG 配置
    graph:
      enable_graph: true
      entity_types:
        - PERSON
        - ORG
        - LOC
        - CONCEPT
        - TECHNOLOGY
      max_hops: 2
      graph_weight: 0.5
      min_confidence: 0.5
  
  # Embedding 配置
  embedding:
    type: "simple"  # simple | bge-m3
    dimension: 768
    
    # BGE-M3 配置
    bge:
      model_path: "./models/bge-m3"
      device: "cpu"
      batch_size: 32
  
  # HNSW 配置
  hnsw:
    dim: 768
    m: 16
    ef_construction: 200
    ef_search: 50
    
  # BM25 配置
  bm25:
    k1: 1.5
    b: 0.75
```

## 5. 错误处理

### 5.1 模型加载失败

```cpp
BGEM3EmbeddingService::BGEM3EmbeddingService(...) {
    try {
        impl_->tokenizer = load_tokenizer(model_path);
        impl_->model = load_transformers_model(model_path, device);
        ready_ = true;
    } catch (const std::exception& e) {
        RAG_WARN("Failed to load BGE model: " + std::string(e.what()));
        RAG_WARN("Using fallback random embeddings");
        ready_ = false;
    }
}
```

### 5.2 检索降级

```cpp
std::vector<RetrievalResult> EnhancedRetriever::retrieve(
    const std::string& query,
    int top_k) {
    
    // 1. 尝试增强检索
    try {
        if (config_.enable_rerank && reranker_->is_ready()) {
            // 完整流程
        }
    } catch (const std::exception& e) {
        RAG_WARN("Rerank failed, falling back to basic: " + std::string(e.what()));
    }
    
    // 2. 回退到基础检索
    return base_retriever_->retrieve(query, top_k);
}
```

## 6. 性能优化

### 6.1 向量缓存

```cpp
class EmbeddingServiceWithCache : public EmbeddingService {
    // LRU 缓存实现
    lru_cache<std::string, std::vector<float>> cache_{10000};
};
```

### 6.2 批量处理

```cpp
// 批量编码减少开销
std::vector<std::vector<float>> BGEM3EmbeddingService::encode_batch(
    const std::vector<std::string>& texts) {
    
    // 检查缓存
    std::vector<std::vector<float>> results;
    std::vector<std::string> uncached;
    std::vector<size_t> uncached_indices;
    
    for (size_t i = 0; i < texts.size(); i++) {
        if (auto cached = cache_.get(texts[i])) {
            results.push_back(*cached);
        } else {
            results.push_back({});
            uncached.push_back(texts[i]);
            uncached_indices.push_back(i);
        }
    }
    
    // 批量推理
    if (!uncached.empty()) {
        auto embeddings = model_->encode(uncached);
        
        for (size_t i = 0; i < uncached.size(); i++) {
            results[uncached_indices[i]] = embeddings[i];
            cache_.put(uncached[i], embeddings[i]);
        }
    }
    
    return results;
}
```

### 6.3 异步重排序

```cpp
// 使用线程池异步处理
std::future<std::vector<RerankResult>> reranker_->rerank_async(
    const std::string& query,
    const std::vector<Chunk>& candidates,
    int top_n);
```
