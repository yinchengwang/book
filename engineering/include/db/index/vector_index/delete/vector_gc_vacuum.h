/*
 * 向量索引垃圾回收器
 *
 * 后台线程扫描已删除向量，执行物理删除并修复图结构。
 */

#ifndef VECTOR_GC_VACUUM_H
#define VECTOR_GC_VACUUM_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GC 扫描模式
 */
typedef enum gc_scan_mode {
    GC_SCAN_MODE_INCREMENTAL,  /* 增量扫描：只扫描新增的已删除向量 */
    GC_SCAN_MODE_FULL,          /* 全量扫描：扫描所有已删除向量 */
    GC_SCAN_MODE_ADAPTIVE       /* 自适应：根据删除比例自动选择 */
} gc_scan_mode_t;

/*
 * GC 状态
 */
typedef enum gc_state {
    GC_STATE_IDLE,      /* 空闲 */
    GC_STATE_SCANNING,  /* 扫描中 */
    GC_STATE_REPAIRING, /* 修复中 */
    GC_STATE_STOPPING   /* 停止中 */
} gc_state_t;

/*
 * GC 进度回调
 */
typedef void (*gc_progress_callback_t)(int32_t processed, int32_t total, void *ctx);

/*
 * GC 配置参数
 */
typedef struct gc_config {
    int32_t  batch_size;           /* 每批处理的向量数 */
    int32_t  min_deleted_ratio;    /* 触发 GC 的最小删除比例（百分比） */
    int32_t  max_concurrency;      /* 最大并发线程数 */
    bool     auto_vacuum;          /* 是否启用自动 GC */
    int32_t  vacuum_interval_sec; /* 自动 GC 间隔（秒） */
} gc_config_t;

/*
 * GC 统计信息
 */
typedef struct gc_stats {
    int32_t total_vacuumed;     /* 累计清理的向量数 */
    int32_t total_scanned;       /* 累计扫描的向量数 */
    int64_t total_time_us;       /* 累计 GC 时间（微秒） */
    int32_t last_vacuumed;       /* 上次清理的向量数 */
    int32_t last_scan_time_us;   /* 上次扫描耗时 */
} gc_stats_t;

/*
 * GC 上下文结构
 */
typedef struct gc_vacuum gc_vacuum_t;

/**
 * 创建 GC 实例
 *
 * @param delete_bitmap 删除位图
 * @param config        GC 配置（NULL 使用默认配置）
 * @return GC 上下文，失败返回 NULL
 */
gc_vacuum_t *gc_vacuum_create(void *delete_bitmap, const gc_config_t *config);

/**
 * 销毁 GC 实例
 *
 * @param vacuum GC 上下文
 */
void gc_vacuum_destroy(gc_vacuum_t *vacuum);

/**
 * 启动后台 GC 线程
 *
 * @param vacuum GC 上下文
 * @return 0 成功，-1 失败
 */
int32_t gc_vacuum_start(gc_vacuum_t *vacuum);

/**
 * 停止后台 GC 线程
 *
 * @param vacuum GC 上下文
 * @return 0 成功，-1 失败
 */
int32_t gc_vacuum_stop(gc_vacuum_t *vacuum);

/**
 * 执行一次 GC（同步调用）
 *
 * @param vacuum   GC 上下文
 * @param mode     扫描模式
 * @param callback 进度回调（可为 NULL）
 * @param ctx      回调上下文
 * @return 清理的向量数，负值表示错误
 */
int32_t gc_vacuum_execute(gc_vacuum_t *vacuum,
                          gc_scan_mode_t mode,
                          gc_progress_callback_t callback,
                          void *ctx);

/**
 * 获取 GC 状态
 *
 * @param vacuum GC 上下文
 * @return 当前状态
 */
gc_state_t gc_vacuum_get_state(const gc_vacuum_t *vacuum);

/**
 * 获取 GC 统计信息
 *
 * @param vacuum GC 上下文
 * @param stats  统计信息输出
 */
void gc_vacuum_get_stats(const gc_vacuum_t *vacuum, gc_stats_t *stats);

/**
 * 更新 GC 配置
 *
 * @param vacuum GC 上下文
 * @param config 新配置
 * @return 0 成功，-1 失败
 */
int32_t gc_vacuum_update_config(gc_vacuum_t *vacuum, const gc_config_t *config);

/**
 * 获取默认 GC 配置
 */
void gc_config_init_default(gc_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_GC_VACUUM_H */
