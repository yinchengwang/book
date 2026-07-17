/**
 * @file nodeModifyTable.h
 * @brief ModifyTable 执行器节点头文件
 */
#ifndef DB_SQL_NODES_MODIFYTABLE_H
#define DB_SQL_NODES_MODIFYTABLE_H

#include "db/sql/sql_executor.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OidIsValid
#define OidIsValid(oid) ((oid) != 0)
#endif

typedef enum {
    MODIFY_TABLE_INSERT,
    MODIFY_TABLE_UPDATE,
    MODIFY_TABLE_DELETE
} CmdType;

typedef struct {
    int type;
    int operation;
    uint32_t relationOid;
    List *targetlist;
    List *updateCols;
    List *returningList;
    List *plans;
} ModifyTablePlan;

typedef struct {
    PlanState *mt_child;
    int operation;
    uint32_t relationOid;
    List *returningList;
    uint64_t mt_processed;
    uint64_t mt_inserted;
    uint64_t mt_updated;
    uint64_t mt_deleted;
    bool mt_finished;
    bool mt_started;
    bool returning_done;
    int returning_index;
} ModifyTableExtState;

PlanState *ExecInitModifyTable(void *node, void *estate, int eflags);
TupleTableSlot *ExecModifyTable(PlanState *pstate);
void ExecEndModifyTable(PlanState *node);
void ExecReScanModifyTable(PlanState *node);
ModifyTablePlan *make_modifytable_plan(int operation, uint32_t relationOid);
ModifyTableExtState *ExecGetModifyTableExtState(PlanState *node);
void ExecGetModifyTableStats(PlanState *node, uint64_t *pinserted,
                             uint64_t *pupdated, uint64_t *pdeleted);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODES_MODIFYTABLE_H */
