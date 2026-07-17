/**
 * @file vector_write_buffer.h
 * @brief 向量写入缓冲区 - 环形队列实现
 *
 * 用于流式插入场景，聚合批量插入后异步合并到主索引。
 */

#ifndef VECTOR_WRITE_BUFFER_H
#define VECTOR_WRITE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct vector_write_buffer vector_write_buffer_t;
typedef struct vector_buffer_config vector_buffer_config_t;

/* ========================================================================
 * 缓冲区配置
 * ======================================================================== */

/**
 * @brief 缓冲区配置
 */
struct vector_buffer_config {
    int32_t capacity;      /* 缓冲区容量（向量数） */
    int32_t flush_threshold; /* 自动刷写阈值（向量数） */
    int32_t dims;          /* 向量维度 */
    int32_t batch_size;    /* 批量大小 */
};

/**
 * @brief 默认配置
 */
#define VECTOR_BUFFER_DEFAULT_CAPACITY 10000
#define VECTOR_BUFFER_DEFAULT_FLUSH_THRESHOLD 1000
#define VECTOR_BUFFER_DEFAULT_BATCH_SIZE 100

/**
 * @brief 创建默认配置
 */
vector_buffer_config_t vector_buffer_config_default(int32_t dims);

/* ========================================================================
 * 缓冲区操作
 * ======================================================================== */

/**
 * @brief 创建写入缓冲区
 * @param config 缓冲区配置（NULL 使用默认配置）
 * @return 缓冲区句柄，失败返回 NULL
 */
vector_write_buffer_t *vector_buffer_create(const vector_buffer_config_t *config);

/**
 * @brief 销毁写入缓冲区
 * @param buf 缓冲区句柄
 */
void vector_buffer_destroy(vector_write_buffer_t *buf);

/**
 * @brief 向缓冲区推送向量（单个）
 * @param buf 缓冲区句柄
 * @param vector 向量数据
 * @return 0 成功，-1 失败
 */
int32_t vector_buffer_push(vector_write_buffer_t *buf, const float *vector);

/**
 * @brief 向缓冲区推送向量（批量）
 * @param buf 缓冲区句柄
 * @param vectors 向量数组
 * @param n 向量数量
 * @return 成功推送的数量，-1 失败
 */
int32_t vector_buffer_push_batch(vector_write_buffer_t *buf, const float *vectors, int32_t n);

/**
 * @brief 从缓冲区弹出向量（用于合并到主索引）
 * @param buf 缓冲区句柄
 * @param vectors 输出向量数组（预分配）
 * @param n 最大数量
 * @return 实际弹出的数量
 */
int32_t vector_buffer_pop(vector_write_buffer_t *buf, float *vectors, int32_t n);

/**
 * @brief 刷新缓冲区（获取所有数据并清空）
 * @param buf 缓冲区句柄
 * @param vectors 输出向量数组（预分配）
 * @param max_n 最大数量
 * @return 实际刷新的数量
 */
int32_t vector_buffer_flush(vector_write_buffer_t *buf, float *vectors, int32_t max_n);

/**
 * @brief 获取当前缓冲区中的向量数量
 * @param buf 缓冲区句柄
 * @return 向量数量
 */
int32_t vector_buffer_size(const vector_write_buffer_t *buf);

/**
 * @brief 获取缓冲区容量
 * @param buf 缓冲区句柄
 * @return 容量
 */
int32_t vector_buffer_capacity(const vector_write_buffer_t *buf);

/**
 * @brief 检查缓冲区是否为空
 * @param buf 缓冲区句柄
 * @return true 空，false 非空
 */
bool vector_buffer_empty(const vector_write_buffer_t *buf);

/**
 * @brief 检查缓冲区是否已满
 * @param buf 缓冲区句柄
 * @return true 满，false 未满
 */
bool vector_buffer_full(const vector_write_buffer_t *buf);

/**
 * @brief 设置刷写阈值
 * @param buf 缓冲区句柄
 * @param threshold 阈值（向量数）
 * @return 0 成功，-1 失败
 */
int32_t vector_buffer_set_flush_threshold(vector_write_buffer_t *buf, int32_t threshold);

/**
 * @brief 获取刷写阈值
 * @param buf 缓冲区句柄
 * @return 阈值
 */
int32_t vector_buffer_get_flush_threshold(const vector_write_buffer_t *buf);

/**
 * @brief 检查是否需要刷写
 * @param buf 缓冲区句柄
 * @return true 需要刷写
 */
bool vector_buffer_need_flush(const vector_write_buffer_t *buf);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 缓冲区统计信息
 */
typedef struct vector_buffer_stats {
    int32_t current_size;      /* 当前大小 */
    int32_t capacity;          /* 容量 */
    int32_t flush_threshold;   /* 刷写阈值 */
    int64_t total_pushed;      /* 累计推送数 */
    int64_t total_flushed;     /* 累计刷新数 */
    int32_t flush_count;       /* 刷写次数 */
} vector_buffer_stats_t;

/**
 * @brief 获取缓冲区统计信息
 * @param buf 缓冲区句柄
 * @param stats 输出统计信息
 */
void vector_buffer_get_stats(const vector_write_buffer_t *buf, vector_buffer_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_WRITE_BUFFER_H */
