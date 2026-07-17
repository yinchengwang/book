/**
 * @file query_expansion.cpp
 * @brief 查询扩展实现
 */

#include "rag/query_expander.h"
#include "rag/logger.h"
#include "rag/embedding.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <chrono>

namespace rag {

// ========== HyDEExpander 实现 ==========

HyDEExpander::HyDEExpander(int max_hypotheses)
    : max_hypotheses_(max_hypotheses), llm_service_(nullptr) {}

ExpansionResult HyDEExpander::expand(const std::string& query) {
    ExpansionResult result;
    result.original_query = query;

    auto start = std::chrono::steady_clock::now();

    // 生成假设性答案
    auto hypotheses = generate_hypotheses(query);

    // 从假设性答案提取查询
    result.expanded_queries.push_back(query);  // 保留原始查询

    for (const auto& hypo : hypotheses) {
        // 简化: 直接使用假设性答案作为扩展查询
        // 实际实现应该从中提取关键查询术语
        if (!hypo.empty()) {
            result.expanded_queries.push_back(hypo);
        }
    }

    // 等权重
    result.weights.resize(result.expanded_queries.size(), 1.0f);
    result.method = "hyde";

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("HyDE expanded query to " + std::to_string(result.expanded_queries.size())
              + " variants");

    return result;
}

std::vector<std::string> HyDEExpander::generate_hypotheses(const std::string& query) {
    std::vector<std::string> hypotheses;

    if (llm_service_ == nullptr) {
        // 无 LLM 时使用简化策略: 模拟生成假设性答案
        // 实际部署时应调用真实 LLM

        // 策略1: 添加领域限定词
        hypotheses.push_back(query + " details");
        hypotheses.push_back(query + " explained");

        // 策略2: 生成问答对
        hypotheses.push_back("What is " + query + "?");
        hypotheses.push_back("How to " + query + "?");

        // 策略3: 添加上下文
        hypotheses.push_back(query + " tutorial");
        hypotheses.push_back(query + " best practices");
    } else {
        // 有 LLM 时调用
        RAG_WARN("LLM service not implemented yet, using fallback");
    }

    // 限制数量
    if (static_cast<int>(hypotheses.size()) > max_hypotheses_) {
        hypotheses.resize(max_hypotheses_);
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

    // 初始化默认同义词表 (通用技术术语)
    add_synonyms("search", {"query", "find", "lookup", "retrieve"});
    add_synonyms("find", {"search", "locate", "discover", "detect"});
    add_synonyms("get", {"obtain", "fetch", "retrieve", "acquire"});
    add_synonyms("set", {"configure", "setup", "define", "assign"});
    add_synonyms("create", {"add", "insert", "generate", "build"});
    add_synonyms("delete", {"remove", "drop", "erase", "discard"});
    add_synonyms("update", {"modify", "edit", "change", "alter"});
    add_synonyms("list", {"enumerate", "show", "display", "retrieve"});
    add_synonyms("error", {"exception", "fault", "issue", "bug"});
    add_synonyms("config", {"configuration", "settings", "parameters", "options"});
    add_synonyms("database", {"db", "storage", "data store"});
    add_synonyms("index", {"search", "lookup", "find"});
    add_synonyms("query", {"search", "request", "inquire", "ask"});
}

ExpansionResult SynonymExpander::expand(const std::string& query) {
    ExpansionResult result;
    result.original_query = query;

    auto start = std::chrono::steady_clock::now();

    // 1. 尝试从同义词表扩展
    auto dict_expansions = expand_from_dict(query);

    // 2. 如果设置了 embedding 服务，尝试语义相似词扩展
    if (embedding_service_ != nullptr) {
        auto embed_expansions = expand_from_embeddings(query);
        dict_expansions.insert(dict_expansions.end(),
                               embed_expansions.begin(),
                               embed_expansions.end());
    }

    // 构建扩展查询
    result.expanded_queries.push_back(query);  // 原始查询权重最高

    for (const auto& word : dict_expansions) {
        if (result.expanded_queries.size() >= static_cast<size_t>(max_expansions_ + 1)) {
            break;
        }
        // 替换原始查询中的某个词
        std::string expanded = query;
        // 简化处理: 在末尾添加同义词
        expanded += " " + word;
        result.expanded_queries.push_back(expanded);
    }

    // 等权重
    result.weights.resize(result.expanded_queries.size(), 1.0f);
    result.method = "synonym";

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("Synonym expanded query to " + std::to_string(result.expanded_queries.size())
              + " variants");

    return result;
}

std::vector<std::string> SynonymExpander::expand_from_dict(const std::string& query) {
    std::vector<std::string> expansions;
    auto words = tokenize(query);

    std::unordered_set<std::string> seen;

    for (const auto& word : words) {
        auto it = synonym_dict_.find(word);
        if (it != synonym_dict_.end()) {
            for (const auto& syn : it->second) {
                if (seen.find(syn) == seen.end()) {
                    seen.insert(syn);
                    expansions.push_back(syn);
                    if (static_cast<int>(expansions.size()) >= max_expansions_) {
                        return expansions;
                    }
                }
            }
        }
    }

    return expansions;
}

std::vector<std::string> SynonymExpander::expand_from_embeddings(const std::string& query) {
    std::vector<std::string> expansions;
    // 简化: 如果没有真实 embedding 服务，跳过
    RAG_DEBUG("Embedding-based synonym expansion not available");
    return expansions;
}

std::vector<std::string> SynonymExpander::tokenize(const std::string& text) {
    std::vector<std::string> words;
    std::string current;

    for (char c : text) {
        if (std::isspace(c) || std::ispunct(c)) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current += static_cast<char>(std::tolower(c));
        }
    }

    if (!current.empty()) {
        words.push_back(current);
    }

    return words;
}

void SynonymExpander::add_synonyms(const std::string& word,
                                    const std::vector<std::string>& synonyms) {
    synonym_dict_[word] = synonyms;
}

void SynonymExpander::set_embedding_service(void* embed_service) {
    embedding_service_ = embed_service;
}

// ========== CompositeExpander 实现 ==========

void CompositeExpander::add_expander(std::shared_ptr<QueryExpander> expander, float weight) {
    expanders_.emplace_back(std::move(expander), weight);
}

ExpansionResult CompositeExpander::expand(const std::string& query) {
    ExpansionResult result;
    result.original_query = query;

    if (expanders_.empty()) {
        result.expanded_queries.push_back(query);
        result.weights.push_back(1.0f);
        return result;
    }

    std::unordered_set<std::string> seen;
    std::vector<std::pair<std::string, float>> weighted_queries;

    for (const auto& [expander, weight] : expanders_) {
        auto exp_result = expander->expand(query);

        for (size_t i = 0; i < exp_result.expanded_queries.size(); ++i) {
            const auto& q = exp_result.expanded_queries[i];
            if (seen.find(q) == seen.end()) {
                seen.insert(q);
                float w = weight * (i == 0 ? 1.0f : 0.5f);  // 原始查询权重更高
                weighted_queries.emplace_back(q, w);
            }
        }
    }

    // 按权重排序
    std::sort(weighted_queries.begin(), weighted_queries.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    for (const auto& [q, w] : weighted_queries) {
        result.expanded_queries.push_back(q);
        result.weights.push_back(w);
    }

    result.method = "composite";

    return result;
}

// ========== 工厂函数 ==========

std::shared_ptr<QueryExpander> create_query_expander(
    const std::string& type,
    void* llm_service,
    void* embed_service) {

    if (type == "hyde") {
        auto expander = std::make_shared<HyDEExpander>();
        if (llm_service) {
            expander->set_llm_service(llm_service);
        }
        return expander;
    }

    if (type == "synonym") {
        auto expander = std::make_shared<SynonymExpander>();
        if (embed_service) {
            expander->set_embedding_service(embed_service);
        }
        return expander;
    }

    if (type == "composite") {
        auto expander = std::make_shared<CompositeExpander>();
        return expander;
    }

    // 默认返回空的 HyDE
    return std::make_shared<HyDEExpander>();
}

}  // namespace rag
