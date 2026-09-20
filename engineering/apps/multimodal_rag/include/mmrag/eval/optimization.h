/**
 * @file optimization.h
 * @brief 评估优化建议生成
 *
 * 根据评估结果自动诊断问题并生成优化建议
 */

#pragma once

#include "mmrag/eval/metrics.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace mmrag {
namespace eval {

/**
 * @brief 问题严重程度
 */
enum class Severity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

/**
 * @brief 问题类别
 */
enum class IssueCategory {
    RETRIEVAL_LOW_RECALL,         // 检索召回率低
    RETRIEVAL_LOW_PRECISION,      // 检索精度低
    GENERATION_HALLUCINATION,     // 答案有幻觉
    GENERATION_IRRELEVANT,        // 答案不相关
    GENERATION_INCOMPLETE,        // 答案不完整
    GENERATION_LOW_FAITHFULNESS,  // 忠实度低
    LATENCY_HIGH,                // 延迟过高
    THROUGHPUT_LOW,              // 吞吐量低
    TOKEN_USAGE_HIGH,            // Token 消耗过高
    CONFIGURATION_SUBOPTIMAL      // 配置欠佳
};

/**
 * @brief 诊断出的问题
 */
struct Issue {
    IssueCategory category;
    Severity severity;
    std::string title;                // 问题标题
    std::string description;          // 问题描述
    std::string metric_name;          // 触发的指标
    double metric_value = 0.0;       // 当前指标值
    double threshold = 0.0;           // 阈值
};

/**
 * @brief 优化动作
 */
struct OptimizationAction {
    IssueCategory related_issue;
    std::string action_name;          // 动作名称
    std::string description;          // 详细描述
    std::string parameter_name;       // 相关参数名
    std::string new_value;           // 建议值
    std::string expected_impact;      // 预期效果
    Severity priority;                // 优先级
    std::vector<std::string> steps;  // 实施步骤
};

/**
 * @brief 完整的优化方案
 */
struct OptimizationPlan {
    std::string plan_id;
    std::string source_run_id;        // 基于哪次评估结果生成
    int64_t timestamp = 0;

    std::vector<Issue> issues;                    // 诊断出的所有问题
    std::vector<OptimizationAction> actions;      // 推荐的优化动作

    // 按优先级排序后的动作
    std::vector<OptimizationAction> priority_actions;

    // 整体评估
    std::string overall_assessment;    // 整体评估
    double overall_score = 0.0;       // 总体评分（0-1）

    // 类别评分
    std::unordered_map<std::string, double> category_scores;
};

/**
 * @brief 优化规则定义
 */
struct OptimizationRule {
    IssueCategory category;
    Severity severity;
    std::string condition;            // 触发条件描述
    std::string metric_name;
    double threshold;                  // 阈值
    bool is_max;                       // true: 超过阈值触发；false: 低于阈值触发
    std::string title;
    std::string description;
    std::string action_name;
    std::string parameter_name;
    std::string new_value;
    std::string expected_impact;
    std::vector<std::string> steps;
};

/**
 * @brief 优化引擎
 *
 * 根据评估结果自动诊断问题、生成优化建议
 */
class OptimizationEngine {
public:
    OptimizationEngine();

    /**
     * @brief 基于评估结果生成优化方案
     */
    OptimizationPlan generate_plan(const EvaluationSummary& summary);

    /**
     * @brief 获取默认的优化规则集
     */
    static std::vector<OptimizationRule> default_rules();

    /**
     * @brief 添加自定义优化规则
     */
    void add_rule(const OptimizationRule& rule);

    /**
     * @brief 获取所有规则
     */
    const std::vector<OptimizationRule>& rules() const { return rules_; }

    /**
     * @brief 将 Severity 转换为字符串
     */
    static std::string severity_to_string(Severity s);

    /**
     * @brief 将 IssueCategory 转换为字符串
     */
    static std::string category_to_string(IssueCategory c);

    /**
     * @brief 将方案导出为 JSON
     */
    static std::string plan_to_json(const OptimizationPlan& plan);

    /**
     * @brief 打印优化方案到控制台
     */
    static void print_plan(const OptimizationPlan& plan);

private:
    std::vector<OptimizationRule> rules_;

    void diagnose_recall(const EvaluationSummary& summary, std::vector<Issue>& issues);
    void diagnose_precision(const EvaluationSummary& summary, std::vector<Issue>& issues);
    void diagnose_generation(const EvaluationSummary& summary, std::vector<Issue>& issues);
    void diagnose_performance(const EvaluationSummary& summary, std::vector<Issue>& issues);

    Issue make_issue(const OptimizationRule& rule, double value) const;
    OptimizationAction make_action(const OptimizationRule& rule) const;

    double compute_overall_score(const EvaluationSummary& summary) const;
};

}  // namespace eval
}  // namespace mmrag