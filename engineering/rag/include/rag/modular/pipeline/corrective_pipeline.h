/**
 * @file corrective_pipeline.h
 * @brief CorrectivePipeline - 自我纠正 RAG Pipeline
 *
 * 流程: Query → 检索 → LLM判断质量 → 质量差→纠正/重检 → LLM
 *
 * CorrectivePipeline 通过评估和纠正来提升检索质量:
 * - 首先执行基础检索获取候选上下文
 * - 使用 LLM 评估检索结果的质量
 * - 如果质量不佳，进行查询重写或扩展检索
 * - 重复评估直到获得足够好的结果
 * - 使用最终上下文生成回答
 *
 * 优势:
 * - 自动检测和修复低质量检索结果
 * - 动态调整检索策略
 * - 提高最终回答的准确性和相关性
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/self_rag.h"
#include "rag/types.h"
#include <memory>
#include <vector>

namespace rag::modular {

/**
 * @brief CorrectivePipeline - 自我纠正 Pipeline
 *
 * 通过迭代评估和纠正来提升检索和生成质量
 */
class CorrectivePipeline : public ModularPipeline {
public:
    CorrectivePipeline();
    ~CorrectivePipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::CORRECTIVE; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "CorrectivePipeline"; }

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
     * @brief 获取检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }
    std::shared_ptr<rag::BM25Retriever> bm25_retriever() const { return bm25_retriever_; }

    /**
     * @brief 设置最大纠正迭代次数
     * @param max_iterations 最大迭代次数
     */
    void set_max_iterations(int max_iterations) { max_iterations_ = max_iterations; }

    /**
     * @brief 获取最大迭代次数
     */
    int max_iterations() const { return max_iterations_; }

    /**
     * @brief 设置质量阈值
     * @param threshold 质量阈值 (0-1)
     */
    void set_quality_threshold(float threshold) { quality_threshold_ = threshold; }

    /**
     * @brief 获取质量阈值
     */
    float quality_threshold() const { return quality_threshold_; }

private:
    /**
     * @brief 执行检索
     * @param query 查询文本
     * @param top_k 结果数
     * @return 检索结果
     */
    std::vector<rag::RetrievalResult> retrieve(
        const std::string& query, int top_k);

    /**
     * @brief 评估检索结果质量
     * @param query 查询文本
     * @param results 检索结果
     * @return 评估结果（包含质量分数和反思信息）
     */
    rag::ReflectionResult evaluate_retrieval_quality(
        const std::string& query,
        const std::vector<rag::RetrievalResult>& results);

    /**
     * @brief 判断是否需要纠正
     * @param evaluation 评估结果
     * @return 是否需要纠正
     */
    bool needs_correction(const rag::ReflectionResult& evaluation);

    /**
     * @brief 执行纠正
     * @param query 当前查询
     * @param evaluation 评估结果
     * @param iteration 当前迭代次数
     * @return 纠正后的新查询
     */
    std::string perform_correction(
        const std::string& query,
        const rag::ReflectionResult& evaluation,
        int iteration);

    /**
     * @brief 构建纠正提示词
     * @param query 原始查询
     * @param evaluation 评估结果
     * @return 提示词
     */
    std::string build_correction_prompt(
        const std::string& query,
        const rag::ReflectionResult& evaluation);

    /**
     * @brief 构建评估提示词
     * @param query 查询文本
     * @param context 上下文内容
     * @return 提示词
     */
    std::string build_evaluation_prompt(
        const std::string& query,
        const std::string& context);

    /**
     * @brief 解析 LLM 的评估结果
     * @param response LLM 响应
     * @return 评估结果
     */
    rag::ReflectionResult parse_evaluation_response(const std::string& response);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;     // HNSW 向量检索器
    std::shared_ptr<rag::BM25Retriever> bm25_retriever_;    // BM25 全文检索器
    std::unique_ptr<rag::CorrectiveRAG> corrective_rag_;     // Corrective RAG 决策器
    rag::SelfRAGConfig self_rag_config_;                     // Self-RAG 配置
    bool initialized_ = false;                                // 初始化标志
    int max_iterations_ = 3;                                 // 最大纠正迭代次数
    float quality_threshold_ = 0.5f;                         // 质量阈值

    // 统计信息
    struct Stats {
        int total_iterations = 0;
        int corrections_performed = 0;
        float avg_quality_score = 0.0f;
    };
    Stats stats_;
};

}  // namespace rag::modular
