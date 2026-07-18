/**
 * @file stream_agg.h
 * @brief 流聚合算子接口
 *
 * 实现流数据的持续聚合，实现将在 Phase 10 完成。
 */
#ifndef DB_EXECUTOR_STREAM_AGG_H
#define DB_EXECUTOR_STREAM_AGG_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/* StreamAggState 已在 sql_executor.h 中定义，这里直接使用 */

/**
 * @brief 初始化流聚合
 */
StreamAggState *exec_stream_agg_init(PlanState *parent,
    int agg_type, int64_t window_size);

/**
 * @brief 执行流聚合，返回下一行
 */
TupleTableSlot *exec_stream_agg_next(StreamAggState *state);

/**
 * @brief 关闭流聚合
 */
void exec_stream_agg_close(StreamAggState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_STREAM_AGG_H */