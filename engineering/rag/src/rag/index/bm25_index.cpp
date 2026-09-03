/**
 * @file bm25_index.cpp
 * @brief BM25 全文检索索引实现
 */

#include "rag/bm25_index.h"
#include "rag/logger.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace rag {

// ========== 辅助函数 ==========

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ========== BM25Index 实现 ==========

BM25Index::BM25Index() = default;

BM25Index::~BM25Index() = default;

void BM25Index::init(const BM25Config& config) {
    config_ = config;
    RAG_INFO("BM25 index initialized: k1=" + std::to_string(config.k1) +
             ", b=" + std::to_string(config.b));
}

std::vector<std::string> BM25Index::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current += std::tolower(c);
        } else if (!current.empty()) {
            if (current.size() >= 2) {  // 过滤短词
                tokens.push_back(current);
            }
            current.clear();
        }
    }

    if (!current.empty() && current.size() >= 2) {
        tokens.push_back(current);
    }

    return tokens;
}

void BM25Index::add(const std::string& id, const std::string& content) {
    if (documents_.find(id) != documents_.end()) {
        // 更新文档
        remove(id);
    }

    documents_[id] = content;
    doc_ids_.push_back(id);

    // 分词
    auto tokens = tokenize(content);
    doc_lengths_.push_back(static_cast<int>(tokens.size()));

    // 统计词频
    std::unordered_map<std::string, int> local_freq;
    for (const auto& token : tokens) {
        local_freq[token]++;
    }

    for (const auto& [term, freq] : local_freq) {
        if (term_freq_.find(term) == term_freq_.end()) {
            term_freq_[term] = std::vector<int>(doc_ids_.size() - 1, 0);
        }
        term_freq_[term].push_back(freq);
        doc_freq_[term]++;
    }

    // 补齐之前文档的词频
    for (auto& [term, freqs] : term_freq_) {
        if (freqs.size() < doc_ids_.size()) {
            freqs.push_back(0);
        }
    }

    doc_count_++;
    update_stats();
}

void BM25Index::add_batch(const std::vector<std::string>& ids,
                          const std::vector<std::string>& contents) {
    for (size_t i = 0; i < ids.size(); ++i) {
        add(ids[i], contents[i]);
    }
    RAG_DEBUG("BM25 batch add: " + std::to_string(ids.size()) + " documents");
}

float BM25Index::calculate_idf(const std::string& term) {
    auto it = doc_freq_.find(term);
    if (it == doc_freq_.end()) {
        return 0;
    }

    int df = it->second;
    if (df == 0) return 0;

    // IDF 公式: log((N - n + 0.5) / (n + 0.5))
    float idf = std::log((doc_count_ - df + 0.5f) / (df + 0.5f));
    return std::max(0.0f, idf);
}

void BM25Index::update_stats() {
    if (doc_lengths_.empty()) {
        avg_doc_len_ = config_.avg_doc_len;
    } else {
        long long sum = 0;
        for (int len : doc_lengths_) {
            sum += len;
        }
        avg_doc_len_ = static_cast<int>(sum / doc_lengths_.size());
    }
}

std::vector<std::pair<std::string, float>> BM25Index::search(
    const std::string& query,
    int top_k) {
    auto terms = tokenize(query);
    return search(terms, top_k);
}

std::vector<std::pair<std::string, float>> BM25Index::search(
    const std::vector<std::string>& terms,
    int top_k) {

    if (doc_count_ == 0) {
        return {};
    }

    // 计算每个文档的 BM25 分数
    struct DocScore {
        int doc_idx;
        float score;
    };

    std::vector<DocScore> scores;
    scores.reserve(doc_count_);

    for (int i = 0; i < doc_count_; ++i) {
        float score = 0;

        for (const auto& term : terms) {
            auto tf_it = term_freq_.find(term);
            if (tf_it == term_freq_.end() || tf_it->second.size() <= static_cast<size_t>(i)) {
                continue;
            }

            int tf = tf_it->second[i];
            if (tf == 0) continue;

            float idf = calculate_idf(term);
            float doc_len = doc_lengths_[i];

            // BM25 公式
            // score = IDF * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * doc_len / avg_doc_len))
            float numerator = tf * (config_.k1 + 1);
            float denominator = tf + config_.k1 * (1 - config_.b + config_.b * doc_len / avg_doc_len_);
            score += idf * numerator / denominator;
        }

        if (score > 0) {
            scores.push_back({i, score});
        }
    }

    // 排序
    std::partial_sort(scores.begin(), scores.begin() + top_k, scores.end(),
                     [](const DocScore& a, const DocScore& b) {
                         return a.score > b.score;
                     });

    std::vector<std::pair<std::string, float>> results;
    for (int i = 0; i < std::min(top_k, static_cast<int>(scores.size())); ++i) {
        results.emplace_back(doc_ids_[scores[i].doc_idx], scores[i].score);
    }

    return results;
}

std::optional<std::string> BM25Index::get(const std::string& id) {
    auto it = documents_.find(id);
    if (it == documents_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void BM25Index::remove(const std::string& id) {
    auto it = std::find(doc_ids_.begin(), doc_ids_.end(), id);
    if (it == doc_ids_.end()) {
        return;
    }

    size_t idx = std::distance(doc_ids_.begin(), it);

    // 从向量中移除
    doc_ids_.erase(it);
    documents_.erase(id);

    // 移除文档长度
    if (idx < doc_lengths_.size()) {
        doc_lengths_.erase(doc_lengths_.begin() + idx);
    }

    // 更新词频统计
    for (auto& [term, freqs] : term_freq_) {
        if (idx < freqs.size()) {
            doc_freq_[term] -= (freqs[idx] > 0 ? 1 : 0);
            freqs.erase(freqs.begin() + idx);
        }
    }

    doc_count_--;
    update_stats();
}

void BM25Index::clear() {
    documents_.clear();
    doc_ids_.clear();
    term_freq_.clear();
    doc_freq_.clear();
    doc_lengths_.clear();
    doc_count_ = 0;
    loaded_ = false;
}

void BM25Index::save(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Cannot save BM25 index to: " + path.string());
    }

    // 写入头部
    int magic = 0x424D3235;  // "BM25"
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

    // 写入配置
    file.write(reinterpret_cast<const char*>(&config_.k1), sizeof(config_.k1));
    file.write(reinterpret_cast<const char*>(&config_.b), sizeof(config_.b));

    // 写入文档数
    file.write(reinterpret_cast<const char*>(&doc_count_), sizeof(doc_count_));

    // 写入文档内容
    for (const auto& id : doc_ids_) {
        int id_len = static_cast<int>(id.size());
        file.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        file.write(id.data(), id_len);

        const auto& content = documents_[id];
        int content_len = static_cast<int>(content.size());
        file.write(reinterpret_cast<const char*>(&content_len), sizeof(content_len));
        file.write(content.data(), content_len);
    }

    // 写入词频统计
    int term_count = static_cast<int>(doc_freq_.size());
    file.write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));

    for (const auto& [term, df] : doc_freq_) {
        int term_len = static_cast<int>(term.size());
        file.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        file.write(term.data(), term_len);
        file.write(reinterpret_cast<const char*>(&df), sizeof(df));

        // 写入该词在各文档的词频
        const auto& freqs = term_freq_[term];
        for (size_t i = 0; i < freqs.size() && i < static_cast<size_t>(doc_count_); ++i) {
            int freq = freqs[i];
            file.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
        }
    }

    // 写入文档长度
    for (int len : doc_lengths_) {
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
    }

    RAG_INFO("BM25 index saved: " + path.string() +
             " (" + std::to_string(doc_count_) + " docs)");
}

void BM25Index::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_NOT_FOUND,
                            "Cannot load BM25 index from: " + path.string());
    }

    clear();

    // 读取头部
    int magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x424D3235) {
        throw IndexException(ErrorCodes::INDEX_CORRUPTED,
                            "Invalid BM25 index file format");
    }

    // 读取配置
    file.read(reinterpret_cast<char*>(&config_.k1), sizeof(config_.k1));
    file.read(reinterpret_cast<char*>(&config_.b), sizeof(config_.b));

    // 读取文档数
    file.read(reinterpret_cast<char*>(&doc_count_), sizeof(doc_count_));

    // 读取文档
    for (int i = 0; i < doc_count_; ++i) {
        int id_len;
        file.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        std::string id(id_len, ' ');
        file.read(id.data(), id_len);

        int content_len;
        file.read(reinterpret_cast<char*>(&content_len), sizeof(content_len));
        std::string content(content_len, ' ');
        file.read(content.data(), content_len);

        documents_[id] = content;
        doc_ids_.push_back(id);
    }

    // 读取词频统计
    int term_count;
    file.read(reinterpret_cast<char*>(&term_count), sizeof(term_count));

    for (int i = 0; i < term_count; ++i) {
        int term_len;
        file.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
        std::string term(term_len, ' ');
        file.read(term.data(), term_len);

        int df;
        file.read(reinterpret_cast<char*>(&df), sizeof(df));
        doc_freq_[term] = df;

        // 读取词频
        term_freq_[term] = std::vector<int>();
        for (int j = 0; j < doc_count_; ++j) {
            int freq;
            file.read(reinterpret_cast<char*>(&freq), sizeof(freq));
            term_freq_[term].push_back(freq);
        }
    }

    // 读取文档长度
    doc_lengths_.resize(doc_count_);
    for (int i = 0; i < doc_count_; ++i) {
        file.read(reinterpret_cast<char*>(&doc_lengths_[i]), sizeof(int));
    }

    loaded_ = true;
    update_stats();

    RAG_INFO("BM25 index loaded: " + path.string() +
             " (" + std::to_string(doc_count_) + " docs)");
}

// ========== 工厂函数 ==========

std::unique_ptr<BM25Index> create_bm25_index(const BM25Config& config) {
    return std::make_unique<BM25Index>();
}

}  // namespace rag
