/**
 * @file pipeline_factory.h
 * @brief Pipeline 工厂类
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/modular/config.h"
#include <memory>
#include <unordered_map>

namespace rag::modular {

/**
 * @brief Pipeline 工厂类
 *
 * 负责创建不同类型的 Pipeline 实例
 */
class PipelineFactory {
public:
    /**
     * @brief 创建指定类型的 Pipeline
     * @param type Pipeline 类型
     * @param config 配置信息
     * @return Pipeline 实例（所有权转移）
     */
    static std::unique_ptr<ModularPipeline> create(
        PipelineType type, const ModularConfig& config);

    /**
     * @brief 创建所有类型的 Pipeline
     * @param config 配置信息
     * @return Pipeline 类型到实例的映射
     */
    static std::unordered_map<PipelineType, std::unique_ptr<ModularPipeline>>
    create_all(const ModularConfig& config);

    /**
     * @brief 获取 Pipeline 类型的描述
     * @param type Pipeline 类型
     * @return 类型描述字符串
     */
    static std::string get_description(PipelineType type);
};

} // namespace rag::modular
