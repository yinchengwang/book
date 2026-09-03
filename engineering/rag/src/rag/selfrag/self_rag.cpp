/**
 * @file self_rag.cpp
 * @brief Self-RAG 和 Corrective-RAG 实现
 */
#include "rag/self_rag.h"
#include "rag/pipeline.h"
#include "rag/llm_service.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cctype>

namespace rag {

// ========== ReflectionResult ==========

std::string ReflectionResult::to_string() const {
    std::ostringstream oss;
    oss << "ReflectionResult {\n";
    oss << "  is_relevant: " << (is_relevant ? "true" : "false") << "\n";
    oss << "  is_supported: " << (is_supported ? "true" : "false") << "\n";
    oss << "  is_complete: " << (is_complete ? "true" : "false") << "\n";
    oss << "  is_useful: " << (is_useful ? "true" : "false") << "\n";
    oss << "  relevance_score: " << relevance_score << "\n";
    oss << "  support_score: " << support_score << "\n";
    oss << "  completeness_score: " << completeness_score << "\n";
    oss << "  usefulness_score: " << usefulness_score << "\n";
    oss << "  overall_score: " << overall_score() << "\n";
    oss << "}";
    return oss.str();
}

// ========== Reflection Token Parsing ==========

ReflectionResult parse_reflection_tokens(const std::string& llm_output) {
    ReflectionResult result;

    std::string lower_output = llm_output;
    std::transform(lower_output.begin(), lower_output.end(), lower_output.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    // 检查相关性
    if (lower_output.find("is_relevant") != std::string::npos ||
        lower_output.find("relevant") != std::string::npos ||
        lower_output.find("相关") != std::string::npos) {
        result.is_relevant = true;
        result.relevance_score = 0.8f;
    }

    if (lower_output.find("not_relevant") != std::string::npos ||
        lower_output.find("irrelevant") != std::string::npos ||
        lower_output.find("不相关") != std::string::npos) {
        result.is_relevant = false;
        result.relevance_score = 0.2f;
    }

    // 检查支持性
    if (lower_output.find("is_supported") != std::string::npos ||
        lower_output.find("supported") != std::string::npos ||
        lower_output.find("支持") != std::string::npos) {
        result.is_supported = true;
        result.support_score = 0.8f;
    }

    if (lower_output.find("not_supported") != std::string::npos ||
        lower_output.find("unsupported") != std::string::npos ||
        lower_output.find("不支持") != std::string::npos) {
        result.is_supported = false;
        result.support_score = 0.2f;
    }

    // 检查完整性
    if (lower_output.find("is_complete") != std::string::npos ||
        lower_output.find("complete") != std::string::npos ||
        lower_output.find("完整") != std::string::npos) {
        result.is_complete = true;
        result.completeness_score = 0.8f;
    }

    if (lower_output.find("is_partial") != std::string::npos ||
        lower_output.find("partial") != std::string::npos ||
        lower_output.find("部分") != std::string::npos) {
        result.is_complete = false;
        result.completeness_score = 0.5f;
    }

    // 检查有用性
    if (lower_output.find("is_useful") != std::string::npos ||
        lower_output.find("useful") != std::string::npos ||
        lower_output.find("有用") != std::string::npos) {
        result.is_useful = true;
        result.usefulness_score = 0.8f;
    }

    if (lower_output.find("not_useful") != std::string::npos ||
        lower_output.find("useless") != std::string::npos ||
        lower_output.find("无用") != std::string::npos) {
        result.is_useful = false;
        result.usefulness_score = 0.2f;
    }

    // FULLY_USEFUL 组合标记
    if (lower_output.find("fully_useful") != std::string::npos ||
        lower_output.find("完全有用") != std::string::npos) {
        result.is_relevant = true;
        result.is_supported = true;
        result.is_useful = true;
        result.relevance_score = 1.0f;
        result.support_score = 1.0f;
        result.usefulness_score = 1.0f;
    }

    // 如果没有任何标记，使用默认值
    if (result.relevance_score == 0.0f && result.support_score == 0.0f &&
        result.completeness_score == 0.0f && result.usefulness_score == 0.0f) {
        result.relevance_score = 0.5f;
        result.support_score = 0.5f;
        result.completeness_score = 0.5f;
        result.usefulness_score = 0.5f;
    }

    return result;
}

// ========== SelfRAGStage ==========

SelfRAGStage::SelfRAGStage(
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : config_(config), llm_service_(llm_service) {
}

StageOutput SelfRAGStage::process(const StageInput& input) {
    StageOutput output;

    if (!config_.enable_self_check) {
        output.status = StageOutput::Status::SKIPPED;
        output.next_action = "continue";
        return output;
    }

    if (input.candidates.empty()) {
        output.status = StageOutput::Status::SUCCESS;
        output.results = {};
        output.next_action = "continue";
        return output;
    }

    // 评估所有 chunks
    std::vector<ReflectionResult> evaluations;
    std::vector<Chunk> chunks;

    for (const auto& candidate : input.candidates) {
        chunks.push_back(candidate.chunk);
        ReflectionResult eval = evaluate_chunk(input.query, candidate.chunk);
        evaluations.push_back(eval);
    }

    // 根据阈值过滤
    std::vector<Chunk> filtered_chunks = filter_by_threshold(chunks, evaluations);
    std::vector<ReflectionResult> filtered_evals;

    for (size_t i = 0; i < chunks.size() && i < evaluations.size(); ++i) {
        // 检查 chunk 是否在 filtered_chunks 中 (通过 id 比较)
        bool found = false;
        for (const auto& fc : filtered_chunks) {
            if (fc.id == chunks[i].id) {
                found = true;
                break;
            }
        }
        if (found) {
            filtered_evals.push_back(evaluations[i]);
        }
    }

    // 构建结果
    for (size_t i = 0; i < filtered_chunks.size(); ++i) {
        RetrievalResult result;
        result.chunk = filtered_chunks[i];
        result.score = (i < filtered_evals.size()) ? filtered_evals[i].overall_score() : 0.5f;
        result.source = "self_rag";
        result.rank = static_cast<int>(i);
        output.results.push_back(result);
    }

    // 判断是否需要重写
    bool needs_rewrite = should_rewrite(filtered_evals);
    output.metadata["needs_rewrite"] = needs_rewrite ? "true" : "false";

    if (needs_rewrite) {
        output.metadata["next_action"] = "rewrite";
    }

    output.status = StageOutput::Status::SUCCESS;
    output.next_action = needs_rewrite ? "retry" : "continue";

    return output;
}

ReflectionResult SelfRAGStage::evaluate_chunk(
    const std::string& query,
    const Chunk& chunk) {

    if (config_.use_llm_evaluation && llm_service_) {
        return llm_evaluate(query, chunk);
    }

    // Mock 评估：基于关键词相似度
    ReflectionResult result;

    std::string query_lower = query;
    std::string chunk_lower = chunk.content;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(chunk_lower.begin(), chunk_lower.end(), chunk_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 简单的关键词匹配
    size_t match_count = 0;
    std::vector<std::string> query_words = {"是什么", "如何", "怎么", "为什么",
                                              "哪个", "哪些", "什么叫", "多少", "谁"};

    for (const auto& word : query_words) {
        if (query_lower.find(word) != std::string::npos) {
            // 知识型查询，chunk 中有内容就给较高分
            result.relevance_score = 0.7f;
            result.support_score = 0.6f;
            result.completeness_score = 0.5f;
            result.usefulness_score = 0.7f;
            result.is_relevant = true;
            result.is_supported = true;
            result.is_complete = false;
            result.is_useful = true;
            break;
        }
    }

    // 默认评估
    if (result.relevance_score == 0.0f) {
        result.relevance_score = 0.6f;
        result.support_score = 0.5f;
        result.completeness_score = 0.5f;
        result.usefulness_score = 0.6f;
        result.is_relevant = true;
        result.is_supported = true;
        result.is_complete = true;
        result.is_useful = true;
    }

    return result;
}

std::vector<ReflectionResult> SelfRAGStage::evaluate_chunks(
    const std::string& query,
    const std::vector<Chunk>& chunks) {
    std::vector<ReflectionResult> results;
    for (const auto& chunk : chunks) {
        results.push_back(evaluate_chunk(query, chunk));
    }
    return results;
}

bool SelfRAGStage::should_rewrite(
    const std::vector<ReflectionResult>& evaluations) {
    if (evaluations.empty()) {
        return true;
    }

    float total_score = 0.0f;
    for (const auto& eval : evaluations) {
        total_score += eval.overall_score();
    }

    float avg_score = total_score / evaluations.size();
    return avg_score < config_.acceptance_threshold;
}

std::vector<Chunk> SelfRAGStage::filter_by_threshold(
    const std::vector<Chunk>& chunks,
    const std::vector<ReflectionResult>& evaluations) {
    std::vector<Chunk> filtered;

    for (size_t i = 0; i < chunks.size() && i < evaluations.size(); ++i) {
        const auto& eval = evaluations[i];
        if (eval.relevance_score >= config_.relevance_threshold &&
            eval.usefulness_score >= config_.usefulness_threshold) {
            filtered.push_back(chunks[i]);
        }
    }

    return filtered;
}

void SelfRAGStage::update_config(const SelfRAGConfig& config) {
    config_ = config;
}

ReflectionResult SelfRAGStage::llm_evaluate(
    const std::string& query,
    const Chunk& chunk) {
    std::string prompt = build_evaluation_prompt(query, chunk);

    // 如果没有 LLM 服务，返回 mock 结果
    if (!llm_service_) {
        return token_evaluate(prompt);
    }

    // 调用 LLM
    GenerateOptions options;
    options.max_tokens = 256;
    options.temperature = 0.0f;  // 更确定性

    auto response = llm_service_->generate(prompt, options);

    if (response.finish_reason != "error" && !response.text.empty()) {
        return parse_llm_evaluation(response.text);
    }

    // LLM 调用失败，使用 token 评估
    return token_evaluate(response.text);
}

ReflectionResult SelfRAGStage::token_evaluate(
    const std::string& llm_output) {
    return parse_reflection_tokens(llm_output);
}

std::string SelfRAGStage::build_evaluation_prompt(
    const std::string& query,
    const Chunk& chunk) {
    std::ostringstream oss;
    oss << "请评估以下检索内容对回答查询的相关性。\n\n";
    oss << "查询: " << query << "\n\n";
    oss << "检索内容: " << chunk.content << "\n\n";
    oss << "请输出以下维度的评估结果 (是/否):\n";
    oss << "- IS_RELEVANT: 内容是否与查询相关\n";
    oss << "- IS_SUPPORTED: 内容是否被充分支持\n";
    oss << "- IS_COMPLETE: 内容是否完整回答了问题\n";
    oss << "- IS_USEFUL: 内容对回答是否有帮助\n";
    return oss.str();
}

ReflectionResult SelfRAGStage::parse_llm_evaluation(const std::string& response) {
    return parse_reflection_tokens(response);
}

// ========== CorrectiveRAG ==========

CorrectiveRAG::CorrectiveRAG(const SelfRAGConfig& config)
    : config_(config) {
}

CorrectiveDecision CorrectiveRAG::decide_action(
    const std::vector<Chunk>& chunks,
    const std::vector<ReflectionResult>& evaluations,
    float avg_score) {
    (void)chunks;
    (void)evaluations;
    CorrectiveDecision decision;

    // 根据 avg_score 决定动作
    if (avg_score > 0.6f) {
        decision.action = CorrectiveAction::PASS;
        decision.confidence = avg_score;
        decision.reason = "High quality retrieval results";
    } else if (avg_score >= 0.4f) {
        decision.action = CorrectiveAction::EXPAND;
        decision.confidence = 1.0f - avg_score;
        decision.reason = "Moderate quality, expanding retrieval";
    } else if (avg_score >= 0.2f) {
        decision.action = CorrectiveAction::REWRITE;
        decision.confidence = 0.8f;
        decision.reason = "Low quality, rewriting query";
    } else {
        decision.action = CorrectiveAction::WEB_FALLBACK;
        decision.confidence = 0.9f;
        decision.reason = "Very low quality, using web fallback";
    }

    return decision;
}

std::string CorrectiveRAG::rewrite_query(
    const std::string& original_query,
    const CorrectiveAction& action,
    const std::vector<Chunk>& chunks) {
    std::ostringstream oss;

    switch (action) {
        case CorrectiveAction::REWRITE:
            // 添加更多上下文
            oss << "详细解释: " << original_query;
            break;

        case CorrectiveAction::EXPAND:
            // 扩展查询
            if (!chunks.empty()) {
                oss << original_query << " ";
                // 添加已有内容的关键词
                std::string content_summary = chunks[0].content.substr(0, 50);
                oss << content_summary << "...";
            } else {
                oss << original_query;
            }
            break;

        case CorrectiveAction::WEB_FALLBACK:
            oss << original_query << " (web search)";
            break;

        default:
            oss << original_query;
            break;
    }

    return oss.str();
}

bool CorrectiveRAG::should_use_web_fallback(float avg_score, int chunk_count) {
    return avg_score < 0.2f || (chunk_count == 0 && avg_score < 0.4f);
}

float CorrectiveRAG::calculate_quality_score(
    const std::vector<ReflectionResult>& evaluations) {
    if (evaluations.empty()) {
        return 0.0f;
    }

    float total = 0.0f;
    for (const auto& eval : evaluations) {
        total += eval.overall_score();
    }

    return total / evaluations.size();
}

CorrectiveAction CorrectiveRAG::decide_by_quality(float quality_score) {
    if (quality_score > 0.6f) {
        return CorrectiveAction::PASS;
    } else if (quality_score >= 0.4f) {
        return CorrectiveAction::EXPAND;
    } else if (quality_score >= 0.2f) {
        return CorrectiveAction::REWRITE;
    } else {
        return CorrectiveAction::WEB_FALLBACK;
    }
}

// ========== SelfRAGPipeline ==========

SelfRAGPipeline::SelfRAGPipeline(
    std::unique_ptr<RetrievalPipeline> base_pipeline,
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : base_pipeline_(std::move(base_pipeline)),
      config_(config),
      llm_service_(llm_service),
      corrective_(config) {
}

PipelineResult SelfRAGPipeline::execute(const std::string& query, int top_k) {
    stats_.total_turns = 0;
    std::string current_query = query;

    for (int turn = 0; turn < config_.max_retrieval_turns; ++turn) {
        stats_.total_turns++;

        // 执行一轮检索
        PipelineResult result = execute_turn(current_query, top_k, turn);

        if (result.success && !result.results.empty()) {
            // 评估结果
            float avg_score = 0.0f;
            if (!result.results.empty()) {
                float total = 0.0f;
                for (const auto& r : result.results) {
                    total += r.score;
                }
                avg_score = total / result.results.size();
            }

            // 决定动作
            CorrectiveDecision decision;

            if (avg_score > 0.6f) {
                decision.action = CorrectiveAction::PASS;
                stats_.successful_turns++;
                return result;
            } else if (avg_score >= 0.4f) {
                decision.action = CorrectiveAction::EXPAND;
                top_k *= 2;  // 增加 top_k
                stats_.repeat_count++;
            } else if (avg_score >= 0.2f) {
                decision.action = CorrectiveAction::REWRITE;
                current_query = corrective_.rewrite_query(current_query, decision.action, {});
                stats_.rewrite_count++;
            } else {
                decision.action = CorrectiveAction::WEB_FALLBACK;
                stats_.web_fallback_count++;
                return result;
            }
        } else {
            // 无结果，尝试重写
            CorrectiveDecision decision;
            decision.action = CorrectiveAction::REWRITE;
            current_query = corrective_.rewrite_query(current_query, decision.action, {});
            stats_.rewrite_count++;
        }
    }

    // 达到最大轮次
    PipelineResult final_result;
    final_result = base_pipeline_->execute(query, top_k);
    return final_result;
}

std::future<PipelineResult> SelfRAGPipeline::execute_async(
    const std::string& query, int top_k) {
    return std::async(std::launch::async, [this, query, top_k]() {
        return execute(query, top_k);
    });
}

PipelineResult SelfRAGPipeline::execute_turn(
    const std::string& query,
    int top_k,
    int turn_number) {
    (void)turn_number;
    return base_pipeline_->execute(query, top_k);
}

std::pair<CorrectiveAction, std::string> SelfRAGPipeline::handle_corrective(
    const CorrectiveDecision& decision,
    const std::string& current_query) {
    std::string new_query = current_query;

    switch (decision.action) {
        case CorrectiveAction::REWRITE:
            new_query = corrective_.rewrite_query(current_query, decision.action, {});
            break;
        case CorrectiveAction::EXPAND:
            // 扩展已有结果
            break;
        case CorrectiveAction::WEB_FALLBACK:
            new_query = current_query + " (web)";
            break;
        default:
            break;
    }

    return {decision.action, new_query};
}

// ========== Factory Functions ==========

std::shared_ptr<SelfRAGStage> create_self_rag_stage(
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service) {
    return std::make_shared<SelfRAGStage>(config, llm_service);
}

std::unique_ptr<CorrectiveRAG> create_corrective_rag(
    const SelfRAGConfig& config) {
    return std::make_unique<CorrectiveRAG>(config);
}

std::unique_ptr<SelfRAGPipeline> create_self_rag_pipeline(
    std::unique_ptr<RetrievalPipeline> base_pipeline,
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service) {
    return std::make_unique<SelfRAGPipeline>(
        std::move(base_pipeline), config, llm_service);
}

}  // namespace rag