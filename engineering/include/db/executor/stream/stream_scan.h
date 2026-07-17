/**
 * @file stream_scan.h
 * @brief 流扫描算子接口
 *
 * 流扫描算子实现流表读取，实现将在 Phase 10 完成。
 */
#ifndef DB_EXECUTOR_STREAM_SCAN_H
#define DB_EXECUTOR_STREAM_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 流扫描状态 */
typedef struct StreamScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *stream_oid;   /**< 流表 OID */
    int64_t   watermark;    /**< 水位线时间戳 */
    int       batch_size;   /**< 批处理大小 */
} StreamScanState;

/**
 * @brief 初始化流扫描
 */
StreamScanState *exec_stream_scan_init(PlanState *parent,
    void *stream_oid, int64_t watermark, int batch_size);

/**
 * @brief 执行流扫描，返回下一行
 */
TupleTableSlot *exec_stream_scan_next(StreamScanState *state);

/**
 * @brief 关闭流扫描
 */
void exec_stream_scan_close(StreamScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_STREAM_SCAN_H */