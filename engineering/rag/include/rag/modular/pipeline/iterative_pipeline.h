/**
 * @file iterative_pipeline.h
 * @brief IterativePipeline - 迭代优化 RAG Pipeline
 *
 * 流程: Query → 检索 → LLM评估 → 不满意→改写Query → 再检索 → ... → LLM
 *
 * IterativePipeline 通过迭代优化提升检索和生成质量:
 * - 首先执行基础检索获取候选上下文
 * - 使用 LLM 评估检索结果是否足够回答查询
 * - 如果不够充分，根据评估改写查询
 * - 重复检索和评估直到获得满意结果
 * - 使用最终上下文生成回答
 *
 * 与 CorrectivePipeline 的区别:
 * - CorrectivePipeline 评估检索结果质量（相关性、支持性等）
 * - IterativePipeline 直接评估检索结果是否足以回答问题
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/types.h"
#include <memory>
#include <vector>
#include <string>

namespace rag::modular {

/**
 * @brief 迭代评估结果
 */
struct IterativeEvaluation {
    bool is_sufficient = false;        // 上下文是否足够回答问题
    float sufficiency_score = 0.0f;    // 充分性分数 (0-1)
    std::string reason;                // 判断理由
    std::string suggested_improvement; // 改进建议
};

/**
 * @brief IterativePipeline - 迭代优化 Pipeline
 */
class IterativePipeline : public ModularPipeline {
public:
    IterativePipeline();
    ~IterativePipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::ITERATIVE; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "IterativePipeline"; }

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
     */
    void set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever);

    /**
     * @brief 设置 BM25 检索器
     */
    void set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever);

    /**
     * @brief 获取检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }
    std::shared_ptr<rag::BM25Retriever> bm25_retriever() const { return bm25_retriever_; }

    /**
     * @brief 设置最大迭代次数
     */
    void set_max_iterations(int max_iterations) { max_iterations_ = max_iterations; }

    /**
     * @brief 获取最大迭代次数
     */
    int max_iterations() const { return max_iterations_; }

    /**
     * @brief 设置充分性阈值
     */
    void set_sufficiency_threshold(float threshold) { sufficiency_threshold_ = threshold; }

    /**
     * @brief 获取充分性阈值
     */
    float sufficiency_threshold() const { return sufficiency_threshold_; }

private:
    /**
     * @brief 执行检索
     */
    std::vector<rag::RetrievalResult> retrieve(const std::string& query, int top_k);

    /**
     * @brief 评估检索结果是否充分
     */
    IterativeEvaluation evaluate_sufficiency(
        const std::string& query,
        const std::vector<rag::RetrievalResult>& results);

    /**
     * @brief 改写查询
     */
    std::string rewrite_query(
        const std::string& original_query,
        const std::string& current_query,
        const IterativeEvaluation& evaluation);

    /**
     * @brief 构建评估提示词
     */
    std::string build_evaluation_prompt(
        const std::string& query,
        const std::string& context);

    /**
     * @brief 构建查询改写提示词
     */
    std::string build_rewrite_prompt(
        const std::string& original_query,
        const std::string& current_query,
        const IterativeEvaluation& evaluation);

    /**
     * @brief 解析评估结果
     */
    IterativeEvaluation parse_evaluation_response(const std::string& response);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;     // HNSW 向量检索器
    std::shared_ptr<rag::BM25Retriever> bm25_retriever_;     // BM25 全文检索器
    bool initialized_ = false;                                // 初始化标志
    int max_iterations_ = 3;                                 // 最大迭代次数
    float sufficiency_threshold_ = 0.6f;                     // 充分性阈值

    // 统计信息
    struct Stats {
        int total_iterations = 0;
        int rewrite_count = 0;
        float avg_sufficiency_score = 0.0f;
    };
    Stats stats_;
};

}  // namespace rag::modular
