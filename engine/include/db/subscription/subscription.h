/**
 * @file subscription.h
 * @brief 订阅管理器接口
 *
 * 定义订阅管理器的接口，支持多个订阅者订阅数据库变更。
 */
#ifndef DB_SUBSCRIPTION_H
#define DB_SUBSCRIPTION_H

#include "cdc.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 最大订阅名称长度 */
#define SUBSCRIPTION_MAX_NAME 64

/** 最大订阅数量 */
#define SUBSCRIPTION_MAX_COUNT 256

/** 默认缓冲区大小 */
#define SUBSCRIPTION_BUFFER_SIZE 4096

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 订阅者回调函数类型
 *
 * @param change 变更记录
 * @param user_data 用户数据
 * @return 0 成功，非0 失败（返回错误码）
 */
typedef int (*subscription_callback_t)(const cdc_change_t *change, void *user_data);

/**
 * @brief 订阅状态
 */
typedef enum {
    SUBSCRIPTION_STATE_INACTIVE = 0,  /**< 未激活 */
    SUBSCRIPTION_STATE_ACTIVE = 1,    /**< 已激活 */
    SUBSCRIPTION_STATE_PAUSED = 2,    /**< 暂停 */
    SUBSCRIPTION_STATE_ERROR = 3,     /**< 错误 */
} subscription_state_t;

/**
 * @brief 订阅信息
 */
typedef struct subscription_s {
    char name[SUBSCRIPTION_MAX_NAME];     /**< 订阅名称 */
    subscription_state_t state;           /**< 订阅状态 */

    /* 过滤条件 */
    int32_t *relation_ids;                /**< 订阅的表 ID 数组 */
    int32_t relation_count;               /**< 表数量 */
    cdc_change_type_t *change_types;      /**< 订阅的变更类型 */
    int32_t change_type_count;            /**< 变更类型数量 */

    /* 回调和用户数据 */
    subscription_callback_t callback;     /**< 回调函数 */
    void *user_data;                      /**< 用户数据 */

    /* 位置追踪 */
    uint64_t start_lsn;                   /**< 起始 LSN */
    uint64_t last_lsn;                    /**< 最后确认 LSN */
    uint64_t last_processed_lsn;          /**< 最后处理的 LSN */

    /* 统计 */
    uint64_t total_changes;               /**< 总变更数 */
    uint64_t successful_deliveries;       /**< 成功投递数 */
    uint64_t failed_deliveries;           /**< 失败投递数 */
} subscription_t;

/**
 * @brief 订阅管理器
 */
typedef struct subscription_manager_s {
    char data_dir[512];                    /**< 数据目录 */
    subscription_t **subscriptions;        /**< 订阅数组 */
    int32_t subscription_count;            /**< 订阅数量 */
    int32_t capacity;                      /**< 容量 */
    cdc_context_t *cdc;                    /**< CDC 上下文 */
    void *change_queue;                    /**< 变更队列（实现时具体化） */
} subscription_manager_t;

/* ========================================================================
 * 订阅管理器 API
 * ======================================================================== */

/**
 * @brief 创建订阅管理器
 *
 * @param data_dir 数据目录
 * @return 订阅管理器指针，失败返回 NULL
 */
subscription_manager_t *subscription_manager_create(const char *data_dir);

/**
 * @brief 销毁订阅管理器
 *
 * @param mgr 订阅管理器
 */
void subscription_manager_destroy(subscription_manager_t *mgr);

/* ========================================================================
 * 订阅操作 API
 * ======================================================================== */

/**
 * @brief 创建订阅
 *
 * @param mgr 订阅管理器
 * @param name 订阅名称
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 订阅指针，失败返回 NULL
 */
subscription_t *subscription_create(subscription_manager_t *mgr,
                                    const char *name,
                                    subscription_callback_t callback,
                                    void *user_data);

/**
 * @brief 销毁订阅
 *
 * @param mgr 订阅管理器
 * @param name 订阅名称
 * @return 0 成功，-1 失败
 */
int subscription_destroy(subscription_manager_t *mgr, const char *name);

/**
 * @brief 激活订阅
 *
 * @param mgr 订阅管理器
 * @param name 订阅名称
 * @return 0 成功，-1 失败
 */
int subscription_activate(subscription_manager_t *mgr, const char *name);

/**
 * @brief 暂停订阅
 *
 * @param mgr 订阅管理器
 * @param name 订阅名称
 * @return 0 成功，-1 失败
 */
int subscription_pause(subscription_manager_t *mgr, const char *name);

/**
 * @brief 恢复订阅
 *
 * @param mgr 订阅管理器
 * @param name 订阅名称
 * @return 0 成功，-1 失败
 */
int subscription_resume(subscription_manager_t *mgr, const char *name);

/**
 * @brief 获取订阅
 *
 * @param mgr 订阅管理器
 * @param name 订阅名称
 * @return 订阅指针，未找到返回 NULL
 */
subscription_t *subscription_get(const subscription_manager_t *mgr, const char *name);

/* ========================================================================
 * 过滤配置 API
 * ======================================================================== */

/**
 * @brief 添加订阅的表
 *
 * @param sub 订阅
 * @param relation_id 表 ID
 * @return 0 成功，-1 失败
 */
int subscription_add_relation(subscription_t *sub, int32_t relation_id);

/**
 * @brief 移除订阅的表
 *
 * @param sub 订阅
 * @param relation_id 表 ID
 * @return 0 成功，-1 失败
 */
int subscription_remove_relation(subscription_t *sub, int32_t relation_id);

/**
 * @brief 添加订阅的变更类型
 *
 * @param sub 订阅
 * @param type 变更类型
 * @return 0 成功，-1 失败
 */
int subscription_add_change_type(subscription_t *sub, cdc_change_type_t type);

/**
 * @brief 设置起始 LSN
 *
 * @param sub 订阅
 * @param lsn 起始 LSN
 */
void subscription_set_start_lsn(subscription_t *sub, uint64_t lsn);

/* ========================================================================
 * 变更分发 API
 * ======================================================================== */

/**
 * @brief 处理变更（内部使用）
 *
 * @param mgr 订阅管理器
 * @param change 变更记录
 * @return 0 成功，-1 失败
 */
int subscription_process_change(subscription_manager_t *mgr, const cdc_change_t *change);

/**
 * @brief 通知所有订阅者
 *
 * @param mgr 订阅管理器
 * @param change 变更记录
 * @return 成功通知的订阅数
 */
int subscription_notify_all(subscription_manager_t *mgr, const cdc_change_t *change);

/**
 * @brief 确认已处理到指定 LSN
 *
 * @param mgr 订阅管理器
 * @param lsn 已确认的 LSN
 */
void subscription_ack(subscription_manager_t *mgr, uint64_t lsn);

/* ========================================================================
 * 统计 API
 * ======================================================================== */

/**
 * @brief 获取订阅统计信息
 *
 * @param sub 订阅
 * @param out_total 输出总变更数
 * @param out_success 输出成功投递数
 * @param out_failed 输出失败投递数
 */
void subscription_get_stats(const subscription_t *sub,
                            uint64_t *out_total,
                            uint64_t *out_success,
                            uint64_t *out_failed);

/**
 * @brief 重置订阅统计
 *
 * @param sub 订阅
 */
void subscription_reset_stats(subscription_t *sub);

#ifdef __cplusplus
}
#endif

#endif /* DB_SUBSCRIPTION_H */
