/**
 * @file metrics.h
 * @brief RAG 评估指标计算库
 *
 * 业界完整标准指标体系：
 * - 检索质量指标 (Precision@K, Recall@K, MRR, NDCG, Hit Rate 等)
 * - 生成质量指标 (Faithfulness, Hallucination Rate, Correctness 等)
 * - 端到端指标 (Context Precision, Context Recall, Answer Relevancy)
 * - 性能指标 (Latency, Throughput)
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

namespace mmrag {
namespace eval {

// ========== 数据结构 ==========

/**
 * @brief 单条测试用例
 */
struct TestCase {
    std::string id;                              // 问题 ID
    std::string query;                           // 用户问题
    std::vector<std::string> relevant_doc_ids;   // 相关文档 ID 列表（ground truth）
    std::string ground_truth;                    // 标准答案
    std::vector<std::string> key_facts;          // 关键事实列表
    std::string category;                        // 问题类别
    std::string difficulty;                      // 难度: easy/medium/hard
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief 单个问题的评估结果
 */
struct QuestionResult {
    std::string question_id;
    std::string query;
    std::vector<std::string> retrieved_doc_ids;  // 检索到的文档 ID 列表
    std::vector<double> retrieval_scores;        // 检索分数
    std::string generated_answer;                // RAG 生成的答案
    std::string ground_truth;                    // 标准答案
    std::vector<std::string> context;            // 检索到的上下文（文本）

    // 检索指标
    double precision_at_5 = 0.0;
    double precision_at_10 = 0.0;
    double recall_at_5 = 0.0;
    double recall_at_10 = 0.0;
    double reciprocal_rank = 0.0;
    double ndcg_at_10 = 0.0;
    double hit_rate = 0.0;                       // 0 或 1
    double map_score = 0.0;
    double f1_at_10 = 0.0;
    double context_precision = 0.0;
    double context_recall = 0.0;

    // 生成指标
    double faithfulness = 0.0;
    double answer_relevancy = 0.0;
    double answer_correctness = 0.0;
    double hallucination_rate = 0.0;            // 0=无幻觉, 1=完全幻觉
    double completeness = 0.0;

    // 性能
    int64_t latency_ms = 0;
    int token_usage = 0;

    std::string error_message;                  // 错误信息
};

/**
 * @brief 评估运行汇总结果
 */
struct EvaluationSummary {
    std::string run_id;                          // 运行唯一 ID
    std::string dataset_id;                      // 测试集 ID
    std::string pipeline_type;                  // 使用的 Pipeline 类型

    // 检索指标（聚合）
    double avg_precision_at_5 = 0.0;
    double avg_precision_at_10 = 0.0;
    double avg_recall_at_5 = 0.0;
    double avg_recall_at_10 = 0.0;
    double mrr = 0.0;
    double avg_ndcg_at_10 = 0.0;
    double hit_rate = 0.0;
    double map_score = 0.0;
    double avg_f1_at_10 = 0.0;

    // 生成指标（聚合）
    double avg_faithfulness = 0.0;
    double avg_answer_relevancy = 0.0;
    double avg_answer_correctness = 0.0;
    double avg_hallucination_rate = 0.0;
    double avg_completeness = 0.0;

    // 性能（聚合）
    double avg_latency_ms = 0.0;
    double p50_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
    double avg_token_usage = 0.0;

    // 统计
    int total_questions = 0;
    int successful_questions = 0;
    int failed_questions = 0;

    std::vector<QuestionResult> details;        // 每个问题的详细结果

    int64_t timestamp = 0;                       // 评估时间戳
    std::string pipeline_config_snapshot;         // 配置快照 JSON
};

// ========== 检索指标计算 ==========

/**
 * @brief 计算 Precision@K
 * @param retrieved 检索到的文档 ID 列表（按相关度排序）
 * @param relevant 相关文档 ID 集合
 * @param k 截断位置
 * @return Precision@K 值，范围 [0, 1]
 */
double compute_precision_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k);

/**
 * @brief 计算 Recall@K
 */
double compute_recall_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k);

/**
 * @brief 计算 Reciprocal Rank（第一个相关文档的排名倒数）
 */
double compute_reciprocal_rank(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant);

/**
 * @brief 计算 MRR（对一组查询的 RR 取平均）
 */
double compute_mrr(
    const std::vector<std::pair<std::vector<std::string>,
                                 std::unordered_set<std::string>>>& queries);

/**
 * @brief 计算 NDCG@K（Normalized Discounted Cumulative Gain）
 * @param retrieved 检索结果（带相关性分数）
 * @param relevance_judge 给定 doc_id 返回相关性分数（通常 0 或 1）
 * @param k 截断位置
 */
double compute_ndcg_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_map<std::string, double>& relevance_judges,
    int k);

/**
 * @brief 计算 Hit Rate@K（是否至少命中一个相关文档）
 */
double compute_hit_rate(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k);

/**
 * @brief 计算 Average Precision（AP）
 */
double compute_average_precision(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k);

/**
 * @brief 计算 MAP@K（多个查询的 AP 均值）
 */
double compute_map_at_k(
    const std::vector<std::pair<std::vector<std::string>,
                                 std::unordered_set<std::string>>>& queries,
    int k);

/**
 * @brief 计算 F1@K（Precision 和 Recall 的调和均值）
 */
double compute_f1_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k);

/**
 * @brief 计算 R-Precision（精度，其中 K = |relevant|）
 */
double compute_r_precision(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant);

// ========== 生成质量指标 ==========

/**
 * @brief 计算 Context Precision
 * 检索到的上下文中相关部分的占比
 */
double compute_context_precision(
    const std::vector<std::string>& retrieved_context,
    const std::unordered_set<std::string>& relevant);

/**
 * @brief 计算 Context Recall
 * 上下文对 ground truth 的覆盖度
 */
double compute_context_recall(
    const std::vector<std::string>& retrieved_context,
    const std::string& ground_truth);

/**
 * @brief 计算 Faithfulness（忠实度）
 * 答案是否忠实于上下文（基于声明验证）
 *
 * @param answer 生成的答案
 * @param context 检索到的上下文
 * @return faithfulness 分数 [0, 1]
 */
double compute_faithfulness(
    const std::string& answer,
    const std::string& context);

/**
 * @brief 计算 Hallucination Rate
 * 答案中虚构内容的比例
 *
 * @return hallucination_rate 分数 [0, 1]
 */
double compute_hallucination_rate(
    const std::string& answer,
    const std::string& context);

/**
 * @brief 计算 Answer Relevancy（答案相关性）
 * 答案与问题的相关程度（基于词重叠 + 长度惩罚）
 */
double compute_answer_relevancy(
    const std::string& answer,
    const std::string& query);

/**
 * @brief 计算 Answer Correctness（答案正确性）
 * 答案与 ground truth 的事实一致性（基于 token 重叠 + 关键事实匹配）
 */
double compute_answer_correctness(
    const std::string& answer,
    const std::string& ground_truth,
    const std::vector<std::string>& key_facts);

/**
 * @brief 计算 Completeness（完整性）
 * 答案信息覆盖的完整程度
 */
double compute_completeness(
    const std::string& answer,
    const std::vector<std::string>& key_facts);

// ========== 文本相似度工具 ==========

/**
 * @brief 计算 Jaccard 相似度
 */
double jaccard_similarity(
    const std::string& a,
    const std::string& b);

/**
 * @brief 计算 token 重叠比（基于简单分词）
 */
double token_overlap_ratio(
    const std::string& a,
    const std::string& b);

/**
 * @brief 将字符串分词为小写 token 集合
 */
std::unordered_set<std::string> tokenize(const std::string& text);

// ========== 聚合工具 ==========

/**
 * @brief 计算百分位延迟（如 P50, P99）
 */
double compute_latency_percentile(
    const std::vector<int64_t>& latencies,
    double percentile);

}  // namespace eval
}  // namespace mmrag