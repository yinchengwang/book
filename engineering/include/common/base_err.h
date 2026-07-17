/*
 * base_err.h - 公共错误码基础
 *
 * 错误码命名规范: <系统>_<级别>_<描述>
 * 级别: WARN / ERROR / FATAL（INFO/DEBUG/TRACE 级别不需要错误码）
 * 系统: SYS（公共层）
 */
#ifndef COMMON_BASE_ERR_H
#define COMMON_BASE_ERR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 错误级别枚举
// ============================================================

typedef enum {
    SYS_ERR_LEVEL_SUCCESS = 0,  // 成功
    SYS_ERR_LEVEL_INFO    = 1,  // 信息（仅日志，无错误码）
    SYS_ERR_LEVEL_WARN    = 2,  // 警告
    SYS_ERR_LEVEL_ERROR   = 3,  // 错误
    SYS_ERR_LEVEL_FATAL   = 4,  // 致命错误
} sys_err_level_t;

// ============================================================
// 错误码定义（字符串格式）
// ============================================================

// --- 通用错误 ---

// 内存相关
#define SYS_ERROR_OUT_OF_MEMORY     "SYS_ERROR_OUT_OF_MEMORY"     // 内存分配失败
#define SYS_ERROR_MEMORY_ALLOC      "SYS_ERROR_MEMORY_ALLOC"      // 内存分配错误

// 参数相关
#define SYS_ERROR_INVALID_PARAM     "SYS_ERROR_INVALID_PARAM"     // 无效参数
#define SYS_ERROR_NULL_POINTER      "SYS_ERROR_NULL_POINTER"      // 空指针
#define SYS_ERROR_OUT_OF_RANGE      "SYS_ERROR_OUT_OF_RANGE"      // 超出范围

// 文件/IO 相关
#define SYS_ERROR_FILE_NOT_FOUND    "SYS_ERROR_FILE_NOT_FOUND"    // 文件未找到
#define SYS_ERROR_FILE_OPEN_FAILED  "SYS_ERROR_FILE_OPEN_FAILED"  // 文件打开失败
#define SYS_ERROR_IO_FAILED         "SYS_ERROR_IO_FAILED"         // IO 操作失败
#define SYS_ERROR_READ_FAILED       "SYS_ERROR_READ_FAILED"       // 读取失败
#define SYS_ERROR_WRITE_FAILED      "SYS_ERROR_WRITE_FAILED"      // 写入失败

// 资源相关
#define SYS_ERROR_RESOURCE_BUSY     "SYS_ERROR_RESOURCE_BUSY"     // 资源忙
#define SYS_ERROR_RESOURCE_LIMIT    "SYS_ERROR_RESOURCE_LIMIT"    // 资源达到限制

// 系统相关
#define SYS_ERROR_SYSCALL_FAILED    "SYS_ERROR_SYSCALL_FAILED"    // 系统调用失败
#define SYS_ERROR_TIMEOUT           "SYS_ERROR_TIMEOUT"           // 操作超时

// --- 警告级别 ---

// 废弃警告
#define SYS_WARN_DEPRECATED_API     "SYS_WARN_DEPRECATED_API"     // 使用了废弃 API
#define SYS_WARN_FEATURE_DISABLED   "SYS_WARN_FEATURE_DISABLED"   // 功能已禁用

// 性能警告
#define SYS_WARN_PERFORMANCE_DEGRADED "SYS_WARN_PERFORMANCE_DEGRADED"  // 性能下降
#define SYS_WARN_SLOW_OPERATION     "SYS_WARN_SLOW_OPERATION"     // 操作过慢

// --- 致命错误 ---

#define SYS_FATAL_PANIC             "SYS_FATAL_PANIC"             // 严重故障
#define SYS_FATAL_ASSERTION_FAILED  "SYS_FATAL_ASSERTION_FAILED"  // 断言失败
#define SYS_FATAL_CORRUPTION        "SYS_FATAL_CORRUPTION"        // 数据损坏

// ============================================================
// 错误码结构（用于返回值）
// ============================================================

typedef struct sys_err_t {
    sys_err_level_t level;  // 错误级别
    const char     *code;   // 错误码字符串
    const char     *msg;    // 错误消息
    const char     *file;   // 发生错误的文件
    int             line;   // 发生错误的行号
    const char     *func;   // 发生错误的函数
} sys_err_t;

// 便捷宏：创建错误码
#define SYS_ERR(level, code, msg) \
    ((sys_err_t){level, code, msg, __FILE__, __LINE__, __func__})

#define SYS_OK()  SYS_ERR(SYS_ERR_LEVEL_SUCCESS, NULL, "OK")

// ============================================================
// 错误码辅助函数
// ============================================================

/**
 * @brief 获取错误码对应的描述文本
 * @param code 错误码字符串
 * @return 错误描述
 */
const char *sys_err_to_string(const char *code);

/**
 * @brief 获取错误级别对应的字符串
 * @param level 错误级别
 * @return 级别字符串
 */
const char *sys_err_level_to_string(sys_err_level_t level);

/**
 * @brief 判断是否为成功状态
 * @param err 错误码结构
 * @return true 表示成功
 */
static inline bool sys_err_is_ok(const sys_err_t *err) {
    return err && err->level == SYS_ERR_LEVEL_SUCCESS;
}

/**
 * @brief 判断是否为错误状态
 * @param err 错误码结构
 * @return true 表示错误
 */
static inline bool sys_err_is_error(const sys_err_t *err) {
    return err && err->level >= SYS_ERR_LEVEL_ERROR;
}

/**
 * @brief 判断是否为警告状态
 * @param err 错误码结构
 * @return true 表示警告
 */
static inline bool sys_err_is_warn(const sys_err_t *err) {
    return err && err->level == SYS_ERR_LEVEL_WARN;
}

#ifdef __cplusplus
}
#endif

#endif // COMMON_BASE_ERR_H
