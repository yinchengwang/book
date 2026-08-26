/**
 * @file query_decomposer.h
 * @brief 查询分解器 - 多跳问题分解
 */
#pragma once

#include "rag/pipeline.h"
#include <string>
#include <vector>
#include <memory>

namespace rag {

// ========== 分解结果 ==========

struct DecomposedQuery {
    std::string sub_query;              // 子查询文本
    int hop_number = 0;                // 第几跳
    std::string expected_entity_type;    // 期望的实体类型
    std::string entity_mention;          // 实体提及
    float confidence = 1.0f;            // 置信度
    std::vector<std::string> dependencies;  // 依赖的子查询ID
};

struct DecompositionResult {
    bool success = false;
    std::vector<DecomposedQuery> sub_queries;
    std::string original_query;
    int64_t processing_time_ms = 0;
    std::string method;
    std::string error_message;
};

// ========== 子查询结果 ==========

struct SubQueryResult {
    std::string sub_query_id;
    std::string sub_query;
    std::vector<RetrievalResult> retrieved_chunks;
    std::string extracted_answer;
    bool success = false;
};

struct MergedResult {
    std::string merged_answer;
    std::vector<std::string> supporting_chunks;
    float confidence = 0.0f;
};

// ========== 配置 ==========

struct QueryDecomposerConfig {
    // 分解方法
    enum class Method {
        RULE_BASED,     // 基于规则
        LLM_ASSISTED,   // LLM 辅助
        HYBRID          // 混合
    };

    Method method = Method::HYBRID;

    // 规则配置
    bool enable_entity_extraction = true;
    bool enable_relation_detection = true;
    bool enable_hop_planning = true;

    // 限制
    int max_sub_queries = 5;
    int max_hops = 3;

    // 阈值
    float min_entity_confidence = 0.5f;
    float min_relation_confidence = 0.5f;

    // LLM 配置
    std::string llm_model;
    std::shared_ptr<void> llm_service;  // LLMService*
};

// ========== QueryDecomposer ==========

/**
 * @brief 查询分解器基类
 */
class QueryDecomposer {
public:
    virtual ~QueryDecomposer() = default;

    virtual DecompositionResult decompose(const std::string& query) = 0;
    virtual MergedResult merge_results(
        const std::string& original_query,
        const std::vector<SubQueryResult>& sub_results) = 0;

    virtual std::string name() const = 0;
    virtual bool is_ready() const { return true; }
};

// ========== 规则分解器 ==========

class RuleBasedQueryDecomposer : public QueryDecomposer {
public:
    explicit RuleBasedQueryDecomposer(const QueryDecomposerConfig& config = {});
    ~RuleBasedQueryDecomposer() override = default;

    DecompositionResult decompose(const std::string& query) override;
    MergedResult merge_results(
        const std::string& original_query,
        const std::vector<SubQueryResult>& sub_results) override;

    std::string name() const override { return "rule_based"; }

private:
    // 初始化规则
    void init_rules();

    // 实体提取
    std::vector<std::pair<std::string, std::string>> extract_entities(const std::string& query);

    // 关系检测
    std::vector<std::pair<std::string, std::string>> detect_relations(const std::string& query);

    // 构建子查询
    std::vector<DecomposedQuery> build_sub_queries(
        const std::string& query,
        const std::vector<std::pair<std::string, std::string>>& entities,
        const std::vector<std::pair<std::string, std::string>>& relations);

    QueryDecomposerConfig config_;

    // 实体模式
    std::vector<std::pair<std::string, std::string>> entity_patterns_;

    // 关系模式
    std::vector<std::pair<std::string, std::string>> relation_patterns_;
};

// ========== LLM 分解器 ==========

class LLMQueryDecomposer : public QueryDecomposer {
public:
    explicit LLMQueryDecomposer(
        const QueryDecomposerConfig& config,
        std::shared_ptr<LLMService> llm_service);
    ~LLMQueryDecomposer() override = default;

    DecompositionResult decompose(const std::string& query) override;
    MergedResult merge_results(
        const std::string& original_query,
        const std::vector<SubQueryResult>& sub_results) override;

    std::string name() const override { return "llm_assisted"; }
    bool is_ready() const override { return llm_service_ != nullptr; }

private:
    std::string build_decomposition_prompt(const std::string& query);
    std::string build_merge_prompt(
        const std::string& original_query,
        const std::vector<SubQueryResult>& sub_results);

    std::vector<DecomposedQuery> parse_decomposition_response(const std::string& response);
    std::string parse_merge_response(const std::string& response);

    QueryDecomposerConfig config_;
    std::shared_ptr<LLMService> llm_service_;
};

// ========== 混合分解器 ==========

class HybridQueryDecomposer : public QueryDecomposer {
public:
    explicit HybridQueryDecomposer(
        const QueryDecomposerConfig& config,
        std::shared_ptr<LLMService> llm_service = nullptr);
    ~HybridQueryDecomposer() override = default;

    DecompositionResult decompose(const std::string& query) override;
    MergedResult merge_results(
        const std::string& original_query,
        const std::vector<SubQueryResult>& sub_results) override;

    std::string name() const override { return "hybrid"; }
    bool is_ready() const override { return true; }

private:
    // 判断是否需要 LLM 分解
    bool needs_llm_decomposition(const std::string& query);

    // 使用规则分解
    DecompositionResult decompose_with_rules(const std::string& query);

    // 使用 LLM 分解
    DecompositionResult decompose_with_llm(const std::string& query);

    QueryDecomposerConfig config_;
    std::shared_ptr<RuleBasedQueryDecomposer> rule_decomposer_;
    std::shared_ptr<LLMQueryDecomposer> llm_decomposer_;
};

// ========== Factory ==========

std::shared_ptr<QueryDecomposer> create_query_decomposer(
    const QueryDecomposerConfig& config,
    std::shared_ptr<LLMService> llm_service = nullptr);

}  // namespace rag
