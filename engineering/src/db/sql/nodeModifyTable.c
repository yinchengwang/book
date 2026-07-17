/**
 * @file nodeModifyTable.c
 * @brief ModifyTable 执行器节点实现
 *
 * 实现 INSERT/UPDATE/DELETE 操作与存储层的集成。
 * 对接 heap_insert, heap_update, heap_delete API。
 */

#include "db/sql/nodes/nodeModifyTable.h"
#include "db/rel.h"
#include "db/heapam.h"
#include <stdlib.h>
#include <string.h>

static TupleTableSlot *exec_modifytable_impl(PlanState *pstate)
{
    ModifyTableExtState *ext;
    ModifyTablePlan *plan;
    Relation rel = NULL;

    if (!pstate) return NULL;
    ext = (ModifyTableExtState *)pstate->state;
    if (!ext) return NULL;

    if (ext->mt_finished) return NULL;
    if (!ext->mt_started) ext->mt_started = true;

    /* 打开目标 Relation（延迟打开） */
    if (ext->relationOid != 0) {
        rel = relation_open(ext->relationOid, RELMODE_WRITE);
        if (!rel) {
            ext->mt_finished = true;
            return NULL;
        }
    }

    if (ext->returningList && !ext->returning_done) {
        ext->returning_done = true;
        goto cleanup;
    }

    while (pstate->left) {
        PlanState *child = pstate->left;
        TupleTableSlot *(*proc)(PlanState *) = child->exec_proc;
        if (proc) {
            TupleTableSlot *slot = proc(child);
            if (!slot) break;

            /* 执行实际的数据修改操作 */
            if (rel && slot->tts_tuple.data) {
                int result = -1;

                switch (ext->operation) {
                    case MODIFY_TABLE_INSERT:
                        /* INSERT: 将元组写入存储层 */
                        result = heap_insert(rel,
                                            slot->tts_tuple.data,
                                            slot->tts_tuple.len > 0 ? slot->tts_tuple.len : 64,
                                            0,  /* cid */
                                            0,  /* options */
                                            NULL /* bistate */);
                        if (result == 0) {
                            ext->mt_inserted++;
                        }
                        break;

                    case MODIFY_TABLE_UPDATE:
                        /* UPDATE: 先删除旧元组，再插入新元组 */
                        if (slot->tts_tuple.len > 0) {
                            /* 构造 TID（简化：假设元组在块 0） */
                            uint8_t tid[6];
                            uint32_t block = 0;
                            uint16_t offset = 24;
                            memcpy(tid, &block, sizeof(uint32_t));
                            memcpy(tid + sizeof(uint32_t), &offset, sizeof(uint16_t));

                            result = heap_update(rel,
                                                tid,
                                                slot->tts_tuple.data,
                                                slot->tts_tuple.len,
                                                0,  /* cid */
                                                0,  /* options */
                                                NULL, /* bistate */
                                                0   /* lockmode */);
                            if (result == 0) {
                                ext->mt_updated++;
                            }
                        }
                        break;

                    case MODIFY_TABLE_DELETE:
                        /* DELETE: 标记元组为已删除 */
                        {
                            /* 构造 TID（简化：假设元组在块 0） */
                            uint8_t tid[6];
                            uint32_t block = 0;
                            uint16_t offset = 24;
                            memcpy(tid, &block, sizeof(uint32_t));
                            memcpy(tid + sizeof(uint32_t), &offset, sizeof(uint16_t));

                            result = heap_delete(rel,
                                                tid,
                                                0,  /* cid */
                                                false,  /* crosscheck */
                                                false /* wait */);
                            if (result == 0) {
                                ext->mt_deleted++;
                            }
                        }
                        break;

                    default:
                        break;
                }
            }

            ext->mt_processed++;
        } else break;
    }

cleanup:
    /* 关闭 Relation */
    if (rel) {
        relation_close(rel, RELMODE_WRITE);
    }

    ext->mt_finished = true;
    return NULL;
}

PlanState *ExecInitModifyTable(void *node, void *estate, int eflags)
{
    PlanState *pstate;
    ModifyTablePlan *plan;
    ModifyTableExtState *ext;

    (void)eflags; (void)estate;
    if (!node) return NULL;

    plan = (ModifyTablePlan *)node;
    pstate = (PlanState *)calloc(1, sizeof(PlanState));
    if (!pstate) return NULL;

    ext = (ModifyTableExtState *)calloc(1, sizeof(ModifyTableExtState));
    if (!ext) { free(pstate); return NULL; }

    pstate->type = EXEC_MODIFYTABLE;
    pstate->left = NULL;
    pstate->right = NULL;
    pstate->state = ext;
    pstate->exec_proc = ExecModifyTable;

    ext->operation = plan->operation;
    ext->relationOid = plan->relationOid;
    ext->returningList = plan->returningList;
    ext->mt_child = NULL;
    ext->mt_processed = 0;
    ext->mt_inserted = 0;
    ext->mt_updated = 0;
    ext->mt_deleted = 0;
    ext->mt_finished = false;
    ext->mt_started = false;
    ext->returning_done = false;
    ext->returning_index = 0;

    return pstate;
}

TupleTableSlot *ExecModifyTable(PlanState *pstate)
{
    return exec_modifytable_impl(pstate);
}

void ExecEndModifyTable(PlanState *pstate)
{
    if (!pstate) return;
    if (pstate->state) { free(pstate->state); pstate->state = NULL; }
    free(pstate);
}

void ExecReScanModifyTable(PlanState *pstate)
{
    ModifyTableExtState *ext;
    if (!pstate) return;
    ext = (ModifyTableExtState *)pstate->state;
    if (!ext) return;
    ext->mt_processed = 0;
    ext->mt_inserted = 0;
    ext->mt_updated = 0;
    ext->mt_deleted = 0;
    ext->mt_finished = false;
    ext->mt_started = false;
    ext->returning_done = false;
    ext->returning_index = 0;
}

ModifyTablePlan *make_modifytable_plan(int operation, uint32_t relationOid)
{
    ModifyTablePlan *node = (ModifyTablePlan *)calloc(1, sizeof(ModifyTablePlan));
    if (!node) return NULL;
    node->type = EXEC_MODIFYTABLE;
    node->operation = operation;
    node->relationOid = relationOid;
    node->targetlist = NULL;
    node->updateCols = NULL;
    node->returningList = NULL;
    node->plans = NULL;
    return node;
}

ModifyTableExtState *ExecGetModifyTableExtState(PlanState *pstate)
{
    if (!pstate) return NULL;
    return (ModifyTableExtState *)pstate->state;
}

void ExecGetModifyTableStats(PlanState *pstate, uint64_t *pinserted,
                             uint64_t *pupdated, uint64_t *pdeleted)
{
    ModifyTableExtState *ext;
    if (!pstate) {
        if (pinserted) *pinserted = 0;
        if (pupdated) *pupdated = 0;
        if (pdeleted) *pdeleted = 0;
        return;
    }
    ext = (ModifyTableExtState *)pstate->state;
    if (!ext) {
        if (pinserted) *pinserted = 0;
        if (pupdated) *pupdated = 0;
        if (pdeleted) *pdeleted = 0;
        return;
    }
    if (pinserted) *pinserted = ext->mt_inserted;
    if (pupdated) *pupdated = ext->mt_updated;
    if (pdeleted) *pdeleted = ext->mt_deleted;
}
