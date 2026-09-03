#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace rag {

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

    // 保存结果
    bool save_results(const std::string& file_path, const EvaluationResult& result);

    // 打印摘要
    void print_summary(const EvaluationResult& result);

private:
    std::vector<TestCase> test_cases_;

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

}  // namespace rag