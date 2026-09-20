/**
 * @file eval_store.h
 * @brief 评估运行结果存储
 *
 * 每次评估运行都会被永久保存，用于历史对比和趋势分析
 */

#pragma once

#include "mmrag/eval/metrics.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace mmrag {
namespace eval {

/**
 * @brief 评估运行记录
 */
struct EvalRunRecord {
    std::string run_id;                            // 运行唯一 ID
    std::string dataset_id;                        // 数据集 ID
    std::string dataset_version;                   // 数据集版本
    std::string pipeline_type;                    // Pipeline 类型
    std::string pipeline_config;                  // 配置快照（JSON）

    EvaluationSummary summary;                    // 聚合结果

    int64_t timestamp = 0;                        // 运行时间
    std::string author;                            // 运行者
    std::string description;                       // 运行描述
    std::vector<std::string> tags;                 // 标签
};

/**
 * @brief 历史对比结果
 */
struct ComparisonResult {
    std::string comparison_id;
    std::vector<std::string> run_ids;
    std::vector<std::string> pipeline_types;

    // 每个指标的对比结果
    struct MetricDiff {
        std::string metric_name;
        double value_a;
        double value_b;
        double delta;
        double percent_change;
        std::string direction;                    // "up" / "down" / "same"
    };

    std::vector<MetricDiff> metric_diffs;

    int64_t timestamp = 0;
};

/**
 * @brief 评估运行存储管理器
 *
 * 每次评估运行保存为一个 JSON 文件
 * 目录结构：
 *   eval_data/
 *     runs/
 *       run_20260905_1430.json
 *       run_20260905_1500.json
 */
class EvalRunStore {
public:
    /**
     * @brief 构造函数
     * @param base_path 存储根目录（默认 "eval_data/runs"）
     */
    explicit EvalRunStore(const std::string& base_path = "eval_data/runs");

    /**
     * @brief 保存评估运行记录
     */
    bool save_run(const EvalRunRecord& record);

    /**
     * @brief 加载指定的评估运行记录
     */
    std::optional<EvalRunRecord> load_run(const std::string& run_id);

    /**
     * @brief 列出所有评估运行（按时间倒序）
     */
    std::vector<EvalRunRecord> list_runs(int limit = 100);

    /**
     * @brief 按 pipeline 类型过滤评估运行
     */
    std::vector<EvalRunRecord> list_runs_by_pipeline(
        const std::string& pipeline_type,
        int limit = 100);

    /**
     * @brief 对比两次评估运行的结果
     */
    ComparisonResult compare_runs(
        const std::string& run_id_a,
        const std::string& run_id_b);

    /**
     * @brief 获取指定指标的历史趋势
     */
    std::vector<std::pair<int64_t, double>> get_metric_trend(
        const std::string& pipeline_type,
        const std::string& metric_name,
        int limit = 30);

    /**
     * @brief 删除评估运行
     */
    bool delete_run(const std::string& run_id);

    /**
     * @brief 导出运行记录为 JSON 字符串
     */
    std::string export_to_json(const EvalRunRecord& record) const;

    /**
     * @brief 从 JSON 字符串导入运行记录
     */
    std::optional<EvalRunRecord> import_from_json(const std::string& json_content) const;

    /**
     * @brief 生成运行 ID（基于时间戳）
     */
    static std::string generate_run_id();

private:
    std::string base_path_;
    std::string get_run_path(const std::string& run_id) const;
};

}  // namespace eval
}  // namespace mmrag