/**
 * @file stream_processor.h
 * @brief 流处理引擎接口
 *
 * Phase12 - 实现流处理，追赶 Kafka/Pulsar 水平。
 */
#ifndef DB_STORAGE_STREAM_PROCESSOR_H
#define DB_STORAGE_STREAM_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 流处理器不透明类型 */
typedef struct stream_processor stream_processor_t;

/** 消费者组 */
typedef struct consumer_group consumer_group_t;

/** 消息回调 */
typedef void (*message_callback_t)(const void *key, size_t key_len,
                                  const void *value, size_t value_len, void *ctx);

/** 创建流处理器 */
stream_processor_t *stream_create(const char *data_dir, size_t retention_bytes);

/** 关闭处理器 */
void stream_close(stream_processor_t *stream);

/** 创建主题 */
int stream_create_topic(stream_processor_t *stream, const char *topic, int partitions);

/** 生产消息 */
int stream_produce(stream_processor_t *stream, const char *topic,
                   const void *key, size_t key_len,
                   const void *value, size_t value_len);

/** 创建消费者组 */
consumer_group_t *stream_create_consumer_group(stream_processor_t *stream,
                                            const char *group_id,
                                            const char *topic,
                                            message_callback_t callback, void *ctx);

/** 消费消息 */
int stream_consume(consumer_group_t *group, int64_t offset);

/** 提交偏移 */
int stream_commit(consumer_group_t *group, int64_t offset);

/** 释放消费者组 */
void stream_consumer_group_close(consumer_group_t *group);

/** 获取主题信息 */
int64_t stream_topic_latest_offset(const stream_processor_t *stream, const char *topic);
int64_t stream_topic_earliest_offset(const stream_processor_t *stream, const char *topic);

#ifdef __cplusplus
}
#endif
#endif /* DB_STORAGE_STREAM_PROCESSOR_H */
