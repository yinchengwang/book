/*
 * ivf_log.h
 *
 * IVF 索引专用日志系统。
 * 通过编译宏 IVF_LOG_LEVEL 控制输出级别，默认 INFO。
 * 输出到 stderr，格式: [级别] 函数名: 消息
 */

#ifndef IVF_LOG_H
#define IVF_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IVF_LOG_LEVEL_TRACE 0
#define IVF_LOG_LEVEL_DEBUG 1
#define IVF_LOG_LEVEL_INFO  2
#define IVF_LOG_LEVEL_WARN  3
#define IVF_LOG_LEVEL_ERROR 4

#ifndef IVF_LOG_LEVEL
#define IVF_LOG_LEVEL IVF_LOG_LEVEL_INFO
#endif

#define IVF_LOG(level, tag, fmt, ...)                                          \
    do {                                                                       \
        if ((level) >= IVF_LOG_LEVEL) {                                        \
            fprintf(stderr, "[%s] %s: " fmt "\n", tag, __func__, ##__VA_ARGS__); \
        }                                                                      \
    } while (0)

#define IVF_LOG_TRACE(fmt, ...) IVF_LOG(IVF_LOG_LEVEL_TRACE, "TRACE", fmt, ##__VA_ARGS__)
#define IVF_LOG_DEBUG(fmt, ...) IVF_LOG(IVF_LOG_LEVEL_DEBUG, "DEBUG", fmt, ##__VA_ARGS__)
#define IVF_LOG_INFO(fmt, ...)  IVF_LOG(IVF_LOG_LEVEL_INFO,  "INFO ", fmt, ##__VA_ARGS__)
#define IVF_LOG_WARN(fmt, ...)  IVF_LOG(IVF_LOG_LEVEL_WARN,  "WARN ", fmt, ##__VA_ARGS__)
#define IVF_LOG_ERROR(fmt, ...) IVF_LOG(IVF_LOG_LEVEL_ERROR, "ERROR", fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* IVF_LOG_H */
