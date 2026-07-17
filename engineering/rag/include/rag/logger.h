/**
 * @file logger.h
 * @brief 简化日志系统
 */
#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

// 取消 Windows 预定义的 ERROR 宏 (如果存在)
#ifdef ERROR
#define RAG_ERROR_MACRO_WAS_DEFINED
#undef ERROR
#endif

namespace rag {

// 日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERR
};

// 获取当前日志级别
inline LogLevel get_log_level() {
    return LogLevel::INFO;
}

// 时间戳格式化
inline std::string format_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// 内部日志函数
inline void log_message(LogLevel level, const std::string& msg) {
    const char* level_str = "UNKNOWN";
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARN: level_str = "WARN"; break;
        case LogLevel::ERR: level_str = "ERROR"; break;
    }
    std::cerr << "[" << format_timestamp() << "] [" << level_str << "] [rag] " << msg << std::endl;
}

}  // namespace rag

// 恢复 Windows ERROR 宏 (如果之前存在)
#ifdef RAG_ERROR_MACRO_WAS_DEFINED
#define ERROR 3
#undef RAG_ERROR_MACRO_WAS_DEFINED
#endif

#define RAG_DEBUG(msg) do { if (rag::get_log_level() <= rag::LogLevel::DEBUG) rag::log_message(rag::LogLevel::DEBUG, msg); } while(0)
#define RAG_INFO(msg) do { if (rag::get_log_level() <= rag::LogLevel::INFO) rag::log_message(rag::LogLevel::INFO, msg); } while(0)
#define RAG_WARN(msg) do { if (rag::get_log_level() <= rag::LogLevel::WARN) rag::log_message(rag::LogLevel::WARN, msg); } while(0)
#define RAG_ERROR(msg) do { if (rag::get_log_level() <= rag::LogLevel::ERR) rag::log_message(rag::LogLevel::ERR, msg); } while(0)
