/**
 * @file evaluator.h
 * @brief RAG 系统评估框架
 *
 * 实现 RAGAS 评估指标计算，支持：
 * - RetrievalMetrics: Recall@K, MRR, NDCG@K
 * - RAGEvaluation: Faithfulness, Answer Relevance, Context Relevance
 */
#pragma once

#include "rag/llm_service.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace rag {

// ========== 评估指标结构 ==========

/**
 * @brief 检索质量指标
 */
struct RetrievalMetrics {
    double recall_at_k = 0.0;     // Recall@K
    double mrr = 0.0;             // MRR (Mean Reciprocal Rank)
    double ndcg_at_k = 0.0;      // NDCG@K
    double precision_at_k = 0.0;  // Precision@K
};

/**
 * @brief RAGAS 评估指标
 */
struct RAGEvaluation {
    double faithfulness = 0.0;      // 忠实度 [0, 1]
    double answer_relevance = 0.0; // 答案相关性 [0, 1]
    double context_relevance = 0.0;// 上下文相关性 [0, 1]
    double ragas_score = 0.0;      // 综合得分 [0, 1]
};

/**
 * @brief 单次评估结果
 */
struct EvaluationResult {
    std::string question;           // 问题
    std::string answer;             // 答案
    std::vector<std::string> contexts;  // 检索到的上下文
    std::vector<std::string> ground_truths;  // 真实相关文档

    RetrievalMetrics retrieval;     // 检索指标
    RAGEvaluation ragas;            // RAGAS 指标

    int64_t latency_ms = 0;         // 端到端延迟
    bool success = true;            // 是否成功
    std::string error_message;      // 错误信息
};

/**
 * @brief 评估汇总统计
 */
struct EvaluationSummary {
    size_t total_queries = 0;
    size_t successful_queries = 0;
    double success_rate = 0.0;

    // 检索指标统计
    double avg_recall_at_k = 0.0;
    double avg_mrr = 0.0;
    double avg_ndcg_at_k = 0.0;
    double avg_precision_at_k = 0.0;

    // RAGAS 指标统计
    double avg_faithfulness = 0.0;
    double avg_answer_relevance = 0.0;
    double avg_context_relevance = 0.0;
    double avg_ragas_score = 0.0;

    // 延迟统计
    double avg_latency_ms = 0.0;
    double p50_latency_ms = 0.0;
    double p95_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
};

// ========== 数据集格式 ==========

/**
 * @brief 评估数据集条目
 */
struct DatasetEntry {
    std::string id;                        // 唯一标识
    std::string question;                  // 问题
    std::vector<std::string> ground_truths; // 真实答案/相关文档
    std::vector<std::string> contexts;     // 上下文（可选）
};

// ========== RAG 评估器接口 ==========

/**
 * @brief RAG 评估器
 *
 * 用于评估 RAG 系统的检索质量和生成质量
 */
class RAGEvaluator {
public:
    /**
     * @brief 构造函数
     * @param llm LLM 服务（用于计算 Faithfulness 和 Relevance）
     */
    explicit RAGEvaluator(std::shared_ptr<LLMService> llm = nullptr);

    /**
     * @brief 默认构造函数
     */
    RAGEvaluator();

    ~RAGEvaluator();

    // ========== 配置 ==========

    /**
     * @brief 设置 LLM 服务
     */
    void set_llm(std::shared_ptr<LLMService> llm);

    /**
     * @brief 设置检索评估的 K 值
     */
    void set_retrieval_k(int k) { retrieval_k_ = k; }

    /**
     * @brief 获取检索评估的 K 值
     */
    int retrieval_k() const { return retrieval_k_; }

    /**
     * @brief 设置 RAGAS 权重
     */
    void set_ragas_weights(double faithfulness, double answer_relevance,
                           double context_relevance);

    // ========== 评估方法 ==========

    /**
     * @brief 评估单次查询
     *
     * @param question 问题
     * @param answer 生成的答案
     * @param contexts 检索到的上下文
     * @param ground_truths 真实相关的文档/答案
     * @return 评估结果
     */
    EvaluationResult evaluate(
        const std::string& question,
        const std::string& answer,
        const std::vector<std::string>& contexts,
        const std::vector<std::string>& ground_truths);

    /**
     * @brief 仅评估检索质量
     */
    RetrievalMetrics evaluate_retrieval(
        const std::vector<std::string>& retrieved,
        const std::vector<std::string>& ground_truths,
        const std::vector<int>& relevance_scores = {});

    /**
     * @brief 仅评估生成质量（RAGAS 指标）
     *
     * 需要 LLM 服务
     */
    RAGEvaluation evaluate_generation(
        const std::string& question,
        const std::string& answer,
        const std::vector<std::string>& contexts);

    /**
     * @brief 评估数据集
     *
     * @param entries 数据集条目
     * @param engine RAG 引擎（用于执行查询）
     * @return 评估结果列表
     */
    std::vector<EvaluationResult> evaluate_dataset(
        const std::vector<DatasetEntry>& entries);

    // ========== 报告生成 ==========

    /**
     * @brief 生成单次评估的 JSON 报告
     */
    std::string generate_json_report(const EvaluationResult& result);

    /**
     * @brief 生成批量评估的汇总报告
     */
    std::string generate_summary_report(const std::vector<EvaluationResult>& results);

    /**
     * @brief 获取评估汇总统计
     */
    EvaluationSummary compute_summary(const std::vector<EvaluationResult>& results);

    // ========== 数据集加载 ==========

    /**
     * @brief 从 JSON 文件加载数据集
     */
    std::vector<DatasetEntry> load_dataset(const std::string& path);

    /**
     * @brief 保存评估结果到 JSON 文件
     */
    int save_results(const std::string& path,
                     const std::vector<EvaluationResult>& results);

private:
    // ========== 检索指标计算 ==========

    /**
     * @brief 计算 Recall@K
     */
    double compute_recall_at_k(const std::vector<std::string>& retrieved,
                               const std::vector<std::string>& ground_truths);

    /**
     * @brief 计算 MRR
     */
    double compute_mrr(const std::vector<std::string>& retrieved,
                       const std::vector<std::string>& ground_truths);

    /**
     * @brief 计算 NDCG@K
     */
    double compute_ndcg_at_k(const std::vector<std::string>& retrieved,
                             const std::vector<std::string>& ground_truths,
                             const std::vector<int>& relevance_scores);

    /**
     * @brief 计算 Precision@K
     */
    double compute_precision_at_k(const std::vector<std::string>& retrieved,
                                  const std::vector<std::string>& ground_truths);

    // ========== RAGAS 指标计算 ==========

    /**
     * @brief 计算 Faithfulness
     */
    double compute_faithfulness(const std::string& answer,
                                const std::vector<std::string>& contexts);

    /**
     * @brief 计算 Answer Relevance
     */
    double compute_answer_relevance(const std::string& question,
                                    const std::string& answer);

    /**
     * @brief 计算 Context Relevance
     */
    double compute_context_relevance(const std::string& question,
                                     const std::vector<std::string>& contexts);

    /**
     * @brief 生成相关问题（用于 Answer Relevance）
     */
    std::vector<std::string> generate_related_questions(
        const std::string& answer, int count = 3);

    /**
     * @brief 提取关键句子（用于 Context Relevance）
     */
    std::vector<std::string> extract_key_sentences(
        const std::string& context, const std::string& question);

    // ========== LLM 调用 ==========

    /**
     * @brief 调用 LLM 生成
     */
    std::string call_llm(const std::string& prompt);

    // ========== 工具方法 ==========

    /**
     * @brief 计算两个文本的相似度（基于字符 n-gram）
     */
    double text_similarity(const std::string& a, const std::string& b);

    /**
     * @brief 计算百分位数
     */
    double percentile(std::vector<double> values, double p);

    // 配置
    int retrieval_k_ = 10;
    double weight_faithfulness_ = 0.3;
    double weight_answer_relevance_ = 0.4;
    double weight_context_relevance_ = 0.3;

    // LLM 服务
    std::shared_ptr<LLMService> llm_;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建 RAG 评估器
 */
std::shared_ptr<RAGEvaluator> create_rag_evaluator(
    std::shared_ptr<LLMService> llm = nullptr);

}  // namespace rag
