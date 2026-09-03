/**
 * @file ts_continuous_agg.h
 * @brief 时序数据连续聚合头文件
 *
 * 实现连续聚合（Continuous Aggregation）：
 * 1. 变更数据捕获（CDC）
 * 2. 增量计算
 * 3. 自动降采样策略
 * 4. 查询改写
 */
#ifndef DB_TS_CONTINUOUS_AGG_H
#define DB_TS_CONTINUOUS_AGG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 连续聚合定义
 * ======================================================================== */

/** 聚合粒度 */
typedef enum CaggBucket_e {
    CAGG_BUCKET_RAW = 0,       /**< 原始数据（不聚合） */
    CAGG_BUCKET_1M = 1,        /**< 1 分钟 */
    CAGG_BUCKET_5M = 5,        /**< 5 分钟 */
    CAGG_BUCKET_15M = 15,      /**< 15 分钟 */
    CAGG_BUCKET_1H = 60,       /**< 1 小时 */
    CAGG_BUCKET_1D = 1440      /**< 1 天 */
} CaggBucket;

/** 聚合函数 */
typedef enum CaggFunc_e {
    CAGG_SUM = 0,
    CAGG_AVG = 1,
    CAGG_MIN = 2,
    CAGG_MAX = 3,
    CAGG_COUNT = 4,
    CAGG_FIRST = 5,
    CAGG_LAST = 6
} CaggFunc;

/** 连续聚合配置 */
typedef struct ContinuousAggConfig_s {
    const char *name;              /**< 聚合名称 */
    const char *source_measurement;/**< 源度量名 */
    const char *target_measurement;/**< 目标度量名（物化视图） */
    CaggBucket bucket;             /**< 聚合粒度 */
    CaggFunc func;                 /**< 聚合函数 */
    const char **group_by_tags;    /**< 分组 Tag 列表 */
    uint32_t group_by_tag_count;   /**< 分组 Tag 数 */
    int64_t offset_ms;             /**< 时间偏移（毫秒） */
    int64_t refresh_interval_ms;   /**< 刷新间隔 */
    int64_t window_start;          /**< 窗口起始（相对当前时间） */
    int64_t window_end;            /**< 窗口结束 */
} ContinuousAggConfig;

/**
 * @brief 创建连续聚合配置
 */
ContinuousAggConfig *cagg_config_create(const char *name, const char *source);

/**
 * @brief 设置聚合参数
 */
void cagg_config_set_bucket(ContinuousAggConfig *config, CaggBucket bucket);
void cagg_config_set_func(ContinuousAggConfig *config, CaggFunc func);
void cagg_config_add_group_by_tag(ContinuousAggConfig *config, const char *tag);
void cagg_config_set_refresh_interval(ContinuousAggConfig *config, int64_t interval_ms);
void cagg_config_set_window(ContinuousAggConfig *config, int64_t start_offset, int64_t end_offset);

/**
 * @brief 释放连续聚合配置
 */
void cagg_config_free(ContinuousAggConfig *config);

/* ========================================================================
 * 连续聚合状态
 * ======================================================================== */

/** 聚合状态 */
typedef struct ContinuousAggState_s {
    char name[128];                /**< 聚合名称 */
    ContinuousAggConfig *config;   /**< 配置 */

    int64_t last_refresh_time;     /**< 上次刷新时间 */
    int64_t last_processed_time;   /**< 上次处理时间 */
    uint64_t processed_points;     /**< 已处理数据点数 */
    uint64_t output_points;        /**< 输出数据点数 */
    uint64_t refresh_count;        /**< 刷新次数 */

    /* 增量状态 */
    void *incremental_state;       /**< 增量计算状态 */
    void *materialized_data;       /**< 物化数据 */
} ContinuousAggState;

/**
 * @brief 创建连续聚合状态
 */
ContinuousAggState *cagg_state_create(const ContinuousAggConfig *config);

/**
 * @brief 释放连续聚合状态
 */
void cagg_state_free(ContinuousAggState *state);

/* ========================================================================
 * 变更数据捕获（CDC）
 * ======================================================================== */

/** CDC 事件类型 */
typedef enum CdcEventType_e {
    CDC_INSERT = 0,
    CDC_UPDATE = 1,
    CDC_DELETE = 2
} CdcEventType;

/** CDC 事件 */
typedef struct CdcEvent_s {
    CdcEventType type;         /**< 事件类型 */
    int64_t timestamp;         /**< 时间戳 */
    double value;              /**< 值 */
    int64_t series_id;         /**< 序列 ID */
    void *old_value;           /**< 旧值（UPDATE/DELETE） */
    void *new_value;           /**< 新值（UPDATE/INSERT） */
} CdcEvent;

/** CDC 消费者 */
typedef struct CdcConsumer_s {
    const char *name;          /**< 消费者名称 */
    void *internal;            /**< 内部状态 */
} CdcConsumer;

/**
 * @brief 创建 CDC 消费者
 */
CdcConsumer *cdc_consumer_create(const char *name);

/**
 * @brief 销毁 CDC 消费者
 */
void cdc_consumer_destroy(CdcConsumer *consumer);

/**
 * @brief 发送 CDC 事件
 *
 * @param consumer 消费者
 * @param event CDC 事件
 * @return 0 成功，-1 失败
 */
int cdc_send_event(CdcConsumer *consumer, const CdcEvent *event);

/**
 * @brief 批量发送 CDC 事件
 */
int cdc_send_events_batch(CdcConsumer *consumer,
                          const CdcEvent *events,
                          uint32_t count);

/**
 * @brief 注册 CDC 回调
 */
typedef void (*CdcCallback)(const CdcEvent *event, void *user_data);

int cdc_register_callback(CdcConsumer *consumer,
                          CdcCallback callback,
                          void *user_data);

/* ========================================================================
 * 增量计算
 * ======================================================================== */

/** 增量计算器 */
typedef struct IncrementalComputer_s {
    CaggFunc func;             /**< 聚合函数 */
    double sum;                /**< 求和 */
    double min;                /**< 最小值 */
    double max;                /**< 最大值 */
    uint64_t count;            /**< 计数 */
    double first;              /**< 第一个值 */
    double last;               /**< 最后一个值 */
} IncrementalComputer;

/**
 * @brief 创建增量计算器
 */
IncrementalComputer *incremental_computer_create(CaggFunc func);

/**
 * @brief 添加数据点
 */
int incremental_computer_add(IncrementalComputer *comp,
                             int64_t timestamp, double value);

/**
 * @brief 合并两个计算器
 */
int incremental_computer_merge(IncrementalComputer *dest,
                               const IncrementalComputer *src);

/**
 * @brief 获取聚合结果
 */
double incremental_computer_get_result(const IncrementalComputer *comp);

/**
 * @brief 重置计算器
 */
void incremental_computer_reset(IncrementalComputer *comp);

/**
 * @brief 释放增量计算器
 */
void incremental_computer_free(IncrementalComputer *comp);

/* ========================================================================
 * 自动降采样策略
 * ======================================================================== */

/** 降采样规则 */
typedef struct DownsampleRule_s {
    CaggBucket bucket;             /**< 聚合粒度 */
    int64_t retention_days;        /**< 保留天数 */
    CaggFunc func;                 /**< 聚合函数 */
    int priority;                  /**< 优先级（越小越高） */
} DownsampleRule;

/** 降采样策略 */
typedef struct DownsamplingPolicy_s {
    DownsampleRule *rules;         /**< 规则数组 */
    uint32_t rule_count;           /**< 规则数 */
    int64_t raw_retention_days;    /**< 原始数据保留天数 */
} DownsamplingPolicy;

/**
 * @brief 创建默认降采样策略
 *
 * 策略：1分钟保留7天，5分钟保留30天，1小时保留1年
 */
DownsamplingPolicy *downsampling_policy_default(void);

/**
 * @brief 添加降采样规则
 */
int downsampling_policy_add_rule(DownsamplingPolicy *policy,
                                 CaggBucket bucket,
                                 int64_t retention_days,
                                 CaggFunc func);

/**
 * @brief 获取适用的降采样规则
 */
const DownsampleRule *downsampling_policy_get_rule(const DownsamplingPolicy *policy,
                                                   int64_t data_age_days);

/**
 * @brief 释放降采样策略
 */
void downsampling_policy_free(DownsamplingPolicy *policy);

/* ========================================================================
 * 连续聚合管理器
 * ======================================================================== */

/** 连续聚合管理器 */
typedef struct ContinuousAggManager_s {
    char data_dir[512];            /**< 数据目录 */

    /* 连续聚合注册表 */
    ContinuousAggState **aggregations;
    uint32_t agg_count;
    uint32_t agg_capacity;

    /* CDC 消费者 */
    CdcConsumer *cdc_consumer;

    /* 降采样策略 */
    DownsamplingPolicy *downsample_policy;

    /* 刷新任务 */
    void *refresh_task;            /**< 刷新定时任务 */
    int64_t next_refresh_time;     /**< 下次刷新时间 */
} ContinuousAggManager;

/**
 * @brief 创建连续聚合管理器
 */
ContinuousAggManager *cagg_manager_create(const char *data_dir);

/**
 * @brief 销毁连续聚合管理器
 */
void cagg_manager_destroy(ContinuousAggManager *mgr);

/* ========================================================================
 * 连续聚合操作
 * ======================================================================== */

/**
 * @brief 创建连续聚合
 */
int cagg_manager_create_agg(ContinuousAggManager *mgr,
                            const ContinuousAggConfig *config);

/**
 * @brief 删除连续聚合
 */
int cagg_manager_drop_agg(ContinuousAggManager *mgr, const char *name);

/**
 * @brief 获取连续聚合状态
 */
ContinuousAggState *cagg_manager_get_agg(const ContinuousAggManager *mgr,
                                         const char *name);

/**
 * @brief 刷新连续聚合
 */
int cagg_manager_refresh(ContinuousAggManager *mgr, const char *name);

/**
 * @brief 刷新所有连续聚合
 */
int cagg_manager_refresh_all(ContinuousAggManager *mgr);

/**
 * @brief 增量刷新（只处理新数据）
 */
int cagg_manager_refresh_incremental(ContinuousAggManager *mgr,
                                     const char *name,
                                     int64_t since_time);

/* ========================================================================
 * 查询改写
 * ======================================================================== */

/** 查询改写建议 */
typedef struct QueryRewrite_s {
    const char *original_query;    /**< 原始查询 */
    const char *rewritten_query;   /**< 改写后的查询 */
    const char *materialized_name; /**< 使用的物化视图 */
    double speedup_estimate;       /**< 估计加速比 */
} QueryRewrite;

/**
 * @brief 分析查询是否可以改写为使用连续聚合
 *
 * @param mgr 连续聚合管理器
 * @param sql SQL 查询语句
 * @param out_rewrite 输出改写建议
 * @return 0 可以改写，-1 无法改写
 */
int cagg_manager_analyze_query(const ContinuousAggManager *mgr,
                               const char *sql,
                               QueryRewrite *out_rewrite);

/**
 * @brief 执行查询改写
 */
int cagg_manager_rewrite_query(const ContinuousAggManager *mgr,
                               const char *sql,
                               char *out_buffer,
                               size_t buffer_size);

/**
 * @brief 释放查询改写
 */
void query_rewrite_free(QueryRewrite *rewrite);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 获取连续聚合统计
 */
void cagg_manager_stats(const ContinuousAggManager *mgr,
                        uint32_t *out_agg_count,
                        uint64_t *out_total_input_points,
                        uint64_t *out_total_output_points,
                        int64_t *out_last_refresh_time);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_CONTINUOUS_AGG_H */
