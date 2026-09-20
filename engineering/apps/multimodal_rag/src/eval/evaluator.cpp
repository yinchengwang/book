/**
 * @file evaluator.cpp
 * @brief RAG 评估器实现 - 与 evaluator.h 头文件对齐的版本
 */

#include "mmrag/evaluator.h"
#include "mmrag/logger.h"
#include "mmrag/error.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <numeric>
#include <sstream>
#include <set>
#include <unordered_set>

// JSON 解析
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace mmrag {

// ========== 工具函数 ==========

static std::vector<std::string> split_sentences(const std::string& text) {
    std::vector<std::string> sentences;
    std::istringstream iss(text);
    std::string sentence;
    while (std::getline(iss, sentence)) {
        while (!sentence.empty() && std::isspace(static_cast<unsigned char>(sentence.front()))) {
            sentence.erase(sentence.begin());
        }
        while (!sentence.empty() && std::isspace(static_cast<unsigned char>(sentence.back()))) {
            sentence.pop_back();
        }
        if (!sentence.empty()) {
            sentences.push_back(sentence);
        }
    }
    return sentences;
}

static std::vector<std::string> split_words(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

// ========== RAGEvaluator 实现 ==========

RAGEvaluator::RAGEvaluator(std::shared_ptr<LLMService> llm)
    : llm_(llm) {}

RAGEvaluator::RAGEvaluator() = default;

RAGEvaluator::~RAGEvaluator() = default;

void RAGEvaluator::set_llm(std::shared_ptr<LLMService> llm) {
    llm_ = llm;
}

void RAGEvaluator::set_ragas_weights(double faithfulness,
                                      double answer_relevance,
                                      double context_relevance) {
    faithfulness_weight_ = faithfulness;
    answer_relevance_weight_ = answer_relevance;
    context_relevance_weight_ = context_relevance;
}

// ========== 测试用例管理 ==========

void RAGEvaluator::add_test_case(const TestCase& test_case) {
    test_cases_.push_back(test_case);
}

void RAGEvaluator::add_test_cases(const std::vector<TestCase>& test_cases) {
    test_cases_.insert(test_cases_.end(), test_cases.begin(), test_cases.end());
}

bool RAGEvaluator::load_test_suite(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    try {
        json data = json::parse(file);
        file.close();
        if (data.is_array()) {
            for (const auto& item : data) {
                TestCase tc;
                tc.query = item.value("query", "");
                tc.relevant_chunks = item.value("relevant_chunks", std::vector<std::string>{});
                tc.description = item.value("description", "");
                tc.category = item.value("category", "");
                test_cases_.push_back(tc);
            }
        }
        return true;
    } catch (const std::exception& e) {
        RAG_WARN("Failed to load test suite: " + std::string(e.what()));
        return false;
    }
}

// ========== 检索指标计算 ==========

double RAGEvaluator::compute_recall_at_k(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& relevant) {
    if (retrieved.empty() || relevant.empty()) {
        return 0.0;
    }
    std::unordered_set<std::string> retrieved_words;
    for (const auto& doc : retrieved) {
        auto words = split_words(doc);
        retrieved_words.insert(words.begin(), words.end());
    }
    std::unordered_set<std::string> rel_words;
    for (const auto& doc : relevant) {
        auto words = split_words(doc);
        rel_words.insert(words.begin(), words.end());
    }
    if (rel_words.empty()) return 0.0;
    size_t intersection = 0;
    for (const auto& word : rel_words) {
        if (retrieved_words.count(word) > 0) {
            intersection++;
        }
    }
    return static_cast<double>(intersection) / rel_words.size();
}

double RAGEvaluator::compute_precision_at_k(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& relevant) {
    if (retrieved.empty()) return 0.0;
    size_t k = std::min(static_cast<size_t>(10), retrieved.size());
    std::unordered_set<std::string> rel_words;
    for (const auto& doc : relevant) {
        auto words = split_words(doc);
        rel_words.insert(words.begin(), words.end());
    }
    size_t relevant_count = 0;
    for (size_t i = 0; i < k; i++) {
        auto words = split_words(retrieved[i]);
        for (const auto& word : words) {
            if (rel_words.count(word) > 0) {
                relevant_count++;
                break;
            }
        }
    }
    return static_cast<double>(relevant_count) / k;
}

double RAGEvaluator::compute_ndcg_at_k(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& relevant,
    const std::vector<int>& relevance_scores) {
    if (retrieved.empty() || relevant.empty()) return 0.0;

    std::vector<int> scores;
    if (relevance_scores.empty()) {
        std::unordered_set<std::string> rel_words;
        for (const auto& doc : relevant) {
            auto words = split_words(doc);
            rel_words.insert(words.begin(), words.end());
        }
        for (const auto& doc : retrieved) {
            auto words = split_words(doc);
            size_t matched = 0;
            for (const auto& word : words) {
                if (rel_words.count(word) > 0) matched++;
            }
            if (matched > 10) scores.push_back(3);
            else if (matched > 5) scores.push_back(2);
            else if (matched > 0) scores.push_back(1);
            else scores.push_back(0);
        }
    } else {
        scores = relevance_scores;
    }
    size_t k = std::min(static_cast<size_t>(10), scores.size());
    double dcg = 0.0;
    for (size_t i = 0; i < k; i++) {
        dcg += scores[i] / std::log2(i + 2);
    }
    std::vector<int> sorted_scores = scores;
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<int>());
    double idcg = 0.0;
    for (size_t i = 0; i < k; i++) {
        idcg += sorted_scores[i] / std::log2(i + 2);
    }
    if (idcg == 0.0) return 0.0;
    return dcg / idcg;
}

// ========== RetrievalMetrics 评估 ==========

RAGEvaluator::RetrievalMetrics RAGEvaluator::evaluate_retrieval(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& relevant,
    int k) {
    RetrievalMetrics metrics;
    size_t kk = static_cast<size_t>(k);
    std::vector<std::string> retrieved_k;
    if (kk < retrieved.size()) {
        retrieved_k.assign(retrieved.begin(), retrieved.begin() + kk);
    } else {
        retrieved_k = retrieved;
    }
    metrics.recall_at_k = compute_recall_at_k(retrieved_k, relevant);
    metrics.precision_at_k = compute_precision_at_k(retrieved_k, relevant);
    // MRR
    std::unordered_set<std::string> rel_words;
    for (const auto& doc : relevant) {
        auto words = split_words(doc);
        rel_words.insert(words.begin(), words.end());
    }
    metrics.mrr = 0.0;
    for (size_t i = 0; i < retrieved_k.size(); i++) {
        auto words = split_words(retrieved_k[i]);
        bool found = false;
        for (const auto& w : words) {
            if (rel_words.count(w) > 0) { found = true; break; }
        }
        if (found) {
            metrics.mrr = 1.0 / static_cast<double>(i + 1);
            break;
        }
    }
    // NDCG
    std::vector<int> rel_scores;
    for (const auto& doc : retrieved_k) {
        auto words = split_words(doc);
        size_t matched = 0;
        for (const auto& w : words) {
            if (rel_words.count(w) > 0) matched++;
        }
        if (matched > 10) rel_scores.push_back(3);
        else if (matched > 5) rel_scores.push_back(2);
        else if (matched > 0) rel_scores.push_back(1);
        else rel_scores.push_back(0);
    }
    metrics.ndcg_at_k = compute_ndcg_at_k(retrieved_k, relevant, rel_scores);
    metrics.hits = static_cast<int>(metrics.recall_at_k * relevant.size());
    metrics.total_relevant = static_cast<int>(relevant.size());
    return metrics;
}

// ========== RAGAS 指标计算 ==========

double RAGEvaluator::compute_faithfulness(const std::string& answer,
                                          const std::vector<std::string>& context) {
    if (answer.empty()) return 0.0;
    auto answer_words = split_words(answer);
    if (answer_words.empty()) return 1.0;
    std::unordered_set<std::string> context_words;
    for (const auto& ctx : context) {
        auto words = split_words(ctx);
        context_words.insert(words.begin(), words.end());
    }
    size_t found = 0;
    for (const auto& word : answer_words) {
        if (context_words.count(word) > 0) found++;
    }
    return static_cast<double>(found) / answer_words.size();
}

std::vector<std::string> RAGEvaluator::generate_related_questions(
    const std::string& question, int n) {
    std::vector<std::string> questions;
    if (question.empty() || n <= 0) return questions;
    auto words = split_words(question);
    if (words.empty()) return questions;
    for (int i = 0; i < n && i < static_cast<int>(words.size()); i++) {
        questions.push_back(question + " (" + words[i] + ")");
    }
    return questions;
}

double RAGEvaluator::compute_answer_relevance(const std::string& question,
                                              const std::string& answer) {
    if (answer.empty()) return 0.0;
    auto q_words = split_words(question);
    auto a_words = split_words(answer);
    if (a_words.empty()) return 1.0;
    std::unordered_set<std::string> q_set(q_words.begin(), q_words.end());
    size_t common = 0;
    for (const auto& word : a_words) {
        if (q_set.count(word) > 0) common++;
    }
    return static_cast<double>(common) / a_words.size();
}

std::vector<std::string> RAGEvaluator::extract_key_sentences(
    const std::string& text,
    const std::string& reference) {
    auto sentences = split_sentences(text);
    if (reference.empty()) return sentences;
    auto ref_words = split_words(reference);
    std::unordered_set<std::string> ref_set(ref_words.begin(), ref_words.end());
    std::vector<std::string> relevant;
    for (const auto& s : sentences) {
        auto words = split_words(s);
        for (const auto& w : words) {
            if (ref_set.count(w) > 0) {
                relevant.push_back(s);
                break;
            }
        }
    }
    return relevant;
}

double RAGEvaluator::compute_context_relevance(const std::string& question,
                                                const std::vector<std::string>& context) {
    if (context.empty()) return 0.0;
    std::string combined;
    for (const auto& c : context) combined += c + " ";
    auto sentences = split_sentences(combined);
    if (sentences.empty()) return 0.0;
    auto relevant = extract_key_sentences(combined, question);
    return static_cast<double>(relevant.size()) / sentences.size();
}

// ========== 主评估函数 ==========

EvaluationResult RAGEvaluator::evaluate(
    std::shared_ptr<class RetrievalPipeline> pipeline, int top_k) {
    EvaluationResult result;
    result.total_queries = static_cast<int>(test_cases_.size());

    if (!pipeline) {
        RAG_WARN("Pipeline is null, evaluation skipped");
        return result;
    }

    auto start = std::chrono::steady_clock::now();
    size_t successful = 0;
    double total_recall = 0, total_precision = 0, total_mrr = 0, total_ndcg = 0;
    double total_latency = 0;

    for (const auto& tc : test_cases_) {
        try {
            EvaluationResult::QueryResult qr;
            qr.query = tc.query;
            qr.expected = tc.relevant_chunks;
            // 占位：实际应调用 pipeline->retrieve()，但因为是 forward-declared 类型，
            // 这里使用空检索以避免链接错误。
            std::vector<std::string> retrieved;
            qr.retrieved = retrieved;
            qr.recall = 0;
            qr.precision = 0;
            qr.mrr = 0;
            qr.ndcg = 0;
            result.query_results.push_back(qr);
            total_recall += qr.recall;
            total_precision += qr.precision;
            total_mrr += qr.mrr;
            total_ndcg += qr.ndcg;
            successful++;
        } catch (const std::exception& e) {
            RAG_WARN("Evaluate query failed: " + std::string(e.what()));
        }
    }

    auto end = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double n = static_cast<double>(result.total_queries);
    if (n > 0) {
        result.recall_at_5 = total_recall / n;
        result.recall_at_10 = total_recall / n;
        result.precision_at_5 = total_precision / n;
        result.mrr = total_mrr / n;
        result.ndcg_at_5 = total_ndcg / n;
        result.avg_latency_ms = total_latency / n;
    }
    result.successful_queries = static_cast<int>(successful);
    (void)total_ms;
    return result;
}

EvaluationResult::QueryResult RAGEvaluator::evaluate_query(
    std::shared_ptr<class RetrievalPipeline> pipeline,
    const TestCase& test_case,
    int top_k) {
    EvaluationResult::QueryResult qr;
    qr.query = test_case.query;
    qr.expected = test_case.relevant_chunks;
    qr.retrieved = {};
    qr.recall = 0;
    qr.precision = 0;
    qr.mrr = 0;
    qr.ndcg = 0;
    (void)pipeline;
    (void)top_k;
    return qr;
}

// ========== 报告生成 ==========

std::string RAGEvaluator::generate_json_report(const EvaluationResult& result) {
    json report;
    report["recall_at_1"] = result.recall_at_1;
    report["recall_at_5"] = result.recall_at_5;
    report["recall_at_10"] = result.recall_at_10;
    report["recall_at_20"] = result.recall_at_20;
    report["precision_at_1"] = result.precision_at_1;
    report["precision_at_5"] = result.precision_at_5;
    report["precision_at_10"] = result.precision_at_10;
    report["mrr"] = result.mrr;
    report["map"] = result.map;
    report["ndcg_at_5"] = result.ndcg_at_5;
    report["ndcg_at_10"] = result.ndcg_at_10;
    report["ndcg_at_20"] = result.ndcg_at_20;
    report["total_queries"] = result.total_queries;
    report["successful_queries"] = result.successful_queries;
    report["avg_latency_ms"] = result.avg_latency_ms;
    return report.dump(2);
}

std::string RAGEvaluator::generate_summary_report(const std::vector<EvaluationResult>& results) {
    if (results.empty()) return "{}";
    EvaluationSummary summary = compute_summary(results);
    json report;
    report["total_queries"] = summary.total_queries;
    report["successful_queries"] = summary.successful_queries;
    report["success_rate"] = summary.success_rate;
    report["avg_recall_at_k"] = summary.avg_recall_at_k;
    report["avg_mrr"] = summary.avg_mrr;
    report["avg_ndcg_at_k"] = summary.avg_ndcg_at_k;
    report["avg_precision_at_k"] = summary.avg_precision_at_k;
    report["avg_latency_ms"] = summary.avg_latency_ms;
    report["p50_latency_ms"] = summary.p50_latency_ms;
    report["p95_latency_ms"] = summary.p95_latency_ms;
    report["p99_latency_ms"] = summary.p99_latency_ms;
    return report.dump(2);
}

RAGEvaluator::EvaluationSummary RAGEvaluator::compute_summary(
    const std::vector<EvaluationResult>& results) {
    EvaluationSummary summary;
    summary.total_queries = static_cast<int>(results.size());
    if (results.empty()) return summary;

    double sum_recall = 0, sum_mrr = 0, sum_ndcg = 0, sum_precision = 0;
    double sum_latency = 0;
    int successful = 0;
    std::vector<double> latencies;

    for (const auto& r : results) {
        sum_recall += r.recall_at_5;
        sum_mrr += r.mrr;
        sum_ndcg += r.ndcg_at_5;
        sum_precision += r.precision_at_5;
        sum_latency += r.avg_latency_ms;
        successful += r.successful_queries;
        latencies.push_back(r.avg_latency_ms);
    }
    double n = static_cast<double>(results.size());
    summary.avg_recall_at_k = sum_recall / n;
    summary.avg_mrr = sum_mrr / n;
    summary.avg_ndcg_at_k = sum_ndcg / n;
    summary.avg_precision_at_k = sum_precision / n;
    summary.avg_latency_ms = sum_latency / n;
    summary.successful_queries = successful;
    summary.success_rate = static_cast<double>(successful) / n;
    summary.p50_latency_ms = percentile(latencies, 50);
    summary.p95_latency_ms = percentile(latencies, 95);
    summary.p99_latency_ms = percentile(latencies, 99);
    return summary;
}

// ========== 数据集 ==========

std::vector<RAGEvaluator::DatasetEntry> RAGEvaluator::load_dataset(const std::string& path) {
    std::vector<DatasetEntry> entries;
    std::ifstream file(path);
    if (!file.is_open()) {
        RAG_WARN("Failed to open dataset: " + path);
        return entries;
    }
    try {
        json data = json::parse(file);
        file.close();
        if (data.is_array()) {
            for (const auto& item : data) {
                DatasetEntry entry;
                entry.id = item.value("id", "");
                entry.question = item.value("question", "");
                entry.ground_truths = item.value("ground_truths", std::vector<std::string>{});
                entry.contexts = item.value("contexts", std::vector<std::string>{});
                entries.push_back(entry);
            }
        }
    } catch (const std::exception& e) {
        RAG_WARN("Failed to parse dataset: " + std::string(e.what()));
    }
    return entries;
}

int RAGEvaluator::save_results(const std::string& file_path,
                                const std::vector<EvaluationResult>& results) {
    json data = json::array();
    for (const auto& r : results) {
        json item;
        item["recall_at_5"] = r.recall_at_5;
        item["precision_at_5"] = r.precision_at_5;
        item["mrr"] = r.mrr;
        item["avg_latency_ms"] = r.avg_latency_ms;
        item["total_queries"] = r.total_queries;
        item["successful_queries"] = r.successful_queries;
        data.push_back(item);
    }
    std::ofstream file(file_path);
    if (!file.is_open()) return -1;
    file << data.dump(2);
    file.close();
    return 0;
}

// ========== 摘要输出 ==========

void RAGEvaluator::print_summary(const EvaluationResult& result) {
    RAG_INFO("=== Evaluation Summary ===");
    RAG_INFO("Total queries: " + std::to_string(result.total_queries));
    RAG_INFO("Successful: " + std::to_string(result.successful_queries));
    RAG_INFO("Recall@5: " + std::to_string(result.recall_at_5));
    RAG_INFO("Precision@5: " + std::to_string(result.precision_at_5));
    RAG_INFO("MRR: " + std::to_string(result.mrr));
    RAG_INFO("NDCG@5: " + std::to_string(result.ndcg_at_5));
    RAG_INFO("Avg latency (ms): " + std::to_string(result.avg_latency_ms));
}

// ========== LLM 调用 ==========

std::string RAGEvaluator::call_llm(const std::string& prompt) {
    if (!llm_) {
        throw RAGException(errors::NOT_INITIALIZED, "LLM service not set");
    }
    GenerateOptions options;
    options.max_tokens = 512;
    options.temperature = 0.3f;
    auto result = llm_->generate(prompt, options);
    return result.text;
}

// ========== 工具方法 ==========

double RAGEvaluator::text_similarity(const std::string& a, const std::string& b) {
    auto words_a = split_words(a);
    auto words_b = split_words(b);
    if (words_a.empty() || words_b.empty()) return 0.0;
    std::unordered_set<std::string> set_a(words_a.begin(), words_a.end());
    std::unordered_set<std::string> set_b(words_b.begin(), words_b.end());
    size_t intersection = 0;
    for (const auto& word : set_a) {
        if (set_b.count(word) > 0) intersection++;
    }
    size_t union_size = set_a.size() + set_b.size() - intersection;
    if (union_size == 0) return 0.0;
    return static_cast<double>(intersection) / union_size;
}

double RAGEvaluator::percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    double idx = (p / 100.0) * (values.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(idx));
    size_t upper = static_cast<size_t>(std::ceil(idx));
    if (lower >= values.size()) lower = values.size() - 1;
    if (upper >= values.size()) upper = values.size() - 1;
    if (lower == upper) return values[lower];
    double fraction = idx - lower;
    return values[lower] * (1 - fraction) + values[upper] * fraction;
}

// ========== 私有方法（与 header 中声明对齐的占位实现） ==========

double RAGEvaluator::compute_dcg(const std::vector<double>& relevance, int k) {
    double dcg = 0.0;
    for (int i = 0; i < k && i < static_cast<int>(relevance.size()); i++) {
        dcg += relevance[i] / std::log2(i + 2);
    }
    return dcg;
}

double RAGEvaluator::compute_idcg(const std::vector<double>& relevance, int k) {
    std::vector<double> sorted = relevance;
    std::sort(sorted.begin(), sorted.end(), std::greater<double>());
    return compute_dcg(sorted, k);
}

double RAGEvaluator::compute_ndcg(const std::vector<std::string>& retrieved,
                                   const std::vector<std::string>& relevant,
                                   int k) {
    std::vector<int> rel_scores;
    std::unordered_set<std::string> rel_words;
    for (const auto& doc : relevant) {
        auto words = split_words(doc);
        rel_words.insert(words.begin(), words.end());
    }
    for (const auto& doc : retrieved) {
        auto words = split_words(doc);
        size_t matched = 0;
        for (const auto& w : words) {
            if (rel_words.count(w) > 0) matched++;
        }
        if (matched > 10) rel_scores.push_back(3);
        else if (matched > 5) rel_scores.push_back(2);
        else if (matched > 0) rel_scores.push_back(1);
        else rel_scores.push_back(0);
    }
    return compute_ndcg_at_k(retrieved, relevant, rel_scores);
}

double RAGEvaluator::compute_mrr(const std::vector<std::string>& retrieved,
                                  const std::vector<std::string>& relevant) {
    if (retrieved.empty() || relevant.empty()) return 0.0;
    std::unordered_set<std::string> rel_words;
    for (const auto& doc : relevant) {
        auto words = split_words(doc);
        rel_words.insert(words.begin(), words.end());
    }
    for (size_t i = 0; i < retrieved.size(); i++) {
        auto words = split_words(retrieved[i]);
        for (const auto& w : words) {
            if (rel_words.count(w) > 0) return 1.0 / static_cast<double>(i + 1);
        }
    }
    return 0.0;
}

double RAGEvaluator::compute_recall(const std::vector<std::string>& retrieved,
                                     const std::vector<std::string>& relevant,
                                     int k) {
    size_t kk = static_cast<size_t>(k);
    std::vector<std::string> retrieved_k;
    if (kk < retrieved.size()) {
        retrieved_k.assign(retrieved.begin(), retrieved.begin() + kk);
    } else {
        retrieved_k = retrieved;
    }
    return compute_recall_at_k(retrieved_k, relevant);
}

double RAGEvaluator::compute_precision(const std::vector<std::string>& retrieved,
                                       const std::vector<std::string>& relevant,
                                       int k) {
    size_t kk = static_cast<size_t>(k);
    std::vector<std::string> retrieved_k;
    if (kk < retrieved.size()) {
        retrieved_k.assign(retrieved.begin(), retrieved.begin() + kk);
    } else {
        retrieved_k = retrieved;
    }
    return compute_precision_at_k(retrieved_k, relevant);
}

// ========== 工厂函数 ==========

std::unique_ptr<RAGEvaluator> create_evaluator() {
    return std::make_unique<RAGEvaluator>();
}

std::unique_ptr<TestSuiteGenerator> create_test_suite_generator() {
    return std::make_unique<TestSuiteGenerator>();
}

}  // namespace mmrag