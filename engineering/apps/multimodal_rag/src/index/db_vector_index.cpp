/**
 * @file db_vector_index.cpp
 * @brief 基于自研向量引擎的 VectorIndex 实现
 *
 * 包装 self-developed vector_engine C API，将 mmrag string ID
 * 映射到 vector_engine uint64_t 内部 ID。
 *
 * 注意：vector_engine_search_hnsw 会自动在首次搜索时构建 HNSW 索引。
 * 持久化使用 vector_index_save/load（VIDX 格式）。
 */

#include "mmrag/db_vector_index.h"
#include "mmrag/logger.h"
#include "db/vector_engine.h"
#include "db/storage/vector/vector_engine.h"
#include <stdexcept>
#include <fstream>
#include <sstream>

namespace mmrag {

// ========== 构造 / 析构 ==========

DbVectorIndex::DbVectorIndex(const std::string& data_dir,
                             const std::string& collection)
    : data_dir_(data_dir), collection_(collection) {}

DbVectorIndex::~DbVectorIndex() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (rel_) {
        vector_engine_close(rel_);
        rel_ = nullptr;
    }
}

// ========== init ==========

void DbVectorIndex::init(const HNSWConfig& config) {
    std::lock_guard<std::mutex> lock(mtx_);
    config_ = config;
    dimension_ = config.dim > 0 ? config.dim : 128;

    // 初始化向量引擎全局（幂等）
    vector_engine_init(data_dir_.c_str());

    // 设置默认维度
    vector_engine_set_default_dimension(dimension_);

    // 尝试打开已有集合
    rel_ = vector_engine_open(collection_.c_str(), ACCESS_MODE_READ_WRITE);

    if (!rel_) {
        // 集合不存在，创建并打开
        storage_schema_t schema = {0};
        if (vector_engine_create(collection_.c_str(), &schema) != 0) {
            throw std::runtime_error("Failed to create vector collection: " + collection_);
        }
        rel_ = vector_engine_open(collection_.c_str(), ACCESS_MODE_READ_WRITE);
        if (!rel_) {
            throw std::runtime_error("Failed to open vector collection: " + collection_);
        }
    }

    // 从 vector_engine_db_t 读取实际维度
    auto* db = static_cast<vector_engine_db_t*>(rel_);
    dimension_ = db->dimension;

    RAG_INFO("DbVectorIndex initialized: collection=" + collection_ +
             ", dim=" + std::to_string(dimension_) +
             ", vectors=" + std::to_string(db->num_vectors));
}

// ========== add ==========

void DbVectorIndex::add(const std::string& id, const std::vector<float>& vector) {
    std::lock_guard<std::mutex> lock(mtx_);
    ensure_initialized();

    if (static_cast<int>(vector.size()) != dimension_) {
        throw std::runtime_error(
            "Vector dimension mismatch: expected " + std::to_string(dimension_) +
            ", got " + std::to_string(vector.size()));
    }

    // 构造内部格式：uint64_t id + int32_t dim + float[dim]
    std::vector<uint8_t> internal_data(sizeof(uint64_t) + sizeof(int32_t) + vector.size() * sizeof(float));
    uint64_t internal_id = next_internal_id_++;
    memcpy(internal_data.data(), &internal_id, sizeof(uint64_t));
    int32_t dim = static_cast<int32_t>(vector.size());
    memcpy(internal_data.data() + sizeof(uint64_t), &dim, sizeof(int32_t));
    memcpy(internal_data.data() + sizeof(uint64_t) + sizeof(int32_t), vector.data(), vector.size() * sizeof(float));

    // 保存映射
    id_to_internal_[id] = internal_id;
    internal_to_id_[internal_id] = id;
    vectors_[internal_id] = vector;

    // 插入向量引擎（传入构造好的内部格式）
    int rc = vector_engine_insert(rel_, internal_data.data(), internal_data.size());
    if (rc != 0) {
        // 回滚映射
        id_to_internal_.erase(id);
        internal_to_id_.erase(internal_id);
        vectors_.erase(internal_id);
        throw std::runtime_error("vector_engine_insert failed for id: " + id);
    }
}

void DbVectorIndex::add_batch(const std::vector<std::string>& ids,
                              const std::vector<std::vector<float>>& vectors) {
    for (size_t i = 0; i < ids.size(); ++i) {
        add(ids[i], vectors[i]);
    }
}

// ========== search ==========

std::vector<std::pair<std::string, float>> DbVectorIndex::search(
    const std::vector<float>& query, int top_k) {

    std::lock_guard<std::mutex> lock(mtx_);
    ensure_initialized();

    if (static_cast<int>(query.size()) != dimension_) {
        throw std::runtime_error(
            "Query dimension mismatch: expected " + std::to_string(dimension_) +
            ", got " + std::to_string(query.size()));
    }

    vector_search_results_t results = {0};

    // 优先使用 HNSW 搜索（自动构建索引）
    int rc = vector_engine_search_hnsw(rel_, query.data(), top_k, &results);

    // 回退到暴力搜索
    if (rc != 0) {
        rc = vector_engine_search(rel_, query.data(), dimension_, top_k, &results);
    }

    if (rc != 0) {
        RAG_WARN("vector_engine_search failed");
        return {};
    }

    // 转换结果：internal_id → string_id
    std::vector<std::pair<std::string, float>> mapped;
    for (int i = 0; i < results.count; ++i) {
        uint64_t int_id = results.results[i].id;
        float distance = results.results[i].distance;

        auto it = internal_to_id_.find(int_id);
        if (it != internal_to_id_.end()) {
            mapped.emplace_back(it->second, distance);
        }
    }

    vector_engine_free_results(&results);
    return mapped;
}

// ========== get ==========

std::optional<std::vector<float>> DbVectorIndex::get(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = id_to_internal_.find(id);
    if (it == id_to_internal_.end()) return std::nullopt;

    auto vit = vectors_.find(it->second);
    if (vit == vectors_.end()) return std::nullopt;
    return vit->second;
}

// ========== remove ==========

void DbVectorIndex::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = id_to_internal_.find(id);
    if (it == id_to_internal_.end()) return;

    uint64_t internal_id = it->second;
    internal_to_id_.erase(internal_id);
    vectors_.erase(internal_id);
    id_to_internal_.erase(it);

    // 注意：vector_engine 目前不支持单条删除。
    // 重建索引时会自然排除已移除的向量。
}

// ========== clear ==========

void DbVectorIndex::clear() {
    std::lock_guard<std::mutex> lock(mtx_);

    if (rel_) {
        vector_engine_close(rel_);
        rel_ = nullptr;
    }

    // 删除集合
    vector_engine_drop(collection_.c_str());

    // 重新创建
    storage_schema_t schema = {0};
    vector_engine_create(collection_.c_str(), &schema);
    rel_ = vector_engine_open(collection_.c_str(), ACCESS_MODE_READ_WRITE);

    id_to_internal_.clear();
    internal_to_id_.clear();
    vectors_.clear();
    next_internal_id_ = 1;

    RAG_INFO("DbVectorIndex cleared: " + collection_);
}

// ========== save / load ==========

void DbVectorIndex::save(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    ensure_initialized();

    // 保存向量引擎索引（VIDX 格式）
    std::string vidx_path = path.string() + ".vidx";
    vector_persist_result_t result = {0};
    int rc = vector_index_save(rel_, vidx_path.c_str(), &result);
    if (rc != 0) {
        throw std::runtime_error("vector_index_save failed: " + vidx_path);
    }

    // 保存 string ID ↔ internal ID 映射（简单文本格式）
    std::string map_path = path.string() + ".idmap.txt";
    std::ofstream ofs(map_path);
    ofs << "next_id=" << next_internal_id_ << "\n";
    for (const auto& [sid, iid] : id_to_internal_) {
        ofs << sid << "\t" << iid << "\n";
    }

    RAG_INFO("DbVectorIndex saved: " + std::to_string(id_to_internal_.size()) +
             " vectors to " + vidx_path);
}

void DbVectorIndex::load(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    ensure_initialized();

    // 加载 string ID ↔ internal ID 映射
    std::string map_path = path.string() + ".idmap.txt";
    if (std::filesystem::exists(map_path)) {
        std::ifstream ifs(map_path);
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.substr(0, 8) == "next_id=") {
                next_internal_id_ = std::stoull(line.substr(8));
            } else {
                auto tab_pos = line.find('\t');
                if (tab_pos != std::string::npos) {
                    std::string sid = line.substr(0, tab_pos);
                    uint64_t iid = std::stoull(line.substr(tab_pos + 1));
                    id_to_internal_[sid] = iid;
                    internal_to_id_[iid] = sid;
                }
            }
        }
    }

    // 加载向量引擎索引
    std::string vidx_path = path.string() + ".vidx";
    if (std::filesystem::exists(vidx_path)) {
        vector_persist_result_t result = {0};
        int rc = vector_index_load(rel_, vidx_path.c_str(), &result);
        if (rc != 0) {
            RAG_WARN("vector_index_load failed: " + vidx_path);
        }
    }

    RAG_INFO("DbVectorIndex loaded: " + std::to_string(id_to_internal_.size()) +
             " vectors from " + vidx_path);
}

// ========== size ==========

size_t DbVectorIndex::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return id_to_internal_.size();
}

// ========== ensure_initialized ==========

void DbVectorIndex::ensure_initialized() {
    if (!rel_) {
        throw std::runtime_error("DbVectorIndex not initialized. Call init() first.");
    }
}

}  // namespace mmrag
