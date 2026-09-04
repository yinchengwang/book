/**
 * @file iterative_pipeline.cpp
 * @brief IterativePipeline 实现
 *
 * 流程: Query → 检索 → LLM评估 → 不满意→改写Query → 再检索 → ... → LLM
 */

#include "rag/modular/pipeline/iterative_pipeline.h"
#include "rag/logger.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <regex>

namespace rag::modular {

namespace {

// 辅助函数：去除字符串首尾空白
std::string trim_string(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// 辅助函数：转小写
std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

}  // anonymous namespace

IterativePipeline::IterativePipeline()
    : initialized_(false),
      max_iterations_(3),
      sufficiency_threshold_(0.6f) {
}

IterativePipeline::~IterativePipeline() = default;

bool IterativePipeline::init(const ModularConfig& config) {
    RAG_INFO("初始化 IterativePipeline...");

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

    initialized_ = true;
    RAG_INFO("IterativePipeline 初始化完成");
    return true;
}

bool IterativePipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && (hnsw_retriever_ || bm25_retriever_);
}

ModularQueryResult IterativePipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_ERROR(result.error_message);
        return result;
    }

    std::string current_query = query.text;
    std::vector<rag::RetrievalResult> all_results;
    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;
    int64_t total_retrieval_time = 0;
    int64_t total_evaluation_time = 0;

    RAG_INFO("IterativePipeline 开始处理查询: " + query.text);

    // 迭代循环
    for (int iteration = 0; iteration < max_iterations_; ++iteration) {
        stats_.total_iterations++;

        RAG_INFO("IterativePipeline 迭代 " + std::to_string(iteration + 1) +
                    "，查询: " + current_query);

        // Step 1: 执行检索
        auto retrieval_start = std::chrono::steady_clock::now();
        auto retrieval_results = retrieve(current_query, top_k);
        int64_t retrieval_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - retrieval_start).count();
        total_retrieval_time += retrieval_time;

        if (retrieval_results.empty() && iteration == 0) {
            // 首次检索为空，直接返回
            result.success = true;
            result.answer = "抱歉，未找到与您查询相关的文档内容。";
            result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            return result;
        }

        // 合并结果（去重）
        for (const auto& r : retrieval_results) {
            bool found = false;
            for (const auto& existing : all_results) {
                if (existing.chunk.id == r.chunk.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                all_results.push_back(r);
            }
        }

        // Step 2: 评估检索结果是否充分
        auto eval_start = std::chrono::steady_clock::now();
        auto evaluation = evaluate_sufficiency(query.text, all_results);
        int64_t eval_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - eval_start).count();
        total_evaluation_time += eval_time;

        // 更新平均充分性分数
        stats_.avg_sufficiency_score = (stats_.avg_sufficiency_score * iteration +
                                        evaluation.sufficiency_score) / (iteration + 1);

        RAG_INFO("充分性评估 - 分数: " + std::to_string(evaluation.sufficiency_score) +
                    "，足够: " + (evaluation.is_sufficient ? "是" : "否") +
                    "，原因: " + evaluation.reason);

        // Step 3: 判断是否满足
        if (evaluation.is_sufficient || iteration == max_iterations_ - 1) {
            result.context = all_results;
            break;
        }

        // Step 4: 改写查询
        if (!evaluation.suggested_improvement.empty()) {
            std::string new_query = rewrite_query(query.text, current_query, evaluation);
            if (!new_query.empty() && new_query != current_query) {
                current_query = new_query;
                stats_.rewrite_count++;
                RAG_INFO("查询已改写: " + current_query);
            }
        }
    }

    // 构建最终回答
    std::string context_str = build_context(query.text, result.context);

    if (context_str.empty()) {
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // 生成回答
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

    RAG_INFO("IterativePipeline 查询完成，迭代: " + std::to_string(stats_.total_iterations) +
                "，改写次数: " + std::to_string(stats_.rewrite_count) +
                "，检索: " + std::to_string(result.retrieval_time_ms) +
                "ms，生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void IterativePipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void IterativePipeline::set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever) {
    bm25_retriever_ = retriever;
}

std::vector<rag::RetrievalResult> IterativePipeline::retrieve(
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

    // 按分数排序
    std::sort(results.begin(), results.end(),
              [](const rag::RetrievalResult& a, const rag::RetrievalResult& b) {
                  return a.score > b.score;
              });

    return results;
}

IterativeEvaluation IterativePipeline::evaluate_sufficiency(
    const std::string& query,
    const std::vector<rag::RetrievalResult>& results) {
    if (results.empty()) {
        IterativeEvaluation empty_eval;
        empty_eval.is_sufficient = false;
        empty_eval.sufficiency_score = 0.0f;
        empty_eval.reason = "没有检索到任何文档";
        return empty_eval;
    }

    // 构建上下文字符串
    std::string context = build_context(query, results);

    // 构建评估提示词
    std::string prompt = build_evaluation_prompt(query, context);

    // 调用 LLM 进行评估
    if (!llm_ || !llm_->is_loaded()) {
        RAG_ERROR("LLM 服务不可用");
        IterativeEvaluation error_eval;
        return error_eval;
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
        RAG_ERROR(std::string("充分性评估异常: ") + e.what());
    }

    IterativeEvaluation fallback;
    return fallback;
}

std::string IterativePipeline::rewrite_query(
    const std::string& original_query,
    const std::string& current_query,
    const IterativeEvaluation& evaluation) {
    // 构建改写提示词
    std::string prompt = build_rewrite_prompt(original_query, current_query, evaluation);

    if (!llm_ || !llm_->is_loaded()) {
        RAG_ERROR("LLM 服务不可用，无法改写查询");
        return current_query;
    }

    try {
        GenerateOptions options;
        options.max_tokens = 128;
        options.temperature = 0.7f;

        auto result = llm_->generate(prompt, options);

        if (result.finished && !result.text.empty()) {
            std::string new_query = trim_string(result.text);
            if (!new_query.empty()) {
                RAG_INFO("查询改写成功: " + current_query + " -> " + new_query);
                return new_query;
            }
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("查询改写异常: ") + e.what());
    }

    return current_query;
}

std::string IterativePipeline::build_evaluation_prompt(
    const std::string& query,
    const std::string& context) {
    std::ostringstream oss;
    oss << "请评估以下检索到的上下文是否足以回答给定的问题。\n\n"
        << "问题: " << query << "\n\n"
        << "检索到的上下文:\n" << context << "\n\n"
        << "请评估（0-1分，1分表示完全足够）:\n"
        << "1. 充分性：上下文是否足够完整地回答问题？\n"
        << "2. 相关性：上下文是否与问题主题相关？\n"
        << "3. 准确性：上下文是否提供准确的信息？\n\n"
        << "请以以下JSON格式返回评估结果（只返回JSON，不要其他内容）:\n"
        << "{\n"
        << "  \"sufficient\": true或false,\n"
        << "  \"score\": 0.0-1.0,\n"
        << "  \"reason\": \"判断理由\",\n"
        << "  \"suggestion\": \"如果不够，如何改进检索（可选）\"\n"
        << "}\n";

    return oss.str();
}

std::string IterativePipeline::build_rewrite_prompt(
    const std::string& original_query,
    const std::string& current_query,
    const IterativeEvaluation& evaluation) {
    std::ostringstream oss;
    oss << "请根据以下信息改写查询以获得更好的检索结果。\n\n"
        << "原始查询: " << original_query << "\n\n"
        << "当前查询: " << current_query << "\n\n"
        << "评估结果:\n"
        << "- 充分性分数: " << evaluation.sufficiency_score << "\n"
        << "- 判断理由: " << evaluation.reason << "\n";

    if (!evaluation.suggested_improvement.empty()) {
        oss << "- 改进建议: " << evaluation.suggested_improvement << "\n";
    }

    oss << "\n请生成一个改进后的查询，使其更容易找到相关文档。\n"
        << "可以尝试:\n"
        << "1. 使用更精确的术语或同义词\n"
        << "2. 扩展或缩小查询范围\n"
        << "3. 改变查询的表达方式\n\n"
        << "改进后的查询（只返回查询文本，不要其他内容）:\n";

    return oss.str();
}

IterativeEvaluation IterativePipeline::parse_evaluation_response(
    const std::string& response) {
    IterativeEvaluation result;

    try {
        // 简单的 JSON 解析
        // 尝试提取 sufficient 字段
        std::regex sufficient_re(R"("sufficient"\s*:\s*(true|false))", std::regex::icase);
        auto sufficient_it = std::sregex_iterator(response.begin(), response.end(), sufficient_re);
        if (sufficient_it != std::sregex_iterator()) {
            std::string value = (*sufficient_it)[1].str();
            result.is_sufficient = (to_lower(value) == "true");
        }

        // 尝试提取 score 字段
        std::regex score_re(R"("score"\s*:\s*([0-9.]+))", std::regex::icase);
        auto score_it = std::sregex_iterator(response.begin(), response.end(), score_re);
        if (score_it != std::sregex_iterator()) {
            result.sufficiency_score = std::stof((*score_it)[1].str());
        }

        // 尝试提取 reason 字段
        std::regex reason_re(R"RX("reason"\s*:\s*"([^"]*)")RX", std::regex::icase);
        auto reason_it = std::sregex_iterator(response.begin(), response.end(), reason_re);
        if (reason_it != std::sregex_iterator()) {
            result.reason = (*reason_it)[1].str();
        }

        // 尝试提取 suggestion 字段
        std::regex suggestion_re(R"RX("suggestion"\s*:\s*"([^"]*)")RX", std::regex::icase);
        auto suggestion_it = std::sregex_iterator(response.begin(), response.end(), suggestion_re);
        if (suggestion_it != std::sregex_iterator()) {
            result.suggested_improvement = (*suggestion_it)[1].str();
        }

        // 如果没有解析到分数，使用默认值
        if (result.sufficiency_score == 0.0f) {
            result.sufficiency_score = result.is_sufficient ? 0.7f : 0.3f;
        }

        // 如果分数超过阈值，标记为足够
        if (result.sufficiency_score >= sufficiency_threshold_) {
            result.is_sufficient = true;
        }

    } catch (const std::exception& e) {
        RAG_ERROR(std::string("评估结果解析异常: ") + e.what());
        // 解析失败时返回保守估计
        result.is_sufficient = false;
        result.sufficiency_score = 0.3f;
        result.reason = "解析评估结果失败";
    }

    return result;
}

}  // namespace rag::modular
