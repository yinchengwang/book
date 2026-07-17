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

/** 流连接状态 */
typedef struct StreamJoinState_s {
    PlanState ps;           /**< 基类 PlanState */
    int64_t   interval;     /**< 连接时间窗口（毫秒） */
    int       join_type;    /**< 连接类型：0=stream-stream, 1=stream-table */
} StreamJoinState;

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