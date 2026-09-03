/**
 * @file mmdb_result.hpp
 * @brief C++ 结果 RAII 包装
 */
#ifndef SDK_IMPL_MMDB_RESULT_HPP
#define SDK_IMPL_MMDB_RESULT_HPP

#include <vector>
#include <string>
#include <cstring>
#include "sdk/mmdb.h"

namespace mmdb {

/**
 * @brief 单条结果（id + distance + metadata + text）
 */
struct Hit {
    std::vector<uint8_t> id;
    float                distance = 0.0f;
    std::string          metadata_json;
    std::string          text;
};

/**
 * @brief 搜索结果 RAII 包装
 */
class Result {
public:
    Result() = default;
    explicit Result(mmdb_result_t* raw) {
        copy_from(raw);
    }
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&& other) noexcept : hits_(std::move(other.hits_)) {}
    Result& operator=(Result&& other) noexcept {
        if (this != &other) hits_ = std::move(other.hits_);
        return *this;
    }
    ~Result() = default;

    const std::vector<Hit>& hits() const noexcept { return hits_; }
    size_t size() const noexcept { return hits_.size(); }
    bool empty() const noexcept { return hits_.empty(); }

private:
    std::vector<Hit> hits_;

    void copy_from(mmdb_result_t* raw) {
        if (!raw) return;
        for (size_t i = 0; i < raw->count; i++) {
            Hit h;
            if (raw->items[i].id && raw->items[i].id_len > 0) {
                h.id.assign(raw->items[i].id, raw->items[i].id + raw->items[i].id_len);
            }
            h.distance = raw->items[i].distance;
            if (raw->items[i].metadata_json) h.metadata_json = raw->items[i].metadata_json;
            if (raw->items[i].text) h.text = raw->items[i].text;
            hits_.push_back(std::move(h));
        }
    }
};

}  // namespace mmdb

#endif
