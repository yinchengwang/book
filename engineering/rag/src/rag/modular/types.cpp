/**
 * @file types.cpp
 * @brief Modular RAG 类型实现
 */

#include "rag/modular/types.h"
#include <unordered_map>

namespace rag::modular {

// ========== 工具函数实现 ==========

std::string pipeline_type_to_string(PipelineType type) {
    static const std::unordered_map<PipelineType, std::string> type_map = {
        {PipelineType::NAIVE, "naive"},
        {PipelineType::ADVANCED, "advanced"},
        {PipelineType::HYBRID, "hybrid"},
        {PipelineType::HYDE, "hyde"},
        {PipelineType::GRAPH, "graph"},
        {PipelineType::CORRECTIVE, "corrective"},
        {PipelineType::REACT, "react"},
        {PipelineType::ITERATIVE, "iterative"},
        {PipelineType::RECURSIVE, "recursive"}
    };

    auto it = type_map.find(type);
    if (it != type_map.end()) {
        return it->second;
    }
    return "unknown";
}

PipelineType string_to_pipeline_type(const std::string& str) {
    static const std::unordered_map<std::string, PipelineType> str_map = {
        {"naive", PipelineType::NAIVE},
        {"advanced", PipelineType::ADVANCED},
        {"hybrid", PipelineType::HYBRID},
        {"hyde", PipelineType::HYDE},
        {"graph", PipelineType::GRAPH},
        {"corrective", PipelineType::CORRECTIVE},
        {"react", PipelineType::REACT},
        {"iterative", PipelineType::ITERATIVE},
        {"recursive", PipelineType::RECURSIVE}
    };

    auto it = str_map.find(str);
    if (it != str_map.end()) {
        return it->second;
    }
    return PipelineType::ADVANCED;  // 默认返回高级 RAG
}

std::vector<std::string> list_pipeline_types() {
    return {
        "naive",
        "advanced",
        "hybrid",
        "hyde",
        "graph",
        "corrective",
        "react",
        "iterative",
        "recursive"
    };
}

}  // namespace rag::modular
