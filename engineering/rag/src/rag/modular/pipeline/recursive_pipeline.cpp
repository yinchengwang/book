/**
 * @file recursive_pipeline.cpp
 * @brief RecursivePipeline 实现
 *
 * 流程: Query → 分解为子问题 → 各子问题检索 → 合并答案 → LLM
 */

#include "rag/modular/pipeline/recursive_pipeline.h"
#include "rag/logger.h"
#include <chrono>
#include <algorithm>
#include <cctype>

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

RecursivePipeline::RecursivePipeline()
    : initialized_(false),
      max_depth_(2),
      complexity_threshold_(0.5f),
      min_subqueries_(2),
      max_subqueries_(5) {
}

RecursivePipeline::~RecursivePipeline() = default;

bool RecursivePipeline::init(const ModularConfig& config) {
    RAG_LOG_INFO("初始化 RecursivePipeline...");

    // 保存配置
    config_ = config;

    // 初始化 LLM 服务
    if (!config.llm.model_path.empty()) {
        llm_ = rag::create_llm_service();
        if (llm_) {
            llm_->load(config.llm.model_path, config.llm);
            RAG_LOG_INFO("LLM 模型加载完成: " + config.llm.model_path);
        } else {
            RAG_LOG_WARN("LLM 服务创建失败");
        }
    }

    initialized_ = true;
    RAG_LOG_INFO("RecursivePipeline 初始化完成");
    return true;
}

bool RecursivePipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && (hnsw_retriever_ || bm25_retriever_);
}

ModularQueryResult RecursivePipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_LOG_ERROR(result.error_message);
        return result;
    }

    RAG_LOG_INFO("RecursivePipeline 开始处理查询: " + query.text);

    // Step 1: 评估查询复杂度
    float complexity = evaluate_complexity(query.text);
    stats_.avg_complexity = (stats_.avg_complexity + complexity) / 2;

    RAG_LOG_INFO("查询复杂度: " + std::to_string(complexity));

    // Step 2: 分解查询
    auto decomposition = decompose_query(query.text);

    if (decomposition.needs_decomposition) {
        RAG_LOG_INFO("查询已分解为 " + std::to_string(decomposition.sub_queries.size()) +
                    " 个子问题");
    } else {
        RAG_LOG_INFO("查询不需要分解，直接检索");
    }

    // Step 3: 检索子问题
    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    if (decomposition.needs_decomposition) {
        // 检索每个子问题
        retrieve_sub_queries(decomposition.sub_queries, 0);
        stats_.total_subqueries += static_cast<int>(decomposition.sub_queries.size());

        // 合并结果
        result.context = merge_results(decomposition.sub_queries);
    } else {
        // 直接检索
        result.context = retrieve(query.text, top_k);
    }

    if (result.context.empty()) {
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // Step 4: 构建上下文字符串并生成回答
    std::string context_str = build_context(query.text, result.context);

    // 构建生成提示词
    std::ostringstream prompt;
    prompt << "你是一个知识问答助手。请根据以下上下文信息回答问题。\n\n"
           << "问题: " << query.text << "\n\n";

    if (decomposition.needs_decomposition) {
        prompt << "这个问题包含以下子问题:\n";
        for (const auto& sq : decomposition.sub_queries) {
            prompt << "- " << sq.text << "\n";
        }
        prompt << "\n";
    }

    prompt << "上下文:\n" << context_str << "\n\n"
           << "请生成一个准确、完整的回答。\n\n"
           << "回答:";

    auto gen_start = std::chrono::steady_clock::now();
    GenerateOptions options;
    options.max_tokens = config_.llm.max_tokens;
    options.temperature = config_.llm.temperature;

    result.answer = generate_with_llm(prompt.str(), options);
    result.generation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - gen_start).count();

    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    result.success = true;

    RAG_LOG_INFO("RecursivePipeline 查询完成，子问题数: " +
                std::to_string(decomposition.sub_queries.size()) +
                "，检索结果数: " + std::to_string(result.context.size()) +
                "，生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void RecursivePipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void RecursivePipeline::set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever) {
    bm25_retriever_ = retriever;
}

float RecursivePipeline::evaluate_complexity(const std::string& query) {
    // 简单的复杂度评估
    // 考虑因素：查询长度、关键词数量、问题类型词等

    float complexity = 0.0f;

    // 长度因素（过长或过短都可能复杂）
    size_t len = query.length();
    if (len > 50) complexity += 0.2f;
    if (len > 100) complexity += 0.1f;

    // 多跳问题的关键词
    std::vector<std::string> multi_hop_keywords = {
        "为什么", "怎么", "如何", "区别", "比较",
        "首先", "然后", "最后", "之前", "之后",
        "原因", "结果", "影响", "关系"
    };

    std::string lower_query = to_lower(query);
    int keyword_count = 0;
    for (const auto& kw : multi_hop_keywords) {
        if (lower_query.find(kw) != std::string::npos) {
            keyword_count++;
        }
    }
    complexity += std::min(0.3f, keyword_count * 0.1f);

    // 包含多个问题
    int question_marks = std::count(query.begin(), query.end(), '？');
    question_marks += std::count(query.begin(), query.end(), '?');
    if (question_marks > 1) complexity += 0.2f;

    // 包含对比词
    std::vector<std::string> compare_keywords = {
        "和", "与", "或者", "还是", "哪个", "什么"
    };
    for (const auto& kw : compare_keywords) {
        if (lower_query.find(kw) != std::string::npos) {
            complexity += 0.1f;
            break;
        }
    }

    return std::min(1.0f, complexity);
}

DecompositionResult RecursivePipeline::decompose_query(const std::string& query) {
    DecompositionResult result;
    result.original_query = query;

    // 评估复杂度
    float complexity = evaluate_complexity(query);
    result.is_complex = (complexity >= complexity_threshold_);

    // 如果不复杂，不需要分解
    if (!result.is_complex) {
        result.needs_decomposition = false;
        result.reason = "查询较为简单，不需要分解";
        return result;
    }

    // 调用 LLM 进行分解
    if (!llm_ || !llm_->is_loaded()) {
        RAG_LOG_WARN("LLM 不可用，使用简单分解");
        // 简单的基于规则的分解
        result.needs_decomposition = true;
        result.reason = "使用简单分解（LLM 不可用）";

        SubQuery sq1;
        sq1.id = 1;
        sq1.text = query;
        sq1.parent_query = query;
        result.sub_queries.push_back(sq1);
        return result;
    }

    // 构建分解提示词
    std::string prompt = build_decomposition_prompt(query);

    try {
        GenerateOptions options;
        options.max_tokens = 512;
        options.temperature = 0.5f;

        auto decomp_result = llm_->generate(prompt, options);

        if (decomp_result.finished && !decomp_result.text.empty()) {
            result = parse_decomposition_response(decomp_result.text);
            result.original_query = query;
            return result;
        }
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("查询分解异常: ") + e.what());
    }

    // 分解失败，返回简单分解
    result.needs_decomposition = true;
    result.reason = "分解失败，使用原始查询";

    SubQuery sq;
    sq.id = 1;
    sq.text = query;
    sq.parent_query = query;
    result.sub_queries.push_back(sq);

    return result;
}

void RecursivePipeline::retrieve_sub_queries(std::vector<SubQuery>& sub_queries, int depth) {
    if (depth >= max_depth_) {
        return;
    }

    int top_k = config_.retrieval.top_k;

    for (auto& sq : sub_queries) {
        if (sq.results.empty()) {
            sq.results = retrieve(sq.text, top_k);
            RAG_LOG_INFO("子问题 " + std::to_string(sq.id) +
                        " 检索到 " + std::to_string(sq.results.size()) + " 个结果");
        }

        // 如果子问题检索结果少，尝试进一步分解
        if (sq.results.size() < 2 && depth < max_depth_ - 1) {
            auto sub_decomp = decompose_query(sq.text);
            if (sub_decomp.needs_decomposition && !sub_decomp.sub_queries.empty()) {
                // 递归处理子问题的子问题
                for (auto& sub_sq : sub_decomp.sub_queries) {
                    sub_sq.results = retrieve(sub_sq.text, top_k);
                }
                sq.results = merge_results(sub_decomp.sub_queries);
            }
        }
    }
}

std::vector<rag::RetrievalResult> RecursivePipeline::merge_results(
    const std::vector<SubQuery>& sub_queries) {
    std::vector<rag::RetrievalResult> merged;
    std::unordered_set<std::string> seen_ids;

    for (const auto& sq : sub_queries) {
        for (const auto& r : sq.results) {
            if (seen_ids.find(r.chunk.id) == seen_ids.end()) {
                seen_ids.insert(r.chunk.id);
                merged.push_back(r);
            }
        }
    }

    // 按分数排序
    std::sort(merged.begin(), merged.end(),
              [](const rag::RetrievalResult& a, const rag::RetrievalResult& b) {
                  return a.score > b.score;
              });

    return merged;
}

std::vector<rag::RetrievalResult> RecursivePipeline::retrieve(
    const std::string& query, int top_k) {
    std::vector<rag::RetrievalResult> results;

    // HNSW 检索
    if (hnsw_retriever_) {
        try {
            auto hnsw_results = hnsw_retriever_->retrieve(query, top_k);
            results.insert(results.end(), hnsw_results.begin(), hnsw_results.end());
        } catch (const std::exception& e) {
            RAG_LOG_ERROR(std::string("HNSW 检索异常: ") + e.what());
        }
    }

    // BM25 检索
    if (bm25_retriever_) {
        try {
            auto bm25_results = bm25_retriever_->retrieve(query, top_k);
            results.insert(results.end(), bm25_results.begin(), bm25_results.end());
        } catch (const std::exception& e) {
            RAG_LOG_ERROR(std::string("BM25 检索异常: ") + e.what());
        }
    }

    // 按分数排序
    std::sort(results.begin(), results.end(),
              [](const rag::RetrievalResult& a, const rag::RetrievalResult& b) {
                  return a.score > b.score;
              });

    return results;
}

std::string RecursivePipeline::build_decomposition_prompt(const std::string& query) {
    std::ostringstream oss;
    oss << "请分析以下查询，如果它是一个复杂查询（涉及多个子问题、多跳推理或多方面内容），"
        << "请将其分解为多个简单的子问题。\n\n"
        << "查询: " << query << "\n\n"
        << "分解原则:\n"
        << "1. 每个子问题应该简洁、单一\n"
        << "2. 子问题应该覆盖原查询的所有方面\n"
        << "3. 重要的子问题标记为必需\n\n"
        << "请以以下JSON格式返回分解结果:\n"
        << "{\n"
        << "  \"needs_decomposition\": true或false,\n"
        << "  \"reason\": \"分解原因或判断理由\",\n"
        << "  \"sub_queries\": [\n"
        << "    {\n"
        << "      \"id\": 1,\n"
        << "      \"text\": \"子问题1\",\n"
        << "      \"is_required\": true或false,\n"
        << "      \"importance\": 0.0-1.0\n"
        << "    },\n"
        << "    ...\n"
        << "  ]\n"
        << "}\n\n"
        << "注意:\n"
        << "- 如果查询简单，needs_decomposition 设为 false\n"
        << "- sub_queries 数量控制在 " << min_subqueries_ << "-" << max_subqueries_ << " 个\n"
        << "- 只返回JSON，不要其他内容\n";

    return oss.str();
}

std::string RecursivePipeline::build_subquery_prompt(
    const std::string& sub_query,
    const std::string& context) {
    std::ostringstream oss;
    oss << "请根据以下上下文信息回答子问题。\n\n"
        << "子问题: " << sub_query << "\n\n"
        << "上下文:\n" << context << "\n\n"
        << "回答:";

    return oss.str();
}

DecompositionResult RecursivePipeline::parse_decomposition_response(
    const std::string& response) {
    DecompositionResult result;

    try {
        // 解析 needs_decomposition
        std::regex decomp_re(R"("needs_decomposition"\s*:\s*(true|false))", std::regex::icase);
        auto decomp_it = std::sregex_iterator(response.begin(), response.end(), decomp_re);
        if (decomp_it != std::sregex_iterator()) {
            std::string value = (*decomp_it)[1].str();
            result.needs_decomposition = (to_lower(value) == "true");
        }

        // 解析 reason
        std::regex reason_re(R"("reason"\s*:\s*"([^"]*)")", std::regex::icase);
        auto reason_it = std::sregex_iterator(response.begin(), response.end(), reason_re);
        if (reason_it != std::sregex_iterator()) {
            result.reason = (*reason_it)[1].str();
        }

        // 解析 sub_queries
        // 简单的子问题解析
        std::regex text_re(R"("text"\s*:\s*"([^"]*)")", std::regex::icase);
        auto text_it = std::sregex_iterator(response.begin(), response.end(), text_re);

        int id = 1;
        while (text_it != std::sregex_iterator()) {
            SubQuery sq;
            sq.id = id++;
            sq.text = (*text_it)[1].str();
            sq.parent_query = result.original_query;

            // 尝试解析 is_required
            // 查找当前 text 之后的 is_required
            size_t pos = text_it->position();
            std::string remaining = response.substr(pos);
            std::regex required_re(R"("is_required"\s*:\s*(true|false))", std::regex::icase);
            auto required_it = std::sregex_iterator(remaining.begin(), remaining.end(), required_re);
            if (required_it != std::sregex_iterator()) {
                std::string value = (*required_it)[1].str();
                sq.is_required = (to_lower(value) == "true");
            }

            // 尝试解析 importance
            std::regex importance_re(R"("importance"\s*:\s*([0-9.]+))", std::regex::icase);
            auto importance_it = std::sregex_iterator(remaining.begin(), remaining.end(), importance_re);
            if (importance_it != std::sregex_iterator()) {
                sq.importance = std::stof((*importance_it)[1].str());
            }

            result.sub_queries.push_back(sq);
            ++text_it;
        }

        // 如果没有解析到子问题，创建默认
        if (result.sub_queries.empty()) {
            SubQuery sq;
            sq.id = 1;
            sq.text = result.original_query;
            sq.is_required = true;
            sq.importance = 1.0f;
            result.sub_queries.push_back(sq);
        }

    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("分解结果解析异常: ") + e.what());
        // 解析失败，返回简单结果
        result.needs_decomposition = false;
        result.reason = "解析失败";

        SubQuery sq;
        sq.id = 1;
        sq.text = result.original_query;
        result.sub_queries.push_back(sq);
    }

    return result;
}

}  // namespace rag::modular
