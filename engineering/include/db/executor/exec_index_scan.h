// engineering/include/db/executor/exec_index_scan.h
#ifndef DB_EXECUTOR_EXEC_INDEX_SCAN_H
#define DB_EXECUTOR_EXEC_INDEX_SCAN_H

#include "exec_node.h"
#include "db/index/index_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IndexScan 状态
 */
typedef struct {
    index_manager_t *mgr;
    int index_id;
    int table_id;
    const void *scan_range;
    void *entry;              /**< 当前索引条目 */
    int exhausted;            /**< 扫描是否结束 */
} IndexScanState;

/**
 * @brief 创建索引扫描 ExecNode
 *
 * @param mgr Index manager
 * @param index_id Index ID
 * @param table_id Table ID
 * @param scan_range Scan range (key range for index lookup)
 * @return ExecNode pointer, NULL on failure
 */
ExecNode *exec_create_index_scan(index_manager_t *mgr,
                                 int index_id,
                                 int table_id,
                                 const void *scan_range);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_INDEX_SCAN_H */
