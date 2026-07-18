/**
 * @file stream_engine.h
 * @brief 流引擎接口定义
 *
 * 流引擎是一个内存中的模拟实现，不依赖任何外部消息队列系统。
 * 数据通过 stream_engine_insert() 写入，算子通过轮询读取。
 */
#ifndef DB_STREAM_ENGINE_H
#define DB_STREAM_ENGINE_H

#include "storage_engine.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 流记录定义
 * ======================================================================== */

/**
 * @brief 流记录
 */
typedef struct stream_record_s {
    int64_t timestamp;         /**< 时间戳（毫秒） */
    int64_t key;               /**< 键 */
    double value;              /**< 值 */
    char tag[64];              /**< 标签 */
} stream_record_t;

/* ========================================================================
 * 流引擎数据库
 * ======================================================================== */

/**
 * @brief 流引擎数据库
 */
typedef struct stream_engine_db_s {
    char name[256];            /**< 流名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */
    stream_record_t *buffer;   /**< 环形缓冲区 */
    uint64_t buffer_size;      /**< 缓冲区大小 */
    uint64_t write_pos;        /**< 写入位置 */
    uint64_t read_pos;         /**< 读取位置 */
    uint64_t count;            /**< 有效记录数 */
    int64_t min_watermark;     /**< 最低水位线 */
} stream_engine_db_t;

/* ========================================================================
 * 流引擎生命周期
 * ======================================================================== */

/**
 * @brief 初始化流引擎系统
 *
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int stream_engine_init(const char *data_dir);

/**
 * @brief 关闭流引擎系统
 *
 * @return 0 成功，-1 失败
 */
int stream_engine_shutdown(void);

/**
 * @brief 创建流
 *
 * @param name 流名称
 * @param schema 流模式（可为 NULL）
 * @return 0 成功，-1 失败
 */
int stream_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开流
 *
 * @param name 流名称
 * @param mode 访问模式
 * @return 流句柄，失败返回 NULL
 */
void *stream_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭流
 *
 * @param rel 流句柄
 * @return 0 成功，-1 失败
 */
int stream_engine_close(void *rel);

/**
 * @brief 删除流
 *
 * @param name 流名称
 * @return 0 成功，-1 失败
 */
int stream_engine_drop(const char *name);

/* ========================================================================
 * 数据操作
 * ======================================================================== */

/**
 * @brief 插入流记录
 *
 * @param rel 流句柄
 * @param data 记录数据
 * @param len 数据长度
 * @return 0 成功，-1 失败（缓冲区满）
 */
int stream_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 查询流记录（按水位线过滤）
 *
 * @param rel 流句柄
 * @param watermark 水位线时间戳（返回 timestamp > watermark 的记录）
 * @param out_timestamps 输出时间戳数组
 * @param out_values 输出值数组
 * @param out_tags 输出标签数组
 * @param out_count 输出记录数
 * @param max_results 最大结果数
 * @return 0 成功，-1 失败
 */
int stream_engine_query(void *rel, int64_t watermark,
                        int64_t *out_timestamps, double *out_values,
                        char (*out_tags)[64], uint32_t *out_count,
                        uint32_t max_results);

/**
 * @brief 查询流记录（返回记录数组）
 *
 * @param rel 流句柄
 * @param watermark 水位线时间戳
 * @param out_records 输出记录数组
 * @param out_count 输出记录数
 * @param max_results 最大结果数
 * @return 0 成功，-1 失败
 */
int stream_engine_query_records(void *rel, int64_t watermark,
                                stream_record_t *out_records,
                                uint32_t *out_count,
                                uint32_t max_results);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 获取流统计信息
 *
 * @param name 流名称
 * @param stats 统计信息输出
 * @return 0 成功，-1 失败
 */
int stream_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 默认缓冲区大小 */
#define STREAM_DEFAULT_BUFFER_SIZE 4096

/** 最大流数量 */
#define STREAM_MAX_STREAMS 16

#ifdef __cplusplus
}
#endif

#endif /* DB_STREAM_ENGINE_H */