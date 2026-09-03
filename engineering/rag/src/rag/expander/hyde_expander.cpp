/**
 * @file hyde_expander.cpp
 * @brief HyDE (Hypothetical Document Embeddings) 扩展器实现
 */

#include "rag/query_expander.h"
#include "rag/pipeline.h"
#include <chrono>
#include <sstream>

namespace rag {

HyDEExpander::HyDEExpander(int max_hypotheses)
    : max_hypotheses_(max_hypotheses > 0 ? max_hypotheses : 3),
      llm_service_(nullptr) {
}

ExpansionResult HyDEExpander::expand(const std::string& query) {
    auto start = std::chrono::high_resolution_clock::now();

    ExpansionResult result;
    result.original_query = query;
    result.method = "hyde";

    // 生成假设性答案
    auto hypotheses = generate_hypotheses(query);

    // 添加原始查询
    result.expanded_queries.push_back(query);
    result.weights.push_back(1.0f);

    // 添加假设性答案作为扩展查询
    for (size_t i = 0; i < hypotheses.size(); ++i) {
        result.expanded_queries.push_back(hypotheses[i]);
        // 递减权重
        result.weights.push_back(0.8f - static_cast<float>(i) * 0.05f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

std::vector<std::string> HyDEExpander::generate_hypotheses(const std::string& query) {
    std::vector<std::string> hypotheses;

    // Mock 实现: 生成固定格式的假设答案
    // 在实际应用中，这里会调用 LLM 生成假设性文档
    std::ostringstream oss;
    oss << "关于「" << query << "」的假设性回答："
        << "这个问题涉及到相关领域知识。"
        << "根据假设性文档，相关内容包括概念定义、实际应用和历史背景。"
        << "总结来说，这是一个值得深入探讨的话题。";

    // 生成 2-3 个假设答案
    for (int i = 0; i < max_hypotheses_ && i < 3; ++i) {
        std::string hypothesis;
        if (i == 0) {
            hypothesis = "【假设答案1】" + oss.str() + " 这是针对问题假设性文档的第一种表述。";
        } else if (i == 1) {
            hypothesis = "【假设答案2】" + oss.str() + " 从另一个角度，这是第二种假设性表述。";
        } else {
            hypothesis = "【假设答案3】" + oss.str() + " 第三种视角的假设性总结。";
        }
        hypotheses.push_back(hypothesis);
    }

    return hypotheses;
}

void HyDEExpander::set_llm_service(void* llm_service) {
    llm_service_ = llm_service;
}

}  // namespace rag