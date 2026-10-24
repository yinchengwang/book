/**
 * @file stream_engine.h
 * @brief 流式数据存储引擎接口
 *
 * 提供流式数据写入、消费、订阅、窗口聚合等能力。
 * 支持 Kafka-style 的 Producer/Consumer 模型。
 */
#ifndef DB_STREAM_ENGINE_H
#define DB_STREAM_ENGINE_H

#include "db/storage_engine.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 流配置
 * ======================================================================== */

/** 压缩类型 */
typedef enum StreamCompressionType_e {
    STREAM_COMPRESS_NONE = 0,
    STREAM_COMPRESS_LZ4,
    STREAM_COMPRESS_ZSTD,
} StreamCompressionType;

/** 流配置 */
typedef struct stream_config_s {
    const char *name;                    /**< 流名称 */
    int32_t partition_count;            /**< 分区数 */
    int64_t retention_ms;               /**< 保留时间（毫秒） */
    int32_t replication_factor;         /**< 副本数 */
    StreamCompressionType compression;  /**< 压缩类型 */
} stream_config_t;

/* ========================================================================
 * 消费者定义
 * ======================================================================== */

/** 消费者状态 */
typedef enum StreamConsumerState_e {
    CONSUMER_STATE_IDLE = 0,
    CONSUMER_STATE_RUNNING,
    CONSUMER_STATE_PAUSED,
} StreamConsumerState;

/** 流式消费者（opaque 类型）*/
typedef struct stream_consumer_s {
    int64_t current_offset;              /**< 当前消费位置 */
    StreamConsumerState state;           /**< 消费者状态 */
    void *internal;                      /**< 内部实现（stream_consumer_impl_t*） */
    void *partition;                     /**< 内部分区指针（兼容性占位）*/
} stream_consumer_t;

/* ========================================================================
 * 流操作接口
 * ======================================================================== */

/**
 * @brief 流式存储引擎操作表
 */
typedef struct stream_ops_s {
    const char *name;                    /**< 引擎名称 */
    DataModel model;                     /**< MODEL_STREAM */

    /* 生命周期 */
    int (*init)(const char *data_dir);
    int (*shutdown)(void);

    /* 流操作 */
    int (*stream_create)(const char *name, const stream_config_t *config);
    void *(*stream_open)(const char *name);
    int (*stream_close)(void *stream);
    int (*stream_drop)(const char *name);
    bool (*stream_exists)(const char *name);

    /* 生产者操作 */
    int (*produce)(void *stream, const void *data, size_t len);
    int (*produce_partition)(void *stream, const void *data, size_t len, int partition);
    int64_t (*get_offset)(void *stream);
    int64_t (*get_lag)(void *stream, int partition);

    /* 消费者操作 */
    stream_consumer_t *(*subscribe)(void *stream, int64_t start_offset);
    int (*consume)(stream_consumer_t *consumer, void *out_data, size_t *out_len, size_t max_len);
    int (*commit_offset)(stream_consumer_t *consumer, int64_t offset);
    int (*consumer_close)(stream_consumer_t *consumer);

    /* 窗口操作 */
    int (*window_create)(const char *stream_name, const char *window_def);
    int (*window_agg)(const char *window_name, const char *agg_func, const char *column, void *out, size_t *out_len);

    /* 统计信息 */
    int (*get_stream_stats)(const char *name, storage_stats_t *stats);
    int (*get_partition_count)(const char *name, int *count);
} stream_ops_t;

/* ========================================================================
 * 引擎入口函数
 * ======================================================================== */

/**
 * @brief 获取流式存储引擎操作表
 */
const storage_ops_t *stream_engine_get_ops(void);

/* ========================================================================
 * 便捷宏（用于 engine_registry.c）
 * ======================================================================== */

#define STREAM_ENGINE_OPS_INITIALIZER { \
    .name = "stream_engine", \
    .model = MODEL_STREAM, \
    .init = stream_engine_init, \
    .shutdown = stream_engine_shutdown, \
    /* ... 其他字段 NULL 或填充 */ \
}

#ifdef __cplusplus
}
#endif

#endif /* DB_STREAM_ENGINE_H */
