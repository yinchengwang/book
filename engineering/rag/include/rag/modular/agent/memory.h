/**
 * @file memory.h
 * @brief Agent 记忆系统 - 短期/长期/工作记忆
 */
#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace rag::modular::agent {

// ========== 记忆条目 ==========

/**
 * @brief 记忆条目
 */
struct MemoryEntry {
    std::string role;              // 角色 (user/assistant/system)
    std::string content;           // 内容
    std::string timestamp;         // 时间戳
};

// ========== 记忆类型 ==========

/**
 * @brief 记忆类型
 */
enum class MemoryType { SHORT_TERM, LONG_TERM, WORKING };

// ========== 记忆类 ==========

/**
 * @brief 记忆基类
 *
 * 支持三种记忆类型:
 * - SHORT_TERM: 短期记忆，用于当前会话的上下文
 * - LONG_TERM: 长期记忆，用于持久化存储
 * - WORKING: 工作记忆，用于当前推理过程
 */
class Memory {
public:
    /**
     * @brief 构造函数
     * @param type 记忆类型
     */
    explicit Memory(MemoryType type);

    /**
     * @brief 添加记忆条目
     * @param entry 记忆条目
     */
    void add(const MemoryEntry& entry);

    /**
     * @brief 获取最近的记忆
     * @param count 要获取的记忆数量
     * @return 记忆条目列表
     */
    std::vector<MemoryEntry> get_recent(int count);

    /**
     * @brief 转换为上下文字符串
     * @return 格式化的上下文字符串
     */
    std::string to_context();

private:
    /**
     * @brief 生成时间戳
     * @return ISO 格式的时间戳字符串
     */
    std::string generate_timestamp();

    MemoryType type_;                              // 记忆类型
    std::vector<MemoryEntry> entries_;             // 记忆条目列表
};

}  // namespace rag::modular::agent
