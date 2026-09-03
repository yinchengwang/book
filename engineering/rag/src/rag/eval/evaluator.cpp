/**
 * @file evaluator.cpp
 * @brief RAG 评估器实现
 */

#include "rag/evaluator.h"
#include "rag/pipeline.h"
#include "rag/logger.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <set>
#include <unordered_set>

// JSON 解析
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace rag {

// ========== RAGEvaluator 实现 ==========

RAGEvaluator::RAGEvaluator() = default;

void RAGEvaluator::add_test_case(const TestCase& test_case) {
    test_cases_.push_back(test_case);
}

void RAGEvaluator::add_test_cases(const std::vector<TestCase>& test_cases) {
    test_cases_.insert(test_cases_.end(), test_cases.begin(), test_cases.end());
}

bool RAGEvaluator::load_test_suite(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        RAG_ERROR("Failed to open test suite: " + file_path);
        return false;
    }

    try {
        json data = json::parse(file);
        file.close();

        if (!data.is_array()) {
            RAG_ERROR("Test suite must be a JSON array");
            return false;
        }

        test_cases_.clear();
        for (const auto& item : data) {
            TestCase tc;
            tc.query = item.value("query", "");
            tc.description = item.value("description", "");
            tc.category = item.value("category", "");

            if (item.contains("relevant_chunks")) {
                for (const auto& chunk : item["relevant_chunks"]) {
                    tc.relevant_chunks.push_back(chunk.get<std::string>());
                }
            }
            test_cases_.push_back(tc);
        }
        return true;
    } catch (const std::exception& e) {
        RAG_ERROR("Failed to parse test suite: " + std::string(e.what()));
        return false;
    }
}

EvaluationResult RAGEvaluator::evaluate(
    std::shared_ptr<RetrievalPipeline> pipeline,
    int top_k) {

    EvaluationResult result;
    result.total_queries = static_cast<int>(test_cases_.size());

    std::vector<double> latencies;
    std::vector<double> recall_scores;
    std::vector<double> precision_scores;
    std::vector<double> mrr_scores;
    std::vector<double> ndcg_scores;

    for (const auto& test_case : test_cases_) {
        auto query_result = evaluate_query(pipeline, test_case, top_k);

        EvaluationResult::QueryResult qr;
        qr.query = test_case.query;
        qr.recall = query_result.recall;
        qr.precision = query_result.precision;
        qr.mrr = query_result.mrr;
        qr.ndcg = query_result.ndcg;
        qr.retrieved = query_result.retrieved;
        qr.expected = test_case.relevant_chunks;
        qr.relevance_scores = query_result.relevance_scores;

        result.query_results.push_back(qr);

        if (!query_result.retrieved.empty()) {
            result.successful_queries++;
            recall_scores.push_back(query_result.recall);
            precision_scores.push_back(query_result.precision);
            mrr_scores.push_back(query_result.mrr);
            ndcg_scores.push_back(query_result.ndcg);
        }
    }

    // 计算平均值
    auto avg = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0;
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    };

    // 计算不同 K 值的召回率
    std::vector<double> recall_at_1, recall_at_5, recall_at_10, recall_at_20;
    std::vector<double> precision_at_1, precision_at_5, precision_at_10;
    std::vector<double> ndcg_at_5, ndcg_at_10, ndcg_at_20;

    for (size_t i = 0; i < result.query_results.size(); ++i) {
        const auto& qr = result.query_results[i];
        const auto& tc = test_cases_[i];

        // 召回率
        if (qr.retrieved.size() >= 1) {
            recall_at_1.push_back(compute_recall(qr.retrieved, tc.relevant_chunks, 1));
        }
        if (qr.retrieved.size() >= 5) {
            recall_at_5.push_back(compute_recall(qr.retrieved, tc.relevant_chunks, 5));
            precision_at_5.push_back(compute_precision(qr.retrieved, tc.relevant_chunks, 5));
            ndcg_at_5.push_back(compute_ndcg(qr.retrieved, tc.relevant_chunks, 5));
        }
        if (qr.retrieved.size() >= 10) {
            recall_at_10.push_back(compute_recall(qr.retrieved, tc.relevant_chunks, 10));
            precision_at_10.push_back(compute_precision(qr.retrieved, tc.relevant_chunks, 10));
            ndcg_at_10.push_back(compute_ndcg(qr.retrieved, tc.relevant_chunks, 10));
        }
        if (qr.retrieved.size() >= 20) {
            recall_at_20.push_back(compute_recall(qr.retrieved, tc.relevant_chunks, 20));
            ndcg_at_20.push_back(compute_ndcg(qr.retrieved, tc.relevant_chunks, 20));
        }
        if (!qr.retrieved.empty()) {
            precision_at_1.push_back(compute_precision(qr.retrieved, tc.relevant_chunks, 1));
        }
    }

    result.recall_at_1 = avg(recall_at_1);
    result.recall_at_5 = avg(recall_at_5);
    result.recall_at_10 = avg(recall_at_10);
    result.recall_at_20 = avg(recall_at_20);

    result.precision_at_1 = avg(precision_at_1);
    result.precision_at_5 = avg(precision_at_5);
    result.precision_at_10 = avg(precision_at_10);

    result.mrr = avg(mrr_scores);
    result.ndcg_at_5 = avg(ndcg_at_5);
    result.ndcg_at_10 = avg(ndcg_at_10);
    result.ndcg_at_20 = avg(ndcg_at_20);

    result.avg_latency_ms = avg(latencies);

    // 计算 MAP
    double map_sum = 0.0;
    int map_count = 0;
    for (size_t i = 0; i < result.query_results.size(); ++i) {
        const auto& qr = result.query_results[i];
        const auto& tc = test_cases_[i];

        if (qr.retrieved.empty() || tc.relevant_chunks.empty()) {
            continue;
        }

        // 计算 Average Precision for this query
        double ap = 0.0;
        int relevant_found = 0;
        std::set<std::string> found;

        for (size_t j = 0; j < qr.retrieved.size() && j < static_cast<size_t>(20); ++j) {
            const auto& doc = qr.retrieved[j];
            // 检查是否相关（简单字符串匹配）
            for (const auto& rel : tc.relevant_chunks) {
                if (doc.find(rel) != std::string::npos || rel.find(doc) != std::string::npos) {
                    if (found.find(doc) == found.end()) {
                        found.insert(doc);
                        relevant_found++;
                        ap += static_cast<double>(relevant_found) / (j + 1);
                    }
                    break;
                }
            }
        }

        if (relevant_found > 0) {
            ap /= std::min(static_cast<size_t>(relevant_found), qr.retrieved.size());
            map_sum += ap;
            map_count++;
        }
    }
    result.map = map_count > 0 ? map_sum / map_count : 0.0;

    return result;
}

EvaluationResult::QueryResult RAGEvaluator::evaluate_query(
    std::shared_ptr<RetrievalPipeline> pipeline,
    const TestCase& test_case,
    int top_k) {

    EvaluationResult::QueryResult result;
    result.query = test_case.query;

    try {
        auto pipeline_result = pipeline->execute(test_case.query, top_k);

        // 提取检索到的文档
        for (const auto& item : pipeline_result.items) {
            result.retrieved.push_back(item.chunk.content);
        }

        // 计算各项指标
        result.recall = compute_recall(result.retrieved, test_case.relevant_chunks, top_k);
        result.precision = compute_precision(result.retrieved, test_case.relevant_chunks, top_k);
        result.mrr = compute_mrr(result.retrieved, test_case.relevant_chunks);
        result.ndcg = compute_ndcg(result.retrieved, test_case.relevant_chunks, top_k);

    } catch (const std::exception& e) {
        RAG_WARN("Query evaluation failed: " + std::string(e.what()));
    }

    return result;
}

bool RAGEvaluator::save_results(const std::string& file_path, const EvaluationResult& result) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        RAG_ERROR("Failed to open result file: " + file_path);
        return false;
    }

    json output;
    output["total_queries"] = result.total_queries;
    output["successful_queries"] = result.successful_queries;
    output["avg_latency_ms"] = result.avg_latency_ms;

    // 召回率
    output["recall_at_1"] = result.recall_at_1;
    output["recall_at_5"] = result.recall_at_5;
    output["recall_at_10"] = result.recall_at_10;
    output["recall_at_20"] = result.recall_at_20;

    // 精确率
    output["precision_at_1"] = result.precision_at_1;
    output["precision_at_5"] = result.precision_at_5;
    output["precision_at_10"] = result.precision_at_10;

    // 排名指标
    output["mrr"] = result.mrr;
    output["map"] = result.map;
    output["ndcg_at_5"] = result.ndcg_at_5;
    output["ndcg_at_10"] = result.ndcg_at_10;
    output["ndcg_at_20"] = result.ndcg_at_20;

    // 详细结果
    json query_results = json::array();
    for (const auto& qr : result.query_results) {
        json item;
        item["query"] = qr.query;
        item["recall"] = qr.recall;
        item["precision"] = qr.precision;
        item["mrr"] = qr.mrr;
        item["ndcg"] = qr.ndcg;
        item["retrieved"] = qr.retrieved;
        item["expected"] = qr.expected;
        item["relevance_scores"] = qr.relevance_scores;
        query_results.push_back(item);
    }
    output["query_results"] = query_results;

    file << output.dump(2);
    file.close();
    return true;
}

void RAGEvaluator::print_summary(const EvaluationResult& result) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "         RAG Evaluation Summary         \n";
    std::cout << "========================================\n";
    std::cout << "\n";

    std::cout << "  Total Queries:        " << result.total_queries << "\n";
    std::cout << "  Successful Queries:   " << result.successful_queries << "\n";
    std::cout << "  Avg Latency:          " << result.avg_latency_ms << " ms\n";
    std::cout << "\n";

    std::cout << "  ----- Recall -----\n";
    std::cout << "  Recall@1:             " << (result.recall_at_1 * 100) << "%\n";
    std::cout << "  Recall@5:             " << (result.recall_at_5 * 100) << "%\n";
    std::cout << "  Recall@10:            " << (result.recall_at_10 * 100) << "%\n";
    std::cout << "  Recall@20:            " << (result.recall_at_20 * 100) << "%\n";
    std::cout << "\n";

    std::cout << "  ----- Precision -----\n";
    std::cout << "  Precision@1:          " << (result.precision_at_1 * 100) << "%\n";
    std::cout << "  Precision@5:          " << (result.precision_at_5 * 100) << "%\n";
    std::cout << "  Precision@10:         " << (result.precision_at_10 * 100) << "%\n";
    std::cout << "\n";

    std::cout << "  ----- Ranking Metrics -----\n";
    std::cout << "  MRR:                  " << result.mrr << "\n";
    std::cout << "  MAP:                  " << result.map << "\n";
    std::cout << "  NDCG@5:               " << result.ndcg_at_5 << "\n";
    std::cout << "  NDCG@10:              " << result.ndcg_at_10 << "\n";
    std::cout << "  NDCG@20:              " << result.ndcg_at_20 << "\n";
    std::cout << "\n";

    std::cout << "========================================\n";
}

// ========== 指标计算实现 ==========

double RAGEvaluator::compute_dcg(const std::vector<double>& relevance, int k) {
    double dcg = 0.0;
    int limit = std::min(static_cast<int>(relevance.size()), k);
    for (int i = 0; i < limit; ++i) {
        dcg += relevance[i] / std::log2(i + 2);  // i+2 because log2(1) = 0
    }
    return dcg;
}

double RAGEvaluator::compute_idcg(const std::vector<double>& relevance, int k) {
    std::vector<double> sorted_relevance = relevance;
    std::sort(sorted_relevance.begin(), sorted_relevance.end(), std::greater<double>());
    return compute_dcg(sorted_relevance, k);
}

double RAGEvaluator::compute_ndcg(const std::vector<std::string>& retrieved,
                                  const std::vector<std::string>& relevant,
                                  int k) {
    if (retrieved.empty() || relevant.empty()) {
        return 0.0;
    }

    // 计算相关性分数
    std::vector<double> relevance_scores;
    std::unordered_set<std::string> relevant_set(relevant.begin(), relevant.end());

    for (const auto& doc : retrieved) {
        double score = 0.0;
        for (const auto& rel : relevant) {
            // 基于字符串相似度计算相关性
            double similarity = 0.0;
            size_t match_count = 0;

            std::istringstream iss_doc(doc);
            std::istringstream iss_rel(rel);
            std::string word;
            std::set<std::string> doc_words, rel_words;

            while (iss_doc >> word) {
                doc_words.insert(word);
            }
            while (iss_rel >> word) {
                rel_words.insert(word);
            }

            for (const auto& w : doc_words) {
                if (rel_words.count(w) > 0) {
                    match_count++;
                }
            }

            if (!doc_words.empty() && !rel_words.empty()) {
                similarity = static_cast<double>(match_count) /
                            std::max(doc_words.size(), rel_words.size());
            }

            score = std::max(score, similarity);
        }
        relevance_scores.push_back(score);
    }

    int limit = std::min(k, static_cast<int>(relevance_scores.size()));
    double dcg = compute_dcg(relevance_scores, limit);
    double idcg = compute_idcg(relevance_scores, limit);

    if (idcg == 0.0) {
        return 0.0;
    }

    return dcg / idcg;
}

double RAGEvaluator::compute_mrr(const std::vector<std::string>& retrieved,
                                  const std::vector<std::string>& relevant) {
    if (retrieved.empty() || relevant.empty()) {
        return 0.0;
    }

    // 将真实结果转换为集合
    std::unordered_set<std::string> relevant_set;
    for (const auto& rel : relevant) {
        std::istringstream iss(rel);
        std::string word;
        while (iss >> word) {
            relevant_set.insert(word);
        }
    }

    // 找到第一个相关文档的位置
    for (size_t i = 0; i < retrieved.size(); ++i) {
        std::istringstream iss(retrieved[i]);
        std::string word;
        bool is_relevant = false;

        while (iss >> word) {
            if (relevant_set.count(word) > 0) {
                is_relevant = true;
                break;
            }
        }

        if (is_relevant) {
            return 1.0 / (i + 1);
        }
    }

    return 0.0;
}

double RAGEvaluator::compute_recall(const std::vector<std::string>& retrieved,
                                    const std::vector<std::string>& relevant,
                                    int k) {
    if (retrieved.empty() || relevant.empty()) {
        return 0.0;
    }

    int limit = std::min(k, static_cast<int>(retrieved.size()));

    // 将相关文档转换为单词集合
    std::set<std::string> relevant_words;
    for (const auto& rel : relevant) {
        std::istringstream iss(rel);
        std::string word;
        while (iss >> word) {
            relevant_words.insert(word);
        }
    }

    if (relevant_words.empty()) {
        return 0.0;
    }

    // 计算检索到的相关文档数
    int relevant_found = 0;
    for (int i = 0; i < limit; ++i) {
        std::set<std::string> doc_words;
        std::istringstream iss(retrieved[i]);
        std::string word;
        while (iss >> word) {
            doc_words.insert(word);
        }

        // 检查是否有交集
        bool is_relevant = false;
        for (const auto& w : doc_words) {
            if (relevant_words.count(w) > 0) {
                is_relevant = true;
                break;
            }
        }
        if (is_relevant) {
            relevant_found++;
        }
    }

    // 返回召回率（使用唯一相关文档数）
    return static_cast<double>(relevant_found) / relevant.size();
}

double RAGEvaluator::compute_precision(const std::vector<std::string>& retrieved,
                                       const std::vector<std::string>& relevant,
                                       int k) {
    if (retrieved.empty()) {
        return 0.0;
    }

    int limit = std::min(k, static_cast<int>(retrieved.size()));
    if (limit == 0) {
        return 0.0;
    }

    // 将相关文档转换为单词集合
    std::set<std::string> relevant_words;
    for (const auto& rel : relevant) {
        std::istringstream iss(rel);
        std::string word;
        while (iss >> word) {
            relevant_words.insert(word);
        }
    }

    if (relevant_words.empty()) {
        return 0.0;
    }

    // 计算检索到的相关文档数
    int relevant_found = 0;
    for (int i = 0; i < limit; ++i) {
        std::set<std::string> doc_words;
        std::istringstream iss(retrieved[i]);
        std::string word;
        while (iss >> word) {
            doc_words.insert(word);
        }

        // 检查是否有交集
        for (const auto& w : doc_words) {
            if (relevant_words.count(w) > 0) {
                relevant_found++;
                break;  // 每个文档只计算一次
            }
        }
    }

    return static_cast<double>(relevant_found) / limit;
}

// ========== 测试集生成器实现 ==========

std::vector<TestCase> TestSuiteGenerator::generate_from_docs(
    const std::string& docs_path,
    int num_queries) {
    // 简化实现：返回空测试用例
    (void)docs_path;
    (void)num_queries;
    return {};
}

std::vector<TestCase> TestSuiteGenerator::generate_synthetic(
    const std::string& domain,
    int num_queries) {
    // 简化实现：返回空测试用例
    (void)domain;
    (void)num_queries;
    return {};
}

// ========== 工厂函数 ==========

std::unique_ptr<RAGEvaluator> create_evaluator() {
    return std::make_unique<RAGEvaluator>();
}

std::unique_ptr<TestSuiteGenerator> create_test_suite_generator() {
    return std::make_unique<TestSuiteGenerator>();
}

}  // namespace rag