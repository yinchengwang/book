/**
 * @file structured_log.h
 * @brief 结构化日志系统
 *
 * 基于 db/log.h 的结构化日志扩展，支持 JSON 格式输出。
 */
#ifndef DB_STRUCTURED_LOG_H
#define DB_STRUCTURED_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 日志输出目标（位掩码）
 * ======================================================================== */

#define SLOG_TARGET_STDOUT  0x01
#define SLOG_TARGET_STDERR  0x02
#define SLOG_TARGET_FILE    0x04
#define SLOG_TARGET_JSON    0x08

/* ========================================================================
 * 日志配置
 * ======================================================================== */

typedef struct StructuredLogConfig_s {
    int min_level;           /**< 最低日志级别（0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=FATAL） */
    int targets;             /**< 输出目标（位掩码） */
    const char *file_path;   /**< 日志文件路径 */
    bool enable_colors;      /**< 是否启用彩色输出 */
    bool enable_timestamp;   /**< 是否启用时间戳 */
    bool enable_module;     /**< 是否启用模块名 */
} StructuredLogConfig;

/* ========================================================================
 * API
 * ======================================================================== */

StructuredLogConfig structured_log_default_config(void);
int structured_log_init(const StructuredLogConfig *config);
void structured_log_shutdown(void);
void structured_log_set_level(int level);
void structured_log_write(int level, const char *module,
                         const char *file, int line,
                         const char *fmt, va_list args);
const char *structured_log_level_string(int level);

/* ========================================================================
 * 宏定义
 * ======================================================================== */

#define SLOG_TRACE(module, fmt, ...) \
    structured_log_write(0, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SLOG_DEBUG(module, fmt, ...) \
    structured_log_write(1, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SLOG_INFO(module, fmt, ...) \
    structured_log_write(2, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SLOG_WARN(module, fmt, ...) \
    structured_log_write(3, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SLOG_ERROR(module, fmt, ...) \
    structured_log_write(4, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SLOG_FATAL(module, fmt, ...) \
    structured_log_write(5, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ========================================================================
 * 请求日志
 * ======================================================================== */

typedef struct RequestLogEntry_s {
    const char *request_id;
    const char *method;
    const char *path;
    const char *client_ip;
    int status;
    double duration_ms;
    int64_t timestamp_ms;
} RequestLogEntry;

void structured_log_request(const RequestLogEntry *entry);

#ifdef __cplusplus
}
#endif

#endif /* DB_STRUCTURED_LOG_H */
