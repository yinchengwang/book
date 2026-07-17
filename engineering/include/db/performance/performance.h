/*
 * performance.h - 性能优化接口
 */

#ifndef DB_PERFORMANCE_H
#define DB_PERFORMANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * SIMD 优化
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 检查 CPU 是否支持 SIMD
 * @return true 支持
 */
bool simd_is_supported(void);

/**
 * @brief 获取 SIMD 类型
 * @return SIMD 类型描述
 */
const char *simd_get_type(void);

/* ─────────────────────────────────────────────────────────────────
 * 向量化操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 批量比较整数
 * @param a 数组 A
 * @param b 数组 B
 * @param result 结果数组（0/1）
 * @param count 元素数量
 */
void simd_compare_int(const int32_t *a, const int32_t *b, uint8_t *result, size_t count);

/**
 * @brief 批量求和（64位整数）
 * @param data 数据数组
 * @param count 元素数量
 * @return 总和
 */
int64_t simd_sum_int64(const int64_t *data, size_t count);

/**
 * @brief 批量求和（32位浮点数）
 * @param data 数据数组
 * @param count 元素数量
 * @return 总和
 */
double simd_sum_double(const double *data, size_t count);

/**
 * @brief 批量计算平均值
 * @param data 数据数组
 * @param count 元素数量
 * @return 平均值
 */
double simd_avg_double(const double *data, size_t count);

/* ─────────────────────────────────────────────────────────────────
 * 并行查询
 * ───────────────────────────────────────────────────────────────── */

typedef struct parallel_executor parallel_executor_t;

/**
 * @brief 并行执行器配置
 */
typedef struct {
    int num_workers;           /* 工作线程数 */
    int chunk_size;           /* 分块大小 */
    bool auto_tuning;         /* 自动调优 */
} parallel_config_t;

/**
 * @brief 创建并行执行器
 * @param config 配置
 * @return 执行器句柄
 */
parallel_executor_t *parallel_executor_create(const parallel_config_t *config);

/**
 * @brief 销毁并行执行器
 * @param executor 执行器
 */
void parallel_executor_destroy(parallel_executor_t *executor);

/**
 * @brief 获取工作线程数
 * @param executor 执行器
 * @return 线程数
 */
int parallel_executor_workers(const parallel_executor_t *executor);

/* ─────────────────────────────────────────────────────────────────
 * 任务定义
 * ───────────────────────────────────────────────────────────────── */

typedef int (*parallel_task_func_t)(void *arg, int worker_id);

typedef struct parallel_task {
    void *arg;                    /* 任务参数 */
    parallel_task_func_t func;   /* 任务函数 */
} parallel_task_t;

/**
 * @brief 执行并行任务
 * @param executor 执行器
 * @param tasks 任务数组
 * @param num_tasks 任务数量
 * @return 0 成功
 */
int parallel_execute(parallel_executor_t *executor,
                     parallel_task_t *tasks, int num_tasks);

/**
 * @brief 执行 MapReduce
 * @param executor 执行器
 * @param data 数据数组
 * @param count 数据数量
 * @param map_func 映射函数
 * @param reduce_func 归约函数
 * @param result 输出结果
 * @return 0 成功
 */
int parallel_map_reduce(parallel_executor_t *executor,
                       void *data, size_t count,
                       void *(*map_func)(void *, int, int),
                       void *(*reduce_func)(void *, void *),
                       void *result);

/* ─────────────────────────────────────────────────────────────────
 * 批量操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 批量插入回调
 */
typedef int (*batch_insert_callback_t)(void *row, void *arg);

/**
 * @brief 执行批量插入
 * @param table 表名
 * @param rows 行数组
 * @param count 行数
 * @param callback 回调函数
 * @param arg 回调参数
 * @return 成功插入的行数
 */
int batch_insert(const char *table, void **rows, size_t count,
                 batch_insert_callback_t callback, void *arg);

/**
 * @brief 批量更新回调
 */
typedef int (*batch_update_callback_t)(void *old_row, void *new_row, void *arg);

/**
 * @brief 执行批量更新
 * @param table 表名
 * @param conditions 条件数组
 * @param updates 更新数组
 * @param count 数量
 * @param callback 回调函数
 * @param arg 回调参数
 * @return 更新的行数
 */
int batch_update(const char *table, void **conditions, void **updates, size_t count,
                batch_update_callback_t callback, void *arg);

/* ─────────────────────────────────────────────────────────────────
 * 缓存优化
 * ───────────────────────────────────────────────────────────────── */

typedef struct query_cache query_cache_t;

/**
 * @brief 创建查询缓存
 * @param max_entries 最大条目数
 * @param ttl_seconds TTL 秒数
 * @return 缓存句柄
 */
query_cache_t *query_cache_create(size_t max_entries, int ttl_seconds);

/**
 * @brief 销毁查询缓存
 * @param cache 缓存
 */
void query_cache_destroy(query_cache_t *cache);

/**
 * @brief 获取缓存条目
 * @param cache 缓存
 * @param key 缓存键
 * @param value 输出值
 * @param value_size 值大小
 * @return true 命中
 */
bool query_cache_get(query_cache_t *cache, const char *key,
                    void *value, size_t *value_size);

/**
 * @brief 设置缓存条目
 * @param cache 缓存
 * @param key 缓存键
 * @param value 值
 * @param value_size 值大小
 */
void query_cache_set(query_cache_t *cache, const char *key,
                    const void *value, size_t value_size);

/**
 * @brief 使缓存失效
 * @param cache 缓存
 * @param key 缓存键（NULL 表示全部）
 */
void query_cache_invalidate(query_cache_t *cache, const char *key);

/**
 * @brief 获取缓存统计
 */
typedef struct {
    size_t hits;          /* 命中次数 */
    size_t misses;        /* 未命中次数 */
    size_t evictions;     /* 驱逐次数 */
    size_t memory_used;   /* 使用内存 */
} query_cache_stats_t;

/**
 * @brief 获取缓存统计
 * @param cache 缓存
 * @return 统计信息
 */
const query_cache_stats_t *query_cache_get_stats(const query_cache_t *cache);

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取 CPU 核心数
 * @return 核心数
 */
int get_cpu_count(void);

/**
 * @brief 获取可用内存
 * @return 可用内存（字节）
 */
uint64_t get_available_memory(void);

/**
 * @brief 估计最佳工作线程数
 * @param workload_type 工作负载类型（0=CPU密集，1=IO密集）
 * @return 推荐线程数
 */
int estimate_optimal_workers(int workload_type);

#ifdef __cplusplus
}
#endif

#endif /* DB_PERFORMANCE_H */