/**
 * @file nodeModifyTable.h
 * @brief ModifyTable 执行器节点头文件
 */
#ifndef DB_SQL_NODE_MODIFYTABLE_H
#define DB_SQL_NODE_MODIFYTABLE_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODIFY_TABLE_INSERT,
    MODIFY_TABLE_UPDATE,
    MODIFY_TABLE_DELETE
} CmdType;

typedef struct ModifyTable {
    Plan         plan;
    CmdType      operation;
    Oid          relationOid;
    List        *targetlist;
    List        *updateCols;
    List        *returningList;
    List        *plans;
} ModifyTable;

typedef struct {
    PlanState    ps;
    CmdType      operation;
    Oid          relationOid;
    List        *returningList;
    uint64_t     mt_processed;
    uint64_t     mt_inserted;
    uint64_t     mt_updated;
    uint64_t     mt_deleted;
    bool         mt_finished;
    bool         mt_started;
    bool         returning_done;
    int          returning_index;
} ModifyTableState;

PlanState *ExecInitModifyTable(Plan *node, EState *estate, int eflags);
TupleTableSlot *ExecModifyTable(PlanState *pstate);
void ExecEndModifyTable(ModifyTableState *node);
void ExecReScanModifyTable(ModifyTableState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_MODIFYTABLE_H */
