/**
 * @file metrics.cpp
 * @brief RAG 评估指标实现
 */

#include "mmrag/eval/metrics.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <set>
#include <cctype>

namespace mmrag {
namespace eval {

// ========== 文本相似度工具 ==========

std::unordered_set<std::string> tokenize(const std::string& text) {
    std::unordered_set<std::string> tokens;
    std::istringstream iss(text);
    std::string token;

    // 中英文混合分词：英文按空格分，中文按字符分
    while (iss >> token) {
        std::string lower;
        lower.reserve(token.size());
        for (char c : token) {
            // 过滤标点符号
            if (std::isalnum(static_cast<unsigned char>(c))) {
                lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (!lower.empty()) {
            tokens.insert(lower);
        }
    }

    // 对中文部分按字符分词
    std::string chinese_chars;
    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 0x80) {  // 非 ASCII（包含中文）
            chinese_chars += c;
        }
    }

    // 提取连续中文片段
    if (!chinese_chars.empty()) {
        // 这里使用简单的字符级分词
        // 生产环境应使用 jieba 等专业分词器
        std::string current;
        for (size_t i = 0; i < chinese_chars.size(); i += 3) {  // UTF-8 中文 3 字节
            if (i + 3 <= chinese_chars.size()) {
                std::string ch = chinese_chars.substr(i, 3);
                if (ch.size() >= 2) {
                    tokens.insert(ch);
                }
            }
        }
    }

    return tokens;
}

double jaccard_similarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0;
    if (a.empty() || b.empty()) return 0.0;

    auto tokens_a = tokenize(a);
    auto tokens_b = tokenize(b);

    if (tokens_a.empty() && tokens_b.empty()) return 1.0;
    if (tokens_a.empty() || tokens_b.empty()) return 0.0;

    std::vector<std::string> intersection;
    std::set_intersection(tokens_a.begin(), tokens_a.end(),
                          tokens_b.begin(), tokens_b.end(),
                          std::back_inserter(intersection));

    std::vector<std::string> union_set;
    std::set_union(tokens_a.begin(), tokens_a.end(),
                   tokens_b.begin(), tokens_b.end(),
                   std::back_inserter(union_set));

    if (union_set.empty()) return 0.0;
    return static_cast<double>(intersection.size()) / union_set.size();
}

double token_overlap_ratio(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return 0.0;

    auto tokens_a = tokenize(a);
    auto tokens_b = tokenize(b);

    if (tokens_a.empty() || tokens_b.empty()) return 0.0;

    int overlap = 0;
    for (const auto& t : tokens_a) {
        if (tokens_b.count(t) > 0) {
            overlap++;
        }
    }

    return static_cast<double>(overlap) / tokens_a.size();
}

// ========== 检索指标实现 ==========

double compute_precision_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k) {
    if (k <= 0 || retrieved.empty() || relevant.empty()) {
        return 0.0;
    }

    int limit = std::min(k, static_cast<int>(retrieved.size()));
    int hits = 0;

    for (int i = 0; i < limit; ++i) {
        if (relevant.count(retrieved[i]) > 0) {
            hits++;
        }
    }

    return static_cast<double>(hits) / limit;
}

double compute_recall_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k) {
    if (relevant.empty() || k <= 0) return 0.0;

    int limit = std::min(k, static_cast<int>(retrieved.size()));
    int hits = 0;

    for (int i = 0; i < limit; ++i) {
        if (relevant.count(retrieved[i]) > 0) {
            hits++;
        }
    }

    return static_cast<double>(hits) / relevant.size();
}

double compute_reciprocal_rank(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant) {
    for (size_t i = 0; i < retrieved.size(); ++i) {
        if (relevant.count(retrieved[i]) > 0) {
            return 1.0 / static_cast<double>(i + 1);
        }
    }
    return 0.0;
}

double compute_mrr(
    const std::vector<std::pair<std::vector<std::string>,
                                 std::unordered_set<std::string>>>& queries) {
    if (queries.empty()) return 0.0;

    double sum = 0.0;
    for (const auto& q : queries) {
        sum += compute_reciprocal_rank(q.first, q.second);
    }
    return sum / queries.size();
}

double compute_ndcg_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_map<std::string, double>& relevance_judges,
    int k) {
    if (k <= 0 || retrieved.empty()) return 0.0;

    int limit = std::min(k, static_cast<int>(retrieved.size()));

    // DCG
    double dcg = 0.0;
    for (int i = 0; i < limit; ++i) {
        auto it = relevance_judges.find(retrieved[i]);
        double rel = (it != relevance_judges.end()) ? it->second : 0.0;
        // 使用 2^rel - 1 公式（与 sklearn 一致）
        dcg += (std::pow(2.0, rel) - 1.0) / std::log2(static_cast<double>(i + 2));
    }

    // IDCG（理想情况下的 DCG）
    std::vector<double> sorted_rels;
    for (const auto& p : relevance_judges) {
        sorted_rels.push_back(p.second);
    }
    std::sort(sorted_rels.begin(), sorted_rels.end(), std::greater<double>());

    double idcg = 0.0;
    for (int i = 0; i < std::min(k, static_cast<int>(sorted_rels.size())); ++i) {
        idcg += (std::pow(2.0, sorted_rels[i]) - 1.0) / std::log2(static_cast<double>(i + 2));
    }

    if (idcg == 0.0) return 0.0;
    return dcg / idcg;
}

double compute_hit_rate(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k) {
    if (k <= 0 || relevant.empty()) return 0.0;

    int limit = std::min(k, static_cast<int>(retrieved.size()));
    for (int i = 0; i < limit; ++i) {
        if (relevant.count(retrieved[i]) > 0) {
            return 1.0;
        }
    }
    return 0.0;
}

double compute_average_precision(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k) {
    if (relevant.empty() || k <= 0) return 0.0;

    int limit = std::min(k, static_cast<int>(retrieved.size()));
    double ap = 0.0;
    int hits = 0;

    for (int i = 0; i < limit; ++i) {
        if (relevant.count(retrieved[i]) > 0) {
            hits++;
            ap += static_cast<double>(hits) / (i + 1);
        }
    }

    return ap / relevant.size();
}

double compute_map_at_k(
    const std::vector<std::pair<std::vector<std::string>,
                                 std::unordered_set<std::string>>>& queries,
    int k) {
    if (queries.empty()) return 0.0;

    double sum = 0.0;
    for (const auto& q : queries) {
        sum += compute_average_precision(q.first, q.second, k);
    }
    return sum / queries.size();
}

double compute_f1_at_k(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant,
    int k) {
    double p = compute_precision_at_k(retrieved, relevant, k);
    double r = compute_recall_at_k(retrieved, relevant, k);

    if (p + r == 0.0) return 0.0;
    return 2.0 * p * r / (p + r);
}

double compute_r_precision(
    const std::vector<std::string>& retrieved,
    const std::unordered_set<std::string>& relevant) {
    if (relevant.empty()) return 0.0;
    int k = static_cast<int>(relevant.size());
    return compute_precision_at_k(retrieved, relevant, k);
}

// ========== 生成指标实现 ==========

double compute_context_precision(
    const std::vector<std::string>& retrieved_context,
    const std::unordered_set<std::string>& relevant) {
    if (retrieved_context.empty()) return 0.0;

    // 计算每个检索到的上下文与 relevant 的相关性，然后加权
    double score = 0.0;
    int n = static_cast<int>(retrieved_context.size());
    int relevant_count = 0;

    for (int i = 0; i < n; ++i) {
        if (relevant.count(retrieved_context[i])) {
            relevant_count++;
            // 排名越靠前的相关文档得分越高
            score += static_cast<double>(relevant_count) / (i + 1);
        }
    }

    if (relevant_count == 0) return 0.0;
    return score / relevant_count;
}

double compute_context_recall(
    const std::vector<std::string>& retrieved_context,
    const std::string& ground_truth) {
    if (ground_truth.empty() || retrieved_context.empty()) return 0.0;

    // 合并所有检索到的上下文
    std::string combined_context;
    for (const auto& ctx : retrieved_context) {
        combined_context += ctx + " ";
    }

    auto ground_tokens = tokenize(ground_truth);
    auto context_tokens = tokenize(combined_context);

    if (ground_tokens.empty()) return 0.0;

    int found = 0;
    for (const auto& t : ground_tokens) {
        if (context_tokens.count(t) > 0) {
            found++;
        }
    }

    return static_cast<double>(found) / ground_tokens.size();
}

double compute_faithfulness(
    const std::string& answer,
    const std::string& context) {
    if (answer.empty()) return 0.0;
    if (context.empty()) return 0.0;  // 没有上下文，无法判断忠实度

    // 将答案按句子分割
    auto answer_tokens = tokenize(answer);
    auto context_tokens = tokenize(context);

    if (answer_tokens.empty()) return 0.0;
    if (context_tokens.empty()) return 0.0;

    // 计算答案中有多少 token 能从上下文中找到支持
    int supported = 0;
    for (const auto& t : answer_tokens) {
        if (context_tokens.count(t) > 0) {
            supported++;
        }
    }

    return static_cast<double>(supported) / answer_tokens.size();
}

double compute_hallucination_rate(
    const std::string& answer,
    const std::string& context) {
    // hallucination_rate = 1 - faithfulness
    return 1.0 - compute_faithfulness(answer, context);
}

double compute_answer_relevancy(
    const std::string& answer,
    const std::string& query) {
    if (answer.empty() || query.empty()) return 0.0;

    // 答案应该与问题有较高的 token 重叠率
    double overlap = token_overlap_ratio(answer, query);

    // 长度惩罚：答案不应该太短或太长
    double answer_len = static_cast<double>(answer.size());
    double query_len = static_cast<double>(query.size());
    double length_ratio = std::min(answer_len / (query_len * 2), 1.0);

    // 综合得分
    return overlap * 0.7 + length_ratio * 0.3;
}

double compute_answer_correctness(
    const std::string& answer,
    const std::string& ground_truth,
    const std::vector<std::string>& key_facts) {
    if (answer.empty()) return 0.0;
    if (ground_truth.empty() && key_facts.empty()) return 0.0;

    double score = 0.0;

    // 1. 与 ground truth 的相似度（占 60%）
    if (!ground_truth.empty()) {
        double similarity = jaccard_similarity(answer, ground_truth);
        score += similarity * 0.6;
    } else {
        score += 0.6;  // 没有 ground_truth 时视为完全正确
    }

    // 2. 关键事实覆盖（占 40%）
    if (!key_facts.empty()) {
        int covered = 0;
        for (const auto& fact : key_facts) {
            if (answer.find(fact) != std::string::npos ||
                token_overlap_ratio(answer, fact) > 0.3) {
                covered++;
            }
        }
        double fact_score = static_cast<double>(covered) / key_facts.size();
        score += fact_score * 0.4;
    } else {
        score += 0.4;
    }

    return std::min(score, 1.0);
}

double compute_completeness(
    const std::string& answer,
    const std::vector<std::string>& key_facts) {
    if (key_facts.empty()) return 1.0;
    if (answer.empty()) return 0.0;

    int covered = 0;
    for (const auto& fact : key_facts) {
        if (answer.find(fact) != std::string::npos ||
            token_overlap_ratio(answer, fact) > 0.3) {
            covered++;
        }
    }

    return static_cast<double>(covered) / key_facts.size();
}

// ========== 百分位计算 ==========

double compute_latency_percentile(
    const std::vector<int64_t>& latencies,
    double percentile) {
    if (latencies.empty()) return 0.0;

    std::vector<int64_t> sorted = latencies;
    std::sort(sorted.begin(), sorted.end());

    if (percentile <= 0.0) return sorted.front();
    if (percentile >= 100.0) return sorted.back();

    double rank = (percentile / 100.0) * (sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(rank));
    size_t upper = static_cast<size_t>(std::ceil(rank));

    if (lower == upper) {
        return static_cast<double>(sorted[lower]);
    }

    // 线性插值
    double weight = rank - lower;
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

}  // namespace eval
}  // namespace mmrag