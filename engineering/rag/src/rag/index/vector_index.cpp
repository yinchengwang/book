/**
 * @file vector_index.cpp
 * @brief 向量索引实现
 *
 * 简化版 HNSW 实现（内存版本），生产环境应使用 hnswlib
 */

#include "rag/vector_index.h"
#include "rag/logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <unordered_map>
#include <fstream>

namespace rag {

// ========== HNSW 实现 ==========

/**
 * @brief HNSW 内部实现
 *
 * 简化版本，用于演示和测试
 */
struct HNSWIndex::Impl {
    int dim = 0;
    int max_elements = 0;
    int m = 16;
    int ef_construction = 200;
    int ef_search = 100;

    // 存储数据
    std::vector<std::vector<float>> vectors;
    std::unordered_map<std::string, size_t> id_to_idx;
    std::vector<std::string> idx_to_id;

    // 距离计算
    float l2_distance(const std::vector<float>& a, const std::vector<float>& b) {
        float dist = 0;
        for (int i = 0; i < dim; ++i) {
            float d = a[i] - b[i];
            dist += d * d;
        }
        return std::sqrt(dist);
    }

    float cosine_distance(const std::vector<float>& a, const std::vector<float>& b) {
        float dot = 0, norm_a = 0, norm_b = 0;
        for (int i = 0; i < dim; ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        return 1.0f - dot / (std::sqrt(norm_a) * std::sqrt(norm_b) + 1e-8f);
    }
};

HNSWIndex::HNSWIndex() : impl_(std::make_unique<Impl>()) {}

HNSWIndex::~HNSWIndex() = default;

void HNSWIndex::init(const HNSWConfig& config) {
    config_ = config;
    impl_->dim = config.dim;
    impl_->max_elements = config.max_elements;
    impl_->m = config.m;
    impl_->ef_construction = config.ef_construction;
    impl_->ef_search = config.ef_search;

    impl_->vectors.reserve(config.max_elements);
    impl_->idx_to_id.reserve(config.max_elements);

    RAG_INFO("HNSW index initialized: dim=" + std::to_string(config.dim) +
             ", max_elements=" + std::to_string(config.max_elements));
}

void HNSWIndex::add(const std::string& id, const std::vector<float>& vector) {
    if (static_cast<int>(vector.size()) != impl_->dim) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Vector dimension mismatch: expected " +
                            std::to_string(impl_->dim) + ", got " +
                            std::to_string(vector.size()));
    }

    if (impl_->id_to_idx.find(id) != impl_->id_to_idx.end()) {
        // 更新现有向量
        size_t idx = impl_->id_to_idx[id];
        impl_->vectors[idx] = vector;
    } else {
        // 添加新向量
        impl_->id_to_idx[id] = impl_->vectors.size();
        impl_->idx_to_id.push_back(id);
        impl_->vectors.push_back(vector);
    }
}

void HNSWIndex::add_batch(const std::vector<std::string>& ids,
                          const std::vector<std::vector<float>>& vectors) {
    if (ids.size() != vectors.size()) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "IDs and vectors count mismatch");
    }

    for (size_t i = 0; i < ids.size(); ++i) {
        add(ids[i], vectors[i]);
    }

    RAG_DEBUG("HNSW batch add: " + std::to_string(ids.size()) + " vectors");
}

std::vector<std::pair<std::string, float>> HNSWIndex::search(
    const std::vector<float>& query,
    int top_k) {

    if (impl_->vectors.empty()) {
        return {};
    }

    if (static_cast<int>(query.size()) != impl_->dim) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Query dimension mismatch");
    }

    // 简化实现：暴力搜索 + 堆排序
    // 生产环境应使用真正的 HNSW 图搜索

    struct Candidate {
        size_t idx;
        float dist;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(impl_->vectors.size());

    for (size_t i = 0; i < impl_->vectors.size(); ++i) {
        float dist = impl_->l2_distance(query, impl_->vectors[i]);
        candidates.push_back({i, dist});
    }

    // 部分排序获取 top_k
    std::partial_sort(candidates.begin(), candidates.begin() + top_k,
                     candidates.end(),
                     [](const Candidate& a, const Candidate& b) {
                         return a.dist < b.dist;
                     });

    std::vector<std::pair<std::string, float>> results;
    for (int i = 0; i < std::min(top_k, static_cast<int>(candidates.size())); ++i) {
        results.emplace_back(impl_->idx_to_id[candidates[i].idx], candidates[i].dist);
    }

    return results;
}

std::optional<std::vector<float>> HNSWIndex::get(const std::string& id) {
    auto it = impl_->id_to_idx.find(id);
    if (it == impl_->id_to_idx.end()) {
        return std::nullopt;
    }
    return impl_->vectors[it->second];
}

void HNSWIndex::remove(const std::string& id) {
    auto it = impl_->id_to_idx.find(id);
    if (it == impl_->id_to_idx.end()) {
        return;
    }

    size_t idx = it->second;

    // 移动最后一个元素到删除位置
    if (idx < impl_->vectors.size() - 1) {
        impl_->vectors[idx] = impl_->vectors.back();
        impl_->idx_to_id[idx] = impl_->idx_to_id.back();
        impl_->id_to_idx[impl_->idx_to_id[idx]] = idx;
    }

    impl_->vectors.pop_back();
    impl_->idx_to_id.pop_back();
    impl_->id_to_idx.erase(id);
}

void HNSWIndex::clear() {
    impl_->vectors.clear();
    impl_->id_to_idx.clear();
    impl_->idx_to_id.clear();
}

void HNSWIndex::save(const std::filesystem::path& path) {
    // 保存为二进制格式
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Cannot save index to: " + path.string());
    }

    // 写入头部
    int magic = 0x52414748;  // "RAGH"
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&impl_->dim), sizeof(impl_->dim));

    // 写入数据
    size_t count = impl_->vectors.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (size_t i = 0; i < count; ++i) {
        // 写入 ID 长度和内容
        const auto& id = impl_->idx_to_id[i];
        int id_len = static_cast<int>(id.size());
        file.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        file.write(id.data(), id_len);

        // 写入向量
        file.write(reinterpret_cast<const char*>(impl_->vectors[i].data()),
                  impl_->dim * sizeof(float));
    }

    RAG_INFO("HNSW index saved: " + path.string() +
             " (" + std::to_string(count) + " vectors)");
}

void HNSWIndex::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_NOT_FOUND,
                            "Cannot load index from: " + path.string());
    }

    // 读取头部
    int magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x52414748) {
        throw IndexException(ErrorCodes::INDEX_CORRUPTED,
                            "Invalid index file format");
    }

    int dim;
    file.read(reinterpret_cast<char*>(&dim), sizeof(dim));
    if (dim != impl_->dim && !impl_->vectors.empty()) {
        throw IndexException(ErrorCodes::INDEX_CORRUPTED,
                            "Dimension mismatch in index file");
    }
    impl_->dim = dim;

    // 读取数据
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    impl_->vectors.clear();
    impl_->id_to_idx.clear();
    impl_->idx_to_id.clear();
    impl_->vectors.reserve(count);
    impl_->idx_to_id.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        // 读取 ID
        int id_len;
        file.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        std::string id(id_len, ' ');
        file.read(id.data(), id_len);

        // 读取向量
        std::vector<float> vector(dim);
        file.read(reinterpret_cast<char*>(vector.data()), dim * sizeof(float));

        impl_->id_to_idx[id] = impl_->vectors.size();
        impl_->idx_to_id.push_back(id);
        impl_->vectors.push_back(std::move(vector));
    }

    loaded_ = true;

    RAG_INFO("HNSW index loaded: " + path.string() +
             " (" + std::to_string(count) + " vectors)");
}

size_t HNSWIndex::size() const {
    return impl_->vectors.size();
}

// ========== 工厂函数 ==========

std::unique_ptr<VectorIndex> create_vector_index(const HNSWConfig& config) {
    return std::make_unique<HNSWIndex>();
}

}  // namespace rag
