/**
 * @file memory.cpp
 * @brief Agent 记忆系统实现
 */

#include "rag/modular/agent/memory.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace rag::modular::agent {

// ========== 构造函数 ==========

Memory::Memory(MemoryType type) : type_(type) {
}

// ========== 添加记忆 ==========

void Memory::add(const MemoryEntry& entry) {
    entries_.push_back(entry);
}

// ========== 获取最近记忆 ==========

std::vector<MemoryEntry> Memory::get_recent(int count) {
    if (count <= 0) {
        return {};
    }

    // 返回最近 count 条记忆
    size_t start_idx = 0;
    if (static_cast<size_t>(count) < entries_.size()) {
        start_idx = entries_.size() - count;
    }

    return std::vector<MemoryEntry>(
        entries_.begin() + start_idx,
        entries_.end()
    );
}

// ========== 转换为上下文 ==========

std::string Memory::to_context() {
    if (entries_.empty()) {
        return "";
    }

    std::ostringstream oss;

    // 根据记忆类型添加前缀
    switch (type_) {
        case MemoryType::SHORT_TERM:
            oss << "[Short-term Memory]\n";
            break;
        case MemoryType::LONG_TERM:
            oss << "[Long-term Memory]\n";
            break;
        case MemoryType::WORKING:
            oss << "[Working Memory]\n";
            break;
    }

    // 格式化每条记忆
    for (const auto& entry : entries_) {
        oss << entry.role << " [" << entry.timestamp << "]: " << entry.content << "\n";
    }

    return oss.str();
}

// ========== 生成时间戳 ==========

std::string Memory::generate_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

}  // namespace rag::modular::agent
