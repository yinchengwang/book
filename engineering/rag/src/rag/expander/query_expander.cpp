/**
 * @file query_expander.cpp
 * @brief QueryExpander 基类实现和工厂函数
 */

#include "rag/query_expander.h"
#include "rag/pipeline.h"
#include <chrono>
#include <stdexcept>

namespace rag {

// ========== QueryExpander 基类 ==========

ExpansionResult QueryExpander::expand(const std::string& query) {
    ExpansionResult result;
    result.original_query = query;
    return result;
}

// ========== HyDEExpander 实现 ==========

HyDEExpander::HyDEExpander(int max_hypotheses)
    : max_hypotheses_(max_hypotheses), llm_service_(nullptr) {
}

ExpansionResult HyDEExpander::expand(const std::string& query) {
    auto start = std::chrono::high_resolution_clock::now();

    ExpansionResult result;
    result.original_query = query;
    result.method = "hyde";

    // 生成假设性答案
    auto hypotheses = generate_hypotheses(query);

    // 构建扩展查询: 原始查询 + 假设答案
    result.expanded_queries.push_back(query);
    result.weights.push_back(1.0f);

    for (size_t i = 0; i < hypotheses.size(); ++i) {
        // 将假设答案作为扩展查询
        result.expanded_queries.push_back(hypotheses[i]);
        result.weights.push_back(0.8f - static_cast<float>(i) * 0.1f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

std::vector<std::string> HyDEExpander::generate_hypotheses(const std::string& query) {
    std::vector<std::string> hypotheses;

    // Mock 实现: 使用 LLM 生成假设性答案
    // 实际实现会调用 llm_service_
    if (llm_service_ != nullptr) {
        // TODO: 调用实际的 LLM 服务
        // hypotheses = llm_service_->generate_hypothetical_answers(query, max_hypotheses_);
    }

    // 简化的 mock 实现: 生成固定格式的假设答案
    if (hypotheses.empty()) {
        for (int i = 0; i < max_hypotheses_; ++i) {
            hypotheses.push_back(
                "[Hypothesis " + std::to_string(i + 1) + "] " + query + " - 自动生成的假设答案"
            );
        }
    }

    return hypotheses;
}

void HyDEExpander::set_llm_service(void* llm_service) {
    llm_service_ = llm_service;
}

// ========== SynonymExpander 实现 ==========

SynonymExpander::SynonymExpander(float similarity_threshold, int max_expansions)
    : similarity_threshold_(similarity_threshold),
      max_expansions_(max_expansions),
      embedding_service_(nullptr) {

    // 初始化默认同义词表
    add_synonyms("搜索", {"查询", "检索", "查找"});
    add_synonyms("查询", {"搜索", "检索", "查找"});
    add_synonyms("检索", {"搜索", "查询", "查找"});
    add_synonyms("找到", {"发现", "获取", "得到"});
    add_synonyms("问题", {"疑问", "问答", "提问"});
    add_synonyms("答案", {"回复", "响应", "结果"});
    add_synonyms("文档", {"文件", "资料", "文章"});
    add_synonyms("相关", {"关联", "有关", "涉及"});
}

ExpansionResult SynonymExpander::expand(const std::string& query) {
    auto start = std::chrono::high_resolution_clock::now();

    ExpansionResult result;
    result.original_query = query;
    result.method = "synonym";

    // 先进行分词
    auto tokens = tokenize(query);

    // 从同义词表扩展
    auto dict_expansions = expand_from_dict(query);

    // 原始查询
    result.expanded_queries.push_back(query);
    result.weights.push_back(1.0f);

    // 添加同义词扩展
    for (const auto& exp : dict_expansions) {
        if (result.expanded_queries.size() >= static_cast<size_t>(max_expansions_ + 1)) {
            break;
        }
        result.expanded_queries.push_back(exp);
        result.weights.push_back(0.9f);
    }

    // 从 Embedding 相似度扩展 (如果可用)
    if (embedding_service_ != nullptr) {
        auto embed_expansions = expand_from_embeddings(query);
        for (const auto& exp : embed_expansions) {
            if (result.expanded_queries.size() >= static_cast<size_t>(max_expansions_)) {
                break;
            }
            result.expanded_queries.push_back(exp);
            result.weights.push_back(similarity_threshold_);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

void SynonymExpander::add_synonyms(const std::string& word,
                                   const std::vector<std::string>& synonyms) {
    synonym_dict_[word] = synonyms;
}

void SynonymExpander::set_embedding_service(void* embed_service) {
    embedding_service_ = embed_service;
}

std::vector<std::string> SynonymExpander::expand_from_dict(const std::string& query) {
    std::vector<std::string> expansions;

    // 分词
    auto tokens = tokenize(query);

    // 对每个 token 查找同义词
    for (const auto& token : tokens) {
        auto it = synonym_dict_.find(token);
        if (it != synonym_dict_.end()) {
            for (const auto& synonym : it->second) {
                // 构建新的查询替换该词
                std::string expanded = query;
                size_t pos = expanded.find(token);
                if (pos != std::string::npos) {
                    expanded.replace(pos, token.length(), synonym);
                    expansions.push_back(expanded);
                }
            }
        }
    }

    return expansions;
}

std::vector<std::string> SynonymExpander::expand_from_embeddings(const std::string& query) {
    std::vector<std::string> expansions;

    // Mock 实现: 使用关键词重叠计算相似度
    // 实际实现会调用 embedding_service_->find_similar_terms(query, max_expansions_)

    return expansions;
}

std::vector<std::string> SynonymExpander::tokenize(const std::string& text) {
    std::vector<std::string> tokens;

    // 简单的中文分词: 按标点和空格分割
    std::string current;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];

        // 中文字符 (UTF-8 高位设置)
        if (static_cast<unsigned char>(c) >= 0x80) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            // UTF-8 中文通常是 3 字节
            if (i + 2 < text.length() &&
                (static_cast<unsigned char>(text[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(text[i + 2]) & 0xC0) == 0x80) {
                std::string token;
                token += c;
                token += text[++i];
                token += text[++i];
                tokens.push_back(token);
            }
        }
        // 标点或空格
        else if (ispunct(c) || c == ' ' || c == '\t' || c == '\n') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
        // 英文字母或数字
        else if (std::isalnum(static_cast<unsigned char>(c))) {
            current += c;
        }
        else {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

// ========== CompositeExpander 实现 ==========

void CompositeExpander::add_expander(std::shared_ptr<QueryExpander> expander, float weight) {
    expanders_.push_back({expander, weight});
}

ExpansionResult CompositeExpander::expand(const std::string& query) {
    auto start = std::chrono::high_resolution_clock::now();

    ExpansionResult result;
    result.original_query = query;
    result.method = "composite";

    std::unordered_map<std::string, float> query_weights;

    // 收集所有扩展器的结果
    for (const auto& [expander, weight] : expanders_) {
        auto exp_result = expander->expand(query);

        for (size_t i = 0; i < exp_result.expanded_queries.size(); ++i) {
            const auto& expanded = exp_result.expanded_queries[i];
            float w = weight * (i < exp_result.weights.size() ? exp_result.weights[i] : 1.0f);

            // 合并权重
            if (query_weights.find(expanded) != query_weights.end()) {
                query_weights[expanded] += w;
            } else {
                query_weights[expanded] = w;
            }
        }
    }

    // 转换为结果格式
    for (const auto& [q, w] : query_weights) {
        result.expanded_queries.push_back(q);
        result.weights.push_back(w);
    }

    // 归一化权重
    float total_weight = 0.0f;
    for (float w : result.weights) {
        total_weight += w;
    }
    if (total_weight > 0.0f) {
        for (float& w : result.weights) {
            w /= total_weight;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

// ========== 工厂函数 ==========

std::shared_ptr<QueryExpander> create_query_expander(
    const std::string& type,
    void* llm_service,
    void* embed_service) {

    if (type == "hyde") {
        auto expander = std::make_shared<HyDEExpander>();
        expander->set_llm_service(llm_service);
        return expander;
    }
    else if (type == "synonym") {
        auto expander = std::make_shared<SynonymExpander>();
        expander->set_embedding_service(embed_service);
        return expander;
    }
    else if (type == "composite") {
        return std::make_shared<CompositeExpander>();
    }
    else {
        throw std::invalid_argument("Unknown query expander type: " + type);
    }
}

}  // namespace rag