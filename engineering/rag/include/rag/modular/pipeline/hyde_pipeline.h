/**
 * @file hyde_pipeline.h
 * @brief HyDEPipeline - Hypothetical Document Embeddings RAG Pipeline
 *
 * 流程: Query → LLM生成假设答案 → 用假设答案检索 → LLM最终生成
 *
 * HyDEPipeline 是一种创新的 RAG 方法:
 * - 首先让 LLM 根据查询生成一个"假设答案"(Hypothetical Document)
 * - 使用假设答案进行向量检索，而不是直接用查询
 * - 假设答案包含了 LLM 认为相关的上下文信息，能更好地匹配目标文档
 * - 最后用真实检索到的上下文让 LLM 生成最终答案
 *
 * 优势:
 * - 比直接查询更能捕捉语义相关性
 * - 假设答案提供了更好的检索"锚点"
 * - 可以发现直接查询可能遗漏的相关文档
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/types.h"
#include <memory>
#include <vector>

namespace rag::modular {

/**
 * @brief HyDEPipeline - Hypothetical Document Embeddings Pipeline
 *
 * 核心思想:
 * 使用 LLM 生成的假设答案作为检索的中间表示，
 * 利用假设答案与真实文档的向量相似度来发现最相关的上下文。
 */
class HyDEPipeline : public ModularPipeline {
public:
    HyDEPipeline();
    ~HyDEPipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::HYDE; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "HyDEPipeline"; }

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
     * @brief 获取 HNSW 检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }

    /**
     * @brief 设置是否启用多假设答案模式
     * @param enable 是否启用
     * @param count 假设答案数量（默认3个）
     */
    void set_multi_hypothesis_mode(bool enable, int count = 3);

    /**
     * @brief 获取多假设答案模式设置
     */
    bool multi_hypothesis_mode() const { return multi_hypothesis_mode_; }
    int hypothesis_count() const { return hypothesis_count_; }

private:
    /**
     * @brief 使用 LLM 生成假设答案
     * @param query 原始查询
     * @return 假设答案列表
     */
    std::vector<std::string> generate_hypothetical_documents(
        const std::string& query);

    /**
     * @brief 使用假设答案执行检索
     * @param hypotheses 假设答案列表
     * @param top_k 返回结果数
     * @return 检索结果
     */
    std::vector<rag::RetrievalResult> retrieve_with_hypotheses(
        const std::vector<std::string>& hypotheses,
        int top_k);

    /**
     * @brief 使用假设答案检索（单个）
     * @param hypothesis 假设答案
     * @param top_k 返回结果数
     * @return 检索结果
     */
    std::vector<rag::RetrievalResult> retrieve_with_single_hypothesis(
        const std::string& hypothesis,
        int top_k);

    /**
     * @brief 构建 HyDE 提示词
     * @param query 原始查询
     * @return 提示词
     */
    std::string build_hyde_prompt(const std::string& query);

    /**
     * @brief 构建多假设答案提示词
     * @param query 原始查询
     * @param count 假设答案数量
     * @return 提示词
     */
    std::string build_multi_hypothesis_prompt(const std::string& query, int count);

    /**
     * @brief 解析 LLM 生成的假设答案
     * @param response LLM 响应
     * @return 假设答案列表
     */
    std::vector<std::string> parse_hypothetical_documents(const std::string& response);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;  // HNSW 向量检索器
    bool initialized_ = false;                             // 初始化标志
    bool multi_hypothesis_mode_ = false;                   // 多假设答案模式
    int hypothesis_count_ = 3;                             // 假设答案数量
};

}  // namespace rag::modular
