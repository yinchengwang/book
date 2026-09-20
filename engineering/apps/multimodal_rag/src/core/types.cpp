/**
 * @file types.cpp
 * @brief 类型实现
 */

#include "mmrag/types.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace mmrag {

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";  // 版本 4
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

int64_t current_timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

int64_t current_timestamp() {
    return current_timestamp_ms() / 1000;
}

std::string truncate_string(const std::string& str, size_t max_len) {
    if (str.size() <= max_len) return str;
    if (max_len <= 3) return str.substr(0, max_len);
    return str.substr(0, max_len - 3) + "...";
}

}  // namespace mmrag
