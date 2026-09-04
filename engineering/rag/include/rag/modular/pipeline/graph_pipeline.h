/**
 * @file graph_pipeline.h
 * @brief GraphPipeline - 知识图谱增强 RAG Pipeline
 *
 * 流程: Query → 实体提取 → 图检索 → 子图 → Context → LLM
 *
 * GraphPipeline 利用知识图谱的结构化信息增强检索:
 * - 从查询中提取实体和关系
 * - 在知识图谱中查找相关实体
 * - 获取实体的多跳邻居子图
 * - 结合子图结构和文本块生成回答
 *
 * 优势:
 * - 利用结构化知识进行精确检索
 * - 能够捕获实体间的复杂关系
 * - 提供更丰富的上下文信息
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/entity_extractor.h"
#include "rag/graph_retriever.h"
#include "rag/types.h"
#include <memory>
#include <vector>

namespace rag::modular {

/**
 * @brief GraphPipeline - 知识图谱增强 Pipeline
 *
 * 利用知识图谱的结构化信息进行增强检索和生成
 */
class GraphPipeline : public ModularPipeline {
public:
    GraphPipeline();
    ~GraphPipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::GRAPH; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "GraphPipeline"; }

    /**
     * @brief 初始化 Pipeline
     * @param config 配置信息
     * @return 初始化是否成功
     */
    bool init(const ModularConfig& config) override;

    /**
     * @brief 执行查询
     * @param query 查询信息
     * @return 查询结果
     */
    ModularQueryResult query(const ModularQuery& query) override;

    /**
     * @brief 检查 Pipeline 是否就绪
     */
    bool is_ready() const override;

    /**
     * @brief 设置实体提取器
     * @param extractor 实体提取器
     */
    void set_entity_extractor(std::shared_ptr<rag::EntityExtractor> extractor);

    /**
     * @brief 设置 Graph 检索器
     * @param retriever Graph 检索器
     */
    void set_graph_retriever(std::shared_ptr<rag::GraphRetriever> retriever);

    /**
     * @brief 设置 HNSW 检索器（用于混合模式）
     * @param retriever HNSW 检索器
     */
    void set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever);

    /**
     * @brief 获取 Graph 检索器
     */
    std::shared_ptr<rag::GraphRetriever> graph_retriever() const { return graph_retriever_; }

    /**
     * @brief 设置是否使用混合模式（Graph + Vector）
     * @param enable 是否启用
     */
    void set_hybrid_mode(bool enable) { hybrid_mode_ = enable; }

    /**
     * @brief 获取混合模式设置
     */
    bool hybrid_mode() const { return hybrid_mode_; }

private:
    /**
     * @brief 从查询中提取实体
     * @param query 查询文本
     * @return 提取的实体列表
     */
    std::vector<rag::KGEntity> extract_entities(const std::string& query);

    /**
     * @brief 执行图检索
     * @param entities 实体列表
     * @param config 检索配置
     * @return 检索结果
     */
    rag::GraphRetrievalResult retrieve_graph(
        const std::vector<rag::KGEntity>& entities,
        const rag::RetrievalConfig& config);

    /**
     * @brief 执行向量检索
     * @param query 查询文本
     * @param top_k 结果数
     * @return 检索结果
     */
    std::vector<rag::RetrievalResult> retrieve_vectors(
        const std::string& query, int top_k);

    /**
     * @brief 构建子图上下文字符串
     * @param subgraph 知识子图
     * @param chunks 关联的文本块
     * @return 格式化的上下文字符串
     */
    std::string build_graph_context(
        const rag::KGSubgraph& subgraph,
        const std::vector<rag::RetrievedChunk>& chunks);

    /**
     * @brief 判断查询是否适合图检索
     * @param query 查询文本
     * @return 是否适合
     */
    bool is_graph_suitable_query(const std::string& query);

    std::shared_ptr<rag::EntityExtractor> entity_extractor_;  // 实体提取器
    std::shared_ptr<rag::GraphRetriever> graph_retriever_;    // Graph 检索器
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;      // HNSW 向量检索器（混合模式）
    rag::GraphRetrievalConfig graph_config_;                  // Graph 检索配置
    bool initialized_ = false;                                 // 初始化标志
    bool hybrid_mode_ = false;                                // 混合模式（Graph + Vector）
};

}  // namespace rag::modular
