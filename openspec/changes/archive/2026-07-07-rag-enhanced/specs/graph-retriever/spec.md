# Graph 检索器规格

> 状态: 草稿 | 创建日期: 2026-07-07

## 1. 概述

### 1.1 目的

定义 Graph 检索器的接口和行为规范，用于从知识图谱中检索与查询相关的实体和子图。

### 1.2 使用场景

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Graph RAG 检索流程                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                      查询输入                                │   │
│   │                "RAG 和 LLM 是什么关系？"                     │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    1. 实体链接                               │   │
│   │         识别查询中的实体: [RAG, LLM]                         │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    2. 子图扩展                               │   │
│   │         RAG ──▶ RELATED_TO ──▶ LLM                          │   │
│   │           │                                                   │   │
│   │           └──▶ USES ──▶ 向量数据库                           │   │
│   │           └──▶ PART_OF ──▶ NLP                               │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    3. 子图检索                               │   │
│   │         基于向量相似度和图结构排序                           │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    4. 结果返回                               │   │
│   │         实体列表 + 关系列表 + 摘要                           │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 2. 接口规范

### 2.1 GraphRetrievalResult

```cpp
namespace rag {

/**
 * @brief Graph 检索结果
 */
struct GraphRetrievalResult {
    std::string chunk_id;        // 来源 Chunk ID
    std::string content;         // 实体描述或 Chunk 内容
    std::string entity_id;       // 实体 ID (如果有)
    std::string entity_name;     // 实体名称
    std::string entity_type;     // 实体类型
    
    float score = 0.0f;          // 排序分数
    int hop_distance = 0;        // 距离查询实体的跳数
    
    // 附加信息
    std::vector<std::string> related_entity_ids;
    std::vector<KGRelation> relations;
};

/**
 * @brief 子图结构
 */
struct Subgraph {
    std::vector<KGEntity> entities;      // 实体列表
    std::vector<KGRelation> relations;    // 关系列表
    std::string summary;                  // 子图摘要
    
    // 统计
    size_t total_entities() const { return entities.size(); }
    size_t total_relations() const { return relations.size(); }
    
    // JSON 序列化
    std::string to_json() const;
};

}  // namespace rag
```

### 2.2 GraphRetriever 类

```cpp
namespace rag {

/**
 * @brief Graph 检索器
 * 
 * 从知识图谱中检索相关实体和子图
 */
class GraphRetriever {
public:
    /**
     * @brief 构造函数
     * @param kg 知识图谱
     * @param extractor 实体提取器
     * @param embed_service Embedding 服务
     */
    GraphRetriever(
        std::shared_ptr<KnowledgeGraph> kg,
        std::shared_ptr<EntityExtractor> extractor,
        std::shared_ptr<EmbeddingService> embed_service);
    
    ~GraphRetriever() = default;
    
    // ========== 核心检索 ==========
    
    /**
     * @brief 图检索
     * @param query 查询文本
     * @param top_k 返回结果数
     * @param max_hops 最大跳数
     * @return 检索结果
     */
    std::vector<GraphRetrievalResult> retrieve(
        const std::string& query,
        int top_k = 10,
        int max_hops = 2);
    
    /**
     * @brief 获取查询相关的子图
     * @param query 查询文本
     * @param max_hops 最大跳数
     * @return 子图结构
     */
    Subgraph get_subgraph(const std::string& query, int max_hops = 2);
    
    // ========== 子图操作 ==========
    
    /**
     * @brief 扩展实体列表
     * @param entity_ids 种子实体 ID 列表
     * @param depth 扩展深度
     * @return 扩展后的实体列表
     */
    std::vector<KGEntity> expand_entities(
        const std::vector<std::string>& entity_ids,
        int depth = 1);
    
    /**
     * @brief 获取实体周围的子图
     * @param entity_id 中心实体 ID
     * @param depth 深度
     * @return 子图
     */
    Subgraph get_local_subgraph(
        const std::string& entity_id,
        int depth = 2);
    
    // ========== 查询分析 ==========
    
    /**
     * @brief 分析查询中的实体
     * @param query 查询文本
     * @return 识别出的实体名称列表
     */
    std::vector<std::string> analyze_query_entities(const std::string& query);
    
    /**
     * @brief 获取检索统计
     */
    struct Stats {
        uint64_t total_queries;
        uint64_t entity_link_count;
        uint64_t subgraph_hits;
        double avg_retrieval_time_ms;
    };
    Stats get_stats() const;
    
    std::string name() const { return "graph"; }

private:
    // 实体链接
    std::vector<std::string> link_query_entities(const std::string& query);
    
    // 子图查询
    std::vector<KGEntity> query_subgraph(
        const std::vector<std::string>& seed_entities,
        int max_hops);
    
    // 结果排序
    std::vector<GraphRetrievalResult> rank_results(
        const std::string& query,
        const std::vector<KGEntity>& entities,
        int max_hops);
    
    std::shared_ptr<KnowledgeGraph> kg_;
    std::shared_ptr<EntityExtractor> extractor_;
    std::shared_ptr<EmbeddingService> embed_service_;
    
    mutable std::atomic<uint64_t> total_queries_{0};
    mutable std::atomic<uint64_t> entity_link_count_{0};
};

}  // namespace rag
```

### 2.3 HybridGraphRetriever 类

```cpp
namespace rag {

/**
 * @brief 混合 Graph + Vector 检索器
 * 
 * 结合 Graph 检索和传统向量检索的结果
 */
class HybridGraphRetriever : public Retriever {
public:
    /**
     * @brief 构造函数
     * @param graph_retriever Graph 检索器
     * @param vector_retriever 向量检索器
     * @param graph_weight Graph 结果权重 [0, 1]
     */
    HybridGraphRetriever(
        std::shared_ptr<GraphRetriever> graph_retriever,
        std::shared_ptr<Retriever> vector_retriever,
        float graph_weight = 0.5f);
    
    // ========== Retriever 接口 ==========
    
    std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) override;
    
    std::string name() const override { return "hybrid_graph"; }
    const RetrievalConfig& config() const override { return config_; }
    
    // ========== 配置方法 ==========
    
    void set_graph_weight(float weight) { graph_weight_ = weight; }
    void set_config(const RetrievalConfig& config) { config_ = config; }
    
private:
    // 结果融合
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

## 3. 检索流程

### 3.1 实体链接

```cpp
std::vector<std::string> GraphRetriever::link_query_entities(
    const std::string& query) {
    
    std::vector<std::string> result;
    
    // 方法1: 从 KG 搜索相似实体名
    auto candidates = kg_->search_entities(query, 10);
    for (const auto& entity : candidates) {
        // 检查实体名是否在查询中
        if (query.find(entity.name) != std::string::npos) {
            result.push_back(entity.id);
        }
    }
    
    // 方法2: LLM 提取后匹配
    if (result.empty()) {
        auto extraction = extractor_->extract(query);
        for (const auto& entity : extraction.entities) {
            auto found = kg_->get_entity(entity.name);
            if (found) {
                result.push_back(found->id);
            }
        }
    }
    
    return result;
}
```

### 3.2 子图扩展

```cpp
std::vector<KGEntity> GraphRetriever::query_subgraph(
    const std::vector<std::string>& seed_entities,
    int max_hops) {
    
    std::vector<KGEntity> result;
    std::unordered_set<std::string> visited;
    
    std::queue<std::pair<std::string, int>> queue;
    for (const auto& id : seed_entities) {
        queue.emplace(id, 0);
        visited.insert(id);
    }
    
    while (!queue.empty()) {
        auto [entity_id, hop] = queue.front();
        queue.pop();
        
        // 获取实体
        auto entity = kg_->get_entity(entity_id);
        if (entity) {
            result.push_back(*entity);
        }
        
        // 限制跳数
        if (hop >= max_hops) continue;
        
        // 扩展邻居
        auto neighbors = kg_->get_neighbors(entity_id, 1);
        for (const auto& neighbor_id : neighbors) {
            if (visited.insert(neighbor_id).second) {
                queue.emplace(neighbor_id, hop + 1);
            }
        }
    }
    
    return result;
}
```

### 3.3 结果排序

```cpp
std::vector<GraphRetrievalResult> GraphRetriever::rank_results(
    const std::string& query,
    const std::vector<KGEntity>& entities,
    int max_hops) {
    
    // 获取查询向量
    auto query_vec = embed_service_->encode(query);
    
    std::vector<std::pair<KGEntity, float>> scored;
    
    for (const auto& entity : entities) {
        float score = 0.0f;
        
        // 1. 向量相似度
        if (!entity.embedding.empty()) {
            float sim = cosine_similarity(query_vec, entity.embedding);
            score += sim * 0.6f;
        }
        
        // 2. 描述相关性
        if (!entity.description.empty()) {
            auto desc_vec = embed_service_->encode(entity.description);
            float sim = cosine_similarity(query_vec, desc_vec);
            score += sim * 0.3f;
        }
        
        // 3. 度数奖励
        score += std::log1p(entity.degree) * 0.1f;
        
        scored.emplace_back(entity, score);
    }
    
    // 排序
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    // 转换为结果
    std::vector<GraphRetrievalResult> results;
    for (const auto& [entity, score] : scored) {
        GraphRetrievalResult r;
        r.entity_id = entity.id;
        r.entity_name = entity.name;
        r.entity_type = entity.type;
        r.content = entity.description;
        r.score = score;
        results.push_back(r);
    }
    
    return results;
}
```

## 4. RRF 融合算法

### 4.1 公式

```
RRF_score(d) = Σ 1 / (k + rank_i(d))

其中:
- d 是文档/实体
- k 是常数 (通常 60)
- rank_i(d) 是第 i 个检索器对 d 的排名
```

### 4.2 实现

```cpp
std::vector<RetrievalResult> HybridGraphRetriever::fuse_results(
    const std::vector<RetrievalResult>& graph_results,
    const std::vector<RetrievalResult>& vector_results) {
    
    std::unordered_map<std::string, float> scores;
    const float k = 60.0f;
    
    // Graph 结果排名
    for (int i = 0; i < static_cast<int>(graph_results.size()); i++) {
        float rrf = 1.0f / (k + i + 1);
        scores[graph_results[i].chunk.id] += graph_weight_ * rrf;
    }
    
    // Vector 结果排名
    float vector_weight = 1.0f - graph_weight_;
    for (int i = 0; i < static_cast<int>(vector_results.size()); i++) {
        float rrf = 1.0f / (k + i + 1);
        scores[vector_results[i].chunk.id] += vector_weight * rrf;
    }
    
    // 排序
    std::vector<std::pair<std::string, float>> sorted(
        scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // 构建结果
    std::vector<RetrievalResult> results;
    std::unordered_map<std::string, RetrievalResult> result_map;
    
    for (const auto& r : graph_results) result_map[r.chunk.id] = r;
    for (const auto& r : vector_results) result_map[r.chunk.id] = r;
    
    for (const auto& [id, score] : sorted) {
        auto it = result_map.find(id);
        if (it != result_map.end()) {
            it->second.score = score;
            results.push_back(it->second);
        }
    }
    
    return results;
}
```

## 5. 行为规范

### 5.1 检索行为

| 场景 | 行为 |
|------|------|
| 无匹配实体 | 返回空结果 |
| 实体无邻居 | 仅返回该实体 |
| max_hops = 0 | 返回空结果 |
| top_k > 结果数 | 返回全部结果 |

### 5.2 子图获取

| 场景 | 行为 |
|------|------|
| 查询实体存在 | 返回以该实体为中心的子图 |
| 查询实体不存在 | 返回空子图 |
| 子图过大 | 限制实体数量 (默认 100) |

### 5.3 异常处理

| 场景 | 行为 |
|------|------|
| KG 不可用 | 记录错误，返回空结果 |
| Embedding 失败 | 仅使用 KG 拓扑排序 |
| LLM 提取失败 | 回退到 KG 搜索 |

## 6. 配置规范

### 6.1 配置文件

```yaml
graph_retriever:
  # 最大跳数
  max_hops: 2
  
  # 返回结果数
  top_k: 10
  
  # 混合检索权重
  graph_weight: 0.5
  
  # 实体链接
  entity_linking:
    enabled: true
    min_confidence: 0.5
    max_candidates: 10
  
  # 子图限制
  subgraph:
    max_entities: 100
    max_depth: 3
  
  # 排序权重
  ranking:
    vector_score_weight: 0.6
    description_score_weight: 0.3
    degree_bonus: 0.1
```

### 6.2 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| max_hops | 2 | 图遍历最大深度 |
| top_k | 10 | 返回结果数 |
| graph_weight | 0.5 | Graph 结果权重 |
| max_entities | 100 | 子图最大实体数 |
| min_confidence | 0.5 | 实体链接最小置信度 |

## 7. 性能规范

### 7.1 延迟要求

| 操作 | 延迟 |
|------|------|
| 实体链接 | < 50ms |
| 1 跳邻居查询 | < 10ms |
| 2 跳邻居查询 | < 50ms |
| 完整检索流程 | < 200ms |

### 7.2 内存要求

| 项目 | 内存 |
|------|------|
| 缓存实体向量 | ~1MB/1000 实体 |
| 子图缓存 | < 100MB |

## 8. 测试规范

### 8.1 单元测试

```cpp
TEST(GraphRetriever, RetrieveWithEntityLink) {
    // 准备测试数据
    auto kg = std::make_shared<MemoryKnowledgeGraph>();
    kg->add_entity({"id": "1", "name": "RAG", "type": "CONCEPT"});
    kg->add_entity({"id": "2", "name": "LLM", "type": "TECHNOLOGY"});
    kg->add_relation({"id": "r1", "source_id": "1", "target_id": "2", "type": "RELATED_TO"});
    
    auto extractor = std::make_shared<RuleBasedExtractor>();
    auto embed = std::make_shared<SimpleEmbeddingService>(128);
    embed->load();
    
    GraphRetriever retriever(kg, extractor, embed);
    
    auto results = retriever.retrieve("RAG", 10, 1);
    
    EXPECT_GT(results.size(), 0);
    EXPECT_TRUE(results[0].entity_name == "RAG" || 
                results[0].entity_name == "LLM");
}

TEST(GraphRetriever, GetSubgraph) {
    auto kg = std::make_shared<MemoryKnowledgeGraph>();
    kg->add_entity({"id": "A", "name": "A", "type": "CONCEPT"});
    kg->add_entity({"id": "B", "name": "B", "type": "CONCEPT"});
    kg->add_entity({"id": "C", "name": "C", "type": "CONCEPT"});
    kg->add_relation({"id": "r1", "source_id": "A", "target_id": "B", "type": "RELATED"});
    kg->add_relation({"id": "r2", "source_id": "B", "target_id": "C", "type": "RELATED"});
    
    auto extractor = std::make_shared<RuleBasedExtractor>();
    auto embed = std::make_shared<SimpleEmbeddingService>(128);
    embed->load();
    
    GraphRetriever retriever(kg, extractor, embed);
    
    auto subgraph = retriever.get_subgraph("A", 2);
    
    EXPECT_EQ(subgraph.entities.size(), 3);
    EXPECT_EQ(subgraph.relations.size(), 2);
}

TEST(HybridGraphRetriever, FuseResults) {
    auto graph_retriever = /* ... */;
    auto vector_retriever = /* ... */;
    
    HybridGraphRetriever hybrid(graph_retriever, vector_retriever, 0.5f);
    
    auto results = hybrid.retrieve("test query", 5);
    
    // 验证结果非空
    EXPECT_GT(results.size(), 0);
    
    // 验证分数在有效范围
    for (const auto& r : results) {
        EXPECT_GE(r.score, 0.0f);
        EXPECT_LE(r.score, 1.0f);
    }
}
```

### 8.2 集成测试

```cpp
TEST(GraphRetriever, FullPipeline) {
    // 1. 构建知识图谱
    auto kg = build_test_knowledge_graph();
    
    // 2. 添加文档和 Chunks
    auto chunks = extract_chunks_from_documents();
    
    // 3. 构建图谱
    for (const auto& chunk : chunks) {
        auto extraction = extractor_->extract_from_chunk(chunk);
        for (const auto& e : extraction.entities) kg->add_entity(e);
        for (const auto& r : extraction.relations) kg->add_relation(r);
    }
    
    // 4. 执行检索
    GraphRetriever retriever(kg, extractor, embed);
    
    auto results = retriever.retrieve("RAG system architecture", 10, 2);
    
    // 5. 验证结果
    ASSERT_GT(results.size(), 0);
    
    // RAG 相关实体应该在结果中
    bool has_rag = false;
    for (const auto& r : results) {
        if (r.entity_name.find("RAG") != std::string::npos) {
            has_rag = true;
            break;
        }
    }
    EXPECT_TRUE(has_rag);
}
```

## 9. 与其他组件集成

### 9.1 在 RAG 引擎中使用

```cpp
class RAGEngine {
    void init_graph_rag() {
        // 初始化图检索器
        graph_retriever_ = std::make_shared<GraphRetriever>(
            kg_storage_,
            entity_extractor_,
            embed_service_);
        
        // 创建混合检索器
        hybrid_graph_retriever_ = std::make_shared<HybridGraphRetriever>(
            graph_retriever_,
            enhanced_retriever_,  // 或 basic retriever
            config_.retrieval.graph.graph_weight);
        
        retriever_ = hybrid_graph_retriever_;
    }
    
    void build_knowledge_graph() {
        // 从已有 chunks 构建图谱
        auto chunks = chunk_repo_->find_all();
        for (const auto& chunk : chunks) {
            auto extraction = entity_extractor_->extract_from_chunk(chunk);
            for (const auto& e : extraction.entities) {
                kg_storage_->add_entity(e);
            }
            for (const auto& r : extraction.relations) {
                kg_storage_->add_relation(r);
            }
        }
    }
};
```

### 9.2 生成答案

```cpp
QueryResult RAGEngine::query(const std::string& query, int top_k) {
    QueryResult result;
    
    // 获取子图
    auto subgraph = graph_retriever_->get_subgraph(query, 2);
    
    // 构建上下文
    std::string context = "相关实体:\n";
    for (const auto& e : subgraph.entities) {
        context += "- " + e.name + ": " + e.description + "\n";
    }
    context += "\n关系:\n";
    for (const auto& r : subgraph.relations) {
        context += "- " + r.source_id + " " + r.type + " " + r.target_id + "\n";
    }
    
    // 调用 LLM 生成答案
    result.answer = llm_service_->generate(prompt_with_context(query, context));
    
    return result;
}
```
