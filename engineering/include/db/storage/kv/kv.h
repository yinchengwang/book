/**
 * @file kv.h
 * @brief KV 数据库 API
 *
 * 提供简洁的键值接口，封装底层存储引擎和索引
 */
#ifndef DB_KV_H
#define DB_KV_H

#include "db/buffer.h"
#include "db/storage/wal/wal.h"
#include "db/lock.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct lock_manager_s lock_manager_t;

/* 注意：kv_ttl_mgr_t 前向声明放在 kv_ttl.h 中 */

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大键长度 */
#define KV_MAX_KEY_SIZE 8192

/** 最大值长度 */
#define KV_MAX_VALUE_SIZE (1024 * 1024)  /* 1MB */

/** 默认数据库名称 */
#define KV_DEFAULT_DB_NAME "kv.db"

/* ============================================================
 * 错误码
 * ============================================================ */

/** KV 操作结果 */
typedef enum kv_result_e {
    KV_OK = 0,           /**< 成功 */
    KV_NOT_FOUND = 1,    /**< 键不存在 */
    KV_ERROR = 2,        /**< 一般错误 */
    KV_CORRUPT = 3,      /**< 数据库损坏 */
    KV_NOMEM = 4,        /**< 内存不足 */
    KV_EXISTS = 5,       /**< 键已存在 */
    KV_INVALID = 6      /**< 无效参数 */
} kv_result_t;

/* ============================================================
 * KV 数据库句柄
 * ============================================================ */

/** KV 数据库内部结构 */
struct kv_s {
    char          *db_path;        /**< 数据库路径（用于 TTL 文件） */
    void          *file;           /**< 磁盘文件 */
    void          *pool;           /**< 缓存池 */
    void          *wal;            /**< WAL 日志 */
    void          *error_msg;      /**< 错误信息 */
    size_t         num_keys;       /**< 键数量 */
    lock_manager_t *lock_mgr;      /**< 锁管理器 */
    void          *ttl_mgr;        /**< TTL 管理器 */
};

/** KV 数据库（公开类型） */
typedef struct kv_s kv_t;

/** 扫描迭代器 */
typedef struct kv_iter_s kv_iter_t;

/* ============================================================
 * 数据库生命周期
 * ============================================================ */

/**
 * @brief 打开或创建数据库
 * @param path 数据库路径
 * @return 数据库句柄，失败返回 NULL
 */
kv_t *kv_open(const char *path);

/**
 * @brief 关闭数据库
 * @param db 数据库句柄
 * @return KV_OK 成功
 */
kv_result_t kv_close(kv_t *db);

/**
 * @brief 获取错误信息
 * @param db 数据库句柄
 * @return 错误信息字符串
 */
const char *kv_errmsg(const kv_t *db);

/**
 * @brief 刷脏页到磁盘
 * @param db 数据库句柄
 * @return KV_OK 成功
 *
 * @note 在事务提交后应调用此函数确保数据持久化
 */
kv_result_t kv_flush(kv_t *db);

/* ============================================================
 * 基本 KV 操作
 * ============================================================ */

/**
 * @brief 插入或更新键值对
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @param value 值
 * @param value_len 值长度
 * @return KV_OK 成功
 */
kv_result_t kv_put(kv_t *db,
                   const void *key, size_t key_len,
                   const void *value, size_t value_len);

/**
 * @brief 获取值
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @param out_value 输出值（调用者负责释放）
 * @param out_len 输出值长度
 * @return KV_OK 成功，KV_NOT_FOUND 键不存在
 */
kv_result_t kv_get(kv_t *db,
                   const void *key, size_t key_len,
                   void **out_value, size_t *out_len);

/**
 * @brief 删除键值对
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @return KV_OK 成功，KV_NOT_FOUND 键不存在
 */
kv_result_t kv_delete(kv_t *db, const void *key, size_t key_len);

/**
 * @brief 检查键是否存在
 * @param db 数据库句柄
 * @param key 键
 * @param key_len 键长度
 * @return true 存在，false 不存在
 */
bool kv_exists(kv_t *db, const void *key, size_t key_len);

/* ============================================================
 * 范围扫描
 * ============================================================ */

/**
 * @brief 创建扫描迭代器
 * @param db 数据库句柄
 * @param start_key 起始键（NULL 表示从头开始）
 * @param start_len 起始键长度
 * @param end_key 结束键（NULL 表示到结尾）
 * @param end_len 结束键长度
 * @return 迭代器，失败返回 NULL
 *
 * 示例：
 * @code
 * kv_iter_t *iter = kv_scan(db, "user:", 5, "user~", 5);
 * while (kv_iter_next(iter) == KV_OK) {
 *     printf("key=%.*s, value=%.*s\n",
 *            (int)kv_iter_key_len(iter), kv_iter_key(iter),
 *            (int)kv_iter_value_len(iter), kv_iter_value(iter));
 * }
 * kv_iter_free(iter);
 * @endcode
 */
kv_iter_t *kv_scan(kv_t *db,
                   const void *start_key, size_t start_len,
                   const void *end_key, size_t end_len);

/**
 * @brief 移动到下一个键值对
 * @param iter 迭代器
 * @return KV_OK 有下一个，KV_NOT_FOUND 遍历结束
 */
kv_result_t kv_iter_next(kv_iter_t *iter);

/**
 * @brief 获取当前键
 * @param iter 迭代器
 * @return 键指针（迭代器生命周期内有效）
 */
const void *kv_iter_key(kv_iter_t *iter);

/**
 * @brief 获取当前键长度
 * @param iter 迭代器
 * @return 键长度
 */
size_t kv_iter_key_len(kv_iter_t *iter);

/**
 * @brief 获取当前值
 * @param iter 迭代器
 * @return 值指针（迭代器生命周期内有效）
 */
const void *kv_iter_value(kv_iter_t *iter);

/**
 * @brief 获取当前值长度
 * @param iter 迭代器
 * @return 值长度
 */
size_t kv_iter_value_len(kv_iter_t *iter);

/**
 * @brief 释放迭代器
 * @param iter 迭代器
 */
void kv_iter_free(kv_iter_t *iter);

/* ============================================================
 * 统计信息
 * ============================================================ */

/**
 * @brief 数据库统计信息
 */
typedef struct kv_stats_s {
    size_t num_keys;       /**< 键数量 */
    size_t total_size;     /**< 总大小（字节） */
    size_t page_count;     /**< 使用页面数 */
    double cache_hit_rate;  /**< 缓存命中率 */
} kv_stats_t;

/**
 * @brief 获取统计信息
 * @param db 数据库句柄
 * @param stats 输出统计信息
 * @return KV_OK 成功
 */
kv_result_t kv_stats(kv_t *db, kv_stats_t *stats);

/* ============================================================
 * 批次操作
 * ============================================================ */

/**
 * @brief 批量获取（暂不支持，可后续实现）
 * @param db 数据库句柄
 * @param keys 键数组
 * @param key_lens 键长度数组
 * @param n 键数量
 * @param out_values 输出值数组（需释放）
 * @param out_lens 输出值长度数组
 * @return KV_OK 成功
 */
kv_result_t kv_batch_get(kv_t *db,
                         const void **keys, const size_t *key_lens, size_t n,
                         void ***out_values, size_t **out_lens);

/**
 * @brief 释放批量获取的结果
 * @param values 值数组
 * @param lens 长度数组
 * @param n 元素数量
 */
void kv_batch_free(void **values, size_t *lens, size_t n);

/* ============================================================
 * 调试辅助函数
 * ============================================================ */

/**
 * @brief 获取 Buffer Pool（用于调试）
 * @param db KV 数据库
 * @return Buffer Pool 句柄
 */
void *kv_get_buffer_pool(kv_t *db);

#ifdef __cplusplus
}
#endif

#endif /* DB_KV_H */
