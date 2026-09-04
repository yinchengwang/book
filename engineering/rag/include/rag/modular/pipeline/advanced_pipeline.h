/**
 * @file advanced_pipeline.h
 * @brief AdvancedPipeline - 高级 RAG Pipeline
 *
 * 流程: Query → QueryExp → Vector+BM25 → RRF → Rerank → Context → LLM
 *
 * AdvancedPipeline 在 NaivePipeline 的基础上增加了:
 * - 查询扩展 (QueryExpander): 扩展查询以获得更好的召回
 * - 混合检索 (HNSW + BM25): 结合向量和关键词检索的优势
 * - RRF 融合: 对多路检索结果进行排名融合
 * - 重排序 (Reranker): 对候选结果进行精细排序
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/query_expander.h"
#include "rag/reranker.h"
#include "rag/adaptive_rrf.h"
#include "rag/types.h"
#include <memory>
#include <vector>

namespace rag::modular {

/**
 * @brief AdvancedPipeline - 高级 RAG Pipeline
 *
 * 特点:
 * - 查询扩展提升召回率
 * - 混合检索结合向量和关键词的优势
 * - RRF 融合多路检索结果
 * - 重排序提升准确性
 *
 * 适用场景:
 * - 需要较高准确性的问答系统
 * - 复杂查询需要扩展理解
 * - 混合检索需求
 */
class AdvancedPipeline : public ModularPipeline {
public:
    AdvancedPipeline();
    ~AdvancedPipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::ADVANCED; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "AdvancedPipeline"; }

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
     * @brief 设置查询扩展器
     * @param expander 查询扩展器
     */
    void set_query_expander(std::shared_ptr<rag::QueryExpander> expander);

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
     * @brief 设置重排序器
     * @param reranker 重排序器
     */
    void set_reranker(std::shared_ptr<rag::Reranker> reranker);

    /**
     * @brief 获取检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }
    std::shared_ptr<rag::BM25Retriever> bm25_retriever() const { return bm25_retriever_; }
    std::shared_ptr<rag::Reranker> reranker() const { return reranker_; }

private:
    /**
     * @brief 执行查询扩展
     * @param query 原始查询
     * @return 扩展后的查询列表
     */
    std::vector<std::string> expand_query(const std::string& query);

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
     * @brief 执行 RRF 融合
     */
    std::vector<rag::RetrievalResult> fuse_with_rrf(
        const std::vector<rag::RetrievalResult>& hnsw_results,
        const std::vector<rag::RetrievalResult>& bm25_results,
        int top_k);

    /**
     * @brief 执行重排序
     */
    std::vector<rag::RetrievalResult> rerank_results(
        const std::string& query,
        const std::vector<rag::RetrievalResult>& candidates,
        int top_n);

    std::shared_ptr<rag::QueryExpander> query_expander_;   // 查询扩展器
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;   // HNSW 向量检索器
    std::shared_ptr<rag::BM25Retriever> bm25_retriever_;  // BM25 全文检索器
    std::shared_ptr<rag::Reranker> reranker_;             // 重排序器
    std::unique_ptr<rag::AdaptiveRRF> adaptive_rrf_;       // 自适应 RRF 融合器
    bool initialized_ = false;                              // 初始化标志
};

}  // namespace rag::modular
