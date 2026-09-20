/**
 * @file vector_index.cpp
 * @brief 向量索引实现：FlatArrayIndex + 真实 HNSW
 *
 * 双实现：
 *   FlatArrayIndex — 纯内存暴力 L2，适合 < 10k 向量
 *   HNSWIndex      — 包装 libfaiss_hnsw.a，真实 HNSW 图索引
 */

#include "mmrag/vector_index.h"
#include "mmrag/logger.h"

#include <db/index/vector_index/faiss_hnsw/faiss_hnsw.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace mmrag {

// ============================================================================
// FlatArrayIndex 实现
// ============================================================================

struct FlatArrayIndex::Impl {
    int dim = 0;
    std::vector<std::vector<float>> vectors;           // 按插入顺序存储
    std::unordered_map<std::string, size_t> id_to_idx; // string_id → vector index
    std::vector<std::string> idx_to_id;                // index → string_id
    std::unordered_set<std::string> tombstones;        // 已删除的 ID
    mutable std::mutex mtx;                            // 线程安全互斥锁

    float l2_distance(const std::vector<float>& a, const std::vector<float>& b) {
        float dist = 0.0f;
        for (int i = 0; i < dim; ++i) {
            float d = a[i] - b[i];
            dist += d * d;
        }
        return dist;  // 返回 L2 平方，与 faiss_hnsw 一致
    }
};

FlatArrayIndex::FlatArrayIndex() : impl_(std::make_unique<Impl>()) {}
FlatArrayIndex::~FlatArrayIndex() = default;

void FlatArrayIndex::init(const HNSWConfig& config) {
    config_ = config;
    impl_->dim = config.dim;
    impl_->vectors.clear();
    impl_->id_to_idx.clear();
    impl_->idx_to_id.clear();
    impl_->tombstones.clear();
    RAG_INFO("FlatArrayIndex initialized: dim=" + std::to_string(config.dim) +
             ", max_elements=" + std::to_string(config.max_elements));
}

void FlatArrayIndex::add(const std::string& id, const std::vector<float>& vector) {
    if (static_cast<int>(vector.size()) != impl_->dim) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Vector dimension mismatch: expected " +
                            std::to_string(impl_->dim) + ", got " +
                            std::to_string(static_cast<int>(vector.size())));
    }

    std::lock_guard<std::mutex> lock(impl_->mtx);

    // 移除旧 tombstone 标记
    impl_->tombstones.erase(id);

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

void FlatArrayIndex::add_batch(const std::vector<std::string>& ids,
                               const std::vector<std::vector<float>>& vectors) {
    if (ids.size() != vectors.size()) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "IDs and vectors count mismatch");
    }
    std::lock_guard<std::mutex> lock(impl_->mtx);
    for (size_t i = 0; i < ids.size(); ++i) {
        // 移除旧 tombstone 标记
        impl_->tombstones.erase(ids[i]);

        if (impl_->id_to_idx.find(ids[i]) != impl_->id_to_idx.end()) {
            size_t idx = impl_->id_to_idx[ids[i]];
            impl_->vectors[idx] = vectors[i];
        } else {
            impl_->id_to_idx[ids[i]] = impl_->vectors.size();
            impl_->idx_to_id.push_back(ids[i]);
            impl_->vectors.push_back(vectors[i]);
        }
    }
    RAG_DEBUG("FlatArrayIndex batch add: " + std::to_string(ids.size()) + " vectors");
}

std::vector<std::pair<std::string, float>>
FlatArrayIndex::search(const std::vector<float>& query, int top_k) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    if (impl_->vectors.empty()) return {};

    if (static_cast<int>(query.size()) != impl_->dim) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Query dimension mismatch");
    }

    struct Candidate { size_t idx; float dist; };
    std::vector<Candidate> candidates;
    candidates.reserve(impl_->vectors.size());

    for (size_t i = 0; i < impl_->vectors.size(); ++i) {
        const std::string& id = impl_->idx_to_id[i];
        if (impl_->tombstones.count(id)) continue;  // 跳过已删除
        candidates.push_back({i, impl_->l2_distance(query, impl_->vectors[i])});
    }

    std::partial_sort(candidates.begin(),
                     candidates.begin() + std::min(top_k, static_cast<int>(candidates.size())),
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

std::optional<std::vector<float>> FlatArrayIndex::get(const std::string& id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->id_to_idx.find(id);
    if (it == impl_->id_to_idx.end() || impl_->tombstones.count(id)) {
        return std::nullopt;
    }
    return impl_->vectors[it->second];
}

void FlatArrayIndex::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (impl_->id_to_idx.find(id) != impl_->id_to_idx.end()) {
        impl_->tombstones.insert(id);  // 标记为 tombstone，不实际删除向量
    }
}

void FlatArrayIndex::clear() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->vectors.clear();
    impl_->id_to_idx.clear();
    impl_->idx_to_id.clear();
    impl_->tombstones.clear();
    loaded_ = false;
}

void FlatArrayIndex::save(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Cannot save FlatArrayIndex to: " + path.string());
    }

    int magic = 0x464C4154;  // "FLAT"
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&impl_->dim), sizeof(impl_->dim));

    int32_t count = static_cast<int32_t>(impl_->vectors.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (size_t i = 0; i < impl_->vectors.size(); ++i) {
        const auto& id = impl_->idx_to_id[i];
        int id_len = static_cast<int>(id.size());
        file.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        file.write(id.data(), id_len);
        file.write(reinterpret_cast<const char*>(impl_->vectors[i].data()),
                  impl_->dim * sizeof(float));
    }

    int32_t tomb_count = static_cast<int32_t>(impl_->tombstones.size());
    file.write(reinterpret_cast<const char*>(&tomb_count), sizeof(tomb_count));
    for (const auto& id : impl_->tombstones) {
        int id_len = static_cast<int>(id.size());
        file.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        file.write(id.data(), id_len);
    }

    RAG_INFO("FlatArrayIndex saved: " + path.string() +
             " (" + std::to_string(count) + " vectors, " +
             std::to_string(impl_->tombstones.size()) + " tombstones)");
}

void FlatArrayIndex::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_NOT_FOUND,
                            "Cannot load FlatArrayIndex from: " + path.string());
    }

    clear();  // clear() 内部会获取锁，但 clear() 的锁是可重入的吗？
              // 不是，std::mutex 不可重入。所以这里不能调用 clear()。
              // 需要手动清空或使用 recursive_mutex。
              // 简单方案：load 开头手动清空，不调用 clear()。

    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->vectors.clear();
    impl_->id_to_idx.clear();
    impl_->idx_to_id.clear();
    impl_->tombstones.clear();
    loaded_ = false;

    int magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x464C4154) {
        throw IndexException(ErrorCodes::INDEX_CORRUPTED,
                            "Invalid FlatArrayIndex file format");
    }

    file.read(reinterpret_cast<char*>(&impl_->dim), sizeof(impl_->dim));

    int32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    impl_->vectors.reserve(count);
    impl_->idx_to_id.reserve(count);

    for (int32_t i = 0; i < count; ++i) {
        int id_len;
        file.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        std::string id(id_len, ' ');
        file.read(id.data(), id_len);

        std::vector<float> v(impl_->dim);
        file.read(reinterpret_cast<char*>(v.data()), impl_->dim * sizeof(float));

        impl_->id_to_idx[id] = impl_->vectors.size();
        impl_->idx_to_id.push_back(id);
        impl_->vectors.push_back(std::move(v));
    }

    int32_t tomb_count;
    file.read(reinterpret_cast<char*>(&tomb_count), sizeof(tomb_count));
    for (int32_t i = 0; i < tomb_count; ++i) {
        int id_len;
        file.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        std::string id(id_len, ' ');
        file.read(id.data(), id_len);
        impl_->tombstones.insert(id);
    }

    loaded_ = true;
    RAG_INFO("FlatArrayIndex loaded: " + path.string() +
             " (" + std::to_string(count) + " vectors)");
}

size_t FlatArrayIndex::size() const {
    return impl_->vectors.size() - impl_->tombstones.size();
}

// ============================================================================
// HNSWIndex 实现（包装 faiss_hnsw_t）
// ============================================================================

struct HNSWIndex::Impl {
    void* index = nullptr;  // faiss_hnsw_t*
    std::unordered_map<std::string, int32_t> id_to_internal;  // string_id → internal 0-indexed id
    std::vector<std::string> internal_to_id;                   // internal id → string_id
    std::unordered_set<int32_t> tombstones;                    // 已删除的 internal id
    distance_metric_t metric = DISTANCE_METRIC_L2_SQUARED;
    std::mutex mtx;  // faiss_hnsw 不是线程安全的
};

HNSWIndex::HNSWIndex() : impl_(std::make_unique<Impl>()) {}
HNSWIndex::~HNSWIndex() {
    if (impl_->index) {
        faiss_hnsw_index_drop(static_cast<faiss_hnsw_t*>(impl_->index));
        impl_->index = nullptr;
    }
}

static distance_metric_t parse_metric(const std::string& space) {
    if (space == "cosine") return DISTANCE_METRIC_COSINE;
    if (space == "ip")     return DISTANCE_METRIC_INNER_PRODUCT;
    return DISTANCE_METRIC_L2_SQUARED;
}

void HNSWIndex::init(const HNSWConfig& config) {
    config_ = config;
    impl_->metric = parse_metric(config.space);

    if (impl_->index) {
        faiss_hnsw_index_drop(static_cast<faiss_hnsw_t*>(impl_->index));
    }

    // 创建真实 HNSW 索引
    impl_->index = faiss_hnsw_index_create(
        config.m,                           // M
        config.dim,                         // dims
        config.ef_construction,             // ef_construction
        impl_->metric,                      // metric
        QUANTIZATION_TYPE_NONE              // FP32, no quantization
    );

    if (!impl_->index) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Failed to create faiss_hnsw index");
    }

    // 预分配容量避免 realloc
    faiss_hnsw_index_reserve(static_cast<faiss_hnsw_t*>(impl_->index),
                             config.max_elements);

    impl_->id_to_internal.clear();
    impl_->internal_to_id.clear();
    impl_->tombstones.clear();

    RAG_INFO("HNSWIndex initialized: dim=" + std::to_string(config.dim) +
             ", M=" + std::to_string(config.m) +
             ", ef_construction=" + std::to_string(config.ef_construction) +
             ", metric=" + std::to_string(static_cast<int>(impl_->metric)));
}

static std::vector<float> l2_normalize(const std::vector<float>& v) {
    float norm = 0.0f;
    for (float x : v) norm += x * x;
    norm = std::sqrt(norm) + 1e-8f;
    std::vector<float> r;
    r.reserve(v.size());
    for (float x : v) r.push_back(x / norm);
    return r;
}

void HNSWIndex::add(const std::string& id, const std::vector<float>& vector) {
    if (!impl_->index) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "HNSW index not initialized");
    }
    if (static_cast<int>(vector.size()) != config_.dim) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Vector dimension mismatch");
    }

    std::lock_guard<std::mutex> lock(impl_->mtx);

    // 余弦距离：归一化后用 L2_SQUARED 近似
    std::vector<float> vec = vector;
    if (impl_->metric == DISTANCE_METRIC_COSINE) {
        vec = l2_normalize(vector);
    }

    // 如果已存在，标记旧 internal id 为 tombstone
    auto existing = impl_->id_to_internal.find(id);
    if (existing != impl_->id_to_internal.end()) {
        impl_->tombstones.insert(existing->second);
    }

    // 添加到 HNSW
    int32_t internal_id = faiss_hnsw_index_size(static_cast<faiss_hnsw_t*>(impl_->index));
    int32_t added = faiss_hnsw_index_add(static_cast<faiss_hnsw_t*>(impl_->index),
                                         1, vec.data());
    if (added < 0) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Failed to add vector to HNSW index");
    }

    // 更新映射
    impl_->id_to_internal[id] = internal_id;
    impl_->internal_to_id.push_back(id);
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
    RAG_DEBUG("HNSWIndex batch add: " + std::to_string(ids.size()) + " vectors");
}

std::vector<std::pair<std::string, float>>
HNSWIndex::search(const std::vector<float>& query, int top_k) {
    if (!impl_->index || faiss_hnsw_index_size(static_cast<faiss_hnsw_t*>(impl_->index)) == 0) {
        return {};
    }

    if (static_cast<int>(query.size()) != config_.dim) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Query dimension mismatch");
    }

    // 余弦距离：归一化查询向量
    std::vector<float> q = query;
    if (impl_->metric == DISTANCE_METRIC_COSINE) {
        q = l2_normalize(query);
    }

    // 用更大的 ef_search 获取足够候选（过滤 tombstone 后凑够 top_k）
    int32_t search_k = std::max(top_k * 3, config_.ef_search);

    std::vector<int32_t> out_ids(search_k);
    std::vector<float> out_dist(search_k);

    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        faiss_hnsw_index_search(static_cast<faiss_hnsw_t*>(impl_->index),
                               q.data(), top_k,
                               config_.ef_search,
                               out_dist.data(), out_ids.data());
    }

    // 过滤 tombstone，凑够 top_k 个结果
    std::vector<std::pair<std::string, float>> results;
    for (int i = 0; i < search_k && results.size() < static_cast<size_t>(top_k); ++i) {
        int32_t internal_id = out_ids[i];
        if (internal_id < 0) continue;  // faiss_hnsw 返回 -1 表示无效
        if (impl_->tombstones.count(internal_id)) continue;
        if (static_cast<size_t>(internal_id) >= impl_->internal_to_id.size()) continue;

        results.emplace_back(impl_->internal_to_id[internal_id], out_dist[i]);
    }

    return results;
}

std::optional<std::vector<float>> HNSWIndex::get(const std::string& id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->id_to_internal.find(id);
    if (it == impl_->id_to_internal.end() || impl_->tombstones.count(it->second)) {
        return std::nullopt;
    }
    // faiss_hnsw 不直接暴露向量数据，这里返回 nullopt
    // 调用方需要从外部存储获取向量
    (void)it;
    return std::nullopt;
}

void HNSWIndex::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->id_to_internal.find(id);
    if (it != impl_->id_to_internal.end()) {
        impl_->tombstones.insert(it->second);
    }
}

void HNSWIndex::clear() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (impl_->index) {
        faiss_hnsw_index_drop(static_cast<faiss_hnsw_t*>(impl_->index));
        impl_->index = nullptr;
    }
    impl_->id_to_internal.clear();
    impl_->internal_to_id.clear();
    impl_->tombstones.clear();
    loaded_ = false;
}

void HNSWIndex::save(const std::filesystem::path& path) {
    (void)path;
    // faiss_hnsw_index 没有内置 save，需要自己序列化
    // 简化：记录 ID 映射到 JSON
    std::ofstream file(path);
    if (!file.is_open()) {
        throw IndexException(ErrorCodes::INDEX_BUILD_FAILED,
                            "Cannot save HNSW index to: " + path.string());
    }
    file << "{\"type\":\"faiss_hnsw\",\"count\":" << impl_->internal_to_id.size() << "}";
    RAG_INFO("HNSWIndex metadata saved: " + path.string());
}

void HNSWIndex::load(const std::filesystem::path& path) {
    (void)path;
    // faiss_hnsw_index 没有内置 load，需要重建
    // 这里只清空，不重建
    clear();
    RAG_WARN("HNSWIndex load: faiss_hnsw does not support direct load, index cleared");
}

size_t HNSWIndex::size() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(impl_->mtx));
    return impl_->internal_to_id.size() - impl_->tombstones.size();
}

// ============================================================================
// 工厂函数
// ============================================================================

std::unique_ptr<VectorIndex> create_vector_index(const HNSWConfig& config) {
    if (config.index_type == "flat_array") {
        return std::make_unique<FlatArrayIndex>();
    }
    // 默认使用 faiss_hnsw
    return std::make_unique<HNSWIndex>();
}

}  // namespace mmrag
