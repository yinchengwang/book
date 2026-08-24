/**
 * @file mmdb_namespace.h
 * @brief 命名空间隔离与资源配额 API（多租户）
 *
 * 每个命名空间拥有独立的 Collection 集合，并可设置资源配额限制。
 */
#ifndef MMDB_NAMESPACE_H
#define MMDB_NAMESPACE_H

#include "sdk/mmdb.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 命名空间句柄（前向声明） */
typedef struct mmdb_namespace_s mmdb_namespace_t;

/**
 * @brief 创建命名空间
 * @param db      数据库句柄
 * @param name    命名空间名称（不能为空，同一数据库内唯一）
 * @param quota   资源配额 JSON，格式：{"vectors_max":N,"collections_max":N,"disk_max_bytes":N}
 *                传 NULL 表示无限制配额
 * @param out_ns  输出：命名空间句柄指针
 * @return MMDB_OK 成功；MMDB_ERR_ALREADY 名称已存在；MMDB_ERR_INVALID 参数非法
 */
int mmdb_namespace_create(mmdb_t* db, const char* name, const char* quota,
                          mmdb_namespace_t** out_ns);

/**
 * @brief 获取已有命名空间
 * @param db      数据库句柄
 * @param name    命名空间名称
 * @param out_ns  输出：命名空间句柄指针
 * @return MMDB_OK 成功；MMDB_ERR_NOT_FOUND 不存在；MMDB_ERR_INVALID 参数非法
 */
int mmdb_namespace_get(mmdb_t* db, const char* name, mmdb_namespace_t** out_ns);

/**
 * @brief 设置命名空间资源配额（覆盖）
 * @param ns     命名空间句柄
 * @param quota  资源配额 JSON，格式同 mmdb_namespace_create
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法
 */
int mmdb_namespace_set_quota(mmdb_namespace_t* ns, const char* quota);

/**
 * @brief 获取命名空间资源使用量
 * @param ns        命名空间句柄
 * @param json_out  输出 JSON 缓冲区
 * @param json_size 缓冲区大小
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法
 */
int mmdb_namespace_usage(mmdb_namespace_t* ns, char* json_out, size_t json_size);

/**
 * @brief 删除命名空间
 * @param ns 命名空间句柄
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法
 */
int mmdb_namespace_drop(mmdb_namespace_t* ns);

#ifdef __cplusplus
}
#endif

#endif /* MMDB_NAMESPACE_H */
