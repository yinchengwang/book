/**
 * @file db_bm25_index.h
 * @brief 基于自研 BM25 引擎的 BM25Index 实现
 *
 * 包装 self-developed bm25_t C API，继承 mmrag::BM25Index 接口。
 * 支持 tombstone 删除（惰性删除，搜索时过滤）。
 */
#pragma once

#include "mmrag/config.h"
#include "mmrag/bm25_index.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <filesystem>

struct bm25;
typedef struct bm25 bm25_t;

namespace mmrag {

/**
 * @brief 基于自研 BM25 引擎的全文检索索引
 *
 * 继承 mmrag::BM25Index，将 mmrag 的 string ID 映射到 bm25_t 的 int32_t 内部 doc_id。
 */
class DbBM25Index : public BM25Index {
public:
    DbBM25Index();
    ~DbBM25Index();

    // 禁用拷贝
    DbBM25Index(const DbBM25Index&) = delete;
    DbBM25Index& operator=(const DbBM25Index&) = delete;

    // 初始化
    void init(const BM25Config& config);

    // 添加文档
    void add(const std::string& id, const std::string& content);

    // 批量添加
    void add_batch(const std::vector<std::string>& ids,
                   const std::vector<std::string>& contents);

    // 搜索
    std::vector<std::pair<std::string, float>> search(
        const std::string& query, int top_k);

    // 删除（tombstone）
    void remove(const std::string& id);

    // 清空
    void clear();

    // 保存到文件
    void save(const std::filesystem::path& path);

    // 从文件加载
    void load(const std::filesystem::path& path);

    // 获取文档数（不含 tombstone）
    size_t size() const;

    // 是否已加载
    bool is_loaded() const { return index_ != nullptr; }

private:
    bm25_t* index_ = nullptr;

    // string ID ↔ int32_t 内部 doc_id 映射
    std::unordered_map<std::string, int32_t> id_to_internal_;
    std::unordered_map<int32_t, std::string> internal_to_id_;

    // tombstone 集合（惰性删除）
    std::unordered_set<std::string> tombstones_;

    BM25Config config_;
    mutable std::mutex mtx_;
};

}  // namespace mmrag
