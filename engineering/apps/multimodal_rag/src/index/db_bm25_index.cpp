/**
 * @file db_bm25_index.cpp
 * @brief 基于自研 BM25 引擎的 BM25Index 实现
 *
 * 包装 self-developed bm25_t C API，将 mmrag string ID
 * 映射到 bm25_t int32_t 内部 doc_id。
 *
 * 删除使用 tombstone 机制：remove 标记文档为已删除，
 * 搜索时过滤掉 tombstone 中的文档。
 */

#include "mmrag/db_bm25_index.h"
#include "mmrag/logger.h"
#include "db/index/vector_index/BM25/bm25.h"
#include <stdexcept>
#include <fstream>

namespace mmrag {

// ========== 构造 / 析构 ==========

DbBM25Index::DbBM25Index() = default;

DbBM25Index::~DbBM25Index() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (index_) {
        bm25_index_drop(index_);
        index_ = nullptr;
    }
}

// ========== init ==========

void DbBM25Index::init(const BM25Config& config) {
    std::lock_guard<std::mutex> lock(mtx_);
    config_ = config;

    // 创建 BM25 索引
    bm25_params_t params = {0};
    params.k1 = config.k1 > 0 ? config.k1 : 1.2f;
    params.b = config.b > 0 ? config.b : 0.75f;

    index_ = bm25_index_create_with_params(&params);
    if (!index_) {
        throw std::runtime_error("Failed to create BM25 index");
    }

    RAG_INFO("DbBM25Index initialized: k1=" + std::to_string(params.k1) +
             ", b=" + std::to_string(params.b));
}

// ========== add ==========

void DbBM25Index::add(const std::string& id, const std::string& content) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!index_) throw std::runtime_error("BM25 index not initialized");

    // 添加文本到 BM25 索引（返回自动分配的 doc_id）
    int32_t doc_id = bm25_index_add_text(index_, content.c_str());
    if (doc_id < 0) {
        throw std::runtime_error("bm25_index_add_text failed for id: " + id);
    }

    // 维护 string ID ↔ internal doc_id 映射
    // 注意：BM25 引擎内部会复用已删除的 doc_id
    id_to_internal_[id] = doc_id;
    internal_to_id_[doc_id] = id;
}

void DbBM25Index::add_batch(const std::vector<std::string>& ids,
                            const std::vector<std::string>& contents) {
    for (size_t i = 0; i < ids.size(); ++i) {
        add(ids[i], contents[i]);
    }
}

// ========== search ==========

std::vector<std::pair<std::string, float>> DbBM25Index::search(
    const std::string& query, int top_k) {

    std::lock_guard<std::mutex> lock(mtx_);
    if (!index_) return {};

    // BM25 搜索：多取一些结果以补偿 tombstone 过滤
    int32_t fetch_k = top_k + static_cast<int32_t>(tombstones_.size()) + 10;
    std::vector<float> scores(fetch_k);
    std::vector<int32_t> doc_ids(fetch_k);

    int32_t hit_count = bm25_index_search_text_with_count(
        index_, query.c_str(), fetch_k,
        scores.data(), doc_ids.data(), &hit_count);

    if (hit_count <= 0) return {};

    // 转换结果：过滤 tombstone，internal_id → string_id
    // 注意：BM25 会复用已删除的 doc_id，所以 tombstone 检查时要匹配 string_id
    std::vector<std::pair<std::string, float>> results;
    for (int32_t i = 0; i < hit_count && static_cast<int>(results.size()) < top_k; ++i) {
        int32_t int_id = doc_ids[i];
        float score = scores[i];

        // int_id == -1 表示该槽位无结果
        if (int_id < 0) continue;

        // 查找 string_id
        auto id_it = internal_to_id_.find(int_id);
        if (id_it == internal_to_id_.end()) continue;

        // 过滤 tombstone（检查 string_id 而非 doc_id，因为 doc_id 会被复用）
        if (tombstones_.count(id_it->second) > 0) continue;

        results.emplace_back(id_it->second, score);
    }

    return results;
}

// ========== remove ==========

void DbBM25Index::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = id_to_internal_.find(id);
    if (it == id_to_internal_.end()) return;

    int32_t doc_id = it->second;
    bm25_delete_document(index_, doc_id);

    // 标记 tombstone（用 string_id，因为 BM25 会复用 doc_id）
    tombstones_.insert(id);

    // 从映射中移除
    internal_to_id_.erase(doc_id);
    id_to_internal_.erase(it);
}

// ========== clear ==========

void DbBM25Index::clear() {
    std::lock_guard<std::mutex> lock(mtx_);

    if (index_) {
        bm25_index_drop(index_);
        index_ = nullptr;
    }

    // 重新创建
    bm25_params_t params = {0};
    params.k1 = config_.k1 > 0 ? config_.k1 : 1.2f;
    params.b = config_.b > 0 ? config_.b : 0.75f;
    index_ = bm25_index_create_with_params(&params);

    id_to_internal_.clear();
    internal_to_id_.clear();
    tombstones_.clear();

    RAG_INFO("DbBM25Index cleared");
}

// ========== save / load ==========

void DbBM25Index::save(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!index_) return;

    // 保存 BM25 索引
    std::string bm25_path = path.string();
    int32_t rc = bm25_save(index_, bm25_path.c_str());
    if (rc != 0) {
        throw std::runtime_error("bm25_save failed: " + bm25_path);
    }

    // 保存 string ID ↔ internal doc_id 映射 + tombstones（简单文本格式）
    std::string map_path = path.string() + ".idmap.txt";
    std::ofstream ofs(map_path);
    for (const auto& [sid, did] : id_to_internal_) {
        ofs << "m\t" << sid << "\t" << did << "\n";
    }
    for (const auto& t : tombstones_) {
        ofs << "t\t" << t << "\n";
    }

    RAG_INFO("DbBM25Index saved: " + std::to_string(id_to_internal_.size()) +
             " docs to " + bm25_path);
}

void DbBM25Index::load(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mtx_);

    // 加载 BM25 索引
    std::string bm25_path = path.string();
    if (index_) {
        bm25_index_drop(index_);
        index_ = nullptr;
    }
    index_ = bm25_index_load(bm25_path.c_str());
    if (!index_) {
        RAG_WARN("bm25_index_load failed: " + bm25_path);
        return;
    }

    // 加载 string ID ↔ internal doc_id 映射 + tombstones
    std::string map_path = path.string() + ".idmap.txt";
    if (std::filesystem::exists(map_path)) {
        std::ifstream ifs(map_path);
        std::string line;
        while (std::getline(ifs, line)) {
            auto tab1 = line.find('\t');
            auto tab2 = line.find('\t', tab1 + 1);
            if (tab1 != std::string::npos && tab2 != std::string::npos) {
                char type = line[0];
                std::string sid = line.substr(tab1 + 1, tab2 - tab1 - 1);
                int32_t did = std::stoi(line.substr(tab2 + 1));
                if (type == 'm') {
                    id_to_internal_[sid] = did;
                    internal_to_id_[did] = sid;
                } else if (type == 't') {
                    tombstones_.insert(sid);
                }
            }
        }
    }

    RAG_INFO("DbBM25Index loaded: " + std::to_string(id_to_internal_.size()) +
             " docs from " + bm25_path);
}

// ========== size ==========

size_t DbBM25Index::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    // 使用 BM25 引擎的文档计数（不含已删除的）
    return static_cast<size_t>(std::max(0, bm25_index_size(index_)));
}

}  // namespace mmrag
