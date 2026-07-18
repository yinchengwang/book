/**
 * @file stream_window.h
 * @brief 流窗口算子接口
 *
 * 实现流数据的窗口计算，实现将在 Phase 10 完成。
 */
#ifndef DB_EXECUTOR_STREAM_WINDOW_H
#define DB_EXECUTOR_STREAM_WINDOW_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/* StreamWindowState 已在 sql_executor.h 中定义，这里直接使用 */

/**
 * @brief 初始化流窗口
 */
StreamWindowState *exec_stream_window_init(PlanState *parent,
    int64_t window_size, int64_t slide, int window_type);

/**
 * @brief 执行流窗口计算，返回下一行
 */
TupleTableSlot *exec_stream_window_next(StreamWindowState *state);

/**
 * @brief 关闭流窗口
 */
void exec_stream_window_close(StreamWindowState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_STREAM_WINDOW_H */