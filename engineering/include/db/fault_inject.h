/**
 * @file fault_inject.h
 * @brief 故障注入框架
 *
 * 主动注入 I/O 错误、内存分配失败等故障，用于测试错误处理路径。
 */
#ifndef DB_FAULT_INJECT_H
#define DB_FAULT_INJECT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大故障注入规则数 */
#define FAULT_MAX_RULES 16

/* ============================================================
 * 故障类型
 * ============================================================ */

/** 故障类型 */
typedef enum fault_type_e {
    FAULT_NONE = 0,          /**< 无故障 */
    FAULT_DISK_READ,         /**< 磁盘读取失败 */
    FAULT_DISK_WRITE,        /**< 磁盘写入失败 */
    FAULT_DISK_SYNC,         /**< 磁盘同步失败 */
    FAULT_MALLOC,            /**< 内存分配失败 */
    FAULT_PAGE_CORRUPT,      /**< 页面损坏注入 */
} fault_type_t;

/* ============================================================
 * 故障注入点
 * ============================================================ */

/** 故障注入点 */
typedef enum fault_point_e {
    FAULT_POINT_READ_PAGE,    /**< disk_read_page */
    FAULT_POINT_WRITE_PAGE,   /**< disk_write_page */
    FAULT_POINT_ALLOC_PAGE,   /**< disk_alloc_page */
    FAULT_POINT_MALLOC,       /**< malloc/calloc */
    FAULT_POINT_BUF_READ,     /**< buf_read */
} fault_point_t;

/* ============================================================
 * 故障注入配置
 * ============================================================ */

/** 故障注入规则 */
typedef struct fault_config_s {
    fault_type_t  type;        /**< 故障类型 */
    fault_point_t point;       /**< 注入点 */
    uint32_t      skip_count;  /**< 跳过前 N 次（用于绕过初始化阶段） */
    uint32_t      max_hits;    /**< 最大触发次数（0=无限） */
    uint32_t      hits;        /**< 已触发次数（内部维护） */
} fault_config_t;

/* ============================================================
 * 故障注入统计
 * ============================================================ */

/** 故障注入统计信息 */
typedef struct fault_stats_s {
    uint32_t total_triggers;   /**< 总触发次数 */
    uint32_t active_rules;     /**< 活跃规则数 */
    uint32_t exhausted_rules;  /**< 已耗尽规则数 */
} fault_stats_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief 初始化故障注入系统
 */
void fault_init(void);

/**
 * @brief 注册故障注入规则
 * @param config 故障注入配置
 * @return 0 成功，-1 失败（规则表已满或参数无效）
 */
int fault_register(const fault_config_t *config);

/**
 * @brief 清除所有故障注入规则
 */
void fault_clear(void);

/**
 * @brief 检查指定注入点是否应触发故障
 * @param point 注入点
 * @return true 应触发故障
 *
 * 内部会跳过 skip_count 次，然后触发，同时更新 hits 计数。
 * 达到 max_hits 后自动移除该规则。
 */
bool fault_should_fail(fault_point_t point);

/**
 * @brief 获取故障注入统计信息
 * @param stats 输出统计信息
 */
void fault_get_stats(fault_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_FAULT_INJECT_H */