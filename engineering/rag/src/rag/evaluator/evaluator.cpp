/**
 * @file evaluator.cpp
 * @brief RAG 评估器实现
 */

#include "rag/evaluator.h"
#include "rag/logger.h"
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

namespace rag {

// ========== 常量定义 ==========

// 默认 prompt 模板
static const char* FAITHFULNESS_PROMPT = R"(
给定以下上下文和答案，请评估答案对上下文的忠实程度。

上下文:
{context}

答案:
{answer}

请判断答案中的每个陈述是否能从上下文中推导出来。
如果所有陈述都能从上下文推导，则忠实度为 1.0。
如果没有任何陈述能从上下文推导，则忠实度为 0.0。

请以 JSON 格式返回结果:
{{"faithfulness": 0.0-1.0, "reasoning": "简短解释"}}
)";

static const char* ANSWER_RELEVANCE_PROMPT = R"(
给定以下问题和答案，请评估答案对问题的相关性。

问题:
{question}

答案:
{answer}

请生成 3 个与答案相关的问题变体，用于评估答案与原问题的相关性。
这些问题应该能够从原答案中自然地推导出来。

请以 JSON 格式返回:
{{"related_questions": ["问题1", "问题2", "问题3"]}}
)";

static const char* CONTEXT_RELEVANCE_PROMPT = R"(
给定以下问题和上下文，请评估上下文对回答问题的相关性。

问题:
{question}

上下文:
{context}

请识别上下文中与问题相关的关键句子。
返回相关句子占所有句子的比例。

请以 JSON 格式返回:
{{"relevant_sentences": ["句子1", "句子2"], "total_sentences": 10, "relevance": 0.0-1.0}}
)";

// ========== 工具函数 ==========

/**
 * @brief 分割文本为句子
 */
static std::vector<std::string> split_sentences(const std::string& text) {
    std::vector<std::string> sentences;
    std::istringstream iss(text);
    std::string sentence;

    while (std::getline(iss, sentence)) {
        // 去除首尾空白
        while (!sentence.empty() && std::isspace(sentence.front())) {
            sentence.erase(sentence.begin());
        }
        while (!sentence.empty() && std::isspace(sentence.back())) {
            sentence.pop_back();
        }

        if (!sentence.empty()) {
            sentences.push_back(sentence);
        }
    }

    return sentences;
}

/**
 * @brief 分割文本为单词
 */
static std::vector<std::string> split_words(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string word;

    while (iss >> word) {
        words.push_back(word);
    }

    return words;
}

/**
 * @brief 计算集合交集大小
 */
static size_t set_intersection_size(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b) {

    std::unordered_set<std::string> set_a(a.begin(), a.end());
    size_t count = 0;
    for (const auto& item : b) {
        if (set_a.count(item) > 0) {
            count++;
        }
    }
    return count;
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
    weight_faithfulness_ = faithfulness;
    weight_answer_relevance_ = answer_relevance;
    weight_context_relevance_ = context_relevance;
}

// ========== 检索指标计算 ==========

double RAGEvaluator::compute_recall_at_k(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& ground_truths) {

    if (retrieved.empty() || ground_truths.empty()) {
        return 0.0;
    }

    // 将检索结果和真实结果分割为单词集合
    std::unordered_set<std::string> retrieved_words;
    for (const auto& doc : retrieved) {
        auto words = split_words(doc);
        retrieved_words.insert(words.begin(), words.end());
    }

    std::unordered_set<std::string> gt_words;
    for (const auto& doc : ground_truths) {
        auto words = split_words(doc);
        gt_words.insert(words.begin(), words.end());
    }

    // 计算交集
    size_t intersection = 0;
    for (const auto& word : gt_words) {
        if (retrieved_words.count(word) > 0) {
            intersection++;
        }
    }

    return static_cast<double>(intersection) / gt_words.size();
}

double RAGEvaluator::compute_mrr(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& ground_truths) {

    if (retrieved.empty() || ground_truths.empty()) {
        return 0.0;
    }

    // 将真实结果转换为集合
    std::unordered_set<std::string> gt_set;
    for (const auto& gt : ground_truths) {
        auto words = split_words(gt);
        gt_set.insert(words.begin(), words.end());
    }

    // 找到第一个相关文档的位置
    for (size_t i = 0; i < retrieved.size(); i++) {
        auto words = split_words(retrieved[i]);
        std::unordered_set<std::string> doc_set(words.begin(), words.end());

        // 检查是否有交集
        bool is_relevant = false;
        for (const auto& word : doc_set) {
            if (gt_set.count(word) > 0) {
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

double RAGEvaluator::compute_ndcg_at_k(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& ground_truths,
    const std::vector<int>& relevance_scores) {

    if (retrieved.empty() || ground_truths.empty()) {
        return 0.0;
    }

    // 如果没有提供相关性分数，使用默认分数（基于与 ground truth 的相似度）
    std::vector<int> scores;
    if (relevance_scores.empty()) {
        std::unordered_set<std::string> gt_words;
        for (const auto& gt : ground_truths) {
            auto words = split_words(gt);
            gt_words.insert(words.begin(), words.end());
        }

        for (const auto& doc : retrieved) {
            auto words = split_words(doc);
            size_t matched = 0;
            for (const auto& word : words) {
                if (gt_words.count(word) > 0) {
                    matched++;
                }
            }
            // 相关性分数：0-3
            if (matched > 10) {
                scores.push_back(3);
            } else if (matched > 5) {
                scores.push_back(2);
            } else if (matched > 0) {
                scores.push_back(1);
            } else {
                scores.push_back(0);
            }
        }
    } else {
        scores = relevance_scores;
    }

    // 限制到 K
    size_t k = std::min(static_cast<size_t>(retrieval_k_), scores.size());

    // 计算 DCG
    double dcg = 0.0;
    for (size_t i = 0; i < k; i++) {
        dcg += scores[i] / std::log2(i + 2);  // i+2 因为 log2(1) = 0
    }

    // 计算 IDCG（理想 DCG）
    std::vector<int> sorted_scores = scores;
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<int>());

    double idcg = 0.0;
    for (size_t i = 0; i < k; i++) {
        idcg += sorted_scores[i] / std::log2(i + 2);
    }

    if (idcg == 0.0) {
        return 0.0;
    }

    return dcg / idcg;
}

double RAGEvaluator::compute_precision_at_k(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& ground_truths) {

    if (retrieved.empty()) {
        return 0.0;
    }

    size_t k = std::min(static_cast<size_t>(retrieval_k_), retrieved.size());

    std::unordered_set<std::string> gt_words;
    for (const auto& gt : ground_truths) {
        auto words = split_words(gt);
        gt_words.insert(words.begin(), words.end());
    }

    size_t relevant = 0;
    for (size_t i = 0; i < k; i++) {
        auto words = split_words(retrieved[i]);
        for (const auto& word : words) {
            if (gt_words.count(word) > 0) {
                relevant++;
                break;  // 每个文档只计算一次
            }
        }
    }

    return static_cast<double>(relevant) / k;
}

RetrievalMetrics RAGEvaluator::evaluate_retrieval(
    const std::vector<std::string>& retrieved,
    const std::vector<std::string>& ground_truths,
    const std::vector<int>& relevance_scores) {

    RetrievalMetrics metrics;
    metrics.recall_at_k = compute_recall_at_k(retrieved, ground_truths);
    metrics.mrr = compute_mrr(retrieved, ground_truths);
    metrics.ndcg_at_k = compute_ndcg_at_k(retrieved, ground_truths, relevance_scores);
    metrics.precision_at_k = compute_precision_at_k(retrieved, ground_truths);
    return metrics;
}

// ========== RAGAS 指标计算 ==========

double RAGEvaluator::compute_faithfulness(const std::string& answer,
                                           const std::vector<std::string>& contexts) {
    // 如果没有 LLM，使用简化实现
    if (!llm_) {
        // 简化的忠实度计算：检查答案中的关键词是否在上下文中
        auto answer_words = split_words(answer);
        std::unordered_set<std::string> context_words;

        for (const auto& ctx : contexts) {
            auto words = split_words(ctx);
            context_words.insert(words.begin(), words.end());
        }

        size_t found = 0;
        for (const auto& word : answer_words) {
            if (context_words.count(word) > 0) {
                found++;
            }
        }

        return answer_words.empty() ? 1.0 :
               static_cast<double>(found) / answer_words.size();
    }

    // 使用 LLM 评估
    std::string context_combined;
    for (const auto& ctx : contexts) {
        context_combined += ctx + "\n\n";
    }

    std::string prompt = FAITHFULNESS_PROMPT;
    // 简单的占位符替换
    size_t ctx_pos = prompt.find("{context}");
    if (ctx_pos != std::string::npos) {
        prompt.replace(ctx_pos, 9, context_combined);
    }
    size_t ans_pos = prompt.find("{answer}");
    if (ans_pos != std::string::npos) {
        prompt.replace(ans_pos, 9, answer);
    }

    try {
        auto response = call_llm(prompt);

        // 解析 JSON 响应
        auto res_json = json::parse(response);
        return res_json.value("faithfulness", 0.5);
    } catch (const std::exception& e) {
        RAG_WARN("Faithfulness calculation failed: " + std::string(e.what()));
        return 0.5;  // 默认值
    }
}

std::vector<std::string> RAGEvaluator::generate_related_questions(
    const std::string& answer, int count) {

    if (!llm_) {
        return {};  // 没有 LLM 无法生成
    }

    std::string prompt = ANSWER_RELEVANCE_PROMPT;
    size_t q_pos = prompt.find("{question}");
    if (q_pos != std::string::npos) {
        prompt.replace(q_pos, 11, "[由答案推断的问题]");
    }
    size_t a_pos = prompt.find("{answer}");
    if (a_pos != std::string::npos) {
        prompt.replace(a_pos, 9, answer);
    }

    try {
        auto response = call_llm(prompt);
        auto res_json = json::parse(response);

        std::vector<std::string> questions;
        if (res_json.contains("related_questions")) {
            for (const auto& q : res_json["related_questions"]) {
                questions.push_back(q.get<std::string>());
            }
        }
        return questions;
    } catch (const std::exception& e) {
        RAG_WARN("Question generation failed: " + std::string(e.what()));
        return {};
    }
}

double RAGEvaluator::compute_answer_relevance(const std::string& question,
                                               const std::string& answer) {
    if (!llm_) {
        // 简化的相关性计算
        auto q_words = split_words(question);
        auto a_words = split_words(answer);

        std::unordered_set<std::string> q_set(q_words.begin(), q_words.end());

        size_t common = 0;
        for (const auto& word : a_words) {
            if (q_set.count(word) > 0) {
                common++;
            }
        }

        return a_words.empty() ? 1.0 :
               static_cast<double>(common) / a_words.size();
    }

    // 生成相关问题
    auto related = generate_related_questions(answer);
    if (related.empty()) {
        return 0.5;  // 默认值
    }

    // 计算原始问题与生成问题的相似度
    auto q_words = split_words(question);
    std::vector<double> similarities;

    for (const auto& rel_q : related) {
        auto rel_words = split_words(rel_q);
        similarities.push_back(text_similarity(question, rel_q));
    }

    // 返回平均相似度
    double sum = std::accumulate(similarities.begin(), similarities.end(), 0.0);
    return sum / similarities.size();
}

std::vector<std::string> RAGEvaluator::extract_key_sentences(
    const std::string& context, const std::string& question) {

    if (!llm_) {
        // 简化实现：返回所有句子
        return split_sentences(context);
    }

    std::string prompt = CONTEXT_RELEVANCE_PROMPT;
    size_t q_pos = prompt.find("{question}");
    if (q_pos != std::string::npos) {
        prompt.replace(q_pos, 11, question);
    }
    size_t c_pos = prompt.find("{context}");
    if (c_pos != std::string::npos) {
        prompt.replace(c_pos, 9, context);
    }

    try {
        auto response = call_llm(prompt);
        auto res_json = json::parse(response);

        std::vector<std::string> sentences;
        if (res_json.contains("relevant_sentences")) {
            for (const auto& s : res_json["relevant_sentences"]) {
                sentences.push_back(s.get<std::string>());
            }
        }
        return sentences;
    } catch (const std::exception& e) {
        RAG_WARN("Sentence extraction failed: " + std::string(e.what()));
        return split_sentences(context);
    }
}

double RAGEvaluator::compute_context_relevance(const std::string& question,
                                                const std::vector<std::string>& contexts) {
    if (contexts.empty()) {
        return 0.0;
    }

    // 合并所有上下文
    std::string combined_context;
    for (const auto& ctx : contexts) {
        combined_context += ctx + " ";
    }

    auto sentences = split_sentences(combined_context);
    if (sentences.empty()) {
        return 0.0;
    }

    // 使用 LLM 或简化方法提取关键句子
    auto key_sentences = extract_key_sentences(combined_context, question);

    // 计算相关性
    return static_cast<double>(key_sentences.size()) / sentences.size();
}

RAGEvaluation RAGEvaluator::evaluate_generation(
    const std::string& question,
    const std::string& answer,
    const std::vector<std::string>& contexts) {

    RAGEvaluation eval;
    eval.faithfulness = compute_faithfulness(answer, contexts);
    eval.answer_relevance = compute_answer_relevance(question, answer);
    eval.context_relevance = compute_context_relevance(question, contexts);

    // 计算综合得分
    eval.ragas_score =
        weight_faithfulness_ * eval.faithfulness +
        weight_answer_relevance_ * eval.answer_relevance +
        weight_context_relevance_ * eval.context_relevance;

    return eval;
}

// ========== 单次评估 ==========

EvaluationResult RAGEvaluator::evaluate(
    const std::string& question,
    const std::string& answer,
    const std::vector<std::string>& contexts,
    const std::vector<std::string>& ground_truths) {

    auto start = std::chrono::steady_clock::now();

    EvaluationResult result;
    result.question = question;
    result.answer = answer;
    result.contexts = contexts;
    result.ground_truths = ground_truths;

    try {
        // 评估检索质量
        result.retrieval = evaluate_retrieval(contexts, ground_truths);

        // 评估生成质量
        result.ragas = evaluate_generation(question, answer, contexts);

        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        RAG_ERROR("Evaluation failed: " + std::string(e.what()));
    }

    auto end = std::chrono::steady_clock::now();
    result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

// ========== 数据集评估 ==========

std::vector<EvaluationResult> RAGEvaluator::evaluate_dataset(
    const std::vector<DatasetEntry>& entries) {

    std::vector<EvaluationResult> results;
    results.reserve(entries.size());

    for (const auto& entry : entries) {
        // 如果没有提供 contexts，需要通过 RAG 引擎检索
        // 这里简化处理，假设 contexts 已经提供
        if (entry.contexts.empty()) {
            RAG_WARN("Dataset entry " + entry.id + " has no contexts, skipping");
            continue;
        }

        auto result = evaluate(
            entry.question,
            "",  // 答案应该通过 RAG 引擎生成
            entry.contexts,
            entry.ground_truths);

        results.push_back(result);
    }

    return results;
}

// ========== 报告生成 ==========

std::string RAGEvaluator::generate_json_report(const EvaluationResult& result) {
    json report;

    report["question"] = result.question;
    report["answer"] = result.answer;
    report["contexts"] = result.contexts;
    report["ground_truths"] = result.ground_truths;
    report["success"] = result.success;
    report["latency_ms"] = result.latency_ms;

    if (!result.success) {
        report["error"] = result.error_message;
    }

    // 检索指标
    report["retrieval"] = {
        {"recall_at_k", result.retrieval.recall_at_k},
        {"mrr", result.retrieval.mrr},
        {"ndcg_at_k", result.retrieval.ndcg_at_k},
        {"precision_at_k", result.retrieval.precision_at_k}
    };

    // RAGAS 指标
    report["ragas"] = {
        {"faithfulness", result.ragas.faithfulness},
        {"answer_relevance", result.ragas.answer_relevance},
        {"context_relevance", result.ragas.context_relevance},
        {"ragas_score", result.ragas.ragas_score}
    };

    return report.dump(2);
}

std::string RAGEvaluator::generate_summary_report(
    const std::vector<EvaluationResult>& results) {

    auto summary = compute_summary(results);

    json report;
    report["total_queries"] = summary.total_queries;
    report["successful_queries"] = summary.successful_queries;
    report["success_rate"] = summary.success_rate;

    // 检索指标
    report["retrieval"] = {
        {"avg_recall_at_k", summary.avg_recall_at_k},
        {"avg_mrr", summary.avg_mrr},
        {"avg_ndcg_at_k", summary.avg_ndcg_at_k},
        {"avg_precision_at_k", summary.avg_precision_at_k}
    };

    // RAGAS 指标
    report["ragas"] = {
        {"avg_faithfulness", summary.avg_faithfulness},
        {"avg_answer_relevance", summary.avg_answer_relevance},
        {"avg_context_relevance", summary.avg_context_relevance},
        {"avg_ragas_score", summary.avg_ragas_score}
    };

    // 延迟统计
    report["latency"] = {
        {"avg_ms", summary.avg_latency_ms},
        {"p50_ms", summary.p50_latency_ms},
        {"p95_ms", summary.p95_latency_ms},
        {"p99_ms", summary.p99_latency_ms}
    };

    return report.dump(2);
}

EvaluationSummary RAGEvaluator::compute_summary(
    const std::vector<EvaluationResult>& results) {

    EvaluationSummary summary;
    summary.total_queries = results.size();

    std::vector<double> latencies;
    std::vector<double> recall_scores, mrr_scores, ndcg_scores, precision_scores;
    std::vector<double> faithful_scores, answer_rel_scores, context_rel_scores, ragas_scores;

    for (const auto& r : results) {
        if (r.success) {
            summary.successful_queries++;

            recall_scores.push_back(r.retrieval.recall_at_k);
            mrr_scores.push_back(r.retrieval.mrr);
            ndcg_scores.push_back(r.retrieval.ndcg_at_k);
            precision_scores.push_back(r.retrieval.precision_at_k);

            faithful_scores.push_back(r.ragas.faithfulness);
            answer_rel_scores.push_back(r.ragas.answer_relevance);
            context_rel_scores.push_back(r.ragas.context_relevance);
            ragas_scores.push_back(r.ragas.ragas_score);

            latencies.push_back(static_cast<double>(r.latency_ms));
        }
    }

    summary.success_rate = summary.total_queries > 0
        ? static_cast<double>(summary.successful_queries) / summary.total_queries
        : 0.0;

    // 计算平均值
    auto avg = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0;
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    };

    summary.avg_recall_at_k = avg(recall_scores);
    summary.avg_mrr = avg(mrr_scores);
    summary.avg_ndcg_at_k = avg(ndcg_scores);
    summary.avg_precision_at_k = avg(precision_scores);
    summary.avg_faithfulness = avg(faithful_scores);
    summary.avg_answer_relevance = avg(answer_rel_scores);
    summary.avg_context_relevance = avg(context_rel_scores);
    summary.avg_ragas_score = avg(ragas_scores);
    summary.avg_latency_ms = avg(latencies);

    // 计算百分位数
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        summary.p50_latency_ms = percentile(latencies, 50);
        summary.p95_latency_ms = percentile(latencies, 95);
        summary.p99_latency_ms = percentile(latencies, 99);
    }

    return summary;
}

// ========== 数据集加载 ==========

std::vector<DatasetEntry> RAGEvaluator::load_dataset(const std::string& path) {
    std::vector<DatasetEntry> entries;

    std::ifstream file(path);
    if (!file.is_open()) {
        throw RAGException(errors::IO_ERROR, "Failed to open dataset: " + path);
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
        throw RAGException(errors::PARSE_ERROR,
            "Failed to parse dataset: " + std::string(e.what()));
    }

    return entries;
}

int RAGEvaluator::save_results(const std::string& path,
                                const std::vector<EvaluationResult>& results) {
    json data = json::array();

    for (const auto& r : results) {
        data.push_back({
            {"question", r.question},
            {"answer", r.answer},
            {"contexts", r.contexts},
            {"ground_truths", r.ground_truths},
            {"success", r.success},
            {"latency_ms", r.latency_ms},
            {"retrieval", {
                {"recall_at_k", r.retrieval.recall_at_k},
                {"mrr", r.retrieval.mrr},
                {"ndcg_at_k", r.retrieval.ndcg_at_k}
            }},
            {"ragas", {
                {"faithfulness", r.ragas.faithfulness},
                {"answer_relevance", r.ragas.answer_relevance},
                {"context_relevance", r.ragas.context_relevance},
                {"ragas_score", r.ragas.ragas_score}
            }}
        });
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return -1;
    }

    file << data.dump(2);
    file.close();

    return 0;
}

// ========== LLM 调用 ==========

std::string RAGEvaluator::call_llm(const std::string& prompt) {
    if (!llm_) {
        throw RAGException(errors::NOT_INITIALIZED, "LLM service not set");
    }

    GenerateOptions options;
    options.max_tokens = 512;
    options.temperature = 0.3f;  // 低温度以获得更确定的输出

    auto result = llm_->generate(prompt, options);
    return result.text;
}

// ========== 工具方法 ==========

double RAGEvaluator::text_similarity(const std::string& a, const std::string& b) {
    auto words_a = split_words(a);
    auto words_b = split_words(b);

    if (words_a.empty() || words_b.empty()) {
        return 0.0;
    }

    std::unordered_set<std::string> set_a(words_a.begin(), words_a.end());
    std::unordered_set<std::string> set_b(words_b.begin(), words_b.end());

    // Jaccard 相似度
    size_t intersection = 0;
    for (const auto& word : set_a) {
        if (set_b.count(word) > 0) {
            intersection++;
        }
    }

    size_t union_size = set_a.size() + set_b.size() - intersection;
    if (union_size == 0) {
        return 0.0;
    }

    return static_cast<double>(intersection) / union_size;
}

double RAGEvaluator::percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());

    double idx = (p / 100.0) * (values.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(idx));
    size_t upper = static_cast<size_t>(std::ceil(idx));

    if (lower == upper) {
        return values[lower];
    }

    double fraction = idx - lower;
    return values[lower] * (1 - fraction) + values[upper] * fraction;
}

// ========== 工厂函数 ==========

std::shared_ptr<RAGEvaluator> create_rag_evaluator(std::shared_ptr<LLMService> llm) {
    return std::make_shared<RAGEvaluator>(llm);
}

}  // namespace rag
