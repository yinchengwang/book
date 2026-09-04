/**
 * @file pipeline_base.cpp
 * @brief Pipeline 基类实现
 */

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/logger.h"
#include <sstream>

namespace rag::modular {

std::string ModularPipeline::build_context(
    const std::string& query,
    const std::vector<RetrievalResult>& results) {
    // 构建上下文字符串，将检索结果格式化为可读的上下文
    std::ostringstream oss;

    if (results.empty()) {
        return "";
    }

    // 添加上下文标题
    oss << "【上下文信息】\n\n";

    // 遍历检索结果，格式化每个块的内容
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        const auto& chunk = result.chunk;

        // 添加来源信息
        oss << "【来源 " << (i + 1) << "】(得分: " << result.score
            << ", 来源: " << result.source << ")\n";

        // 添加文件路径（如果存在）
        if (!chunk.metadata.file_path.empty()) {
            oss << "文件: " << chunk.metadata.file_path;
            if (chunk.chunk_index > 0) {
                oss << " (块 " << chunk.chunk_index << ")";
            }
            oss << "\n";
        }

        // 添加块内容
        oss << chunk.content << "\n\n";
    }

    return oss.str();
}

std::string ModularPipeline::generate_with_llm(
    const std::string& prompt,
    const GenerateOptions& options) {
    // 检查 LLM 服务是否可用
    if (!llm_ || !llm_->is_loaded()) {
        RAG_LOG_ERROR("LLM 服务未初始化或未加载模型");
        return "错误: LLM 服务不可用";
    }

    // 使用 LLM 生成回答
    try {
        auto result = llm_->generate(prompt, options);

        if (result.finished) {
            return result.text;
        } else {
            RAG_LOG_WARN("LLM 生成未正常完成: " + result.finish_reason);
            return result.text;
        }
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("LLM 生成异常: ") + e.what());
        return std::string("错误: ") + e.what();
    }
}

} // namespace rag::modular
