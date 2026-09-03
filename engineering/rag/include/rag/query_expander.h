/**
 * @file query_expander.h
 * @brief 查询扩展 - HyDE 和同义词扩展
 */
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace rag {

// ========== 扩展结果 ==========

/**
 * @brief 查询扩展结果
 */
struct ExpansionResult {
    std::string original_query;           // 原始查询
    std::vector<std::string> expanded_queries;  // 扩展后的查询
    std::vector<float> weights;            // 每个查询的权重
    int64_t processing_time_ms = 0;        // 处理时间
    std::string method;                     // 使用的扩展方法
};

// ========== 查询扩展器接口 ==========

/**
 * @brief 查询扩展器接口
 */
class QueryExpander {
public:
    virtual ~QueryExpander() = default;

    // 扩展查询
    virtual ExpansionResult expand(const std::string& query) = 0;

    // 获取扩展器名称
    virtual std::string name() const = 0;
};

// ========== HyDE 扩展器 ==========

/**
 * @brief HyDE (Hypothetical Document Embeddings) 扩展器
 *
 * 使用 LLM 生成假设性答案，从答案中提取查询
 */
class HyDEExpander : public QueryExpander {
public:
    explicit HyDEExpander(int max_hypotheses = 3);

    ExpansionResult expand(const std::string& query) override;
    std::string name() const override { return "hyde"; }

    // 设置 LLM 服务
    void set_llm_service(void* llm_service);

    // 配置
    void set_max_hypotheses(int max) { max_hypotheses_ = max; }

private:
    // 生成假设性答案
    std::vector<std::string> generate_hypotheses(const std::string& query);

    int max_hypotheses_;
    void* llm_service_;  // LLM 服务占位
};

// ========== 同义词扩展器 ==========

/**
 * @brief 同义词扩展器
 *
 * 使用预定义同义词表或 Embedding 相似词扩展查询
 */
class SynonymExpander : public QueryExpander {
public:
    explicit SynonymExpander(float similarity_threshold = 0.8f,
                             int max_expansions = 5);

    ExpansionResult expand(const std::string& query) override;
    std::string name() const override { return "synonym"; }

    // 添加同义词组
    void add_synonyms(const std::string& word,
                      const std::vector<std::string>& synonyms);

    // 设置 Embedding 服务用于找相似词
    void set_embedding_service(void* embed_service);

    // 配置
    void set_similarity_threshold(float threshold) {
        similarity_threshold_ = threshold;
    }

    void set_max_expansions(int max) { max_expansions_ = max; }

private:
    // 从同义词表扩展
    std::vector<std::string> expand_from_dict(const std::string& query);

    // 从 Embedding 相似度扩展
    std::vector<std::string> expand_from_embeddings(const std::string& query);

    // 分词
    std::vector<std::string> tokenize(const std::string& text);

    float similarity_threshold_;
    int max_expansions_;

    // 同义词表: word -> synonyms
    std::unordered_map<std::string, std::vector<std::string>> synonym_dict_;

    void* embedding_service_;  // Embedding 服务占位
};

// ========== 组合扩展器 ==========

/**
 * @brief 组合多个扩展器
 */
class CompositeExpander : public QueryExpander {
public:
    void add_expander(std::shared_ptr<QueryExpander> expander, float weight = 1.0f);

    ExpansionResult expand(const std::string& query) override;
    std::string name() const override { return "composite"; }

private:
    std::vector<std::pair<std::shared_ptr<QueryExpander>, float>> expanders_;
};

// ========== 工厂函数 ==========

std::shared_ptr<QueryExpander> create_query_expander(
    const std::string& type,
    void* llm_service = nullptr,
    void* embed_service = nullptr);

}  // namespace rag
