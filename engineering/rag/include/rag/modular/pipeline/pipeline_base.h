/**
 * @file pipeline_base.h
 * @brief Modular RAG Pipeline 基类
 */
#pragma once

#include "rag/modular/types.h"
#include "rag/modular/config.h"
#include "rag/llm_service.h"
#include "rag/retriever.h"
#include <string>
#include <memory>

namespace rag::modular {

/**
 * @brief Modular RAG Pipeline 基类
 *
 * 所有具体 Pipeline 实现都应继承此类
 */
class ModularPipeline {
public:
    virtual ~ModularPipeline() = default;

    /**
     * @brief 获取 Pipeline 类型
     */
    virtual PipelineType type() const = 0;

    /**
     * @brief 获取 Pipeline 名称
     */
    virtual std::string name() const = 0;

    /**
     * @brief 初始化 Pipeline
     * @param config 配置信息
     * @return 初始化是否成功
     */
    virtual bool init(const ModularConfig& config) = 0;

    /**
     * @brief 执行查询
     * @param query 查询信息
     * @return 查询结果
     */
    virtual ModularQueryResult query(const ModularQuery& query) = 0;

    /**
     * @brief 检查 Pipeline 是否就绪
     */
    virtual bool is_ready() const = 0;

protected:
    /**
     * @brief 从检索结果构建上下文字符串
     * @param query 查询文本
     * @param results 检索结果
     * @return 格式化的上下文字符串
     */
    std::string build_context(const std::string& query,
                              const std::vector<RetrievalResult>& results);

    /**
     * @brief 使用 LLM 生成回答
     * @param prompt 提示词
     * @param options 生成选项
     * @return 生成的文本
     */
    std::string generate_with_llm(const std::string& prompt,
                                  const GenerateOptions& options = {});

    std::shared_ptr<LLMService> llm_;           // LLM 服务
    std::shared_ptr<Retriever> retriever_;      // 检索器
    ModularConfig config_;                       // 配置信息
};

} // namespace rag::modular
