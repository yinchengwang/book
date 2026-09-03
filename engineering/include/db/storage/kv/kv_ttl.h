/**
 * @file kv_ttl.h
 * @brief KV 数据库 TTL（过期时间）支持
 *
 * 提供键值对的过期时间管理功能
 */
#ifndef DB_KV_TTL_H
#define DB_KV_TTL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct kv_s kv_t;
typedef struct kv_ttl_mgr_s kv_ttl_mgr_t;

/* ============================================================
 * TTL 常量定义
 * ============================================================ */

/** 最大键长度 */
#define KV_TTL_MAX_KEY_SIZE 8192

/** TTL 文件扩展名 */
#define KV_TTL_FILE_EXT ".ttl"

/** TTL 文件版本 */
#define KV_TTL_FILE_VERSION 1

/** 无穷大 TTL（表示永不过期） */
#define KV_TTL_INFINITE INT64_MAX

/** TTL 条目最大数量（可动态扩展） */
#define KV_TTL_MAX_ENTRIES 100000

/* ============================================================
 * TTL 条目结构
 * ============================================================ */

/**
 * @brief TTL 条目
 *
 * 存储单个键的过期时间信息
 */
typedef struct kv_ttl_entry_s {
    char        key[KV_TTL_MAX_KEY_SIZE];  /**< 键（固定长度便于比较） */
    size_t      key_len;                   /**< 键长度 */
    int64_t     expire_at_ms;              /**< 过期时间戳（毫秒） */
    bool        deleted;                   /**< 逻辑删除标记 */
} kv_ttl_entry_t;

/* ============================================================
 * TTL 管理器
 * ============================================================ */

/**
 * @brief 创建 TTL 管理器
 * @param db_path 数据库路径（用于生成 TTL 文件名）
 * @return TTL 管理器，失败返回 NULL
 */
kv_ttl_mgr_t *kv_ttl_mgr_create(const char *db_path);

/**
 * @brief 销毁 TTL 管理器
 * @param mgr TTL 管理器
 */
void kv_ttl_mgr_destroy(kv_ttl_mgr_t *mgr);

/**
 * @brief 从文件加载 TTL 数据
 * @param mgr TTL 管理器
 * @return 0 成功，非 0 失败
 */
int kv_ttl_mgr_load(kv_ttl_mgr_t *mgr);

/**
 * @brief 保存 TTL 数据到文件
 * @param mgr TTL 管理器
 * @return 0 成功，非 0 失败
 */
int kv_ttl_mgr_save(kv_ttl_mgr_t *mgr);

/* ============================================================
 * TTL 操作 API
 * ============================================================ */

/**
 * @brief 设置键的过期时间
 * @param mgr TTL 管理器
 * @param key 键
 * @param key_len 键长度
 * @param ttl_ms TTL 毫秒数（<= 0 表示永不过期）
 * @return 0 成功，非 0 失败
 */
int kv_ttl_set(kv_ttl_mgr_t *mgr, const void *key, size_t key_len, int64_t ttl_ms);

/**
 * @brief 获取键的剩余 TTL
 * @param mgr TTL 管理器
 * @param key 键
 * @param key_len 键长度
 * @return 剩余毫秒数，-1 表示不存在，-2 表示永不过期
 */
int64_t kv_ttl_get_remaining(kv_ttl_mgr_t *mgr, const void *key, size_t key_len);

/**
 * @brief 检查键是否已过期
 * @param mgr TTL 管理器
 * @param key 键
 * @param key_len 键长度
 * @return true 已过期或不存在，false 未过期
 */
bool kv_ttl_is_expired(kv_ttl_mgr_t *mgr, const void *key, size_t key_len);

/**
 * @brief 移除键的过期（使键持久化）
 * @param mgr TTL 管理器
 * @param key 键
 * @param key_len 键长度
 * @return 0 成功（键存在且已移除 TTL），非 0 失败或键不存在
 */
int kv_ttl_persist(kv_ttl_mgr_t *mgr, const void *key, size_t key_len);

/**
 * @brief 删除键的 TTL 条目
 * @param mgr TTL 管理器
 * @param key 键
 * @param key_len 键长度
 * @return 0 成功，非 0 失败
 */
int kv_ttl_delete(kv_ttl_mgr_t *mgr, const void *key, size_t key_len);

/* ============================================================
 * TTL 批量操作
 * ============================================================ */

/**
 * @brief 获取所有已过期的键
 * @param mgr TTL 管理器
 * @param out_keys 输出键数组（调用者负责释放）
 * @param out_lens 输出键长度数组
 * @param max_count 最大返回数量
 * @return 实际返回数量
 */
size_t kv_ttl_get_expired_keys(kv_ttl_mgr_t *mgr,
                               void **out_keys, size_t *out_lens,
                               size_t max_count);

/**
 * @brief 清理所有过期键的 TTL 条目
 * @param mgr TTL 管理器
 * @return 清理的条目数量
 */
size_t kv_ttl_cleanup_expired(kv_ttl_mgr_t *mgr);

/* ============================================================
 * 高级 API（与 kv_t 集成）
 * ============================================================ */

/** TTL 操作结果（与 kv_result_t 兼容） */
#define KV_TTL_OK 0
#define KV_TTL_NOT_FOUND 1
#define KV_TTL_ERROR 2

/**
 * @brief 插入或更新带 TTL 的键值对
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @param value 值
 * @param value_len 值长度
 * @param ttl_ms TTL 毫秒数（0 或负数表示永不过期）
 * @return 0 成功，非 0 失败
 *
 * @note 此函数先调用 kv_put，然后设置 TTL
 */
int kv_put_with_ttl(kv_t *db,
                    const void *key, size_t key_len,
                    const void *value, size_t value_len,
                    int64_t ttl_ms);

/**
 * @brief 获取键的剩余 TTL
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @return 剩余毫秒数，-1 表示不存在，-2 表示永不过期
 *
 * @note 如果键不存在，返回 -1
 */
int64_t kv_get_ttl(kv_t *db, const void *key, size_t key_len);

/**
 * @brief 移除键的过期时间（使键持久化）
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @return 0 成功，非 0 失败
 */
int kv_persist(kv_t *db, const void *key, size_t key_len);

/**
 * @brief 初始化 KV 引擎的 TTL 功能
 * @param db 数据库句柄
 * @return 0 成功，非 0 失败
 *
 * @note kv_open 后自动调用，无需手动调用
 */
int kv_ttl_init(kv_t *db);

/**
 * @brief 关闭 KV 引擎的 TTL 功能
 * @param db 数据库句柄
 *
 * @note kv_close 前自动调用
 */
void kv_ttl_shutdown(kv_t *db);

/* ============================================================
 * 统计信息
 * ============================================================ */

/**
 * @brief TTL 统计信息
 */
typedef struct kv_ttl_stats_s {
    size_t total_entries;     /**< TTL 条目总数 */
    size_t expired_entries;   /**< 已过期条目数 */
    size_t persistent_keys;   /**< 持久化（永不过期）键数 */
    int64_t next_expire_time; /**< 下一个过期时间（毫秒） */
} kv_ttl_stats_t;

/**
 * @brief 获取 TTL 统计信息
 * @param mgr TTL 管理器
 * @param stats 输出统计信息
 */
void kv_ttl_get_stats(kv_ttl_mgr_t *mgr, kv_ttl_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_KV_TTL_H */
