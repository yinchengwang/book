/**
 * @file mmdb_error.hpp
 * @brief C++ 异常类型
 */
#ifndef SDK_IMPL_MMDB_ERROR_HPP
#define SDK_IMPL_MMDB_ERROR_HPP

#include <stdexcept>
#include <string>
#include "sdk/mmdb_error.h"

namespace mmdb {

/**
 * @brief SDK 异常类，包装 mmdb_error_t 错误码
 */
class Error : public std::runtime_error {
public:
    explicit Error(int code, const std::string& msg)
        : std::runtime_error(msg), code_(code) {}

    int code() const noexcept { return code_; }

private:
    int code_;
};

/**
 * @brief 抛出异常（如果 rc != MMDB_OK）
 */
inline void check(int rc, mmdb_t* db = nullptr) {
    if (rc != MMDB_OK) {
        const char* msg = db ? mmdb_last_error_message(db) : mmdb_strerror(rc);
        throw Error(rc, msg ? msg : "unknown error");
    }
}

}  // namespace mmdb

#endif
