/**
 * @file graph_retriever.h
 * @brief Graph 检索器 - 基于知识图谱的检索
 */
#pragma once

#include "rag/knowledge_graph.h"
#include "rag/retriever.h"
#include "rag/types.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace rag {

// ========== Graph 检索配置 ==========

/**
 * @brief Graph 检索配置
 */
struct GraphRetrievalConfig {
    int max_hops = 2;                      // 最大跳数
    int max_entities = 50;                 // 最大实体数
    int max_relations = 100;               // 最大关系数
    float entity_similarity_threshold = 0.6f;  // 实体相似度阈值
    bool include_subgraph = true;          // 是否返回子图
    bool include_text_chunks = true;       // 是否包含关联文本块
    std::string entity_link_method = "embedding";  // 实体链接方法: embedding, keyword, hybrid
    std::string subgraph_expansion = "bfs";        // 子图扩展方法: bfs, dfs, random_walk
};

// ========== RetrievedChunk 结构 ==========

/**
 * @brief 已检索的文本块
 */
struct RetrievedChunk {
    Chunk chunk;                    // 文本块
    float score = 0.0f;             // 分数
    std::string source;             // 来源 (graph/vector/hybrid)
};

// ========== Graph 检索结果 ==========

/**
 * @brief Graph 检索结果
 */
struct GraphRetrievalResult {
    std::string query;                     // 原始查询
    KGSubgraph subgraph;                   // 检索到的子图
    std::vector<RetrievedChunk> chunks;    // 关联的文本块
    std::vector<KGEntity> matched_entities; // 匹配到的实体
    std::string method;                    // 检索方法
    int64_t processing_time_ms = 0;        // 处理时间
    float score = 0.0f;                    // 综合分数
};

// ========== 实体链接结果 ==========

/**
 * @brief 实体链接结果
 */
struct EntityLinkResult {
    std::string query_term;                // 查询中的术语
    std::string linked_entity_id;          // 链接到的实体 ID
    std::string linked_entity_name;        // 链接到的实体名称
    float similarity = 0.0f;               // 相似度
    bool is_exact_match = false;           // 是否精确匹配
};

// ========== Graph 检索器接口 ==========

/**
 * @brief Graph 检索器抽象接口
 */
class GraphRetriever {
public:
    virtual ~GraphRetriever() = default;

    /// 执行 Graph 检索
    virtual GraphRetrievalResult retrieve(
        const std::string& query,
        const RetrievalConfig& config) = 0;

    /// 实体链接 - 将查询中的术语链接到知识图谱中的实体
    virtual std::vector<EntityLinkResult> link_query_entities(
        const std::string& query) = 0;

    /// 子图扩展 - 获取实体的 N 跳邻居子图
    virtual KGSubgraph expand_subgraph(
        const std::string& entity_id,
        int max_hops) = 0;

    /// 获取检索器名称
    virtual std::string name() const = 0;

    /// 设置知识图谱
    virtual void set_knowledge_graph(KnowledgeGraph* kg) = 0;

    /// 设置文本检索器 (用于混合检索)
    virtual void set_text_retriever(Retriever* retriever) = 0;

    /// 获取配置
    virtual const GraphRetrievalConfig& config() const = 0;
};

// ========== 基础 Graph 检索器 ==========

/**
 * @brief 基础 Graph 检索器
 *
 * 基于实体链接和子图扩展的检索
 */
class BaseGraphRetriever : public GraphRetriever {
public:
    explicit BaseGraphRetriever(const GraphRetrievalConfig& config);

    GraphRetrievalResult retrieve(
        const std::string& query,
        const RetrievalConfig& config) override;

    std::vector<EntityLinkResult> link_query_entities(
        const std::string& query) override;

    KGSubgraph expand_subgraph(
        const std::string& entity_id,
        int max_hops) override;

    std::string name() const override { return "base-graph"; }

    void set_knowledge_graph(KnowledgeGraph* kg) override { kg_ = kg; }
    void set_text_retriever(Retriever* retriever) override { text_retriever_ = retriever; }

    const GraphRetrievalConfig& config() const override { return config_; }

protected:
    /// 提取查询中的候选实体
    virtual std::vector<std::string> extract_candidate_entities(
        const std::string& query);

    /// 搜索相似实体
    virtual std::vector<KGEntity> search_similar_entities(
        const std::string& term);

    /// 构建检索结果
    virtual GraphRetrievalResult build_result(
        const std::string& query,
        const std::vector<EntityLinkResult>& linked_entities,
        const KGSubgraph& subgraph);

    GraphRetrievalConfig config_;
    KnowledgeGraph* kg_ = nullptr;
    Retriever* text_retriever_ = nullptr;
};

// ========== Hybrid Graph 检索器 ==========

/**
 * @brief 混合 Graph 检索器
 *
 * 结合 Graph 检索和 Vector 检索
 */
class HybridGraphRetriever : public BaseGraphRetriever {
public:
    explicit HybridGraphRetriever(const GraphRetrievalConfig& config);

    GraphRetrievalResult retrieve(
        const std::string& query,
        const RetrievalConfig& config) override;

    std::string name() const override { return "hybrid-graph"; }

    /// 设置 RRF 融合参数
    void set_rrf_k(int k) { rrf_k_ = k; }

private:
    /// RRF (Reciprocal Rank Fusion) 融合
    std::vector<RetrievedChunk> rrf_fusion(
        const std::vector<RetrievalResult>& graph_results,
        const std::vector<RetrievalResult>& vector_results);

    int rrf_k_ = 60;  // RRF 常数参数
};

// ========== 工厂函数 ==========

std::unique_ptr<GraphRetriever> create_graph_retriever(
    const std::string& type = "base",
    const GraphRetrievalConfig& config = GraphRetrievalConfig());

}  // namespace rag
