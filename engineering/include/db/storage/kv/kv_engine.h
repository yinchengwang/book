/**
 * @file kv_engine.h
 * @brief KV 存储引擎头文件
 *
 * 定义 KV 存储引擎的接口和数据结构，封装底层 KV 实现。
 */
#ifndef DB_KV_ENGINE_H
#define DB_KV_ENGINE_H

#include "storage_engine.h"
#include "kv.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief KV 引擎数据库结构
 */
typedef struct kv_engine_db_s {
    kv_t *kv;                  /**< KV 数据库句柄 */
    char db_path[512];         /**< 数据库路径 */
    char name[256];            /**< 数据库名称 */
    AccessMode mode;           /**< 访问模式 */
} kv_engine_db_t;

/**
 * @brief KV 扫描描述符
 */
typedef struct kv_engine_scan_s {
    kv_engine_db_t *db;        /**< 数据库句柄 */
    kv_iter_t *iter;           /**< KV 迭代器 */
} kv_engine_scan_t;

/**
 * @brief 获取 KV 引擎操作表
 */
const storage_ops_t *kv_engine_get_ops(void);

/**
 * @brief 初始化 KV 引擎
 */
int kv_engine_init(const char *data_dir);

/**
 * @brief 关闭 KV 引擎
 */
int kv_engine_shutdown(void);

/**
 * @brief 创建 KV 数据库
 */
int kv_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开 KV 数据库
 */
void *kv_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭 KV 数据库
 */
int kv_engine_close(void *rel);

/**
 * @brief 删除 KV 数据库
 */
int kv_engine_drop(const char *name);

/**
 * @brief 插入键值对
 */
int kv_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 删除键值对
 */
int kv_engine_delete(void *rel, const void *key, size_t key_len);

/**
 * @brief 获取值
 */
int kv_engine_get(void *rel, const void *key, size_t key_len,
                  void **out_value, size_t *out_len);

/**
 * @brief 检查键是否存在
 */
bool kv_engine_exists(void *rel, const void *key, size_t key_len);

/**
 * @brief 获取统计信息
 */
int kv_engine_stats(const char *name, storage_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_KV_ENGINE_H */
