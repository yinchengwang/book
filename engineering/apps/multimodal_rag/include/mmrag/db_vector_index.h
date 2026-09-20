/**
 * @file db_vector_index.h
 * @brief 基于自研向量引擎的 VectorIndex 实现
 *
 * 包装 self-developed vector_engine C API，实现 mmrag::VectorIndex 接口。
 * 使用 vector_engine_create/open/insert/search/search_hnsw/build_index。
 */
#pragma once

#include "mmrag/vector_index.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace mmrag {

/**
 * @brief 基于自研向量引擎的向量索引
 *
 * 将 mmrag 的 string ID 映射到 vector_engine 的 uint64_t 内部 ID。
 * 支持 HNSW 搜索（自动构建索引）、持久化、WAL。
 */
class DbVectorIndex : public VectorIndex {
public:
    /**
     * @param data_dir   向量引擎数据目录（如 engineering/data/rag）
     * @param collection 集合名称（如 "rag_vectors"）
     */
    DbVectorIndex(const std::string& data_dir, const std::string& collection);
    ~DbVectorIndex() override;

    // 禁用拷贝
    DbVectorIndex(const DbVectorIndex&) = delete;
    DbVectorIndex& operator=(const DbVectorIndex&) = delete;

    void init(const HNSWConfig& config) override;

    void add(const std::string& id, const std::vector<float>& vector) override;
    void add_batch(const std::vector<std::string>& ids,
                   const std::vector<std::vector<float>>& vectors) override;

    std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query, int top_k) override;

    std::optional<std::vector<float>> get(const std::string& id) override;
    void remove(const std::string& id) override;
    void clear() override;

    void save(const std::filesystem::path& path) override;
    void load(const std::filesystem::path& path) override;

    int dimension() const override { return dimension_; }
    size_t size() const override;
    bool is_loaded() const override { return rel_ != nullptr; }

private:
    // string ID ↔ uint64_t 内部 ID 映射
    uint64_t next_internal_id_ = 1;
    std::unordered_map<std::string, uint64_t> id_to_internal_;
    std::unordered_map<uint64_t, std::string> internal_to_id_;
    std::unordered_map<uint64_t, std::vector<float>> vectors_;  // 内存中保留向量（用于 get）

    void* rel_ = nullptr;  // vector_engine_db_t*
    std::string collection_;
    std::string data_dir_;
    int dimension_ = 128;
    HNSWConfig config_;
    mutable std::mutex mtx_;

    void ensure_initialized();
};

}  // namespace mmrag
