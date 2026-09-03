/**
 * @file query_decomposer.cpp
 * @brief 查询分解器实现
 */

#include "rag/query_decomposer.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>

namespace rag {

// ========== RuleBasedQueryDecomposer ==========

RuleBasedQueryDecomposer::RuleBasedQueryDecomposer(const QueryDecomposerConfig& config)
    : config_(config) {
    init_rules();
}

void RuleBasedQueryDecomposer::init_rules() {
    // 实体模式: (pattern, entity_type)
    entity_patterns_ = {
        // 人名
        {R"(([一-龥]{2,4})(?:的|是谁|住在|工作)", R"(人名)"},
        {R"(([A-Z][a-z]+ [A-Z][a-z]+))", R"(人名)"},
        // 组织
        {R"(([一-龥]+公司|[一-龥]+大学|[一-龥]+医院)", R"(组织)"},
        // 地名
        {R"(([一-龥]+市|[一-龥]+省|[一-龥]+国)", R"(地点)"},
    };

    // 关系模式
    relation_patterns_ = {
        // 拥有关系
        {R"((.*)有(.*))", R"(拥有)"},
        {R"((.*)属于(.*))", R"(属于)"},
        // 工作关系
        {R"((.*)在(.*)工作)", R"(工作于)"},
        {R"((.*)是(.*)的员工)", R"(雇佣)"},
        // 比较关系
        {R"((.*)和(.*)的区别)", R"(比较)"},
        {R"((.*)比(.*).*)", R"(比较)"},
        // 因果关系
        {R"((.*)导致(.*))", R"(导致)"},
        {R"((.*)因为(.*))", R"(原因)"},
    };
}

DecompositionResult RuleBasedQueryDecomposer::decompose(const std::string& query) {
    auto start = std::chrono::steady_clock::now();

    DecompositionResult result;
    result.original_query = query;
    result.method = "rule_based";

    // 1. 提取实体
    auto entities = extract_entities(query);

    // 2. 检测关系
    auto relations = detect_relations(query);

    // 3. 构建子查询
    result.sub_queries = build_sub_queries(query, entities, relations);

    result.success = true;
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return result;
}

std::vector<std::pair<std::string, std::string>> RuleBasedQueryDecomposer::extract_entities(
    const std::string& query) {

    std::vector<std::pair<std::string, std::string>> entities;

    for (const auto& [pattern, entity_type] : entity_patterns_) {
        try {
            std::regex re(pattern);
            std::sregex_iterator it(query.begin(), query.end(), re);
            std::sregex_iterator end;

            while (it != end) {
                std::string entity = it->str(1);
                if (!entity.empty()) {
                    entities.push_back({entity, entity_type});
                }
                ++it;
            }
        } catch (const std::regex_error&) {
            // 忽略无效正则
        }
    }

    return entities;
}

std::vector<std::pair<std::string, std::string>> RuleBasedQueryDecomposer::detect_relations(
    const std::string& query) {

    std::vector<std::pair<std::string, std::string>> relations;

    for (const auto& [pattern, relation_type] : relation_patterns_) {
        try {
            std::regex re(pattern);
            std::smatch match;
            if (std::regex_search(query, match, re)) {
                relations.push_back({relation_type, match.str()});
            }
        } catch (const std::regex_error&) {
            // 忽略
        }
    }

    return relations;
}

std::vector<DecomposedQuery> RuleBasedQueryDecomposer::build_sub_queries(
    const std::string& query,
    const std::vector<std::pair<std::string, std::string>>& entities,
    const std::vector<std::pair<std::string, std::string>>& relations) {

    std::vector<DecomposedQuery> sub_queries;

    // 简单场景：如果没有复杂关系，直接返回原查询
    if (entities.empty() && relations.empty()) {
        sub_queries.push_back({
            query,  // sub_query
            0,      // hop_number
            "",     // expected_entity_type
            "",     // entity_mention
            1.0f,   // confidence
            {}      // dependencies
        });
        return sub_queries;
    }

    // 多实体场景：分解为针对每个实体的子查询
    int hop = 0;
    for (const auto& [entity, entity_type] : entities) {
        DecomposedQuery sub;
        sub.hop_number = hop++;
        sub.entity_mention = entity;
        sub.expected_entity_type = entity_type;
        sub.confidence = 0.8f;

        // 构建子查询
        if (entity_type == "人名") {
            sub.sub_query = entity + "的基本信息是什么？";
        } else if (entity_type == "组织") {
            sub.sub_query = entity + "是什么样的组织？";
        } else if (entity_type == "地点") {
            sub.sub_query = entity + "在哪里？";
        } else {
            sub.sub_query = entity + "是什么？";
        }

        sub_queries.push_back(sub);
    }

    // 关系场景：添加关系查询
    for (const auto& [relation_type, relation_text] : relations) {
        if (relation_type == "比较" || relation_type == "工作于") {
            // 需要综合的结果
            DecomposedQuery sub;
            sub.hop_number = hop;
            sub.expected_entity_type = "关系";
            sub.confidence = 0.7f;
            sub.sub_query = query;  // 保留原查询用于最终综合
            sub.dependencies.push_back(std::to_string(hop - 1));
            sub_queries.push_back(sub);
        }
    }

    // 限制子查询数量
    if (sub_queries.size() > static_cast<size_t>(config_.max_sub_queries)) {
        sub_queries.resize(config_.max_sub_queries);
    }

    return sub_queries;
}

MergedResult RuleBasedQueryDecomposer::merge_results(
    const std::string& original_query,
    const std::vector<SubQueryResult>& sub_results) {

    MergedResult result;

    std::stringstream ss;
    ss << "根据查询「" << original_query << "」，综合如下：\n\n";

    float total_confidence = 0.0f;
    int success_count = 0;

    for (const auto& sub : sub_results) {
        if (sub.success && !sub.extracted_answer.empty()) {
            ss << "- " << sub.sub_query << "\n  " << sub.extracted_answer << "\n\n";
            result.supporting_chunks.push_back(sub.sub_query);
            total_confidence += sub.confidence;
            success_count++;
        }
    }

    result.merged_answer = ss.str();
    if (success_count > 0) {
        result.confidence = total_confidence / success_count;
    }

    return result;
}

// ========== LLMQueryDecomposer ==========

LLMQueryDecomposer::LLMQueryDecomposer(
    const QueryDecomposerConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : config_(config), llm_service_(llm_service) {}

DecompositionResult LLMQueryDecomposer::decompose(const std::string& query) {
    auto start = std::chrono::steady_clock::now();

    DecompositionResult result;
    result.original_query = query;
    result.method = "llm_assisted";

    if (!llm_service_) {
        result.error_message = "LLM service not available";
        return result;
    }

    try {
        auto prompt = build_decomposition_prompt(query);
        auto response = llm_service_->generate(prompt);
        result.sub_queries = parse_decomposition_response(response);
        result.success = true;
    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return result;
}

MergedResult LLMQueryDecomposer::merge_results(
    const std::string& original_query,
    const std::vector<SubQueryResult>& sub_results) {

    MergedResult result;

    if (!llm_service_) {
        // Fallback 到规则合并
        RuleBasedQueryDecomposer fallback;
        return fallback.merge_results(original_query, sub_results);
    }

    try {
        auto prompt = build_merge_prompt(original_query, sub_results);
        auto response = llm_service_->generate(prompt);
        result.merged_answer = parse_merge_response(response);
        result.confidence = 0.8f;

        for (const auto& sub : sub_results) {
            if (sub.success) {
                result.supporting_chunks.push_back(sub.sub_query);
            }
        }
    } catch (const std::exception& e) {
        RAG_WARN("LLM merge failed: " + std::string(e.what()));
        RuleBasedQueryDecomposer fallback;
        return fallback.merge_results(original_query, sub_results);
    }

    return result;
}

std::string LLMQueryDecomposer::build_decomposition_prompt(const std::string& query) {
    return R"(
请将以下复杂查询分解为多个简单的子查询。

复杂查询: )" + query + R"(

分解要求:
1. 识别查询中的多个实体
2. 识别实体之间的关系
3. 将复杂问题分解为针对单个实体或关系的简单问题
4. 规划执行顺序（哪些需要先查）

输出格式 (JSON):
{
  "sub_queries": [
    {
      "sub_query": "子查询文本",
      "hop_number": 0,
      "expected_entity_type": "人名/组织/地点/概念",
      "dependencies": []
    }
  ]
}

请只输出JSON，不要其他内容。
)";
}

std::string LLMQueryDecomposer::build_merge_prompt(
    const std::string& original_query,
    const std::vector<SubQueryResult>& sub_results) {

    std::stringstream ss;
    ss << "原始查询: " << original_query << "\n\n";
    ss << "子查询结果:\n";

    for (const auto& sub : sub_results) {
        ss << "- 子查询: " << sub.sub_query << "\n";
        ss << "  结果: " << sub.extracted_answer << "\n";
    }

    ss << "\n请综合以上结果，回答原始查询。只输出最终答案，不要解释。";

    return ss.str();
}

std::vector<DecomposedQuery> LLMQueryDecomposer::parse_decomposition_response(
    const std::string& response) {

    std::vector<DecomposedQuery> sub_queries;

    // 简单解析：提取 sub_query 字段
    // 实际应该用 JSON 解析器
    std::regex sub_query_regex(R"("sub_query"\s*:\s*"([^"]+)")");
    std::sregex_iterator it(response.begin(), response.end(), sub_query_regex);
    std::sregex_iterator end;

    int hop = 0;
    while (it != end) {
        DecomposedQuery sub;
        sub.sub_query = it->str(1);
        sub.hop_number = hop++;
        sub.confidence = 0.8f;
        sub_queries.push_back(sub);
        ++it;
    }

    return sub_queries;
}

std::string LLMQueryDecomposer::parse_merge_response(const std::string& response) {
    // 简单处理：直接返回响应
    // 实际可能需要进一步处理
    return response;
}

// ========== HybridQueryDecomposer ==========

HybridQueryDecomposer::HybridQueryDecomposer(
    const QueryDecomposerConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : config_(config) {

    rule_decomposer_ = std::make_shared<RuleBasedQueryDecomposer>(config);

    if (llm_service) {
        llm_decomposer_ = std::make_shared<LLMQueryDecomposer>(config, llm_service);
    }
}

bool HybridQueryDecomposer::needs_llm_decomposition(const std::string& query) {
    // 启发式判断：包含特定模式时使用 LLM
    std::vector<std::string> llm_indicators = {
        "关系", "比较", "区别", "综合", "分析",
        "relationship", "compare", "difference", "analyze"
    };

    for (const auto& indicator : llm_indicators) {
        if (query.find(indicator) != std::string::npos) {
            // 有指示词，检查是否复杂
            if (query.length() > 20) {
                return true;
            }
        }
    }

    return false;
}

DecompositionResult HybridQueryDecomposer::decompose(const std::string& query) {
    if (needs_llm_decomposition(query) && llm_decomposer_ && llm_decomposer_->is_ready()) {
        return decompose_with_llm(query);
    }
    return decompose_with_rules(query);
}

DecompositionResult HybridQueryDecomposer::decompose_with_rules(const std::string& query) {
    auto result = rule_decomposer_->decompose(query);
    result.method = "rule_based";
    return result;
}

DecompositionResult HybridQueryDecomposer::decompose_with_llm(const std::string& query) {
    if (!llm_decomposer_ || !llm_decomposer_->is_ready()) {
        return decompose_with_rules(query);
    }

    auto result = llm_decomposer_->decompose(query);
    result.method = "llm_assisted";

    // 如果 LLM 分解结果为空，回退到规则
    if (result.sub_queries.empty()) {
        RAG_WARN("LLM decomposition returned empty, falling back to rules");
        return decompose_with_rules(query);
    }

    return result;
}

MergedResult HybridQueryDecomposer::merge_results(
    const std::string& original_query,
    const std::vector<SubQueryResult>& sub_results) {

    // 使用规则合并
    return rule_decomposer_->merge_results(original_query, sub_results);
}

// ========== Factory ==========

std::shared_ptr<QueryDecomposer> create_query_decomposer(
    const QueryDecomposerConfig& config,
    std::shared_ptr<LLMService> llm_service) {

    switch (config.method) {
        case QueryDecomposerConfig::Method::RULE_BASED:
            return std::make_shared<RuleBasedQueryDecomposer>(config);

        case QueryDecomposerConfig::Method::LLM_ASSISTED:
            return std::make_shared<LLMQueryDecomposer>(config, llm_service);

        case QueryDecomposerConfig::Method::HYBRID:
        default:
            return std::make_shared<HybridQueryDecomposer>(config, llm_service);
    }
}

}  // namespace rag
