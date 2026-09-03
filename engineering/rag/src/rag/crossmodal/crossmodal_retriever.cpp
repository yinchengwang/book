/**
 * @file crossmodal_retriever.cpp
 * @brief 跨模态检索器实现
 */

#include "rag/crossmodal_retriever.h"
#include "rag/embedding.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <fstream>

namespace rag {

// ========== SimpleImageFeatureExtractor 实现 ==========

SimpleImageFeatureExtractor::SimpleImageFeatureExtractor(int dimension)
    : dimension_(dimension), ready_(false) {}

std::vector<float> SimpleImageFeatureExtractor::extract(const std::string& image_path) {
    if (!ready_) {
        return std::vector<float>(dimension_, 0.0f);
    }

    // 基于路径生成确定性随机向量 (用于测试)
    std::vector<float> features(dimension_);
    std::mt19937 gen(static_cast<unsigned>(std::hash<std::string>{}(image_path)));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (int i = 0; i < dimension_; ++i) {
        features[i] = dist(gen);
    }

    // L2 归一化
    float norm = 0.0f;
    for (float v : features) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (float& v : features) {
            v /= norm;
        }
    }

    return features;
}

// ========== CrossModalRetriever 实现 ==========

CrossModalRetriever::CrossModalRetriever() : config_() {}

std::vector<RetrievalResult> CrossModalRetriever::retrieve(const std::string& query, int top_k) {
    // 默认使用文本查询
    return retrieve_by_text(query, top_k);
}

std::vector<RetrievalResult> CrossModalRetriever::retrieve_by_text(
    const std::string& query, int top_k) {

    if (!embedding_service_ || !embedding_service_->is_ready()) {
        return {};
    }

    if (image_index_.empty()) {
        return {};
    }

    // 1. 提取文本特征
    auto query_features = embedding_service_->encode(query);

    // 2. 搜索相似图片
    auto similar = search_similar(query_features, top_k);

    // 3. 构建结果
    std::vector<RetrievalResult> results;
    for (const auto& [idx, score] : similar) {
        results.push_back(to_retrieval_result(image_index_[idx], score, results.size() + 1));
    }

    return results;
}

std::vector<RetrievalResult> CrossModalRetriever::retrieve_by_image(
    const std::string& image_path, int top_k) {

    if (!feature_extractor_ || !feature_extractor_->is_ready()) {
        return {};
    }

    if (image_index_.empty()) {
        return {};
    }

    // 1. 提取图片特征
    auto query_features = feature_extractor_->extract(image_path);

    // 2. 搜索相似图片
    auto similar = search_similar(query_features, top_k);

    // 3. 构建结果
    std::vector<RetrievalResult> results;
    for (const auto& [idx, score] : similar) {
        results.push_back(to_retrieval_result(image_index_[idx], score, results.size() + 1));
    }

    return results;
}

void CrossModalRetriever::add_image(const ImageEntry& image) {
    image_index_.push_back(image);
}

void CrossModalRetriever::add_images(const std::vector<ImageEntry>& images) {
    for (const auto& img : images) {
        image_index_.push_back(img);
    }
}

void CrossModalRetriever::set_feature_extractor(std::shared_ptr<ImageFeatureExtractor> extractor) {
    feature_extractor_ = std::move(extractor);
}

void CrossModalRetriever::set_embedding_service(std::shared_ptr<EmbeddingService> service) {
    embedding_service_ = std::move(service);
}

float CrossModalRetriever::compute_similarity(
    const std::vector<float>& a,
    const std::vector<float>& b) {

    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }

    // 余弦相似度
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 1e-8f) {
        return 0.0f;
    }

    return dot / denom;
}

std::vector<std::pair<size_t, float>> CrossModalRetriever::search_similar(
    const std::vector<float>& query_features, int top_k) {

    std::vector<std::pair<size_t, float>> scores;

    for (size_t i = 0; i < image_index_.size(); ++i) {
        float sim = compute_similarity(query_features, image_index_[i].features);
        scores.emplace_back(i, sim);
    }

    // 按相似度降序排序
    std::partial_sort(scores.begin(), scores.begin() + top_k, scores.end(),
                     [](const auto& a, const auto& b) {
                         return a.second > b.second;
                     });

    // 截取 top_k
    if (static_cast<int>(scores.size()) > top_k) {
        scores.resize(top_k);
    }

    return scores;
}

RetrievalResult CrossModalRetriever::to_retrieval_result(
    const ImageEntry& entry, float score, int rank) {

    RetrievalResult result;
    result.chunk.id = entry.image_id;
    result.chunk.content = entry.caption;
    result.chunk.metadata.file_path = entry.image_path;
    result.score = score;
    result.vector_score = score;
    result.rank = rank;
    result.source = "crossmodal";
    return result;
}

// ========== ImageIndex 实现 ==========

ImageIndex::ImageIndex() {}

void ImageIndex::add(const ImageEntry& entry) {
    index_.push_back(entry);
}

void ImageIndex::add_batch(const std::vector<ImageEntry>& entries) {
    for (const auto& e : entries) {
        index_.push_back(e);
    }
}

std::vector<std::pair<std::string, float>> ImageIndex::search(
    const std::vector<float>& query_features, int top_k) {

    std::vector<std::pair<size_t, float>> scores;

    for (size_t i = 0; i < index_.size(); ++i) {
        // 计算余弦相似度
        const auto& features = index_[i].features;
        if (features.size() != query_features.size()) {
            continue;
        }

        float dot = 0.0f, norm_q = 0.0f, norm_i = 0.0f;
        for (size_t j = 0; j < features.size(); ++j) {
            dot += query_features[j] * features[j];
            norm_q += query_features[j] * query_features[j];
            norm_i += features[j] * features[j];
        }

        float denom = std::sqrt(norm_q) * std::sqrt(norm_i);
        float sim = (denom < 1e-8f) ? 0.0f : (dot / denom);

        scores.emplace_back(i, sim);
    }

    // 排序
    std::partial_sort(scores.begin(), scores.begin() + top_k, scores.end(),
                     [](const auto& a, const auto& b) {
                         return a.second > b.second;
                     });

    std::vector<std::pair<std::string, float>> results;
    for (int i = 0; i < std::min(top_k, static_cast<int>(scores.size())); ++i) {
        results.emplace_back(index_[scores[i].first].image_id, scores[i].second);
    }

    return results;
}

bool ImageIndex::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    index_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        ImageEntry entry;
        entry.image_id = line;

        if (!std::getline(file, entry.image_path)) break;
        if (!std::getline(file, entry.caption)) break;

        std::getline(file, line);
        size_t dim = std::stoul(line);
        entry.features.resize(dim);
        for (size_t i = 0; i < dim; ++i) {
            std::getline(file, line);
            entry.features[i] = std::stof(line);
        }

        index_.push_back(entry);
    }

    return true;
}

bool ImageIndex::save(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& entry : index_) {
        file << entry.image_id << "\n";
        file << entry.image_path << "\n";
        file << entry.caption << "\n";
        file << entry.features.size() << "\n";
        for (float f : entry.features) {
            file << f << "\n";
        }
    }

    return true;
}

std::vector<std::string> ImageIndex::get_all_ids() const {
    std::vector<std::string> ids;
    for (const auto& entry : index_) {
        ids.push_back(entry.image_id);
    }
    return ids;
}

// ========== 工厂函数 ==========

std::unique_ptr<CrossModalRetriever> create_crossmodal_retriever() {
    return std::make_unique<CrossModalRetriever>();
}

std::unique_ptr<ImageIndex> create_image_index() {
    return std::make_unique<ImageIndex>();
}

std::unique_ptr<ImageFeatureExtractor> create_simple_image_feature_extractor(int dimension) {
    auto extractor = std::make_unique<SimpleImageFeatureExtractor>(dimension);
    extractor->load();
    return extractor;
}

}  // namespace rag