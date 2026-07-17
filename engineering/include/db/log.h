/**
 * @file log.h
 * @brief 统一日志模块头文件
 *
 * 提供分级日志输出、速率限制、折叠显示功能。
 */
#ifndef DB_LOG_H
#define DB_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 日志级别定义
 * ======================================================================== */

/**
 * @brief 日志级别枚举
 */
typedef enum {
    LOG_LEVEL_TRACE = 0,  /**< 跟踪信息，最详细 */
    LOG_LEVEL_DEBUG = 1,   /**< 调试信息 */
    LOG_LEVEL_INFO = 2,    /**< 一般信息 */
    LOG_LEVEL_WARN = 3,    /**< 警告信息 */
    LOG_LEVEL_ERROR = 4,   /**< 错误信息 */
    LOG_LEVEL_FATAL = 5,   /**< 致命错误 */
    LOG_LEVEL_OFF = 6,     /**< 关闭所有日志 */
} log_level_t;

/* ========================================================================
 * 日志输出目标
 * ======================================================================== */

/**
 * @brief 日志输出目标类型
 */
typedef enum {
    LOG_TARGET_CONSOLE = 1 << 0,  /**< 控制台输出 */
    LOG_TARGET_FILE    = 1 << 1,  /**< 文件输出 */
    LOG_TARGET_RING   = 1 << 2,  /**< 环形缓冲区 */
} log_target_t;

/* ========================================================================
 * 配置结构
 * ======================================================================== */

/**
 * @brief 速率限制配置
 */
typedef struct {
    uint32_t second_burst;   /**< 1秒内最大打印次数，默认: 2 */
    uint32_t hour_cap;       /**< 1小时内最大打印次数，默认: 50 */
} log_rate_limit_t;

/**
 * @brief 日志配置
 */
typedef struct {
    log_level_t     level;              /**< 日志级别 */
    log_target_t    target;              /**< 输出目标 */
    const char     *file_path;           /**< 日志文件路径 */
    uint32_t       max_file_size;       /**< 单文件最大大小，默认: 10MB */
    uint32_t       max_file_count;      /**< 保留文件数，默认: 5 */
    bool           enable_colors;        /**< 启用彩色输出 */
    bool           enable_rate_limit;    /**< 启用速率限制 */
    log_rate_limit_t rate_limit;         /**< 速率限制配置 */
    uint32_t       ring_buffer_size;    /**< 环形缓冲区大小，默认: 4096 */
} log_config_t;

/* ========================================================================
 * 抑制条目结构
 * ======================================================================== */

/**
 * @brief 抑制条目（内部使用）
 */
typedef struct log_entry_node_s {
    char        key[256];               /**< 唯一标识键 */
    uint64_t    hash;                   /**< key 哈希值 */
    uint32_t    print_count;            /**< 已打印次数 */
    uint32_t    total_count;            /**< 总发生次数 */
    time_t      first_time;             /**< 首次出现时间 */
    time_t      last_time;              /**< 上次打印时间 */
    uint32_t    second_count;           /**< 本秒内已打印 */
    time_t      second_reset;           /**< 秒计数重置时间 */
    uint32_t    hour_count;             /**< 本小时内已打印 */
    time_t      hour_reset;             /**< 小时计数重置时间 */
    uint32_t    suppress_since_print;   /**< 上次打印后被抑制次数 */
} log_entry_node_t;

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 日志统计信息
 */
typedef struct {
    uint64_t total_logged;      /**< 总打印数 */
    uint64_t total_suppressed;  /**< 总抑制数 */
    uint64_t active_entries;     /**< 当前活跃条目数 */
    uint64_t peak_entries;       /**< 峰值条目数 */
} log_stats_t;

/* ========================================================================
 * 环形缓冲区条目
 * ======================================================================== */

/**
 * @brief 日志条目（用于环形缓冲区）
 */
typedef struct {
    time_t      timestamp;       /**< 时间戳 */
    log_level_t level;            /**< 日志级别 */
    char        file[64];        /**< 文件名 */
    int         line;            /**< 行号 */
    char        function[64];     /**< 函数名 */
    char        message[512];     /**< 日志消息 */
    uint64_t    suppressed_count;/**< 被抑制次数 */
} log_entry_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/* --- 生命周期 --- */

/**
 * @brief 初始化日志系统
 * @param config 日志配置
 * @return 成功返回 0，失败返回 -1
 */
int log_init(const log_config_t *config);

/**
 * @brief 重新配置日志系统
 * @param config 新的日志配置
 * @return 成功返回 0，失败返回 -1
 */
int log_reconfigure(const log_config_t *config);

/**
 * @brief 获取当前日志配置
 * @return 当前日志配置
 */
log_config_t log_get_config(void);

/**
 * @brief 关闭日志系统
 */
void log_shutdown(void);

/**
 * @brief 检查日志系统是否已初始化
 * @return 已初始化返回 true，否则返回 false
 */
bool log_is_initialized(void);

/* --- 级别控制 --- */

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void log_set_level(log_level_t level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
log_level_t log_get_level(void);

/**
 * @brief 设置是否启用速率限制
 * @param enable true 启用，false 禁用
 */
void log_set_rate_limit(bool enable);

/* --- 统计 --- */

/**
 * @brief 获取日志统计信息
 * @param stats 输出统计信息
 */
void log_get_stats(log_stats_t *stats);

/**
 * @brief 重置统计信息
 */
void log_reset_stats(void);

/* --- 日志输出 --- */

/**
 * @brief 记录日志（支持速率限制）
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @return 是否打印成功（可能被抑制返回 false）
 */
bool log_log(log_level_t level, const char *file, int line,
             const char *func, const char *fmt, ...);

/**
 * @brief 强制记录日志（不受速率限制影响）
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_log_force(log_level_t level, const char *file, int line,
                   const char *func, const char *fmt, ...);

/**
 * @brief vprintf 版本的 log_log
 */
bool log_logv(log_level_t level, const char *file, int line,
              const char *func, const char *fmt, va_list ap);

/**
 * @brief vprintf 版本的 log_log_force
 */
void log_logv_force(log_level_t level, const char *file, int line,
                    const char *func, const char *fmt, va_list ap);

/* --- 环形缓冲区 --- */

/**
 * @brief 读取环形缓冲区中的日志
 * @param out 输出缓冲区
 * @param max_count 最大读取条数
 * @return 实际读取条数
 */
uint32_t log_ring_peek(log_entry_t *out, uint32_t max_count);

/**
 * @brief 清空环形缓冲区
 */
void log_ring_clear(void);

/* ========================================================================
 * 日志宏定义
 * ======================================================================== */

/**
 * @brief 标准日志宏
 */
#define LOG_TRACE(fmt, ...) log_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, \
                                    __func__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, \
                                    __func__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, \
                                    __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, \
                                    __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, \
                                    __func__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) log_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, \
                                    __func__, fmt, ##__VA_ARGS__)

/**
 * @brief 强制打印宏（不受速率限制影响）
 */
#define LOGF_TRACE(fmt, ...) log_log_force(LOG_LEVEL_TRACE, __FILE__, __LINE__, \
                                           __func__, fmt, ##__VA_ARGS__)
#define LOGF_DEBUG(fmt, ...) log_log_force(LOG_LEVEL_DEBUG, __FILE__, __LINE__, \
                                           __func__, fmt, ##__VA_ARGS__)
#define LOGF_INFO(fmt, ...)  log_log_force(LOG_LEVEL_INFO,  __FILE__, __LINE__, \
                                           __func__, fmt, ##__VA_ARGS__)
#define LOGF_WARN(fmt, ...)  log_log_force(LOG_LEVEL_WARN,  __FILE__, __LINE__, \
                                           __func__, fmt, ##__VA_ARGS__)
#define LOGF_ERROR(fmt, ...) log_log_force(LOG_LEVEL_ERROR, __FILE__, __LINE__, \
                                           __func__, fmt, ##__VA_ARGS__)
#define LOGF_FATAL(fmt, ...) log_log_force(LOG_LEVEL_FATAL, __FILE__, __LINE__, \
                                           __func__, fmt, ##__VA_ARGS__)

/**
 * @brief 条件日志宏
 */
#define LOG_IF(level, cond, fmt, ...) \
    do { if (cond) log_log(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while (0)

#define LOGD_IF(cond, fmt, ...) LOG_IF(LOG_LEVEL_DEBUG, cond, fmt, ##__VA_ARGS__)
#define LOGI_IF(cond, fmt, ...) LOG_IF(LOG_LEVEL_INFO,  cond, fmt, ##__VA_ARGS__)
#define LOGW_IF(cond, fmt, ...) LOG_IF(LOG_LEVEL_WARN,  cond, fmt, ##__VA_ARGS__)
#define LOGE_IF(cond, fmt, ...) LOG_IF(LOG_LEVEL_ERROR, cond, fmt, ##__VA_ARGS__)

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 获取日志级别对应的名称
 * @param level 日志级别
 * @return 级别名称字符串
 */
const char *log_level_name(log_level_t level);

/**
 * @brief 获取日志级别对应的 ANSI 颜色码
 * @param level 日志级别
 * @return ANSI 颜色码字符串
 */
const char *log_level_color(log_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* DB_LOG_H */
