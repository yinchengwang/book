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

/** 流聚合状态 */
typedef struct StreamAggState_s {
    PlanState ps;           /**< 基类 PlanState */
    int       agg_type;     /**< 聚合类型：0=AVG, 1=MAX, 2=MIN, 3=SUM, 4=COUNT */
    int64_t   window_size;  /**< 窗口大小（毫秒） */
    void     *acc_state;    /**< 累积状态 */
} StreamAggState;

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