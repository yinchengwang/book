/**
 * @file types.h
 * @brief Modular RAG 类型定义
 */
#pragma once

#include "rag/types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace rag::modular {

// ========== Pipeline 类型枚举 ==========

/**
 * @brief RAG Pipeline 类型
 */
enum class PipelineType {
    NAIVE,        // 基础 RAG
    ADVANCED,     // 高级 RAG (混合检索 + 重排序)
    HYBRID,       // 三路召回 (Vector + BM25 + Graph)
    HYDE,         // HyDE (假设答案引导)
    GRAPH,        // Graph RAG (知识图谱)
    CORRECTIVE,   // Corrective RAG (纠正)
    REACT,        // ReAct RAG (推理+行动)
    ITERATIVE,    // Iterative RAG (迭代)
    RECURSIVE     // Recursive RAG (递归分解)
};

// ========== 查询结构 ==========

/**
 * @brief Modular RAG 查询结构
 */
struct ModularQuery {
    std::string text;                                // 查询文本
    PipelineType pipeline_type = PipelineType::ADVANCED;  // Pipeline 类型
    int top_k = 5;                                   // 返回结果数
    std::unordered_map<std::string, std::string> options;  // 额外选项
};

// ========== 查询结果 ==========

/**
 * @brief Modular RAG 查询结果
 */
struct ModularQueryResult {
    bool success = false;                            // 是否成功
    std::string answer;                              // 生成的回答
    std::vector<RetrievalResult> context;            // 检索到的上下文
    int64_t retrieval_time_ms = 0;                   // 检索耗时（毫秒）
    int64_t generation_time_ms = 0;                  // 生成耗时（毫秒）
    int64_t total_time_ms = 0;                       // 总耗时（毫秒）
    int total_tokens = 0;                            // 使用的 token 数
    std::string error_message;                       // 错误信息
};

// ========== 工具函数 ==========

/**
 * @brief PipelineType 转字符串
 * @param type Pipeline 类型
 * @return 类型对应的字符串
 */
std::string pipeline_type_to_string(PipelineType type);

/**
 * @brief 字符串转 PipelineType
 * @param str 字符串
 * @return 对应的 Pipeline 类型
 */
PipelineType string_to_pipeline_type(const std::string& str);

/**
 * @brief 列出所有 Pipeline 类型
 * @return Pipeline 类型名称列表
 */
std::vector<std::string> list_pipeline_types();

}  // namespace rag::modular
