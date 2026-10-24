/**
 * @file kv.h
 * @brief KV 数据库 API
 *
 * 提供简洁的键值接口，封装底层存储引擎和索引
 */
#ifndef DB_KV_H
#define DB_KV_H

#include "db/buffer.h"
#include "db/wal.h"
#include "db/lock.h"
#include "db/common_rwlock.h"  /* C1-3 T2 */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct lock_manager_s lock_manager_t;

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大键长度 */
#define KV_MAX_KEY_SIZE 8192

/** 最大值长度 */
#define KV_MAX_VALUE_SIZE (16 * 1024 * 1024)  /* 16MB (C1-3 T5) */

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
    KV_INVALID = 6,      /**< 无效参数 */
    /* C1-3 T3：专用错误码 */
    KV_FULL = 7,         /**< page full（替代 KV_ERROR 用于此场景） */
    KV_CONFLICT = 8,     /**< CAS 失败 */
    KV_LOCKED = 9        /**< 锁等待超时 */
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
    /* C1-3 T2：common_rwlock 并发保护（put/get/delete 包裹） */
    common_rwlock_t *rwlock;
    /* C3-5 T22：watch 通知链表 */
    void *watch_list;              /**< 内部 watch 链表（kw_list_t） */
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

/* ============================================================
 * WAL 相关函数
 * ============================================================ */

/**
 * @brief 重放 WAL 文件恢复数据
 * @param db KV 数据库句柄（需已初始化）
 * @param wal_path WAL 文件路径
 * @return 0 成功，-1 失败
 */
int kv_replay_wal(kv_t *db, const char *wal_path);

/* ========================================================================
 * C0-3：KV → DBERR 适配宏（向后兼容）
 * ======================================================================== */

#include "db/errors.h"

#define KV_TO_DBERR(rc) \
    ((rc) == KV_OK        ? DBERR_OK        : \
     (rc) == KV_NOT_FOUND ? DBERR_NOT_FOUND : \
     (rc) == KV_FULL      ? DBERR_FULL      : \
     (rc) == KV_NOMEM     ? DBERR_NOMEM     : \
     (rc) == KV_EXISTS    ? DBERR_EXISTS    : \
     (rc) == KV_INVALID   ? DBERR_INVALID   : \
     (rc) == KV_CORRUPT   ? DBERR_CORRUPT   : \
     (rc) == KV_CONFLICT  ? DBERR_CONFLICT  : \
     (rc) == KV_LOCKED    ? DBERR_LOCKED    : \
                             DBERR_MOD_KV)

/* C3-5 T22：自定义 key 比较器注入点 */
typedef int (*kv_comparator_fn)(const void *a, size_t alen,
                               const void *b, size_t blen);
void kv_set_comparator(kv_t *db, kv_comparator_fn cmp);

/* C3-5 T20：CAS（compare-and-swap）
 * 仅当旧值等于 expected_old 时替换为 new_value
 */
kv_result_t kv_cas(kv_t *db,
                   const void *key, size_t key_len,
                   const void *expected_old, size_t expected_old_len,
                   const void *new_value, size_t new_value_len);

/* ============================================================
 * Watch 通知（C3-5 T22）
 * ============================================================ */

/** 回调函数类型：键值变更通知 */
typedef void (*kv_watch_callback_t)(void *user_data,
                                    const char *key, size_t key_len,
                                    const void *old_value, size_t old_len,
                                    const void *new_value, size_t new_len);

/** 句柄：watch 订阅 */
typedef struct kv_watch_s kv_watch_t;

/**
 * @brief 订阅键变更通知
 * @param db 数据库句柄
 * @param key 要监听的键（NULL = 监听所有键）
 * @param key_len 键长度
 * @param callback 回调函数
 * @param user_data 回调用户数据
 * @return watch 句柄，失败返回 NULL
 */
kv_watch_t *kv_watch(kv_t *db, const void *key, size_t key_len,
                     kv_watch_callback_t callback, void *user_data);

/**
 * @brief 取消订阅
 * @param db 数据库句柄
 * @param watch watch 句柄
 */
void kv_unwatch(kv_t *db, kv_watch_t *watch);

/* ============================================================
 * Multi 批量操作（C3-5 T22）
 * ============================================================ */

/** 批量操作条目 */
typedef struct {
    void *key;           /**< 键 */
    size_t key_len;      /**< 键长度 */
    void *value;         /**< 值（set 时使用） */
    size_t value_len;    /**< 值长度 */
    bool is_set;         /**< true=写入，false=读取 */
} kv_multi_entry_t;

/**
 * @brief 批量读取
 * @param db 数据库句柄
 * @param entries 条目数组
 * @param count 条目数量
 * @return KV_OK 成功
 */
kv_result_t kv_multi_get(kv_t *db, kv_multi_entry_t *entries, size_t count);

/**
 * @brief 批量写入
 * @param db 数据库句柄
 * @param entries 条目数组
 * @param count 条目数量
 * @return KV_OK 成功
 */
kv_result_t kv_multi_set(kv_t *db, kv_multi_entry_t *entries, size_t count);

/**
 * @brief 批量删除
 * @param db 数据库句柄
 * @param entries 条目数组
 * @param count 条目数量
 * @return KV_OK 成功
 */
kv_result_t kv_multi_del(kv_t *db, kv_multi_entry_t *entries, size_t count);

/**
 * @brief C1-3 T6：kv_get 释放契约
 *
 * kv_get 成功时将 value 拷贝到 malloc 分配的缓冲区，通过 *out_value 返回。
 * **调用方必须**对 *out_value 指向的内存调用 free()，否则内存泄漏。
 *
 * 示例：
 *   void *val = nullptr;
 *   size_t val_len = 0;
 *   kv_result_t rc = kv_get(db, key, key_len, &val, &val_len);
 *   if (rc == KV_OK) {
 *       // ... 使用 val ...
 *       free(val);
 *   }
 */

#ifdef __cplusplus
}
#endif

#endif /* DB_KV_H */
