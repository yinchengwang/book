/**
 * @file pipeline_base.cpp
 * @brief Pipeline 基类实现
 */

#include "mmrag/modular/pipeline/pipeline_base.h"
#include "mmrag/logger.h"
#include <algorithm>
#include <sstream>

namespace mmrag::modular {

ModularQueryResult ModularPipeline::query(const ModularQuery& query) {
    ensure_intent_gate();

    auto decision = intent_gate_->route(query.text);
    if (decision.action != mmrag::RouteAction::PROCEED) {
        ModularQueryResult result;
        result.success = true;
        result.answer = decision.reply;
        result.routed_by = (decision.action == mmrag::RouteAction::CHAT_REPLY)
                               ? "intent_gate_chat" : "intent_gate_oos";
        RAG_INFO("IntentGate 短路: routed_by=" + result.routed_by +
                 " matched_by=" + decision.matched_by +
                 " confidence=" + std::to_string(decision.confidence));
        return result;
    }

    auto result = do_query(query);
    if (result.routed_by.empty()) {
        result.routed_by = "normal";
    }
    return result;
}

void ModularPipeline::ensure_intent_gate() {
    std::call_once(gate_once_flag_, [this] {
        const auto& gc = config_.intent_gate;
        std::shared_ptr<mmrag::MiniLMEmbedder> embedder;
        if (gc.enabled && gc.strategy == "rule_then_embedding" &&
            !gc.embedder_model_dir.empty()) {
            auto e = std::make_shared<mmrag::MiniLMEmbedder>(
                gc.embedder_model_dir, gc.embedder_dim);
            if (e->is_ready()) {
                embedder = std::move(e);
            } else {
                RAG_WARN("IntentGate embedder 加载失败，降级为 rule_only: " +
                         gc.embedder_model_dir);
            }
        }
        intent_gate_ = std::make_unique<mmrag::IntentGate>(gc, embedder);
    });
}

bool ModularPipeline::has_sufficient_evidence(
    const std::vector<RetrievalResult>& results) const {
    if (results.empty()) return false;
    float best = 0.0f;
    for (const auto& r : results) {
        best = std::max(best, r.score);
    }
    return best >= config_.intent_gate.evidence_threshold;
}

ModularQueryResult ModularPipeline::make_no_evidence_result(
    const ModularQuery& /*query*/) const {
    ModularQueryResult result;
    result.success = true;
    result.answer = config_.intent_gate.no_evidence_reply;
    result.routed_by = "evidence_gate";
    return result;
}

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
        RAG_ERROR("LLM 服务未初始化或未加载模型");
        return "错误: LLM 服务不可用";
    }

    // 使用 LLM 生成回答
    try {
        auto result = llm_->generate(prompt, options);

        if (result.finished) {
            return result.text;
        } else {
            RAG_WARN("LLM 生成未正常完成: " + result.finish_reason);
            return result.text;
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("LLM 生成异常: ") + e.what());
        return std::string("错误: ") + e.what();
    }
}

} // namespace mmrag::modular
