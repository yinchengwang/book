/**
 * @file query_classifier.h
 * @brief 查询分类器
 *
 * 支持多种分类策略:
 * - 规则匹配
 * - 关键词分析
 * - LLM 分类 (可选)
 */
#pragma once

#include "rag/pipeline.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace rag {

// ========== 分类结果 ==========

struct ClassificationResult {
    QueryType type = QueryType::FACTUAL;
    float confidence = 0.0f;
    std::vector<std::string> keywords;
    std::string intent;
    std::unordered_map<std::string, float> type_scores;
};

// ========== 分类器配置 ==========

struct QueryClassifierConfig {
    // 分类策略
    enum class Strategy {
        RULE_ONLY,      // 仅规则
        KEYWORD_ONLY,   // 仅关键词
        LLM_ASSISTED,   // LLM 辅助
        ENSEMBLE         // 集成
    };

    Strategy strategy = Strategy::RULE_ONLY;

    // 规则配置
    bool enable_pattern_matching = true;
    bool enable_keyword_analysis = true;
    bool enable_llm_classification = false;

    // 阈值
    float min_confidence = 0.5f;
    float llm_confidence_boost = 0.2f;

    // LLM 配置 (用于 LLM_ASSISTED)
    std::string llm_model;
    std::string llm_endpoint;

    // 关键词配置
    std::unordered_map<QueryType, std::vector<std::string>> type_keywords;
};

// ========== QueryClassifier ==========

/**
 * @brief 查询分类器基类
 */
class BaseQueryClassifier : public QueryClassifier {
public:
    explicit BaseQueryClassifier(const QueryClassifierConfig& config);
    ~BaseQueryClassifier() override = default;

    QueryType classify(const std::string& query) override;
    std::pair<QueryType, float> classify_with_confidence(const std::string& query) override;
    std::vector<std::string> extract_keywords(const std::string& query) override;

    // 分类统计
    struct ClassifierStats {
        uint64_t total_classifications = 0;
        std::unordered_map<QueryType, uint64_t> type_counts;
        double avg_confidence = 0.0;
    };
    ClassifierStats get_stats() const;

protected:
    virtual ClassificationResult do_classify(const std::string& query) = 0;
    virtual std::vector<std::string> do_extract_keywords(const std::string& query) = 0;

    QueryClassifierConfig config_;
    ClassifierStats stats_;
    mutable std::mutex stats_mutex_;
};

// ========== 规则分类器 ==========

/**
 * @brief 基于规则的查询分类器
 *
 * 使用正则表达式和关键词匹配进行分类
 */
class RuleBasedQueryClassifier : public BaseQueryClassifier {
public:
    explicit RuleBasedQueryClassifier(const QueryClassifierConfig& config = {});
    ~RuleBasedQueryClassifier() override = default;

protected:
    ClassificationResult do_classify(const std::string& query) override;
    std::vector<std::string> do_extract_keywords(const std::string& query) override;

private:
    // 初始化规则
    void init_rules();

    // 模式匹配
    ClassificationResult match_patterns(const std::string& query);

    // 关键词分析
    ClassificationResult analyze_keywords(const std::string& query, const std::vector<std::string>& keywords);

    // 规则定义
    std::unordered_map<QueryType, std::vector<std::string>> patterns_;
    std::unordered_set<std::string> stopwords_;
};

// ========== 关键词分类器 ==========

/**
 * @brief 基于关键词权重的分类器
 */
class KeywordQueryClassifier : public BaseQueryClassifier {
public:
    explicit KeywordQueryClassifier(const QueryClassifierConfig& config = {});
    ~KeywordQueryClassifier() override = default;

    // 添加自定义关键词
    void add_keyword(QueryType type, const std::string& keyword, float weight = 1.0f);

protected:
    ClassificationResult do_classify(const std::string& query) override;
    std::vector<std::string> do_extract_keywords(const std::string& query) override;

private:
    std::unordered_map<std::string, std::vector<std::pair<QueryType, float>>> keyword_weights_;
    std::unordered_set<std::string> stopwords_;
};

// ========== LLM 分类器 (可选) ==========

/**
 * @brief 基于 LLM 的查询分类器
 */
class LLMQueryClassifier : public BaseQueryClassifier {
public:
    explicit LLMQueryClassifier(
        const QueryClassifierConfig& config,
        std::shared_ptr<LLMService> llm_service);
    ~LLMQueryClassifier() override = default;

    bool is_llm_available() const;

protected:
    ClassificationResult do_classify(const std::string& query) override;
    std::vector<std::string> do_extract_keywords(const std::string& query) override;

private:
    std::string build_classification_prompt(const std::string& query);
    std::string build_keyword_prompt(const std::string& query);
    QueryType parse_llm_response(const std::string& response);
    std::vector<std::string> parse_keywords_response(const std::string& response);

    std::shared_ptr<LLMService> llm_service_;
};

// ========== 集成分类器 ==========

/**
 * @brief 集成分类器
 *
 * 结合规则、关键词和 LLM 多种方法
 */
class EnsembleQueryClassifier : public BaseQueryClassifier {
public:
    explicit EnsembleQueryClassifier(
        const QueryClassifierConfig& config,
        std::shared_ptr<LLMService> llm_service = nullptr);
    ~EnsembleQueryClassifier() override = default;

    // 添加分类器并设置权重
    void add_classifier(const std::string& name,
                       std::shared_ptr<BaseQueryClassifier> classifier,
                       float weight);

protected:
    ClassificationResult do_classify(const std::string& query) override;
    std::vector<std::string> do_extract_keywords(const std::string& query) override;

private:
    struct ClassifierInfo {
        std::shared_ptr<BaseQueryClassifier> classifier;
        float weight;
    };

    std::vector<ClassifierInfo> classifiers_;
    std::shared_ptr<LLMService> llm_service_;
};

// ========== Factory ==========

std::shared_ptr<QueryClassifier> create_query_classifier(
    const QueryClassifierConfig& config,
    std::shared_ptr<LLMService> llm_service = nullptr);

}  // namespace rag
