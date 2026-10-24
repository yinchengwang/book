/**
 * @file mmdb_collection.hpp
 * @brief C++ Collection RAII 包装
 */
#ifndef SDK_IMPL_MMDB_COLLECTION_HPP
#define SDK_IMPL_MMDB_COLLECTION_HPP

#include <string>
#include <vector>
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_graph.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/mmdb_text.h"
#include "sdk/impl/mmdb_error.hpp"
#include "sdk/impl/mmdb_result.hpp"
#include "sdk/impl/mmdb_internal.h"

namespace mmdb {

class DB;  /* 前向声明 */

class Collection {
public:
    Collection() noexcept = default;
    Collection(mmdb_collection_t* raw, DB* db) noexcept
        : raw_(raw), db_(db), name_(raw && mmdb_collection_name(raw) ? mmdb_collection_name(raw) : "") {}
    Collection(const Collection&) = delete;
    Collection& operator=(const Collection&) = delete;
    Collection(Collection&& other) noexcept
        : raw_(other.raw_), db_(other.db_), name_(std::move(other.name_)) {
        other.raw_ = nullptr;
        other.db_ = nullptr;
    }
    Collection& operator=(Collection&& other) noexcept {
        if (this != &other) {
            drop();
            raw_ = other.raw_;
            db_ = other.db_;
            name_ = std::move(other.name_);
            other.raw_ = nullptr;
            other.db_ = nullptr;
        }
        return *this;
    }
    ~Collection() { drop(); }

    const std::string& name() const noexcept { return name_; }

    /* 向量操作 */
    void add_vectors(const std::vector<mmdb_vector_t>& vecs) {
        check(mmdb_vectors_add(raw_, vecs.data(), vecs.size()), raw_->db);
    }

    void upsert_vectors(const std::vector<mmdb_vector_t>& vecs) {
        check(mmdb_vectors_upsert(raw_, vecs.data(), vecs.size()), raw_->db);
    }

    void delete_vector(const std::vector<uint8_t>& id) {
        check(mmdb_vectors_delete(raw_, id.data(), id.size()), raw_->db);
    }

    Result search_vectors(const mmdb_query_t& q) {
        mmdb_result_t raw = {};
        check(mmdb_vectors_search(raw_, &q, &raw), raw_->db);
        Result r(&raw);
        mmdb_result_free(&raw);
        return r;
    }

    /* 图操作 */
    void add_graph_node(const std::string& id, const std::string& label,
                         const std::string& props = "") {
        mmdb_node_t n = {id.c_str(), label.empty() ? nullptr : label.c_str(),
                         props.empty() ? nullptr : props.c_str()};
        check(mmdb_graph_add_node(raw_, &n), raw_->db);
    }

    /* 时序操作 */
    void append_timeseries(int64_t timestamp, double value,
                            const std::string& tags = "") {
        mmdb_datapoint_t dp = {timestamp, value,
                                tags.empty() ? nullptr : tags.c_str()};
        check(mmdb_timeseries_append(raw_, &dp), raw_->db);
    }

    /* 文本操作 */
    void add_text(const std::string& id, const std::string& text,
                   const std::string& meta = "") {
        mmdb_text_entry_t e = {
            id.empty() ? nullptr : id.c_str(),
            text.c_str(),
            meta.empty() ? nullptr : meta.c_str()
        };
        check(mmdb_text_add(raw_, &e), raw_->db);
    }

    Result search_text(const std::string& query, size_t top_k,
                        const std::string& filter = "") {
        mmdb_text_query_t q = {
            query.c_str(),
            top_k,
            filter.empty() ? nullptr : filter.c_str()
        };
        mmdb_result_t raw = {};
        check(mmdb_text_search(raw_, &q, &raw), raw_->db);
        Result r(&raw);
        mmdb_result_free(&raw);
        return r;
    }

    mmdb_collection_t* raw() noexcept { return raw_; }

    void drop() {
        if (raw_) {
            mmdb_collection_drop(raw_);
            raw_ = nullptr;
            db_ = nullptr;
            name_.clear();
        }
    }

private:
    mmdb_collection_t* raw_ = nullptr;
    DB*                db_ = nullptr;
    std::string        name_;
};

}  // namespace mmdb

#endif
