/**
 * @file stream_join.h
 * @brief 流连接算子接口
 *
 * 实现流数据流-流连接和流-表连接，实现将在 Phase 10 完成。
 */
#ifndef DB_EXECUTOR_STREAM_JOIN_H
#define DB_EXECUTOR_STREAM_JOIN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/* StreamJoinState 已在 sql_executor.h 中定义，这里直接使用 */

/**
 * @brief 初始化流连接
 */
StreamJoinState *exec_stream_join_init(PlanState *parent,
    int64_t interval, int join_type);

/**
 * @brief 执行流连接，返回下一行
 */
TupleTableSlot *exec_stream_join_next(StreamJoinState *state);

/**
 * @brief 关闭流连接
 */
void exec_stream_join_close(StreamJoinState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_STREAM_JOIN_H */