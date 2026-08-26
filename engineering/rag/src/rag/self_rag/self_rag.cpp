/**
 * @file self_rag.cpp
 * @brief Self-RAG 和 Corrective-RAG 实现
 */

#include "rag/self_rag.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>

namespace rag {

// ========== ReflectionResult ==========

std::string ReflectionResult::to_string() const {
    std::ostringstream ss;
    ss << "ReflectionResult {\n";
    ss << "  relevant: " << (is_relevant ? "true" : "false") << " (" << relevance_score << ")\n";
    ss << "  supported: " << (is_supported ? "true" : "false") << " (" << support_score << ")\n";
    ss << "  complete: " << (is_complete ? "true" : "false") << " (" << completeness_score << ")\n";
    ss << "  useful: " << (is_useful ? "true" : "false") << " (" << usefulness_score << ")\n";
    ss << "  overall: " << overall_score() << "\n";
    ss << "}";
    return ss.str();
}

ReflectionResult parse_reflection_tokens(const std::string& llm_output) {
    ReflectionResult result;

    // 简单的基于关键词的解析
    std::string lower = llm_output;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 相关性
    if (lower.find("relevant") != std::string::npos ||
        lower.find("相关") != std::string::npos) {
        result.is_relevant = true;
        result.relevance_score = 0.8f;
    } else if (lower.find("not relevant") != std::string::npos ||
               lower.find("不相关") != std::string::npos) {
        result.is_relevant = false;
        result.relevance_score = 0.2f;
    }

    // 支持性
    if (lower.find("supported") != std::string::npos ||
        lower.find("支持") != std::string::npos ||
        lower.find("cite") != std::string::npos) {
        result.is_supported = true;
        result.support_score = 0.8f;
    } else if (lower.find("not supported") != std::string::npos ||
               lower.find("不支持") != std::string::npos) {
        result.is_supported = false;
        result.support_score = 0.2f;
    }

    // 完整性
    if (lower.find("complete") != std::string::npos ||
        lower.find("完整") != std::string::npos) {
        result.is_complete = true;
        result.completeness_score = 0.8f;
    } else if (lower.find("partial") != std::string::npos ||
               lower.find("部分") != std::string::npos) {
        result.is_complete = false;
        result.completeness_score = 0.5f;
    }

    // 有用性
    if (lower.find("useful") != std::string::npos ||
        lower.find("有用") != std::string::npos) {
        result.is_useful = true;
        result.usefulness_score = 0.8f;
    } else if (lower.find("not useful") != std::string::npos ||
               lower.find("无用") != std::string::npos) {
        result.is_useful = false;
        result.usefulness_score = 0.2f;
    }

    // 尝试从数值提取分数
    std::regex score_regex(R"(score[:\s]+(\d+\.?\d*))");
    std::smatch match;
    if (std::regex_search(llm_output, match, score_regex)) {
        try {
            float score = std::stof(match[1].str());
            if (result.relevance_score == 0.0f) {
                result.relevance_score = score;
            }
        } catch (...) {}
    }

    return result;
}

// ========== SelfRAGStage ==========

SelfRAGStage::SelfRAGStage(
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : config_(config), llm_service_(llm_service) {}

StageOutput SelfRAGStage::process(const StageInput& input) {
    auto start = std::chrono::steady_clock::now();

    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    if (input.candidates.empty()) {
        // 无候选项，尝试重新检索
        output.next_action = "retry";
        return output;
    }

    // 1. 评估每个 chunk
    auto evaluations = evaluate_chunks(input.query, input.candidates);

    // 2. 根据阈值过滤
    auto filtered_chunks = filter_by_threshold(input.candidates, evaluations);

    if (filtered_chunks.empty()) {
        // 所有 chunk 都被过滤，需要重检
        output.next_action = "rewrite";
        output.status = StageOutput::Status::NEED_RETRY;
        return output;
    }

    // 3. 判断是否需要重新检索
    if (should_rewrite(evaluations)) {
        output.next_action = "rewrite";
    } else {
        output.next_action = "done";
    }

    // 4. 构建输出
    output.results.clear();
    for (const auto& chunk : filtered_chunks) {
        output.results.push_back({chunk, 1.0f});
    }

    output.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return output;
}

ReflectionResult SelfRAGStage::evaluate_chunk(const std::string& query, const Chunk& chunk) {
    if (config_.evaluation_mode == SelfRAGConfig::EvaluationMode::TOKEN_BASED) {
        // Token 模式需要 LLM 输出中包含反思 token
        // 这里返回默认评估
        ReflectionResult result;
        result.is_relevant = true;
        result.is_supported = true;
        result.is_complete = true;
        result.is_useful = true;
        result.relevance_score = 0.7f;
        result.support_score = 0.7f;
        result.completeness_score = 0.7f;
        result.usefulness_score = 0.7f;
        return result;
    }

    // LLM 评估或混合模式
    return llm_evaluate(query, chunk);
}

std::vector<ReflectionResult> SelfRAGStage::evaluate_chunks(
    const std::string& query,
    const std::vector<Chunk>& chunks) {

    std::vector<ReflectionResult> results;
    results.reserve(chunks.size());

    for (const auto& chunk : chunks) {
        results.push_back(evaluate_chunk(query, chunk));
    }

    return results;
}

bool SelfRAGStage::should_rewrite(const std::vector<ReflectionResult>& evaluations) {
    if (evaluations.empty()) {
        return true;
    }

    // 计算平均得分
    float avg_score = 0.0f;
    int relevant_count = 0;

    for (const auto& eval : evaluations) {
        if (eval.is_relevant) {
            avg_score += eval.overall_score();
            relevant_count++;
        }
    }

    if (relevant_count == 0) {
        return true;
    }

    avg_score /= relevant_count;

    // 如果平均分低于阈值，需要重写
    return avg_score < config_.acceptance_threshold;
}

std::vector<Chunk> SelfRAGStage::filter_by_threshold(
    const std::vector<Chunk>& chunks,
    const std::vector<ReflectionResult>& evaluations) {

    std::vector<Chunk> filtered;

    for (size_t i = 0; i < chunks.size() && i < evaluations.size(); ++i) {
        const auto& eval = evaluations[i];

        // 检查各项阈值
        if (eval.relevance_score >= config_.relevance_threshold &&
            eval.support_score >= config_.support_threshold &&
            eval.usefulness_score >= config_.usefulness_threshold) {
            filtered.push_back(chunks[i]);
        }
    }

    return filtered;
}

ReflectionResult SelfRAGStage::llm_evaluate(
    const std::string& query,
    const Chunk& chunk) {

    if (!llm_service_) {
        // 没有 LLM，返回默认评估
        ReflectionResult result;
        result.is_relevant = true;
        result.relevance_score = 0.5f;
        result.support_score = 0.5f;
        result.completeness_score = 0.5f;
        result.usefulness_score = 0.5f;
        return result;
    }

    try {
        auto prompt = build_evaluation_prompt(query, chunk);
        auto response = llm_service_->generate(prompt);
        return parse_llm_evaluation(response);
    } catch (const std::exception& e) {
        RAG_WARN("LLM evaluation failed: " + std::string(e.what()));
        ReflectionResult result;
        result.relevance_score = 0.5f;
        return result;
    }
}

ReflectionResult SelfRAGStage::token_evaluate(const std::string& llm_output) {
    return parse_reflection_tokens(llm_output);
}

std::string SelfRAGStage::build_evaluation_prompt(
    const std::string& query,
    const Chunk& chunk) {

    std::ostringstream ss;
    ss << R"(
请评估以下检索内容对回答查询的相关性和有用性。

查询: )" << query << R"(

检索内容:
---
)" << chunk.content << R"(
---

请评估以下方面（给出 0-1 的分数）:

1. 相关性 (relevance): 检索内容是否与查询主题相关？
2. 支持性 (support): 检索内容是否提供了可用于回答的信息？
3. 完整性 (completeness): 检索内容是否完整回答了查询？
4. 有用性 (usefulness): 检索内容对最终回答有多大帮助？

请用以下 JSON 格式返回评估结果:
{
  "relevance": 0.0-1.0,
  "support": 0.0-1.0,
  "completeness": 0.0-1.0,
  "usefulness": 0.0-1.0,
  "reasoning": "简短的理由"
}

只返回 JSON，不要其他内容。
)";
    return ss.str();
}

ReflectionResult SelfRAGStage::parse_llm_evaluation(const std::string& response) {
    ReflectionResult result;

    // 解析 JSON 响应
    // 简单实现：正则提取数值
    std::regex rel_regex(R"("relevance"\s*:\s*(\d+\.?\d*))");
    std::regex sup_regex(R"("support"\s*:\s*(\d+\.?\d*))");
    std::regex comp_regex(R"("completeness"\s*:\s*(\d+\.?\d*))");
    std::regex use_regex(R"("usefulness"\s*:\s*(\d+\.?\d*))");

    std::smatch match;

    if (std::regex_search(response, match, rel_regex)) {
        result.relevance_score = std::stof(match[1].str());
        result.is_relevant = result.relevance_score >= config_.relevance_threshold;
    }

    if (std::regex_search(response, match, sup_regex)) {
        result.support_score = std::stof(match[1].str());
        result.is_supported = result.support_score >= config_.support_threshold;
    }

    if (std::regex_search(response, match, comp_regex)) {
        result.completeness_score = std::stof(match[1].str());
        result.is_complete = result.completeness_score >= config_.completeness_threshold;
    }

    if (std::regex_search(response, match, use_regex)) {
        result.usefulness_score = std::stof(match[1].str());
        result.is_useful = result.usefulness_score >= config_.usefulness_threshold;
    }

    return result;
}

void SelfRAGStage::update_config(const SelfRAGConfig& config) {
    config_ = config;
}

// ========== CorrectiveRAG ==========

CorrectiveRAG::CorrectiveRAG(const SelfRAGConfig& config) : config_(config) {}

CorrectiveDecision CorrectiveRAG::decide_action(
    const std::vector<Chunk>& chunks,
    const std::vector<ReflectionResult>& evaluations,
    float avg_score) {

    CorrectiveDecision decision;
    decision.confidence = avg_score;

    if (chunks.empty()) {
        decision.action = CorrectiveAction::WEB_FALLBACK;
        decision.reason = "No chunks retrieved";
        return decision;
    }

    // 计算质量分数
    float quality_score = calculate_quality_score(evaluations);
    decision.confidence = quality_score;

    // 记录历史
    quality_history_.push_back(quality_score);
    if (quality_history_.size() > 100) {
        quality_history_.erase(quality_history_.begin());
    }

    // 决定动作
    decision.action = decide_by_quality(quality_score);

    switch (decision.action) {
        case CorrectiveAction::PASS:
            decision.reason = "Quality sufficient, proceed with generation";
            break;
        case CorrectiveAction::REWRITE:
            decision.reason = "Low relevance, need query rewrite";
            decision.new_query = rewrite_query("", decision.action, chunks);
            break;
        case CorrectiveAction::REPEAT:
            decision.reason = "Incomplete results, retry retrieval";
            break;
        case CorrectiveAction::EXPAND:
            decision.reason = "Need more candidates, expanding retrieval";
            break;
        case CorrectiveAction::WEB_FALLBACK:
            decision.reason = "Quality too low, fallback to web search";
            break;
        case CorrectiveAction::DIRECT_GENERATE:
            decision.reason = "Minimal context, direct generation";
            break;
    }

    return decision;
}

std::string CorrectiveRAG::rewrite_query(
    const std::string& original_query,
    const CorrectiveAction& action,
    const std::vector<Chunk>& chunks) {

    if (action != CorrectiveAction::REWRITE) {
        return original_query;
    }

    // 简单实现：从 chunks 提取关键词重写
    std::set<std::string> keywords;

    for (const auto& chunk : chunks) {
        // 提取前几个词的关键词
        std::istringstream ss(chunk.content);
        std::string word;
        int count = 0;
        while (ss >> word && count < 10) {
            if (word.length() > 2) {
                keywords.insert(word);
                count++;
            }
        }
    }

    // 构建新查询
    std::ostringstream ss;
    ss << original_query;
    for (const auto& kw : keywords) {
        ss << " " << kw;
    }

    return ss.str();
}

bool CorrectiveRAG::should_use_web_fallback(float avg_score, int chunk_count) {
    if (chunk_count == 0) {
        return true;
    }

    // 如果本地检索质量太低，使用 Web 回退
    return avg_score < 0.2f && chunk_count < 3;
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
    if (quality_score > 0.8f) {
        return CorrectiveAction::PASS;
    } else if (quality_score > 0.6f) {
        return CorrectiveAction::PASS;
    } else if (quality_score > 0.4f) {
        return CorrectiveAction::REWRITE;
    } else if (quality_score > 0.2f) {
        return CorrectiveAction::REPEAT;
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
      corrective_(config) {}

PipelineResult SelfRAGPipeline::execute(const std::string& query, int top_k) {
    std::string current_query = query;
    int turn = 0;

    while (turn < config_.max_retrieval_turns) {
        turn++;
        stats_.total_turns++;

        // 执行一轮检索
        auto result = execute_turn(current_query, top_k, turn);

        // 如果成功或达到最大轮次，返回
        if (result.success || turn >= config_.max_retrieval_turns) {
            if (result.success) {
                stats_.successful_turns++;
            }
            return result;
        }

        // 评估结果
        float avg_score = 0.0f;
        if (!result.results.empty()) {
            // 简单计算平均分
            avg_score = 0.6f;
        }

        // 决定修正动作
        std::vector<Chunk> chunks;
        for (const auto& r : result.results) {
            chunks.push_back(r.chunk);
        }

        auto decision = corrective_.decide_action(chunks, {}, avg_score);

        // 处理修正
        auto [action, new_query] = handle_corrective(decision, current_query);

        switch (action) {
            case CorrectiveAction::PASS:
                return result;

            case CorrectiveAction::REWRITE:
                current_query = new_query.empty() ? current_query : new_query;
                stats_.rewrite_count++;
                break;

            case CorrectiveAction::REPEAT:
                stats_.repeat_count++;
                break;

            case CorrectiveAction::EXPAND:
                top_k *= 2;
                break;

            case CorrectiveAction::WEB_FALLBACK:
                stats_.web_fallback_count++;
                // TODO: 调用 Web 搜索
                return result;

            case CorrectiveAction::DIRECT_GENERATE:
                return result;
        }
    }

    // 达到最大轮次
    PipelineResult final_result = base_pipeline_->execute(query, top_k);
    return final_result;
}

std::future<PipelineResult> SelfRAGPipeline::execute_async(const std::string& query, int top_k) {
    return std::async(std::launch::async, [this, query, top_k]() {
        return execute(query, top_k);
    });
}

PipelineResult SelfRAGPipeline::execute_turn(
    const std::string& query,
    int top_k,
    int turn_number) {

    if (config_.verbosity > 0) {
        RAG_INFO("Self-RAG turn " + std::to_string(turn_number) +
                 ": executing with query: " + query);
    }

    return base_pipeline_->execute(query, top_k);
}

std::pair<CorrectiveAction, std::string> SelfRAGPipeline::handle_corrective(
    const CorrectiveDecision& decision,
    const std::string& current_query) {

    CorrectiveAction action = decision.action;
    std::string new_query = decision.new_query;

    // 如果没有新查询，生成一个
    if (action == CorrectiveAction::REWRITE && new_query.empty()) {
        new_query = current_query + " " + decision.reason;
    }

    if (config_.verbosity > 0) {
        RAG_INFO("Corrective action: " + std::to_string(static_cast<int>(action)) +
                 ", reason: " + decision.reason);
    }

    return {action, new_query};
}

// ========== Factory ==========

std::shared_ptr<SelfRAGStage> create_self_rag_stage(
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service) {

    return std::make_shared<SelfRAGStage>(config, llm_service);
}

std::unique_ptr<CorrectiveRAG> create_corrective_rag(const SelfRAGConfig& config) {
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
