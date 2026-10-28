/**
 * @file common.h
 * @brief 公共跨平台工具（Task 14 起）
 */
#ifndef DB_COMMON_H
#define DB_COMMON_H

#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>  /* _mkdir */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 可移植目录创建
 *
 * POSIX mkdir(path, mode)；Windows _mkdir(path)（无 mode，继承默认权限）。
 * 统一两参签名，业务代码不再写 #ifdef _WIN32。
 * 已存在时返回 0（与调用方 `!= 0 && errno != EEXIST` 的容错配合无害）。
 */
static inline int mm_mkdir(const char *path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

/**
 * @brief 可移植目录删除（仅能删除空目录；配合先清空内容使用）
 */
static inline int mm_rmdir(const char *path)
{
#ifdef _WIN32
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* DB_COMMON_H */
