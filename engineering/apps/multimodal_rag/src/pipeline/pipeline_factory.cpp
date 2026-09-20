/**
 * @file pipeline_factory.cpp
 * @brief Pipeline 工厂实现
 */

#include "mmrag/modular/pipeline/pipeline_factory.h"
#include "mmrag/modular/pipeline/naive_pipeline.h"
#include "mmrag/modular/pipeline/advanced_pipeline.h"
#include "mmrag/modular/pipeline/hybrid_pipeline.h"
#include "mmrag/modular/pipeline/hyde_pipeline.h"
#include "mmrag/modular/pipeline/graph_pipeline.h"
#include "mmrag/modular/pipeline/corrective_pipeline.h"
#include "mmrag/modular/pipeline/react_pipeline.h"
#include "mmrag/modular/pipeline/iterative_pipeline.h"
#include "mmrag/modular/pipeline/recursive_pipeline.h"
#include "mmrag/logger.h"
#include <unordered_map>

namespace mmrag::modular {

std::unique_ptr<ModularPipeline> PipelineFactory::create(
    PipelineType type, const ModularConfig& config) {
    // 根据类型创建对应的 Pipeline 实例
    // 注意: 具体的 Pipeline 类需要在后续实现中创建
    // 这里暂时返回 nullptr，实际使用时需要先实现具体的 Pipeline 类

    switch (type) {
        case PipelineType::NAIVE:
            return std::make_unique<NaivePipeline>();

        case PipelineType::ADVANCED:
            return std::make_unique<AdvancedPipeline>();

        case PipelineType::HYBRID:
            return std::make_unique<HybridPipeline>();

        case PipelineType::HYDE:
            return std::make_unique<HyDEPipeline>();

        case PipelineType::GRAPH:
            return std::make_unique<GraphPipeline>();

        case PipelineType::CORRECTIVE:
            return std::make_unique<CorrectivePipeline>();

        case PipelineType::REACT:
            return std::make_unique<ReActPipeline>();

        case PipelineType::ITERATIVE:
            return std::make_unique<IterativePipeline>();

        case PipelineType::RECURSIVE:
            return std::make_unique<RecursivePipeline>();

        default:
            RAG_ERROR("未知的 Pipeline 类型: " +
                          pipeline_type_to_string(type));
            return nullptr;
    }
}

std::unordered_map<PipelineType, std::unique_ptr<ModularPipeline>>
PipelineFactory::create_all(const ModularConfig& config) {
    std::unordered_map<PipelineType, std::unique_ptr<ModularPipeline>> pipelines;

    // 遍历所有 Pipeline 类型并创建实例
    // 注意: 这里只是预留接口，具体实现需要等各个 Pipeline 类完成
    for (auto type : {
        PipelineType::NAIVE,
        PipelineType::ADVANCED,
        PipelineType::HYBRID,
        PipelineType::HYDE,
        PipelineType::GRAPH,
        PipelineType::CORRECTIVE,
        PipelineType::REACT,
        PipelineType::ITERATIVE,
        PipelineType::RECURSIVE
    }) {
        auto pipeline = create(type, config);
        if (pipeline) {
            pipelines[type] = std::move(pipeline);
        }
    }

    return pipelines;
}

std::string PipelineFactory::get_description(PipelineType type) {
    switch (type) {
        case PipelineType::NAIVE:
            return "基础 RAG: 直接检索+生成，简单但高效";
        case PipelineType::ADVANCED:
            return "高级 RAG: 混合检索+重排序，提升准确性";
        case PipelineType::HYBRID:
            return "三路召回: Vector + BM25 + Graph 融合";
        case PipelineType::HYDE:
            return "HyDE: 假设答案引导检索";
        case PipelineType::GRAPH:
            return "Graph RAG: 知识图谱增强检索";
        case PipelineType::CORRECTIVE:
            return "Corrective RAG: 自我纠正检索结果";
        case PipelineType::REACT:
            return "ReAct RAG: 推理+行动交替进行";
        case PipelineType::ITERATIVE:
            return "Iterative RAG: 迭代优化检索和生成";
        case PipelineType::RECURSIVE:
            return "Recursive RAG: 递归分解复杂查询";
        default:
            return "未知类型";
    }
}

} // namespace mmrag::modular
