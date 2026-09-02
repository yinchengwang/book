// engineering/src/db/executor/operators/index_scan_exec.c
#include "db/executor/exec_index_scan.h"
#include "db/executor/exec_node.h"
#include "db/index/index_manager.h"
#include "db/index/index_catalog.h"
#include <stdlib.h>

static int index_scan_open(ExecNode *node) {
    IndexScanState *state = (IndexScanState *)node->state;
    if (!state || !state->mgr) return -1;

    /* Get index entry from manager */
    const index_entry_t *entry = index_manager_get_index(state->mgr, state->index_id);
    if (!entry) {
        state->exhausted = 1;
        return -1;
    }

    state->entry = (void *)entry;
    state->exhausted = 0;
    return 0;
}

static VectorBlock *index_scan_next(ExecNode *node) {
    IndexScanState *state = (IndexScanState *)node->state;
    if (!state || state->exhausted) return NULL;

    /* TODO: Implement actual index scan iteration
     * For now, mark as exhausted after first call.
     * Real implementation would:
     * 1. Use index_scan_start() to begin scanning with scan_range
     * 2. Iterate through index entries using index_scan_next()
     * 3. For each matching entry, fetch the heap tuple
     * 4. Return VectorBlock with matching rows
     */
    state->exhausted = 1;
    return NULL;
}

static void index_scan_reset(ExecNode *node) {
    IndexScanState *state = (IndexScanState *)node->state;
    if (!state) return;

    state->entry = NULL;
    state->exhausted = 0;
}

static void index_scan_close(ExecNode *node) {
    IndexScanState *state = (IndexScanState *)node->state;
    if (!state) return;

    state->entry = NULL;
    state->exhausted = 1;
}

ExecNode *exec_create_index_scan(index_manager_t *mgr,
                                 int index_id,
                                 int table_id,
                                 const void *scan_range) {
    if (!mgr) return NULL;

    IndexScanState *state = (IndexScanState *)calloc(1, sizeof(IndexScanState));
    if (!state) return NULL;

    state->mgr = mgr;
    state->index_id = index_id;
    state->table_id = table_id;
    state->scan_range = scan_range;
    state->entry = NULL;
    state->exhausted = 0;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_SCAN_INDEX;
    node->state = state;
    node->open = index_scan_open;
    node->next = index_scan_next;
    node->reset = index_scan_reset;
    node->close = index_scan_close;

    return node;
}
