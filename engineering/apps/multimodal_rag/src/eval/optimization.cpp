/**
 * @file optimization.cpp
 * @brief 优化引擎实现
 */

#include "mmrag/eval/optimization.h"
#include "mmrag/types.h"
#include "mmrag/logger.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

#include <nlohmann/json.hpp>
using nlohmann::json;

namespace mmrag {
namespace eval {

// ========== 辅助函数 ==========

namespace {

int64_t now_ms() {
    return mmrag::current_timestamp_ms();
}

std::string severity_name(Severity s) {
    switch (s) {
        case Severity::LOW: return "low";
        case Severity::MEDIUM: return "medium";
        case Severity::HIGH: return "high";
        case Severity::CRITICAL: return "critical";
    }
    return "unknown";
}

std::string category_name(IssueCategory c) {
    switch (c) {
        case IssueCategory::RETRIEVAL_LOW_RECALL: return "retrieval_low_recall";
        case IssueCategory::RETRIEVAL_LOW_PRECISION: return "retrieval_low_precision";
        case IssueCategory::GENERATION_HALLUCINATION: return "generation_hallucination";
        case IssueCategory::GENERATION_IRRELEVANT: return "generation_irrelevant";
        case IssueCategory::GENERATION_INCOMPLETE: return "generation_incomplete";
        case IssueCategory::GENERATION_LOW_FAITHFULNESS: return "generation_low_faithfulness";
        case IssueCategory::LATENCY_HIGH: return "latency_high";
        case IssueCategory::THROUGHPUT_LOW: return "throughput_low";
        case IssueCategory::TOKEN_USAGE_HIGH: return "token_usage_high";
        case IssueCategory::CONFIGURATION_SUBOPTIMAL: return "configuration_suboptimal";
    }
    return "unknown";
}

}  // anonymous namespace

// ========== OptimizationEngine 实现 ==========

OptimizationEngine::OptimizationEngine() {
    rules_ = default_rules();
}

std::vector<OptimizationRule> OptimizationEngine::default_rules() {
    return {
        // 召回率问题
        {
            IssueCategory::RETRIEVAL_LOW_RECALL,
            Severity::HIGH,
            "recall_at_10 < 0.7",
            "recall_at_10", 0.7, false,
            "检索召回率不足",
            "前10个检索结果未能充分覆盖相关文档，建议提高召回能力",
            "increase_top_k",
            "top_k",
            "20",
            "预计 recall 提升 10-20%",
            {
                "1. 将 top_k 从当前值增加到 20",
                "2. 测试不同 top_k 值（10, 15, 20, 30）",
                "3. 启用查询扩展 (Query Expansion)",
                "4. 考虑使用混合检索（向量 + 关键词）"
            }
        },
        // 精度问题
        {
            IssueCategory::RETRIEVAL_LOW_PRECISION,
            Severity::HIGH,
            "precision_at_5 < 0.6",
            "precision_at_5", 0.6, false,
            "检索精度偏低",
            "Top-5 结果中相关文档比例较低，存在较多噪音",
            "add_reranker",
            "reranker",
            "true",
            "预计 precision 提升 15-25%",
            {
                "1. 启用重排序模型 (Reranker)",
                "2. 设置 rerank top_k = 50",
                "3. 微调 reranker 阈值",
                "4. 调整 chunk 大小以提高单块精度"
            }
        },
        // 幻觉问题
        {
            IssueCategory::GENERATION_HALLUCINATION,
            Severity::CRITICAL,
            "hallucination_rate > 0.15",
            "hallucination_rate", 0.15, true,
            "幻觉率过高",
            "RAG 系统生成了超出上下文的信息，存在严重幻觉",
            "optimize_prompt",
            "prompt_template",
            "strict_grounded",
            "预计 hallucination 降低 30-50%",
            {
                "1. 在 prompt 中强化\"严格基于上下文\"指令",
                "2. 降低 temperature 至 0.3-0.5",
                "3. 添加\"如果无法回答，请说明\"的兜底逻辑",
                "4. 考虑使用 Self-RAG 或 Corrective RAG"
            }
        },
        // 相关性
        {
            IssueCategory::GENERATION_IRRELEVANT,
            Severity::MEDIUM,
            "answer_relevancy < 0.7",
            "answer_relevancy", 0.7, false,
            "答案相关性不足",
            "生成的答案与用户问题的相关性偏低",
            "improve_query_understanding",
            "query_expansion",
            "enabled",
            "预计 answer_relevancy 提升 10-20%",
            {
                "1. 启用查询扩展 (Query Expansion)",
                "2. 添加问题分类器路由到合适的 Pipeline",
                "3. 优化 prompt 模板使其更聚焦",
                "4. 增大检索上下文窗口"
            }
        },
        // 完整性
        {
            IssueCategory::GENERATION_INCOMPLETE,
            Severity::MEDIUM,
            "completeness < 0.7",
            "completeness", 0.7, false,
            "答案完整性不足",
            "答案未覆盖所有关键信息",
            "expand_context",
            "top_k",
            "15",
            "预计 completeness 提升 15%",
            {
                "1. 增加 top_k 至 15-20",
                "2. 优化 chunk 大小以提高完整性",
                "3. 考虑使用 Hierarchical RAG",
                "4. 增加多跳推理（Graph RAG）"
            }
        },
        // 忠实度
        {
            IssueCategory::GENERATION_LOW_FAITHFULNESS,
            Severity::HIGH,
            "faithfulness < 0.7",
            "faithfulness", 0.7, false,
            "忠实度偏低",
            "答案未充分基于检索到的上下文",
            "add_grounding",
            "system_prompt",
            "strict",
            "预计 faithfulness 提升 20%",
            {
                "1. 加强 system prompt 中的 grounding 指令",
                "2. 在 prompt 中标注每个引用",
                "3. 使用 Self-Check 机制验证答案",
                "4. 降低 LLM temperature"
            }
        },
        // 延迟
        {
            IssueCategory::LATENCY_HIGH,
            Severity::MEDIUM,
            "p99_latency_ms > 3000",
            "p99_latency_ms", 3000, true,
            "P99 延迟过高",
            "99% 的查询延迟超过 3 秒，用户体验差",
            "enable_cache",
            "cache_enabled",
            "true",
            "预计 P99 降至 2s 以内",
            {
                "1. 启用查询结果缓存",
                "2. 优化检索算法（减少 HNSW ef 参数）",
                "3. 实施检索结果预计算",
                "4. 异步流水线优化"
            }
        },
        // Token 使用
        {
            IssueCategory::TOKEN_USAGE_HIGH,
            Severity::LOW,
            "avg_token_usage > 2048",
            "avg_token_usage", 2048, true,
            "Token 消耗过高",
            "平均每次查询的 token 消耗过大",
            "optimize_context",
            "context_size",
            "1024",
            "预计 token 消耗降低 30-50%",
            {
                "1. 减少上下文窗口大小",
                "2. 启用上下文压缩 (Context Compression)",
                "3. 使用更简洁的 prompt",
                "4. 提取关键句子而非完整段落"
            }
        }
    };
}

void OptimizationEngine::add_rule(const OptimizationRule& rule) {
    rules_.push_back(rule);
}

Issue OptimizationEngine::make_issue(
    const OptimizationRule& rule, double value) const {
    Issue issue;
    issue.category = rule.category;
    issue.severity = rule.severity;
    issue.title = rule.title;
    issue.description = rule.description;
    issue.metric_name = rule.metric_name;
    issue.metric_value = value;
    issue.threshold = rule.threshold;
    return issue;
}

OptimizationAction OptimizationEngine::make_action(
    const OptimizationRule& rule) const {
    OptimizationAction action;
    action.related_issue = rule.category;
    action.action_name = rule.action_name;
    action.description = rule.description;
    action.parameter_name = rule.parameter_name;
    action.new_value = rule.new_value;
    action.expected_impact = rule.expected_impact;
    action.priority = rule.severity;
    action.steps = rule.steps;
    return action;
}

void OptimizationEngine::diagnose_recall(
    const EvaluationSummary& summary, std::vector<Issue>& issues) {
    for (const auto& rule : rules_) {
        if (rule.category != IssueCategory::RETRIEVAL_LOW_RECALL) continue;

        double value = 0.0;
        if (rule.metric_name == "recall_at_10") value = summary.avg_recall_at_10;
        else if (rule.metric_name == "recall_at_5") value = summary.avg_recall_at_5;
        else continue;

        bool triggered = rule.is_max ? (value > rule.threshold) : (value < rule.threshold);
        if (triggered) {
            issues.push_back(make_issue(rule, value));
        }
    }
}

void OptimizationEngine::diagnose_precision(
    const EvaluationSummary& summary, std::vector<Issue>& issues) {
    for (const auto& rule : rules_) {
        if (rule.category != IssueCategory::RETRIEVAL_LOW_PRECISION) continue;

        double value = 0.0;
        if (rule.metric_name == "precision_at_5") value = summary.avg_precision_at_5;
        else if (rule.metric_name == "precision_at_10") value = summary.avg_precision_at_10;
        else continue;

        bool triggered = rule.is_max ? (value > rule.threshold) : (value < rule.threshold);
        if (triggered) {
            issues.push_back(make_issue(rule, value));
        }
    }
}

void OptimizationEngine::diagnose_generation(
    const EvaluationSummary& summary, std::vector<Issue>& issues) {
    for (const auto& rule : rules_) {
        double value = 0.0;
        bool applicable = true;

        if (rule.metric_name == "hallucination_rate") value = summary.avg_hallucination_rate;
        else if (rule.metric_name == "answer_relevancy") value = summary.avg_answer_relevancy;
        else if (rule.metric_name == "completeness") value = summary.avg_completeness;
        else if (rule.metric_name == "faithfulness") value = summary.avg_faithfulness;
        else applicable = false;

        if (!applicable) continue;

        bool triggered = rule.is_max ? (value > rule.threshold) : (value < rule.threshold);
        if (triggered) {
            issues.push_back(make_issue(rule, value));
        }
    }
}

void OptimizationEngine::diagnose_performance(
    const EvaluationSummary& summary, std::vector<Issue>& issues) {
    for (const auto& rule : rules_) {
        double value = 0.0;
        bool applicable = true;

        if (rule.metric_name == "p99_latency_ms") value = summary.p99_latency_ms;
        else if (rule.metric_name == "avg_latency_ms") value = summary.avg_latency_ms;
        else if (rule.metric_name == "avg_token_usage") value = summary.avg_token_usage;
        else applicable = false;

        if (!applicable) continue;

        bool triggered = rule.is_max ? (value > rule.threshold) : (value < rule.threshold);
        if (triggered) {
            issues.push_back(make_issue(rule, value));
        }
    }
}

double OptimizationEngine::compute_overall_score(
    const EvaluationSummary& summary) const {
    // 加权计算综合得分
    double score = 0.0;
    double weight_sum = 0.0;

    auto add = [&](double value, double weight) {
        score += value * weight;
        weight_sum += weight;
    };

    // 检索指标（权重 0.4）
    add(summary.avg_precision_at_10, 0.05);
    add(summary.avg_recall_at_10, 0.10);
    add(summary.mrr, 0.10);
    add(summary.avg_ndcg_at_10, 0.10);
    add(summary.hit_rate, 0.05);

    // 生成指标（权重 0.5）
    add(summary.avg_faithfulness, 0.15);
    add(summary.avg_answer_relevancy, 0.10);
    add(summary.avg_answer_correctness, 0.10);
    add(summary.avg_completeness, 0.10);
    add(1.0 - summary.avg_hallucination_rate, 0.05);  // 1 - hallucination

    // 性能指标（权重 0.1，倒数）
    if (summary.avg_latency_ms > 0) {
        double latency_score = std::max(0.0, 1.0 - summary.avg_latency_ms / 5000.0);
        add(latency_score, 0.10);
    }

    if (weight_sum > 0) {
        return score / weight_sum;
    }
    return 0.0;
}

OptimizationPlan OptimizationEngine::generate_plan(
    const EvaluationSummary& summary) {
    OptimizationPlan plan;
    plan.plan_id = "plan_" + std::to_string(now_ms());
    plan.timestamp = now_ms();
    plan.source_run_id = summary.run_id;

    // 1. 诊断问题
    diagnose_recall(summary, plan.issues);
    diagnose_precision(summary, plan.issues);
    diagnose_generation(summary, plan.issues);
    diagnose_performance(summary, plan.issues);

    // 2. 生成优化动作
    for (const auto& issue : plan.issues) {
        // 查找匹配规则
        for (const auto& rule : rules_) {
            if (rule.category == issue.category &&
                rule.metric_name == issue.metric_name) {
                plan.actions.push_back(make_action(rule));
                break;
            }
        }
    }

    // 3. 按优先级排序
    plan.priority_actions = plan.actions;
    std::sort(plan.priority_actions.begin(), plan.priority_actions.end(),
              [](const OptimizationAction& a, const OptimizationAction& b) {
                  return static_cast<int>(a.priority) > static_cast<int>(b.priority);
              });

    // 4. 计算整体评分
    plan.overall_score = compute_overall_score(summary);

    // 5. 类别评分
    plan.category_scores["retrieval"] = (summary.avg_recall_at_10 + summary.mrr) / 2.0;
    plan.category_scores["generation"] = (
        summary.avg_faithfulness + summary.avg_answer_relevancy +
        summary.avg_answer_correctness + summary.avg_completeness) / 4.0;
    plan.category_scores["performance"] = summary.avg_latency_ms > 0
        ? std::max(0.0, 1.0 - summary.avg_latency_ms / 5000.0)
        : 1.0;

    // 6. 整体评估
    std::ostringstream oss;
    if (plan.overall_score >= 0.85) {
        oss << "系统表现优秀，整体评分 " << std::fixed << std::setprecision(2) << plan.overall_score;
        plan.overall_assessment = oss.str();
    } else if (plan.overall_score >= 0.7) {
        oss << "系统表现良好，但仍有优化空间。整体评分 " << std::fixed << std::setprecision(2)
            << plan.overall_score;
        plan.overall_assessment = oss.str();
    } else if (plan.overall_score >= 0.5) {
        oss << "系统表现中等，建议进行优化。整体评分 " << std::fixed << std::setprecision(2)
            << plan.overall_score;
        plan.overall_assessment = oss.str();
    } else {
        oss << "系统表现欠佳，需要立即优化。整体评分 " << std::fixed << std::setprecision(2)
            << plan.overall_score;
        plan.overall_assessment = oss.str();
    }

    RAG_INFO("Generated optimization plan: " + plan.plan_id +
             " (score: " + std::to_string(plan.overall_score) +
             ", issues: " + std::to_string(plan.issues.size()) +
             ", actions: " + std::to_string(plan.actions.size()) + ")");

    return plan;
}

std::string OptimizationEngine::severity_to_string(Severity s) {
    return severity_name(s);
}

std::string OptimizationEngine::category_to_string(IssueCategory c) {
    return category_name(c);
}

std::string OptimizationEngine::plan_to_json(const OptimizationPlan& plan) {
    json j;
    j["plan_id"] = plan.plan_id;
    j["source_run_id"] = plan.source_run_id;
    j["timestamp"] = plan.timestamp;
    j["overall_score"] = plan.overall_score;
    j["overall_assessment"] = plan.overall_assessment;
    j["issue_count"] = plan.issues.size();
    j["action_count"] = plan.actions.size();

    j["category_scores"] = plan.category_scores;

    j["issues"] = json::array();
    for (const auto& i : plan.issues) {
        json ij;
        ij["category"] = category_name(i.category);
        ij["severity"] = severity_name(i.severity);
        ij["title"] = i.title;
        ij["description"] = i.description;
        ij["metric_name"] = i.metric_name;
        ij["metric_value"] = i.metric_value;
        ij["threshold"] = i.threshold;
        j["issues"].push_back(ij);
    }

    j["priority_actions"] = json::array();
    for (const auto& a : plan.priority_actions) {
        json aj;
        aj["action_name"] = a.action_name;
        aj["related_issue"] = category_name(a.related_issue);
        aj["description"] = a.description;
        aj["parameter_name"] = a.parameter_name;
        aj["new_value"] = a.new_value;
        aj["expected_impact"] = a.expected_impact;
        aj["priority"] = severity_name(a.priority);
        aj["steps"] = a.steps;
        j["priority_actions"].push_back(aj);
    }

    return j.dump(2);
}

void OptimizationEngine::print_plan(const OptimizationPlan& plan) {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "              RAG Optimization Plan                            \n";
    std::cout << "================================================================\n";
    std::cout << "Plan ID:        " << plan.plan_id << "\n";
    std::cout << "Source Run:     " << plan.source_run_id << "\n";
    std::cout << "Overall Score:  " << std::fixed << std::setprecision(3)
              << plan.overall_score << "\n";
    std::cout << "Assessment:     " << plan.overall_assessment << "\n";
    std::cout << "\n";

    // 类别评分
    std::cout << "----- Category Scores -----\n";
    for (const auto& p : plan.category_scores) {
        std::cout << "  " << p.first << ": " << std::fixed << std::setprecision(3)
                  << p.second << "\n";
    }
    std::cout << "\n";

    // 问题列表
    std::cout << "----- Identified Issues (" << plan.issues.size() << ") -----\n";
    for (size_t i = 0; i < plan.issues.size(); ++i) {
        const auto& issue = plan.issues[i];
        std::cout << "  [" << (i + 1) << "] [" << severity_name(issue.severity) << "] "
                  << issue.title << "\n";
        std::cout << "      " << issue.description << "\n";
        std::cout << "      Metric: " << issue.metric_name
                  << " = " << std::fixed << std::setprecision(3) << issue.metric_value
                  << " (threshold: " << issue.threshold << ")\n";
    }
    std::cout << "\n";

    // 优化动作
    std::cout << "----- Priority Actions (" << plan.priority_actions.size() << ") -----\n";
    for (size_t i = 0; i < plan.priority_actions.size(); ++i) {
        const auto& action = plan.priority_actions[i];
        std::cout << "  [" << (i + 1) << "] [" << severity_name(action.priority) << "] "
                  << action.action_name << "\n";
        std::cout << "      Parameter: " << action.parameter_name
                  << " = " << action.new_value << "\n";
        std::cout << "      Impact: " << action.expected_impact << "\n";
        std::cout << "      Steps:\n";
        for (const auto& step : action.steps) {
            std::cout << "        - " << step << "\n";
        }
    }
    std::cout << "\n================================================================\n";
}

}  // namespace eval
}  // namespace mmrag