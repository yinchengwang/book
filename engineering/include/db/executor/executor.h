/**
 * @file executor.h
 * @brief 统一执行器入口
 *
 * 聚合所有执行器功能，包括标准算子（Volcano 模型）和多模态算子。
 * 标准算子实现位于 src/db/sql/ 下，多模态算子位于 src/db/executor/ 下。
 */
#ifndef DB_EXECUTOR_H
#define DB_EXECUTOR_H

#include "db/sql/executor.h"        /* 执行器入口（execMain 等价） */
#include "db/sql/sql_executor.h"    /* Volcono 执行器主接口 */
#include "db/sql/expr.h"            /* 表达式求值 */
#include "db/sql/nodes/execnodes.h" /* 执行节点类型 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行器版本信息
 */
typedef struct {
    int major;           /**< 主版本号 */
    int minor;           /**< 次版本号 */
    const char *engine;  /**< 引擎名称 */
} executor_version_t;

/**
 * @brief 获取执行器版本
 */
executor_version_t executor_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_H */