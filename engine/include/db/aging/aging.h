/**
 * @file aging.h
 * @brief 数据老化接口
 *
 * 定义数据老化模块的接口，支持冷热数据分层、自动清理和归档。
 */
#ifndef DB_AGING_H
#define DB_AGING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 老化文件魔数 */
#define AGING_FILE_MAGIC 0x4147494E  /* "AGIN" */

/** 老化文件版本 */
#define AGING_FILE_VERSION 1

/** 数据层级定义 */
#define AGING_TIER_HOT  0  /**< 热数据层（内存） */
#define AGING_TIER_WARM 1  /**< 温数据层（SSD） */
#define AGING_TIER_COLD 2  /**< 冷数据层（HDD/归档） */

/** 默认调度间隔（秒） */
#define AGING_DEFAULT_SCHEDULE_INTERVAL 3600

/** 默认保留期（天） */
#define AGING_DEFAULT_RETENTION_DAYS 30

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 老化策略类型
 */
typedef enum {
    AGING_POLICY_TTL = 0,           /**< 基于时间的 TTL 策略 */
    AGING_POLICY_ACCESS_FREQ = 1,   /**< 基于访问频率的策略 */
    AGING_POLICY_SIZE = 2,          /**< 基于容量的策略 */
    AGING_POLICY_LRU = 3,           /**< LRU 策略 */
    AGING_POLICY_CUSTOM = 4,        /**< 自定义策略 */
} aging_policy_type_t;

/**
 * @brief 老化动作
 */
typedef enum {
    AGING_ACTION_KEEP = 0,          /**< 保留在当前位置 */
    AGING_ACTION_MOVE_HOT = 1,      /**< 移至热层 */
    AGING_ACTION_MOVE_WARM = 2,     /**< 移至温层 */
    AGING_ACTION_MOVE_COLD = 3,     /**< 移至冷层 */
    AGING_ACTION_ARCHIVE = 4,       /**< 归档 */
    AGING_ACTION_DELETE = 5,        /**< 删除 */
} aging_action_t;

/**
 * @brief 老化策略配置
 */
typedef struct aging_policy_config_s {
    aging_policy_type_t type;       /**< 策略类型 */
    int32_t priority;               /**< 优先级（数字越小优先级越高） */
    union {
        struct {
            uint64_t max_age_seconds;   /**< 最大存活时间（秒） */
        } ttl;
        struct {
            int32_t max_access_count;   /**< 时间窗口内最大访问次数 */
            uint64_t time_window_seconds;  /**< 时间窗口（秒） */
        } access_freq;
        struct {
            int64_t max_size_bytes;     /**< 最大容量（字节） */
            float evict_ratio;          /**< 淘汰比例（0-1） */
        } size;
    } config;
} aging_policy_config_t;

/**
 * @brief 数据热度信息
 */
typedef struct data_temperature_s {
    void *data_id;                  /**< 数据标识符 */
    uint64_t last_access_time;      /**< 最后访问时间（微秒） */
    uint64_t create_time;           /**< 创建时间（微秒） */
    int32_t access_count;           /**< 访问次数 */
    int32_t current_tier;           /**< 当前层级 */
    float temperature;              /**< 热度评分（0-1，1=最热） */
} data_temperature_t;

/**
 * @brief 老化统计
 */
typedef struct aging_stats_s {
    uint64_t total_evictions;       /**< 总淘汰次数 */
    uint64_t total_archives;        /**< 总归档次数 */
    uint64_t total_deletes;         /**< 总删除次数 */
    uint64_t hot_count;             /**< 热数据数量 */
    uint64_t warm_count;            /**< 温数据数量 */
    uint64_t cold_count;            /**< 冷数据数量 */
    int64_t hot_size_bytes;         /**< 热数据大小 */
    int64_t warm_size_bytes;        /**< 温数据大小 */
    int64_t cold_size_bytes;        /**< 冷数据大小 */
} aging_stats_t;

/**
 * @brief 老化管理器
 */
typedef struct aging_manager_s {
    char data_dir[512];              /**< 数据目录 */
    aging_policy_config_t *policies; /**< 策略数组 */
    int32_t policy_count;            /**< 策略数量 */
    int32_t capacity;                /**< 策略容量 */

    /* 分层容量 */
    int64_t hot_capacity;            /**< 热层容量（字节） */
    int64_t warm_capacity;           /**< 温层容量（字节） */
    int64_t cold_capacity;           /**< 冷层容量（字节） */

    /* 当前使用量 */
    int64_t hot_usage;               /**< 热层已用 */
    int64_t warm_usage;              /**< 温层已用 */
    int64_t cold_usage;              /**< 冷层已用 */

    /* 调度 */
    uint64_t schedule_interval;      /**< 调度间隔（秒） */
    uint64_t last_run_time;          /**< 上次运行时间 */

    /* 统计 */
    aging_stats_t stats;
} aging_manager_t;

/**
 * @brief 老化回调函数
 *
 * @param data_id 数据标识符
 * @param action 老化动作
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
typedef int (*aging_callback_t)(void *data_id, aging_action_t action, void *user_data);

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建老化管理器
 *
 * @param data_dir 数据目录
 * @return 老化管理器指针，失败返回 NULL
 */
aging_manager_t *aging_manager_create(const char *data_dir);

/**
 * @brief 销毁老化管理器
 *
 * @param mgr 老化管理器
 */
void aging_manager_destroy(aging_manager_t *mgr);

/**
 * @brief 加载老化状态
 *
 * @param mgr 老化管理器
 * @return 0 成功，-1 失败
 */
int aging_load_state(aging_manager_t *mgr);

/**
 * @brief 保存老化状态
 *
 * @param mgr 老化管理器
 * @return 0 成功，-1 失败
 */
int aging_save_state(aging_manager_t *mgr);

/* ========================================================================
 * 策略管理 API
 * ======================================================================== */

/**
 * @brief 添加老化策略
 *
 * @param mgr 老化管理器
 * @param policy 策略配置
 * @return 0 成功，-1 失败
 */
int aging_add_policy(aging_manager_t *mgr, const aging_policy_config_t *policy);

/**
 * @brief 移除老化策略
 *
 * @param mgr 老化管理器
 * @param policy_type 策略类型
 * @return 0 成功，-1 失败
 */
int aging_remove_policy(aging_manager_t *mgr, aging_policy_type_t policy_type);

/**
 * @brief 获取策略
 *
 * @param mgr 老化管理器
 * @param policy_type 策略类型
 * @return 策略配置，未找到返回 NULL
 */
const aging_policy_config_t *aging_get_policy(const aging_manager_t *mgr,
                                               aging_policy_type_t policy_type);

/* ========================================================================
 * 容量管理 API
 * ======================================================================== */

/**
 * @brief 设置分层容量
 *
 * @param mgr 老化管理器
 * @param hot_capacity 热层容量
 * @param warm_capacity 温层容量
 * @param cold_capacity 冷层容量
 */
void aging_set_capacity(aging_manager_t *mgr,
                        int64_t hot_capacity,
                        int64_t warm_capacity,
                        int64_t cold_capacity);

/**
 * @brief 检查容量是否需要淘汰
 *
 * @param mgr 老化管理器
 * @param tier 层级
 * @param data_size 数据大小
 * @return true 需要淘汰
 */
bool aging_needs_eviction(const aging_manager_t *mgr, int32_t tier, int64_t data_size);

/* ========================================================================
 * 热度评估 API
 * ======================================================================== */

/**
 * @brief 记录数据访问
 *
 * @param mgr 老化管理器
 * @param data_id 数据标识符
 * @param data_size 数据大小
 */
void aging_record_access(aging_manager_t *mgr, void *data_id, int64_t data_size);

/**
 * @brief 评估数据热度
 *
 * @param mgr 老化管理器
 * @param temp 热度信息
 * @return 推荐的层级
 */
int32_t aging_evaluate_tier(const aging_manager_t *mgr, const data_temperature_t *temp);

/**
 * @brief 推荐老化动作
 *
 * @param mgr 老化管理器
 * @param temp 热度信息
 * @return 老化动作
 */
aging_action_t aging_recommend_action(const aging_manager_t *mgr,
                                       const data_temperature_t *temp);

/* ========================================================================
 * 执行老化 API
 * ======================================================================== */

/**
 * @brief 执行老化调度
 *
 * 扫描所有数据，应用策略，返回需要处理的数据列表
 *
 * @param mgr 老化管理器
 * @param out_actions 输出动作数组
 * @param max_actions 最大动作数
 * @return 实际动作数
 */
int32_t aging_schedule(aging_manager_t *mgr,
                       aging_action_t *out_actions,
                       int32_t max_actions);

/**
 * @brief 执行淘汰
 *
 * @param mgr 老化管理器
 * @param data_id 数据标识符
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0 成功，-1 失败
 */
int aging_evict(aging_manager_t *mgr,
                void *data_id,
                aging_callback_t callback,
                void *user_data);

/**
 * @brief 执行归档
 *
 * @param mgr 老化管理器
 * @param data_id 数据标识符
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0 成功，-1 失败
 */
int aging_archive(aging_manager_t *mgr,
                  void *data_id,
                  aging_callback_t callback,
                  void *user_data);

/**
 * @brief 执行删除
 *
 * @param mgr 老化管理器
 * @param data_id 数据标识符
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0 成功，-1 失败
 */
int aging_delete(aging_manager_t *mgr,
                 void *data_id,
                 aging_callback_t callback,
                 void *user_data);

/* ========================================================================
 * 统计 API
 * ======================================================================== */

/**
 * @brief 获取老化统计
 *
 * @param mgr 老化管理器
 * @param stats 输出统计
 */
void aging_get_stats(const aging_manager_t *mgr, aging_stats_t *stats);

/**
 * @brief 重置统计
 *
 * @param mgr 老化管理器
 */
void aging_reset_stats(aging_manager_t *mgr);

/* ========================================================================
 * 调度器 API
 * ======================================================================== */

/**
 * @brief 老化调度器
 */
typedef struct aging_scheduler_s aging_scheduler_t;

/**
 * @brief 调度器统计信息
 */
typedef struct aging_scheduler_stats_s {
    bool running;               /**< 是否运行 */
    uint64_t interval_seconds;  /**< 调度间隔 */
    int32_t queue_size;         /**< 待处理任务数 */
    uint64_t last_run_time;     /**< 上次运行时间 */
} aging_scheduler_stats_t;

/**
 * @brief 创建老化调度器
 *
 * @param mgr 老化管理器
 * @param interval_seconds 调度间隔（秒）
 * @return 调度器指针，失败返回 NULL
 */
aging_scheduler_t *aging_scheduler_create(aging_manager_t *mgr,
                                           uint64_t interval_seconds);

/**
 * @brief 销毁老化调度器
 *
 * @param scheduler 调度器
 */
void aging_scheduler_destroy(aging_scheduler_t *scheduler);

/**
 * @brief 启动调度器
 *
 * @param scheduler 调度器
 * @return 0 成功，-1 失败
 */
int aging_scheduler_start(aging_scheduler_t *scheduler);

/**
 * @brief 停止调度器
 *
 * @param scheduler 调度器
 * @return 0 成功，-1 失败
 */
int aging_scheduler_stop(aging_scheduler_t *scheduler);

/**
 * @brief 检查调度器是否运行
 *
 * @param scheduler 调度器
 * @return true 运行中，false 已停止
 */
bool aging_scheduler_is_running(const aging_scheduler_t *scheduler);

/**
 * @brief 获取调度间隔
 *
 * @param scheduler 调度器
 * @return 间隔秒数
 */
uint64_t aging_scheduler_get_interval(const aging_scheduler_t *scheduler);

/**
 * @brief 设置调度间隔
 *
 * @param scheduler 调度器
 * @param interval_seconds 间隔秒数
 * @return 0 成功，-1 失败
 */
int aging_scheduler_set_interval(aging_scheduler_t *scheduler, uint64_t interval_seconds);

/**
 * @brief 批量评估数据
 *
 * @param scheduler 调度器
 * @param data_ids 数据 ID 数组
 * @param temps 热度信息数组
 * @param count 数据数量
 * @param out_actions 输出动作数组
 * @param max_actions 最大动作数
 * @return 实际动作数
 */
int32_t aging_scheduler_batch_evaluate(aging_scheduler_t *scheduler,
                                       void **data_ids,
                                       data_temperature_t *temps,
                                       int32_t count,
                                       aging_action_t *out_actions,
                                       int32_t max_actions);

/**
 * @brief 执行所有待处理任务
 *
 * @param scheduler 调度器
 * @param max_tasks 最大执行任务数
 * @return 实际执行数
 */
int32_t aging_scheduler_process_queue(aging_scheduler_t *scheduler, int32_t max_tasks);

/**
 * @brief 获取待处理任务数
 *
 * @param scheduler 调度器
 * @return 待处理任务数
 */
int32_t aging_scheduler_get_queue_size(const aging_scheduler_t *scheduler);

/**
 * @brief 强制立即执行一次调度
 *
 * @param scheduler 调度器
 * @return 0 成功，-1 失败
 */
int aging_scheduler_run_once(aging_scheduler_t *scheduler);

/**
 * @brief 获取调度器统计信息
 *
 * @param scheduler 调度器
 * @param stats 输出统计
 */
void aging_scheduler_get_stats(const aging_scheduler_t *scheduler,
                               aging_scheduler_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_AGING_H */
