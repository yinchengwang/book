/**
 * @file ts_window.h
 * @brief 时序窗口聚合算子接口
 *
 * 实现滑动窗口聚合和降采样，实现将在 Phase 4 完成。
 */
#ifndef DB_EXECUTOR_TS_WINDOW_H
#define DB_EXECUTOR_TS_WINDOW_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 时序窗口聚合状态 */
typedef struct TsWindowState_s {
    PlanState ps;           /**< 基类 PlanState */
    int64_t   window_size;  /**< 窗口大小（毫秒） */
    int64_t   slide;        /**< 滑动步长（毫秒） */
    int       agg_type;     /**< 聚合类型：0=AVG, 1=MAX, 2=MIN, 3=SUM, 4=COUNT */
} TsWindowState;

/**
 * @brief 初始化时序窗口聚合
 */
TsWindowState *exec_ts_window_init(PlanState *parent,
    int64_t window_size, int64_t slide, int agg_type);

/**
 * @brief 执行时序窗口聚合，返回下一行
 */
TupleTableSlot *exec_ts_window_next(TsWindowState *state);

/**
 * @brief 关闭时序窗口聚合
 */
void exec_ts_window_close(TsWindowState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_TS_WINDOW_H */