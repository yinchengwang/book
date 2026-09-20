#pragma once

#include "mmrag/llm_service.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace mmrag {

// ========== 测试用例 ==========

struct TestCase {
    std::string query;
    std::vector<std::string> relevant_chunks;  // 标注的正确答案
    std::string description;
    std::string category;
};

// ========== 评估结果 ==========

struct EvaluationResult {
    // 召回率
    double recall_at_1 = 0;
    double recall_at_5 = 0;
    double recall_at_10 = 0;
    double recall_at_20 = 0;

    // 精确率
    double precision_at_1 = 0;
    double precision_at_5 = 0;
    double precision_at_10 = 0;

    // 排名指标
    double mrr = 0;            // Mean Reciprocal Rank
    double map = 0;           // Mean Average Precision
    double ndcg_at_5 = 0;
    double ndcg_at_10 = 0;
    double ndcg_at_20 = 0;

    // 统计
    int total_queries = 0;
    int successful_queries = 0;
    double avg_latency_ms = 0;

    // 每个查询的详细结果
    struct QueryResult {
        std::string query;
        double recall;
        double precision;
        double mrr;
        double ndcg;
        std::vector<std::string> retrieved;
        std::vector<std::string> expected;
        std::vector<double> relevance_scores;
    };
    std::vector<QueryResult> query_results;
};

// ========== RAG 评估器 ==========

class RAGEvaluator {
public:
    RAGEvaluator();
    RAGEvaluator(std::shared_ptr<LLMService> llm);
    ~RAGEvaluator();

    // LLM service setter
    void set_llm(std::shared_ptr<LLMService> llm);

    // RAGAS weights setter
    void set_ragas_weights(double faithfulness, double answer_relevance, double context_relevance);

    // 添加测试用例
    void add_test_case(const TestCase& test_case);
    void add_test_cases(const std::vector<TestCase>& test_cases);

    // 加载测试集
    bool load_test_suite(const std::string& file_path);

    // 评估 Pipeline
    EvaluationResult evaluate(std::shared_ptr<class RetrievalPipeline> pipeline, int top_k = 10);

    // 评估单个查询
    EvaluationResult::QueryResult evaluate_query(
        std::shared_ptr<class RetrievalPipeline> pipeline,
        const TestCase& test_case,
        int top_k);

    // Save multiple results (overload)
    int save_results(const std::string& file_path, const std::vector<EvaluationResult>& results);

    // LLM helper
    std::string call_llm(const std::string& prompt);

    // Text similarity
    double text_similarity(const std::string& a, const std::string& b);

    // Percentile
    double percentile(std::vector<double> values, double p);

    // Generate JSON report
    std::string generate_json_report(const EvaluationResult& result);

    // Generate summary report
    std::string generate_summary_report(const std::vector<EvaluationResult>& results);

    // Compute summary
    struct EvaluationSummary {
        int total_queries = 0;
        int successful_queries = 0;
        double success_rate = 0;
        double avg_recall_at_k = 0;
        double avg_mrr = 0;
        double avg_ndcg_at_k = 0;
        double avg_precision_at_k = 0;
        double avg_faithfulness = 0;
        double avg_answer_relevance = 0;
        double avg_context_relevance = 0;
        double avg_ragas_score = 0;
        double avg_latency_ms = 0;
        double p50_latency_ms = 0;
        double p95_latency_ms = 0;
        double p99_latency_ms = 0;
    };
    EvaluationSummary compute_summary(const std::vector<EvaluationResult>& results);

    // Load dataset
    struct DatasetEntry {
        std::string id;
        std::string question;
        std::vector<std::string> ground_truths;
        std::vector<std::string> contexts;
    };
    std::vector<DatasetEntry> load_dataset(const std::string& path);

    // 打印摘要
    void print_summary(const EvaluationResult& result);

    // Retrieval metrics
    struct RetrievalMetrics {
        double recall_at_k = 0;
        double precision_at_k = 0;
        double mrr = 0;
        double ndcg_at_k = 0;
        int hits = 0;
        int total_relevant = 0;
    };

    // RAGAS metrics
    struct RAGASMetrics {
        double faithfulness = 0;
        double answer_relevance = 0;
        double context_relevance = 0;
        double overall = 0;
    };

    // Evaluate retrieval metrics
    RetrievalMetrics evaluate_retrieval(
        const std::vector<std::string>& retrieved,
        const std::vector<std::string>& relevant,
        int k);

    // Compute faithfulness (how grounded the answer is in context)
    double compute_faithfulness(const std::string& answer,
                                const std::vector<std::string>& context);

    // Generate related questions
    std::vector<std::string> generate_related_questions(
        const std::string& question, int n = 5);

    // Compute answer relevance
    double compute_answer_relevance(const std::string& question,
                                    const std::string& answer);

    // Extract key sentences
    std::vector<std::string> extract_key_sentences(
        const std::string& text,
        const std::string& reference);

    // Compute context relevance
    double compute_context_relevance(const std::string& question,
                                     const std::vector<std::string>& context);

    // Compute recall at k
    double compute_recall_at_k(
        const std::vector<std::string>& retrieved,
        const std::vector<std::string>& relevant);

    // Compute precision at k
    double compute_precision_at_k(
        const std::vector<std::string>& retrieved,
        const std::vector<std::string>& relevant);

    // Compute ndcg at k
    double compute_ndcg_at_k(
        const std::vector<std::string>& retrieved,
        const std::vector<std::string>& relevant,
        const std::vector<int>& relevance_scores);

private:
    std::vector<TestCase> test_cases_;
    std::shared_ptr<LLMService> llm_;
    double faithfulness_weight_ = 0.4;
    double answer_relevance_weight_ = 0.4;
    double context_relevance_weight_ = 0.2;

    // 计算 DCG
    double compute_dcg(const std::vector<double>& relevance, int k);

    // 计算 IDCG
    double compute_idcg(const std::vector<double>& relevance, int k);

    // 计算 NDCG
    double compute_ndcg(const std::vector<std::string>& retrieved,
                        const std::vector<std::string>& relevant,
                        int k);

    // 计算 MRR
    double compute_mrr(const std::vector<std::string>& retrieved,
                       const std::vector<std::string>& relevant);

    // 计算 Recall
    double compute_recall(const std::vector<std::string>& retrieved,
                          const std::vector<std::string>& relevant,
                          int k);

    // 计算 Precision
    double compute_precision(const std::vector<std::string>& retrieved,
                            const std::vector<std::string>& relevant,
                            int k);
};

// ========== 测试集生成器 ==========

class TestSuiteGenerator {
public:
    // 从文件生成测试用例
    static std::vector<TestCase> generate_from_docs(
        const std::string& docs_path,
        int num_queries = 10);

    // 生成合成测试用例
    static std::vector<TestCase> generate_synthetic(
        const std::string& domain,
        int num_queries = 10);
};

// ========== 工厂函数 ==========

std::unique_ptr<RAGEvaluator> create_evaluator();
std::unique_ptr<TestSuiteGenerator> create_test_suite_generator();

}  // namespace mmrag