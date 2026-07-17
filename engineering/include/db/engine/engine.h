/**
 * @file engine.h
 * @brief 多模态引擎统一接口声明
 *
 * 各引擎（vector/kv/ts/doc/spatial/graph/yang/stream）通过此接口
 * 被执行器算子直调，不走 Access Method 层包装。
 */
#ifndef DB_ENGINE_H
#define DB_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 引擎类型枚举 */
typedef enum {
    ENGINE_VECTOR,      /**< 向量引擎 */
    ENGINE_KV,          /**< KV 引擎 */
    ENGINE_TS,          /**< 时序引擎 */
    ENGINE_DOC,         /**< 文档引擎 */
    ENGINE_SPATIAL,     /**< 空间引擎 */
    ENGINE_GRAPH,       /**< 图引擎 */
    ENGINE_YANG,        /**< 树/Yang 引擎 */
    ENGINE_STREAM,      /**< 流引擎 */
    ENGINE_COUNT
} engine_type_t;

/** 引擎操作结果 */
typedef struct engine_result_s {
    int     code;        /**< 0 成功，非 0 错误码 */
    char   *msg;         /**< 错误信息 */
    void   *data;        /**< 结果数据 */
    int64_t nrows;       /**< 影响行数 */
} engine_result_t;

#ifdef __cplusplus
}
#endif

#endif /* DB_ENGINE_H */