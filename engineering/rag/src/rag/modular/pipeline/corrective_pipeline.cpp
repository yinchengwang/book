/**
 * @file corrective_pipeline.cpp
 * @brief CorrectivePipeline 实现
 *
 * 流程: Query → 检索 → LLM判断质量 → 质量差→纠正/重检 → LLM
 */

#include "rag/modular/pipeline/corrective_pipeline.h"
#include "rag/logger.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>

namespace rag::modular {

namespace {

// 辅助函数：去除字符串首尾空白
std::string trim_string(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

}  // anonymous namespace

CorrectivePipeline::CorrectivePipeline()
    : initialized_(false),
      max_iterations_(3),
      quality_threshold_(0.5f) {
    // 初始化 Self-RAG 配置
    self_rag_config_.enable_self_check = true;
    self_rag_config_.max_retrieval_turns = max_iterations_;
    self_rag_config_.use_llm_evaluation = true;
    self_rag_config_.evaluation_mode = rag::SelfRAGConfig::EvaluationMode::LLM_JUDGE;
}

CorrectivePipeline::~CorrectivePipeline() = default;

bool CorrectivePipeline::init(const ModularConfig& config) {
    RAG_INFO("初始化 CorrectivePipeline...");

    // 保存配置
    config_ = config;

    // 初始化 LLM 服务
    if (!config.llm.model_path.empty()) {
        llm_ = rag::create_llm_service();
        if (llm_) {
            llm_->load(config.llm.model_path, config.llm);
            RAG_INFO("LLM 模型加载完成: " + config.llm.model_path);
        } else {
            RAG_WARN("LLM 服务创建失败");
        }
    }

    // 初始化 CorrectiveRAG
    corrective_rag_ = rag::create_corrective_rag(self_rag_config_);

    initialized_ = true;
    RAG_INFO("CorrectivePipeline 初始化完成");
    return true;
}

bool CorrectivePipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && (hnsw_retriever_ || bm25_retriever_);
}

ModularQueryResult CorrectivePipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_ERROR(result.error_message);
        return result;
    }

    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    std::string current_query = query.text;
    std::vector<rag::RetrievalResult> all_retrieval_results;
    int64_t total_retrieval_time = 0;
    int64_t total_evaluation_time = 0;

    // 迭代纠正循环
    for (int iteration = 0; iteration < max_iterations_; ++iteration) {
        stats_.total_iterations++;

        RAG_INFO("CorrectivePipeline 迭代 " + std::to_string(iteration + 1) +
                    "，查询: " + current_query);

        // Step 1: 执行检索
        auto retrieval_start = std::chrono::steady_clock::now();
        auto retrieval_results = retrieve(current_query, top_k);
        int64_t retrieval_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - retrieval_start).count();
        total_retrieval_time += retrieval_time;

        if (!retrieval_results.empty()) {
            all_retrieval_results.insert(all_retrieval_results.end(),
                                        retrieval_results.begin(),
                                        retrieval_results.end());
        }

        if (retrieval_results.empty() && iteration == 0) {
            // 首次检索为空，直接返回
            result.success = true;
            result.answer = "抱歉，未找到与您查询相关的文档内容。";
            result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            return result;
        }

        // Step 2: 评估检索结果质量
        auto eval_start = std::chrono::steady_clock::now();
        auto evaluation = evaluate_retrieval_quality(current_query, retrieval_results);
        int64_t eval_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - eval_start).count();
        total_evaluation_time += eval_time;

        // 更新平均质量分数
        stats_.avg_quality_score = (stats_.avg_quality_score * iteration +
                                   evaluation.overall_score()) / (iteration + 1);

        RAG_INFO("质量评估完成 - 相关性: " + std::to_string(evaluation.relevance_score) +
                    ", 支持性: " + std::to_string(evaluation.support_score) +
                    ", 完整性: " + std::to_string(evaluation.completeness_score) +
                    ", 有用性: " + std::to_string(evaluation.usefulness_score));

        // Step 3: 判断是否需要纠正
        if (iteration > 0 || !needs_correction(evaluation)) {
            // 质量足够好或已达到最大迭代，使用当前结果
            RAG_INFO("检索质量可接受或达到最大迭代，停止纠正");
            result.context = retrieval_results;
            break;
        }

        // Step 4: 执行纠正
        RAG_INFO("检索质量不足，执行纠正...");
        stats_.corrections_performed++;

        std::string new_query = perform_correction(current_query, evaluation, iteration);
        if (new_query == current_query) {
            RAG_WARN("纠正未产生新查询，停止迭代");
            result.context = retrieval_results;
            break;
        }

        current_query = new_query;
    }

    // 去重并整理最终结果
    std::sort(result.context.begin(), result.context.end(),
              [](const rag::RetrievalResult& a, const rag::RetrievalResult& b) {
                  return a.score > b.score;
              });

    std::vector<rag::RetrievalResult> unique_results;
    std::unordered_set<std::string> seen_ids;
    for (const auto& r : result.context) {
        if (seen_ids.find(r.chunk.id) == seen_ids.end()) {
            seen_ids.insert(r.chunk.id);
            unique_results.push_back(r);
        }
    }
    result.context = unique_results;

    // Step 5: 构建上下文字符串并生成回答
    std::string context_str = build_context(query.text, result.context);

    if (context_str.empty()) {
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    std::string prompt = "请根据以下上下文信息回答问题。如果上下文中没有相关信息，请说明无法回答。\n\n"
                       "问题: " + query.text + "\n\n"
                       "上下文:\n" + context_str + "\n\n"
                       "回答:";

    auto gen_start = std::chrono::steady_clock::now();
    GenerateOptions options;
    options.max_tokens = config_.llm.max_tokens;
    options.temperature = config_.llm.temperature;

    result.answer = generate_with_llm(prompt, options);
    result.generation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - gen_start).count();

    result.retrieval_time_ms = total_retrieval_time;
    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    result.success = true;

    RAG_INFO("CorrectivePipeline 查询完成，迭代: " + std::to_string(stats_.total_iterations) +
                "，纠正次数: " + std::to_string(stats_.corrections_performed) +
                "，检索: " + std::to_string(result.retrieval_time_ms) +
                "ms，生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void CorrectivePipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void CorrectivePipeline::set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever) {
    bm25_retriever_ = retriever;
}

std::vector<rag::RetrievalResult> CorrectivePipeline::retrieve(
    const std::string& query, int top_k) {
    std::vector<rag::RetrievalResult> results;

    // HNSW 检索
    if (hnsw_retriever_) {
        try {
            auto hnsw_results = hnsw_retriever_->retrieve(query, top_k);
            results.insert(results.end(), hnsw_results.begin(), hnsw_results.end());
        } catch (const std::exception& e) {
            RAG_ERROR(std::string("HNSW 检索异常: ") + e.what());
        }
    }

    // BM25 检索
    if (bm25_retriever_) {
        try {
            auto bm25_results = bm25_retriever_->retrieve(query, top_k);
            results.insert(results.end(), bm25_results.begin(), bm25_results.end());
        } catch (const std::exception& e) {
            RAG_ERROR(std::string("BM25 检索异常: ") + e.what());
        }
    }

    return results;
}

rag::ReflectionResult CorrectivePipeline::evaluate_retrieval_quality(
    const std::string& query,
    const std::vector<rag::RetrievalResult>& results) {
    if (results.empty()) {
        rag::ReflectionResult empty_result;
        return empty_result;
    }

    // 构建上下文字符串
    std::string context = build_context(query, results);

    // 构建评估提示词
    std::string prompt = build_evaluation_prompt(query, context);

    // 调用 LLM 进行评估
    if (!llm_ || !llm_->is_loaded()) {
        RAG_ERROR("LLM 服务不可用");
        rag::ReflectionResult error_result;
        return error_result;
    }

    try {
        GenerateOptions options;
        options.max_tokens = 256;
        options.temperature = 0.1f;  // 低温度以获得更一致的评估

        auto eval_result = llm_->generate(prompt, options);

        if (eval_result.finished && !eval_result.text.empty()) {
            return parse_evaluation_response(eval_result.text);
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("质量评估异常: ") + e.what());
    }

    rag::ReflectionResult fallback_result;
    return fallback_result;
}

bool CorrectivePipeline::needs_correction(const rag::ReflectionResult& evaluation) {
    // 如果评估分数低于阈值，需要纠正
    return evaluation.overall_score() < quality_threshold_;
}

std::string CorrectivePipeline::perform_correction(
    const std::string& query,
    const rag::ReflectionResult& evaluation,
    int iteration) {
    // 构建纠正提示词
    std::string prompt = build_correction_prompt(query, evaluation);

    if (!llm_ || !llm_->is_loaded()) {
        RAG_ERROR("LLM 服务不可用，无法执行纠正");
        return query;
    }

    try {
        GenerateOptions options;
        options.max_tokens = 256;
        options.temperature = 0.7f;

        auto result = llm_->generate(prompt, options);

        if (result.finished && !result.text.empty()) {
            std::string new_query = trim_string(result.text);
            if (!new_query.empty() && new_query != query) {
                RAG_INFO("纠正成功: " + query + " -> " + new_query);
                return new_query;
            }
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("纠正执行异常: ") + e.what());
    }

    return query;
}

std::string CorrectivePipeline::build_correction_prompt(
    const std::string& query,
    const rag::ReflectionResult& evaluation) {
    std::ostringstream oss;
    oss << "请分析以下查询的检索问题，并生成一个改进的查询。\n\n"
        << "原始查询: " << query << "\n\n"
        << "检索质量评估:\n"
        << "- 相关性: " << (evaluation.is_relevant ? "相关" : "不相关")
        << " (得分: " << evaluation.relevance_score << ")\n"
        << "- 支持性: " << (evaluation.is_supported ? "支持" : "不支持")
        << " (得分: " << evaluation.support_score << ")\n"
        << "- 完整性: " << (evaluation.is_complete ? "完整" : "不完整")
        << " (得分: " << evaluation.completeness_score << ")\n"
        << "- 有用性: " << (evaluation.is_useful ? "有用" : "无用")
        << " (得分: " << evaluation.usefulness_score << ")\n\n"
        << "请根据以上评估结果，生成一个改进后的查询。\n"
        << "改进策略可以包括:\n"
        << "1. 如果相关性低：使用更精确的术语或同义词\n"
        << "2. 如果完整性低：扩展查询范围或分解问题\n"
        << "3. 如果支持性低：尝试不同的表达角度\n\n"
        << "改进后的查询(只返回查询文本，不要其他内容):\n";

    return oss.str();
}

std::string CorrectivePipeline::build_evaluation_prompt(
    const std::string& query,
    const std::string& context) {
    std::ostringstream oss;
    oss << "请评估以下检索到的上下文对于回答查询的质量。\n\n"
        << "查询: " << query << "\n\n"
        << "检索到的上下文:\n" << context << "\n\n"
        << "请从以下四个维度评估（每个维度0-1分）:\n"
        << "1. 相关性(RELEVANCE): 上下文与查询主题的相关程度\n"
        << "2. 支持性(SUPPORT): 上下文能否支持生成一个准确的回答\n"
        << "3. 完整性(COMPLETENESS): 上下文是否完整回答了查询\n"
        << "4. 有用性(USEFULNESS): 上下文对于最终回答的有用程度\n\n"
        << "请以以下JSON格式返回评估结果（只返回JSON，不要其他内容）:\n"
        << "{\n"
        << "  \"relevance\": 0.0-1.0,\n"
        << "  \"support\": 0.0-1.0,\n"
        << "  \"completeness\": 0.0-1.0,\n"
        << "  \"usefulness\": 0.0-1.0\n"
        << "}\n";

    return oss.str();
}

rag::ReflectionResult CorrectivePipeline::parse_evaluation_response(
    const std::string& response) {
    rag::ReflectionResult result;

    try {
        // 简单的 JSON 解析
        std::regex re(R"RX("(\w+)":\s*([0-9.]+))RX");
        std::sregex_iterator it(response.begin(), response.end(), re);
        std::sregex_iterator end;

        while (it != end) {
            std::string key = (*it)[1].str();
            float value = std::stof((*it)[2].str());

            if (key == "relevance") {
                result.relevance_score = value;
                result.is_relevant = value > 0.5f;
            } else if (key == "support") {
                result.support_score = value;
                result.is_supported = value > 0.3f;
            } else if (key == "completeness") {
                result.completeness_score = value;
                result.is_complete = value > 0.3f;
            } else if (key == "usefulness") {
                result.usefulness_score = value;
                result.is_useful = value > 0.5f;
            }

            ++it;
        }

        // 如果解析失败，尝试简单的关键词匹配
        if (result.overall_score() == 0.0f) {
            std::string lower_response = response;
            std::transform(lower_response.begin(), lower_response.end(),
                         lower_response.begin(), ::tolower);
            if (lower_response.find("relevant") != std::string::npos ||
                lower_response.find("相关") != std::string::npos) {
                result.is_relevant = true;
                result.relevance_score = 0.7f;
            }
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("评估结果解析异常: ") + e.what());
    }

    return result;
}

}  // namespace rag::modular
