/*
 * base_log.h - 统一日志系统
 *
 * 提供分级日志宏，支持子系统标签
 * 日志级别: TRACE < DEBUG < INFO < WARN < ERROR < FATAL
 */
#ifndef COMMON_BASE_LOG_H
#define COMMON_BASE_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 日志级别定义
// ============================================================

typedef enum {
    LOG_LEVEL_TRACE = 0,  // 跟踪（最详细）
    LOG_LEVEL_DEBUG = 1,  // 调试
    LOG_LEVEL_INFO  = 2,  // 信息
    LOG_LEVEL_WARN  = 3,  // 警告
    LOG_LEVEL_ERROR = 4,  // 错误
    LOG_LEVEL_FATAL = 5,  // 致命
} log_level_t;

// ============================================================
// 日志级别转字符串
// ============================================================

static inline const char *log_level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_TRACE: return "TRACE";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

// ============================================================
// 日志配置
// ============================================================

typedef struct log_config {
    log_level_t level;       // 日志级别阈值
    bool        console;     // 是否输出到控制台
    bool        file;        // 是否输出到文件
    const char *file_path;   // 日志文件路径
    size_t      max_file_size;  // 单个日志文件最大大小（字节）
    int         max_file_count; // 保留的日志文件数量
} log_config_t;

// 默认配置
#define LOG_CONFIG_DEFAULT { \
    .level = LOG_LEVEL_INFO,  \
    .console = true,          \
    .file = false,            \
    .file_path = NULL,        \
    .max_file_size = 10 * 1024 * 1024,  /* 10MB */ \
    .max_file_count = 5,      \
}

// ============================================================
// 日志写入函数（内部使用）
// ============================================================

/**
 * @brief 内部日志写入函数
 * @param level   日志级别
 * @param tag     子系统标签（如 "DB"、"RAG"），可为 NULL
 * @param file    源文件
 * @param line    源行号
 * @param func    函数名
 * @param fmt     格式化字符串
 * @param args    可变参数列表
 */
void log_write_impl(log_level_t level, const char *tag,
                    const char *file, int line, const char *func,
                    const char *fmt, va_list args);

/**
 * @brief 日志写入函数（简化版，内部调用 log_write_impl）
 * @param level   日志级别
 * @param tag     子系统标签
 * @param file    源文件
 * @param line    源行号
 * @param func    函数名
 * @param fmt     格式化字符串
 */
void log_write(log_level_t level, const char *tag,
               const char *file, int line, const char *func,
               const char *fmt, ...);

// ============================================================
// 标准日志宏（全局）
// ============================================================

/**
 * TRACE 日志 - 最详细的跟踪信息
 * 格式: [时间] [TRACE] [文件:行号] 函数名 - 消息
 */
#define LOG_TRACE(fmt, ...) \
    log_write(LOG_LEVEL_TRACE, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * DEBUG 日志 - 调试信息
 */
#define LOG_DEBUG(fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * INFO 日志 - 一般信息
 */
#define LOG_INFO(fmt, ...) \
    log_write(LOG_LEVEL_INFO, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * WARN 日志 - 警告信息
 */
#define LOG_WARN(fmt, ...) \
    log_write(LOG_LEVEL_WARN, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * ERROR 日志 - 错误信息
 */
#define LOG_ERROR(fmt, ...) \
    log_write(LOG_LEVEL_ERROR, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * FATAL 日志 - 致命错误
 */
#define LOG_FATAL(fmt, ...) \
    log_write(LOG_LEVEL_FATAL, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// ============================================================
// 带标签的日志宏（用于子系统）
// ============================================================

/**
 * 带标签的 TRACE 日志
 * @param tag 子系统标签（如 "DB"、"RAG"、"STORAGE"）
 */
#define LOGT(tag, fmt, ...) \
    log_write(LOG_LEVEL_TRACE, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * 带标签的 DEBUG 日志
 */
#define LOGD(tag, fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * 带标签的 INFO 日志
 */
#define LOGI(tag, fmt, ...) \
    log_write(LOG_LEVEL_INFO, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * 带标签的 WARN 日志
 */
#define LOGW(tag, fmt, ...) \
    log_write(LOG_LEVEL_WARN, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * 带标签的 ERROR 日志
 */
#define LOGE(tag, fmt, ...) \
    log_write(LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * 带标签的 FATAL 日志
 */
#define LOGF(tag, fmt, ...) \
    log_write(LOG_LEVEL_FATAL, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// ============================================================
// 条件日志宏
// ============================================================

/**
 * 条件日志 - 仅在条件为真时记录日志
 */
#define LOG_IF(cond, level, fmt, ...) \
    do { if (cond) log_write(level, NULL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while (0)

/**
 * 带标签的条件日志
 */
#define LOGT_IF(cond, tag, level, fmt, ...) \
    do { if (cond) log_write(level, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while (0)

// ============================================================
// 日志系统初始化
// ============================================================

/**
 * @brief 初始化日志系统
 * @param config 日志配置
 */
void log_init(const log_config_t *config);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
log_level_t log_get_level(void);

/**
 * @brief 设置日志级别
 * @param level 新的日志级别
 */
void log_set_level(log_level_t level);

/**
 * @brief 刷新日志缓冲区
 */
void log_flush(void);

/**
 * @brief 关闭日志系统
 */
void log_shutdown(void);

// ============================================================
// 便捷宏：子系统的标准日志（使用 X-Macro 生成）
// ============================================================

#define LOG_MACROS(XX) \
    XX(DB, "DB") \
    XX(DS, "DS") \
    XX(RAG, "RAG")

#define DEFINE_LOG_MACRO(prefix, tag) \
    #define prefix##_LOG_TRACE(fmt, ...) LOGT(tag, fmt, ##__VA_ARGS__) \
    #define prefix##_LOG_DEBUG(fmt, ...) LOGD(tag, fmt, ##__VA_ARGS__) \
    #define prefix##_LOG_INFO(fmt, ...)  LOGI(tag, fmt, ##__VA_ARGS__) \
    #define prefix##_LOG_WARN(fmt, ...)  LOGW(tag, fmt, ##__VA_ARGS__) \
    #define prefix##_LOG_ERROR(fmt, ...) LOGE(tag, fmt, ##__VA_ARGS__) \
    #define prefix##_LOG_FATAL(fmt, ...) LOGF(tag, fmt, ##__VA_ARGS__)

LOG_MACROS(DEFINE_LOG_MACRO)

#undef LOG_MACROS
#undef DEFINE_LOG_MACRO

#ifdef __cplusplus
}
#endif

#endif // COMMON_BASE_LOG_H
