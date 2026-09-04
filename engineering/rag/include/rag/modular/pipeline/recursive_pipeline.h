/**
 * @file recursive_pipeline.h
 * @brief RecursivePipeline - 递归分解 RAG Pipeline
 *
 * 流程: Query → 分解为子问题 → 各子问题检索 → 合并答案 → LLM
 *
 * RecursivePipeline 通过递归分解处理复杂查询:
 * - 首先分析复杂查询，判断是否需要分解
 * - 如果查询复杂，将其分解为多个子问题
 * - 对每个子问题分别执行检索
 * - 合并所有子问题的检索结果
 * - 使用 LLM 基于合并后的上下文生成回答
 *
 * 适用场景:
 * - 多跳问题（需要多个步骤推理）
 * - 多方面问题（涉及多个主题）
 * - 比较问题（需要对比多个选项）
 * - 复杂问题（包含多个子问题）
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
 * @brief 子问题结构
 */
struct SubQuery {
    int id = 0;                           // 子问题 ID
    std::string text;                     // 子问题文本
    std::string parent_query;             // 父查询
    bool is_required = true;              // 是否必需
    float importance = 1.0f;              // 重要性权重
    std::vector<RetrievalResult> results; // 检索结果
};

/**
 * @brief 分解结果
 */
struct DecompositionResult {
    bool is_complex = false;              // 原查询是否复杂
    bool needs_decomposition = false;     // 是否需要分解
    std::string original_query;           // 原始查询
    std::vector<SubQuery> sub_queries;    // 分解后的子问题
    std::string reason;                   // 分解原因
};

/**
 * @brief RecursivePipeline - 递归分解 Pipeline
 */
class RecursivePipeline : public ModularPipeline {
public:
    RecursivePipeline();
    ~RecursivePipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::RECURSIVE; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "RecursivePipeline"; }

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
     * @brief 设置最大递归深度
     */
    void set_max_depth(int max_depth) { max_depth_ = max_depth; }

    /**
     * @brief 获取最大递归深度
     */
    int max_depth() const { return max_depth_; }

    /**
     * @brief 设置复杂度阈值
     */
    void set_complexity_threshold(float threshold) { complexity_threshold_ = threshold; }

    /**
     * @brief 获取复杂度阈值
     */
    float complexity_threshold() const { return complexity_threshold_; }

private:
    /**
     * @brief 分解查询为子问题
     */
    DecompositionResult decompose_query(const std::string& query);

    /**
     * @brief 递归检索子问题
     */
    void retrieve_sub_queries(std::vector<SubQuery>& sub_queries, int depth);

    /**
     * @brief 合并子问题结果
     */
    std::vector<rag::RetrievalResult> merge_results(const std::vector<SubQuery>& sub_queries);

    /**
     * @brief 执行检索
     */
    std::vector<rag::RetrievalResult> retrieve(const std::string& query, int top_k);

    /**
     * @brief 构建分解提示词
     */
    std::string build_decomposition_prompt(const std::string& query);

    /**
     * @brief 构建子问题检索提示词
     */
    std::string build_subquery_prompt(const std::string& sub_query, const std::string& context);

    /**
     * @brief 解析分解结果
     */
    DecompositionResult parse_decomposition_response(const std::string& response);

    /**
     * @brief 判断查询复杂度
     */
    float evaluate_complexity(const std::string& query);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;     // HNSW 向量检索器
    std::shared_ptr<rag::BM25Retriever> bm25_retriever_;     // BM25 全文检索器
    bool initialized_ = false;                                // 初始化标志
    int max_depth_ = 2;                                       // 最大递归深度
    float complexity_threshold_ = 0.5f;                       // 复杂度阈值
    int min_subqueries_ = 2;                                  // 最小子问题数
    int max_subqueries_ = 5;                                  // 最大子问题数

    // 统计信息
    struct Stats {
        int total_subqueries = 0;
        int total_depth = 0;
        float avg_complexity = 0.0f;
    };
    Stats stats_;
};

}  // namespace rag::modular
