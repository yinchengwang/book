/**
 * @file naive_pipeline.h
 * @brief NaivePipeline - 基础 RAG Pipeline
 *
 * 流程: Query → Vector检索 → Context → LLM
 *
 * 这是最简单的 RAG Pipeline，直接使用向量检索获取相关上下文，
 * 然后传递给 LLM 生成回答。
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/retriever.h"
#include "rag/types.h"
#include <memory>

namespace rag::modular {

/**
 * @brief NaivePipeline - 基础 RAG Pipeline
 *
 * 特点:
 * - 简单直接，易于理解和调试
 * - 使用向量检索获取 top-k 相关块
 * - 直接将检索结果作为上下文传给 LLM
 *
 * 限制:
 * - 不支持查询扩展，可能遗漏相关内容
 * - 不支持重排序，可能返回次优结果
 */
class NaivePipeline : public ModularPipeline {
public:
    NaivePipeline();
    ~NaivePipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::NAIVE; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "NaivePipeline"; }

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
     * @brief 设置向量检索器
     * @param retriever HNSW 检索器
     */
    void set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever);

    /**
     * @brief 获取向量检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }

private:
    /**
     * @brief 执行向量检索
     * @param query 查询文本
     * @param top_k 返回结果数
     * @return 检索结果
     */
    std::vector<rag::RetrievalResult> retrieve_vectors(
        const std::string& query, int top_k);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;  // HNSW 向量检索器
    bool initialized_ = false;                              // 初始化标志
};

}  // namespace rag::modular
