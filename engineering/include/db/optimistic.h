/**
 * @file optimistic.h
 * @brief 乐观锁接口
 *
 * 基于版本号的非阻塞并发控制机制
 */
#ifndef DB_OPTIMISTIC_H
#define DB_OPTIMISTIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 乐观锁配置
 * ============================================================ */

/** 最大重试次数 */
#define OPTIMISTIC_MAX_RETRIES 3

/** 默认版本号 */
#define OPTIMISTIC_INITIAL_VERSION 0

/* ============================================================
 * 乐观锁结果码
 * ============================================================ */

typedef enum optimistic_result_e {
    OPT_OK = 0,              /**< 成功 */
    OPT_CONFLICT = 1,        /**< 版本冲突 */
    OPT_RETRY = 2,           /**< 需要重试 */
    OPT_ERROR = -1           /**< 一般错误 */
} optimistic_result_t;

/* ============================================================
 * 版本化数据
 * ============================================================ */

/** 版本化数据头 */
typedef struct versioned_header_s {
    uint64_t version;         /**< 版本号 */
    uint64_t checksum;       /**< 校验和（可选） */
} versioned_header_t;

/**
 * @brief 检查版本是否匹配
 * @param expected 期望的版本号
 * @param actual 实际的版本号
 * @return true 匹配，false 不匹配
 */
bool optimistic_version_match(uint64_t expected, uint64_t actual);

/**
 * @brief 生成新版本号
 * @param current 当前版本号
 * @return 新版本号
 */
uint64_t optimistic_next_version(uint64_t current);

/* ============================================================
 * 乐观更新回调
 * ============================================================ */

/**
 * @brief 更新回调函数类型
 *
 * @param context 用户上下文
 * @param old_value 旧值
 * @param old_len 旧值长度
 * @param out_new_value 输出新值（调用者分配）
 * @param out_new_len 输出新值长度
 * @return true 成功，false 失败
 */
typedef bool (*optimistic_update_fn)(
    void *context,
    const void *old_value, size_t old_len,
    void **out_new_value, size_t *out_new_len
);

/* ============================================================
 * 乐观锁操作
 * ============================================================ */

/**
 * @brief 乐观读取
 *
 * @param version 指向版本号的指针
 * @param data 指向数据的指针
 * @param len 数据长度
 * @return OPT_OK 成功
 */
optimistic_result_t optimistic_read(
    uint64_t *version,
    const void *data, size_t len
);

/**
 * @brief 乐观更新
 *
 * 使用 CAS 原子操作尝试更新数据。
 * 如果版本号匹配，更新成功；否则返回冲突。
 *
 * @param version 指向版本号的指针
 * @param data 数据指针
 * @param len 数据长度
 * @param update_fn 更新回调函数
 * @param context 用户上下文
 * @return OPT_OK 成功，OPT_CONFLICT 版本冲突，OPT_ERROR 错误
 */
optimistic_result_t optimistic_update(
    uint64_t *version,
    void *data, size_t len,
    optimistic_update_fn update_fn,
    void *context
);

/**
 * @brief 带重试的乐观更新
 *
 * 如果遇到版本冲突，自动重试最多 max_retries 次。
 *
 * @param version 指向版本号的指针
 * @param data 数据指针
 * @param len 数据长度
 * @param update_fn 更新回调函数
 * @param context 用户上下文
 * @param max_retries 最大重试次数
 * @return OPT_OK 成功，OPT_CONFLICT 重试耗尽，OPT_ERROR 错误
 */
optimistic_result_t optimistic_update_with_retry(
    uint64_t *version,
    void *data, size_t len,
    optimistic_update_fn update_fn,
    void *context,
    int max_retries
);

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 初始化版本化数据
 * @param header 版本化数据头
 */
void optimistic_init(versioned_header_t *header);

/**
 * @brief 获取版本号
 * @param header 版本化数据头
 * @return 版本号
 */
uint64_t optimistic_get_version(const versioned_header_t *header);

/**
 * @brief 设置版本号
 * @param header 版本化数据头
 * @param version 版本号
 */
void optimistic_set_version(versioned_header_t *header, uint64_t version);

/**
 * @brief 检查是否需要升级为悲观锁
 * @param conflict_count 冲突次数
 * @return true 应该升级，false 继续使用乐观锁
 */
bool optimistic_should_escalate(int conflict_count);

#ifdef __cplusplus
}
#endif

#endif /* DB_OPTIMISTIC_H */
