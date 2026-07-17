/**
 * @file ts_scan.h
 * @brief 时序扫描算子接口
 *
 * 时序扫描算子直接调用 ts_engine，实现将在 Phase 4 完成。
 */
#ifndef DB_EXECUTOR_TS_SCAN_H
#define DB_EXECUTOR_TS_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 时序扫描状态 */
typedef struct TsScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    int64_t   time_start;   /**< 开始时间戳 */
    int64_t   time_end;     /**< 结束时间戳 */
    void     *tag_filters;  /**< 标签过滤器 */
    int64_t   down_sample_window; /**< 降采样窗口（毫秒） */
} TsScanState;

/**
 * @brief 初始化时序扫描
 */
TsScanState *exec_ts_scan_init(PlanState *parent,
    int64_t time_start, int64_t time_end, void *tag_filters);

/**
 * @brief 执行时序扫描，返回下一行
 */
TupleTableSlot *exec_ts_scan_next(TsScanState *state);

/**
 * @brief 关闭时序扫描
 */
void exec_ts_scan_close(TsScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_TS_SCAN_H */