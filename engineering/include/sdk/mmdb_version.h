/**
 * @file mmdb_version.h
 * @brief 版本信息宏与运行时版本查询
 */
#ifndef SDK_MMDB_VERSION_H
#define SDK_MMDB_VERSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 编译期版本号（ABI 兼容性判定基准） */
#define MMDB_VERSION_MAJOR 1
#define MMDB_VERSION_MINOR 0
#define MMDB_VERSION_PATCH 0
#define MMDB_VERSION_STRING "1.0.0"

/**
 * @brief 运行时查询 SDK 版本号
 * @param[out] major 主版本号（可为 NULL）
 * @param[out] minor 次版本号（可为 NULL）
 * @param[out] patch 修订号（可为 NULL）
 */
void mmdb_version(int* major, int* minor, int* patch);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_VERSION_H */