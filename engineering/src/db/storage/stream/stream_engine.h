/**
 * @file stream_engine.h
 * @brief 流式存储引擎内部头文件
 */
#ifndef DB_STORAGE_STREAM_STREAM_ENGINE_H
#define DB_STORAGE_STREAM_STREAM_ENGINE_H

#include "db/stream_engine.h"
#include "db/storage_engine.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 消息记录 */
typedef struct stream_record_s {
    int64_t offset;           /**< 消息偏移 */
    int32_t partition;        /**< 分区号 */
    int64_t timestamp;        /**< 时间戳 */
    void *data;               /**< 数据指针 */
    size_t len;               /**< 数据长度 */
    struct stream_record_s *next;
} stream_record_t;

/** 分区 */
typedef struct stream_partition_s {
    int32_t id;               /**< 分区 ID */
    stream_record_t *head;    /**< 消息链表头 */
    stream_record_t *tail;    /**< 消息链表尾 */
    int64_t first_offset;     /**< 起始偏移 */
    int64_t last_offset;      /**< 最后偏移 */
    int64_t record_count;     /**< 消息数量 */
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
} stream_partition_t;

/** 流描述符 */
typedef struct stream_handle_s {
    char name[256];           /**< 流名称 */
    stream_config_t config;   /**< 流配置 */
    stream_partition_t *partitions;  /**< 分区数组 */
    int32_t num_partitions;   /**< 分区数量 */
    int64_t total_records;    /**< 总消息数 */
    char data_dir[512];       /**< 数据目录 */
} stream_handle_t;

/* ========================================================================
 * 消费者实现
 * ======================================================================== */

/** 消费者实现结构（内部扩展） */
typedef struct stream_consumer_impl_s {
    stream_partition_t *partition;  /**< 消费的分区 */
    int64_t current_offset;         /**< 当前消费位置 */
    StreamConsumerState state;      /**< 状态 */
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
} stream_consumer_impl_t;

#endif /* DB_STORAGE_STREAM_STREAM_ENGINE_H */
