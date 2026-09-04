/**
 * @file hyde_pipeline.cpp
 * @brief HyDEPipeline 实现
 *
 * 流程: Query → LLM生成假设答案 → 用假设答案检索 → LLM最终生成
 */

#include "rag/modular/pipeline/hyde_pipeline.h"
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

// 辅助函数：按字符分割字符串
std::vector<std::string> split_by_char(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

// 辅助函数：按字符串分割
std::vector<std::string> split_by_string(const std::string& s, const std::string& delim) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t pos = s.find(delim);
    while (pos != std::string::npos) {
        result.push_back(s.substr(start, pos - start));
        start = pos + delim.length();
        pos = s.find(delim, start);
    }
    result.push_back(s.substr(start));
    return result;
}

}  // anonymous namespace

HyDEPipeline::HyDEPipeline()
    : initialized_(false),
      multi_hypothesis_mode_(false),
      hypothesis_count_(3) {}

HyDEPipeline::~HyDEPipeline() = default;

bool HyDEPipeline::init(const ModularConfig& config) {
    RAG_LOG_INFO("初始化 HyDEPipeline...");

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
    RAG_LOG_INFO("HyDEPipeline 初始化完成");
    return true;
}

bool HyDEPipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded() && hnsw_retriever_;
}

ModularQueryResult HyDEPipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_LOG_ERROR(result.error_message);
        return result;
    }

    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    // Step 1: 使用 LLM 生成假设答案
    auto hyde_start = std::chrono::steady_clock::now();
    auto hypotheses = generate_hypothetical_documents(query.text);
    int64_t hyde_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - hyde_start).count();

    if (hypotheses.empty()) {
        RAG_LOG_WARN("HyDE 生成假设答案失败，使用原始查询检索");
        hypotheses = {query.text};
    }

    RAG_LOG_INFO("HyDE 生成 " + std::to_string(hypotheses.size()) +
                " 个假设答案，耗时: " + std::to_string(hyde_time_ms) + "ms");

    // Step 2: 使用假设答案执行检索
    auto retrieval_start = std::chrono::steady_clock::now();
    auto retrieval_results = retrieve_with_hypotheses(hypotheses, top_k);
    result.retrieval_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - retrieval_start).count();

    if (retrieval_results.empty()) {
        RAG_LOG_WARN("假设答案检索结果为空");
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // 保存检索结果
    result.context = retrieval_results;

    // Step 3: 构建上下文字符串
    std::string context_str = build_context(query.text, retrieval_results);

    // Step 4: 构建提示词并生成回答
    // 将假设答案也包含在上下文中，帮助 LLM 理解检索意图
    std::string hyde_context = "【假设答案参考】\n";
    for (size_t i = 0; i < hypotheses.size(); ++i) {
        hyde_context += "假设答案 " + std::to_string(i + 1) + ": " + hypotheses[i] + "\n\n";
    }

    std::string prompt = "请根据以下上下文信息回答问题。如果上下文中没有相关信息，请说明无法回答。\n\n"
                       "问题: " + query.text + "\n\n"
                       + hyde_context +
                       "【真实检索到的上下文】\n" + context_str + "\n\n"
                       "回答:";

    auto gen_start = std::chrono::steady_clock::now();
    GenerateOptions options;
    options.max_tokens = config_.llm.max_tokens;
    options.temperature = config_.llm.temperature;

    result.answer = generate_with_llm(prompt, options);
    result.generation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - gen_start).count();

    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    result.success = true;

    RAG_LOG_INFO("HyDEPipeline 查询完成，HyDE: " + std::to_string(hyde_time_ms) +
                "ms, 检索: " + std::to_string(result.retrieval_time_ms) +
                "ms, 生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void HyDEPipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void HyDEPipeline::set_multi_hypothesis_mode(bool enable, int count) {
    multi_hypothesis_mode_ = enable;
    hypothesis_count_ = (count > 0) ? count : 3;
}

std::vector<std::string> HyDEPipeline::generate_hypothetical_documents(
    const std::string& query) {
    if (!llm_ || !llm_->is_loaded()) {
        RAG_LOG_ERROR("LLM 服务不可用");
        return {};
    }

    try {
        std::string prompt;
        if (multi_hypothesis_mode_) {
            prompt = build_multi_hypothesis_prompt(query, hypothesis_count_);
        } else {
            prompt = build_hyde_prompt(query);
        }

        GenerateOptions options;
        options.max_tokens = 512;
        options.temperature = 0.7f;

        auto gen_result = llm_->generate(prompt, options);

        if (gen_result.finished && !gen_result.text.empty()) {
            return parse_hypothetical_documents(gen_result.text);
        }

        RAG_LOG_WARN("HyDE LLM 生成未正常完成");
        return {};
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("HyDE 生成假设答案异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> HyDEPipeline::retrieve_with_hypotheses(
    const std::vector<std::string>& hypotheses,
    int top_k) {
    if (hypotheses.empty()) {
        return {};
    }

    // 如果只有一个假设答案，直接检索
    if (hypotheses.size() == 1) {
        return retrieve_with_single_hypothesis(hypotheses[0], top_k);
    }

    // 多假设答案模式：分别检索并合并结果
    std::vector<rag::RetrievalResult> all_results;

    for (const auto& hypothesis : hypotheses) {
        auto results = retrieve_with_single_hypothesis(hypothesis, top_k);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    // 按分数排序并去重
    std::sort(all_results.begin(), all_results.end(),
              [](const rag::RetrievalResult& a, const rag::RetrievalResult& b) {
                  return a.score > b.score;
              });

    // 去重（基于 chunk_id）
    std::vector<rag::RetrievalResult> unique_results;
    std::unordered_set<std::string> seen_ids;
    for (const auto& result : all_results) {
        if (seen_ids.find(result.chunk.id) == seen_ids.end()) {
            seen_ids.insert(result.chunk.id);
            unique_results.push_back(result);
            if (static_cast<int>(unique_results.size()) >= top_k) {
                break;
            }
        }
    }

    return unique_results;
}

std::vector<rag::RetrievalResult> HyDEPipeline::retrieve_with_single_hypothesis(
    const std::string& hypothesis,
    int top_k) {
    if (!hnsw_retriever_) {
        RAG_LOG_ERROR("HNSW 检索器未设置");
        return {};
    }

    try {
        // 使用假设答案作为查询向量进行检索
        auto results = hnsw_retriever_->retrieve(hypothesis, top_k);
        RAG_LOG_DEBUG("假设答案检索到 " + std::to_string(results.size()) + " 个结果");
        return results;
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("假设答案检索异常: ") + e.what());
        return {};
    }
}

std::string HyDEPipeline::build_hyde_prompt(const std::string& query) {
    // 构建生成单个假设答案的提示词
    // 指导 LLM 生成一个假设性的"完美答案"，这个答案应该包含
    // 能够回答查询的相关信息，并尽量模拟真实文档的风格
    std::ostringstream oss;
    oss << "请根据以下问题，生成一个假设性的文档片段。\n"
        << "这个文档片段应该是一个假想的、能够回答该问题的完美答案。\n"
        << "它应该包含你认为是回答该问题所需的相关信息和上下文。\n"
        << "答案长度适中(100-300字)，语言自然流畅。\n\n"
        << "问题: " << query << "\n\n"
        << "假设性文档:\n";

    return oss.str();
}

std::string HyDEPipeline::build_multi_hypothesis_prompt(
    const std::string& query, int count) {
    // 构建生成多个假设答案的提示词
    // 不同的假设答案从不同角度或风格来回答问题
    std::ostringstream oss;
    oss << "请根据以下问题，生成 " << count << " 个不同的假设性文档片段。\n"
        << "每个文档片段应该从不同角度或风格来回答该问题。\n"
        << "它们可以是不同详细程度、不同关注点或不同表达风格的答案。\n"
        << "每个答案长度适中(100-200字)。\n"
        << "请用数字分隔每个答案，如: 【答案1】... 【答案2】...\n\n"
        << "问题: " << query << "\n\n"
        << "假设性文档:\n";

    return oss.str();
}

std::vector<std::string> HyDEPipeline::parse_hypothetical_documents(
    const std::string& response) {
    std::vector<std::string> documents;

    if (response.empty()) {
        return documents;
    }

    std::string trimmed_response = trim_string(response);

    // 尝试多种分割方式解析假设答案

    // 方式1: 按 【答案N】 或 【Answer N】 分割
    std::regex answer_pattern(R"(\【答案\s*(\d+)\】|\【Answer\s*(\d+)\】|\[答案(\d+)\]|\[Answer(\d+)\])");
    std::sregex_iterator it(trimmed_response.begin(), trimmed_response.end(), answer_pattern);
    std::sregex_iterator end;

    if (it != end) {
        // 找到了分割标记，按标记分割
        std::vector<std::pair<size_t, std::string>> parts;
        size_t last_pos = 0;

        for (; it != end; ++it) {
            size_t pos = it->position();
            std::string match = it->str();

            // 提取分割标记后的内容
            size_t content_start = pos + match.length();
            size_t next_pos = (++it == end) ? trimmed_response.length() : it->position();
            --it;  // 恢复迭代器

            std::string content = trimmed_response.substr(content_start, next_pos - content_start);
            content = trim_string(content);
            if (!content.empty()) {
                parts.emplace_back(pos, content);
            }
            last_pos = next_pos;
        }

        // 处理最后一个答案
        if (last_pos < trimmed_response.length()) {
            std::string last_content = trim_string(trimmed_response.substr(last_pos));
            if (!last_content.empty()) {
                parts.emplace_back(last_pos, last_content);
            }
        }

        for (const auto& part : parts) {
            documents.push_back(trim_string(part.second));
        }

        if (!documents.empty()) {
            return documents;
        }
    }

    // 方式2: 按 数字+. 分割 (如 "1. ... 2. ... 3. ...")
    std::regex num_pattern(R"(^\d+\.\s*)");
    std::vector<std::string> lines = split_by_char(trimmed_response, '\n');
    std::string current_doc;

    for (const auto& line : lines) {
        std::string line_trimmed = trim_string(line);

        // 检查是否是新答案的开始（数字开头）
        if (std::regex_search(line_trimmed, num_pattern)) {
            if (!current_doc.empty()) {
                documents.push_back(trim_string(current_doc));
                current_doc.clear();
            }
            // 移除数字前缀
            current_doc = std::regex_replace(line_trimmed, num_pattern, "");
        } else {
            if (!current_doc.empty() || !line_trimmed.empty()) {
                current_doc += " " + line_trimmed;
            }
        }
    }

    if (!current_doc.empty()) {
        documents.push_back(trim_string(current_doc));
    }

    if (!documents.empty()) {
        return documents;
    }

    // 方式3: 尝试按空行分割
    std::vector<std::string> paragraphs = split_by_string(trimmed_response, "\n\n");
    for (const auto& para : paragraphs) {
        std::string para_trimmed = trim_string(para);
        if (para_trimmed.length() > 20) {  // 过滤太短的段落
            documents.push_back(para_trimmed);
        }
    }

    if (!documents.empty()) {
        return documents;
    }

    // 方式4: 如果都失败，返回整个响应作为一个假设答案
    if (!trimmed_response.empty()) {
        documents.push_back(trimmed_response);
    }

    return documents;
}

}  // namespace rag::modular
