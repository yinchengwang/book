/**
 * @file synonym_expander.cpp
 * @brief 同义词扩展器实现
 */

#include "rag/query_expander.h"
#include "rag/pipeline.h"
#include <chrono>
#include <algorithm>
#include <set>

namespace rag {

SynonymExpander::SynonymExpander(float similarity_threshold, int max_expansions)
    : similarity_threshold_(similarity_threshold > 0.0f ? similarity_threshold : 0.8f),
      max_expansions_(max_expansions > 0 ? max_expansions : 5),
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
    add_synonyms("文本", {"文字", "内容", "文章"});
    add_synonyms("向量", {"矢量", "嵌入"});
    add_synonyms("模型", {"算法", "系统"});
    add_synonyms("知识", {"信息", "数据"});
    add_synonyms("数据库", {"资料库", "存储"});
}

ExpansionResult SynonymExpander::expand(const std::string& query) {
    auto start = std::chrono::high_resolution_clock::now();

    ExpansionResult result;
    result.original_query = query;
    result.method = "synonym";

    // 添加原始查询
    result.expanded_queries.push_back(query);
    result.weights.push_back(1.0f);

    // 从同义词表扩展
    auto dict_expansions = expand_from_dict(query);

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
    // 避免重复添加
    if (synonym_dict_.find(word) == synonym_dict_.end()) {
        synonym_dict_[word] = synonyms;
    } else {
        // 合并已有同义词
        std::set<std::string> existing(synonym_dict_[word].begin(),
                                       synonym_dict_[word].end());
        for (const auto& s : synonyms) {
            if (existing.find(s) == existing.end()) {
                synonym_dict_[word].push_back(s);
                existing.insert(s);
            }
        }
    }
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

    auto tokens = tokenize(query);
    for (const auto& token : tokens) {
        auto it = synonym_dict_.find(token);
        if (it != synonym_dict_.end()) {
            for (const auto& synonym : it->second) {
                // 简单的关键词重叠计算
                int overlap = 0;
                for (const auto& t : tokens) {
                    if (t == synonym) {
                        overlap++;
                        break;
                    }
                }
                // 如果重叠率超过阈值，添加扩展
                if (overlap > 0 && expansions.size() < static_cast<size_t>(max_expansions_)) {
                    std::string expanded = query;
                    size_t pos = expanded.find(token);
                    if (pos != std::string::npos) {
                        expanded.replace(pos, token.length(), synonym);
                        expansions.push_back(expanded);
                    }
                }
            }
        }
    }

    return expansions;
}

std::vector<std::string> SynonymExpander::tokenize(const std::string& text) {
    std::vector<std::string> tokens;

    // 使用 UTF-8 分词: 中文字符和 ASCII 字符分别处理
    size_t i = 0;
    while (i < text.length()) {
        // 检查是否是 UTF-8 多字节字符 (中文)
        char c = text[i];
        if (static_cast<unsigned char>(c) >= 0x80) {
            // UTF-8 中文通常是 3 字节 (U+4E00 到 U+9FFF)
            if (i + 2 < text.length() &&
                (static_cast<unsigned char>(text[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(text[i + 2]) & 0xC0) == 0x80) {
                // 提取单个汉字
                std::string token;
                token += c;
                token += text[++i];
                token += text[++i];
                tokens.push_back(token);
            } else {
                // 其他多字节字符
                std::string token;
                while (i < text.length() && (static_cast<unsigned char>(c) & 0x80)) {
                    token += c;
                    i++;
                    if (i < text.length()) c = text[i];
                }
                if (!token.empty()) tokens.push_back(token);
            }
        }
        // 空白字符
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
        }
        // ASCII 单词 (字母数字)
        else if (std::isalnum(static_cast<unsigned char>(c))) {
            std::string token;
            while (i < text.length() && std::isalnum(static_cast<unsigned char>(text[i]))) {
                token += text[i++];
            }
            if (!token.empty()) tokens.push_back(token);
        }
        // 其他标点符号
        else {
            i++;
        }
    }

    return tokens;
}

}  // namespace rag