/**
 * @file hybrid_pipeline.h
 * @brief HybridPipeline - 三路召回 RAG Pipeline
 *
 * 流程: Query → Vector + BM25 + Graph → RRF融合 → LLM
 *
 * HybridPipeline 在 AdvancedPipeline 基础上引入 Graph 检索:
 * - 三路召回: HNSW 向量检索 + BM25 关键词检索 + Graph 知识图谱检索
 * - RRF 融合: 对三路检索结果进行排名融合
 * - 保留 Graph 的结构化信息优势
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/graph_retriever.h"
#include "rag/adaptive_rrf.h"
#include "rag/types.h"
#include <memory>
#include <vector>

namespace rag::modular {

/**
 * @brief HybridPipeline - 三路召回 RAG Pipeline
 *
 * 特点:
 * - 三路同时召回: Vector + BM25 + Graph
 * - 充分利用结构化知识 (Knowledge Graph)
 * - RRF 融合确保各路结果公平竞争
 * - 结合语义相似度、关键词匹配和结构化推理
 *
 * 适用场景:
 * - 需要利用知识图谱的复杂问答
 * - 需要结构化和非结构化融合检索
 * - 实体关系相关的查询
 */
class HybridPipeline : public ModularPipeline {
public:
    HybridPipeline();
    ~HybridPipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::HYBRID; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "HybridPipeline"; }

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
     * @brief 设置 HNSW 检索器
     * @param retriever HNSW 检索器
     */
    void set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever);

    /**
     * @brief 设置 BM25 检索器
     * @param retriever BM25 检索器
     */
    void set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever);

    /**
     * @brief 设置 Graph 检索器
     * @param retriever Graph 检索器
     */
    void set_graph_retriever(std::shared_ptr<rag::GraphRetriever> retriever);

    /**
     * @brief 获取检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }
    std::shared_ptr<rag::BM25Retriever> bm25_retriever() const { return bm25_retriever_; }
    std::shared_ptr<rag::GraphRetriever> graph_retriever() const { return graph_retriever_; }

private:
    /**
     * @brief 执行向量检索
     */
    std::vector<rag::RetrievalResult> retrieve_vectors(
        const std::string& query, int top_k);

    /**
     * @brief 执行 BM25 检索
     */
    std::vector<rag::RetrievalResult> retrieve_bm25(
        const std::string& query, int top_k);

    /**
     * @brief 执行 Graph 检索
     */
    std::vector<rag::RetrievalResult> retrieve_graph(
        const std::string& query, int top_k);

    /**
     * @brief 三路 RRF 融合
     */
    std::vector<rag::RetrievalResult> fuse_with_rrf(
        const std::vector<rag::RetrievalResult>& hnsw_results,
        const std::vector<rag::RetrievalResult>& bm25_results,
        const std::vector<rag::RetrievalResult>& graph_results,
        int top_k);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;    // HNSW 向量检索器
    std::shared_ptr<rag::BM25Retriever> bm25_retriever_;   // BM25 全文检索器
    std::shared_ptr<rag::GraphRetriever> graph_retriever_; // Graph 知识图谱检索器
    std::unique_ptr<rag::AdaptiveRRF> adaptive_rrf_;        // 自适应 RRF 融合器
    bool initialized_ = false;                              // 初始化标志
};

}  // namespace rag::modular
