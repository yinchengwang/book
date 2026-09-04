/**
 * @file config.h
 * @brief Modular RAG 配置定义
 */
#pragma once

#include "rag/config.h"
#include "rag/modular/types.h"
#include <string>

namespace rag::modular {

// ========== Agent 配置 ==========

/**
 * @brief Agent 配置
 */
struct AgentConfig {
    int max_iterations = 10;          // 最大迭代次数
    int timeout_ms = 30000;           // 超时时间（毫秒）
    float temperature = 0.7f;         // 温度参数
    bool verbose = false;             // 详细输出
    std::string strategy = "react";   // 策略类型
};

// ========== Modular 配置 ==========

/**
 * @brief Modular RAG 主配置
 */
struct ModularConfig {
    PipelineType default_pipeline = PipelineType::ADVANCED;  // 默认 Pipeline 类型
    LLMConfig llm;                     // LLM 配置
    EmbeddingConfig embedding;         // Embedding 配置
    RetrievalConfig retrieval;         // 检索配置
    AgentConfig agent;                 // Agent 配置

    std::string model_path;            // 模型路径
    std::string embedding_model_path;  // Embedding 模型路径
    std::string data_dir;              // 数据目录
    std::string index_dir;             // 索引目录
};

}  // namespace rag::modular
