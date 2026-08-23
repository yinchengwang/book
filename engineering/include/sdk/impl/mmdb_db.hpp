/**
 * @file mmdb_db.hpp
 * @brief C++ DB RAII 包装
 */
#ifndef SDK_IMPL_MMDB_DB_HPP
#define SDK_IMPL_MMDB_DB_HPP

#include <string>
#include <memory>
#include "sdk/mmdb.h"
#include "sdk/mmdb_types.h"
#include "sdk/impl/mmdb_error.hpp"
#include "sdk/impl/mmdb_collection.hpp"

namespace mmdb {

class DB {
public:
    explicit DB(const std::string& path, const mmdb_options_t* opts = nullptr) {
        raw_ = mmdb_open(path.c_str(), opts);
        if (!raw_) {
            throw Error(MMDB_ERR_INTERNAL, "mmdb_open failed for " + path);
        }
    }
    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;
    DB(DB&& other) noexcept : raw_(other.raw_) {
        other.raw_ = nullptr;
    }
    DB& operator=(DB&& other) noexcept {
        if (this != &other) {
            close();
            raw_ = other.raw_;
            other.raw_ = nullptr;
        }
        return *this;
    }
    ~DB() { close(); }

    Collection get_collection(const std::string& name) {
        mmdb_collection_t* c = mmdb_collection_get(raw_, name.c_str());
        if (!c) {
            throw Error(MMDB_ERR_NOT_FOUND, "collection not found: " + name);
        }
        return Collection(c, this);
    }

    Collection create_collection(const std::string& name,
                                   const mmdb_schema_t& schema) {
        mmdb_collection_t* c = mmdb_collection_create(raw_, name.c_str(), &schema);
        if (!c) {
            throw Error(mmdb_last_error_code(raw_),
                        mmdb_last_error_message(raw_));
        }
        return Collection(c, this);
    }

    int last_error_code() const {
        return raw_ ? mmdb_last_error_code(raw_) : MMDB_ERR_INVALID;
    }

    std::string last_error_message() const {
        if (!raw_) return mmdb_strerror(MMDB_ERR_INVALID);
        const char* msg = mmdb_last_error_message(raw_);
        return msg ? std::string(msg) : std::string();
    }

    mmdb_t* raw() noexcept { return raw_; }

private:
    void close() {
        if (raw_) {
            mmdb_close(raw_);
            raw_ = nullptr;
        }
    }

    mmdb_t* raw_ = nullptr;
};

}  // namespace mmdb

#endif
