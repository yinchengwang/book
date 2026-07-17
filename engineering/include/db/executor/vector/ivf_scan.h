/**
 * @file ivf_scan.h
 * @brief IVF 索引扫描算子接口
 *
 * IVF 扫描算子直接调用 vector_engine 的 IVF-PQ 搜索接口。
 * 注意：IvfScanState 已在 db/sql/sql_executor.h 中定义，
 * 此处仅提供算子生命周期函数声明。
 */
#ifndef DB_EXECUTOR_IVF_SCAN_H
#define DB_EXECUTOR_IVF_SCAN_H

#include "db/sql/sql_executor.h"  /* IvfScanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 IVF 扫描
 */
IvfScanState *exec_ivf_scan_init(PlanState *parent,
    float *query_vector, int top_k, int distance_metric,
    int nprobe, int nlist);

/**
 * @brief 执行 IVF 扫描，返回下一行
 */
TupleTableSlot *exec_ivf_scan_next(IvfScanState *state);

/**
 * @brief 关闭 IVF 扫描
 */
void exec_ivf_scan_close(IvfScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_IVF_SCAN_H */