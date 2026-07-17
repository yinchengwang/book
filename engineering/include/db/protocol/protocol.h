/**
 * @file protocol.h
 * @brief 非 SQL 协议层统一接口声明
 *
 * 各模型独立协议：RESP（KV）、InfluxDB Line Protocol（时序）、
 * GQL（图）、Datalog、Yang Path、Kafka（流）
 */
#ifndef DB_PROTOCOL_H
#define DB_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 协议类型枚举 */
typedef enum {
    PROTOCOL_RESP,          /**< RESP（Redis 序列化协议） */
    PROTOCOL_INFLUX_LINE,   /**< InfluxDB Line Protocol */
    PROTOCOL_GQL,           /**< GQL 图查询语言 */
    PROTOCOL_DATALOG,       /**< Datalog 逻辑编程语言 */
    PROTOCOL_YANG_PATH,     /**< Yang Path / NETCONF */
    PROTOCOL_KAFKA,         /**< Kafka 协议 */
    PROTOCOL_COUNT
} protocol_type_t;

#ifdef __cplusplus
}
#endif

#endif /* DB_PROTOCOL_H */