// engineering/src/db/executor/operators/shard_scan_exec.c
#include "db/executor/exec_shard.h"
#include "db/executor/exec_node.h"
#include "db/sharding/sharding.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief ShardScan 状态
 */
typedef struct {
    shard_coordinator_t *coordinator;
    shard_router_t *router;
    void *key;
    size_t key_len;
    int selected_shard;
    VectorBlock *cur_block;
    int exhausted;
} ShardScanState;

static int shard_scan_open(ExecNode *node) {
    ShardScanState *state = (ShardScanState *)node->state;
    if (!state || !state->coordinator || !state->key) return -1;

    // 获取所有分片信息
    int max_shards = shard_count(state->router);
    if (max_shards <= 0) {
        state->exhausted = 1;
        return 0;
    }

    shard_info_t *all_shards = (shard_info_t *)malloc(sizeof(shard_info_t) * max_shards);
    if (!all_shards) return -1;

    int count = shard_get_all(state->router, all_shards, max_shards);
    if (count <= 0) {
        free(all_shards);
        state->exhausted = 1;
        return 0;
    }

    // 提取分片 ID 数组
    int *shard_ids = (int *)malloc(sizeof(int) * count);
    if (!shard_ids) {
        free(all_shards);
        return -1;
    }
    for (int i = 0; i < count; i++) {
        shard_ids[i] = all_shards[i].shard_id;
    }
    free(all_shards);

    // 使用协调器选择最小负载的分片
    state->selected_shard = shard_coordinator_select_least_load(
        state->coordinator, shard_ids, count);

    free(shard_ids);

    if (state->selected_shard < 0) {
        state->exhausted = 1;
        return 0;
    }

    state->exhausted = 0;
    state->cur_block = NULL;
    return 0;
}

static VectorBlock *shard_scan_next(ExecNode *node) {
    ShardScanState *state = (ShardScanState *)node->state;
    if (!state || state->exhausted) return NULL;

    // TODO: 根据 selected_shard 获取对应分片数据
    // 这里暂时返回 NULL 表示迭代结束，实际需要连接分片获取数据
    state->exhausted = 1;
    return NULL;
}

static void shard_scan_reset(ExecNode *node) {
    ShardScanState *state = (ShardScanState *)node->state;
    if (!state) return;

    state->cur_block = NULL;
    state->exhausted = 0;

    // 重新选择分片
    int max_shards = shard_count(state->router);
    if (max_shards <= 0) {
        state->exhausted = 1;
        return;
    }

    shard_info_t *all_shards = (shard_info_t *)malloc(sizeof(shard_info_t) * max_shards);
    if (!all_shards) {
        state->exhausted = 1;
        return;
    }

    int count = shard_get_all(state->router, all_shards, max_shards);
    if (count <= 0) {
        free(all_shards);
        state->exhausted = 1;
        return;
    }

    int *shard_ids = (int *)malloc(sizeof(int) * count);
    if (!shard_ids) {
        free(all_shards);
        state->exhausted = 1;
        return;
    }
    for (int i = 0; i < count; i++) {
        shard_ids[i] = all_shards[i].shard_id;
    }
    free(all_shards);

    state->selected_shard = shard_coordinator_select_least_load(
        state->coordinator, shard_ids, count);

    free(shard_ids);
}

static void shard_scan_close(ExecNode *node) {
    ShardScanState *state = (ShardScanState *)node->state;
    if (!state) return;

    state->cur_block = NULL;
    state->exhausted = 1;
}

ExecNode *exec_create_shard_scan(shard_coordinator_t *coord,
                                  const void *key, size_t key_len) {
    if (!coord || !key) return NULL;

    // 获取 router 从 coordinator
    shard_router_t *router = shard_coordinator_get_router(coord);
    if (!router) return NULL;

    ShardScanState *state = (ShardScanState *)calloc(1, sizeof(ShardScanState));
    if (!state) return NULL;

    state->coordinator = coord;
    state->router = router;
    state->key = malloc(key_len);
    if (!state->key) {
        free(state);
        return NULL;
    }
    memcpy(state->key, key, key_len);
    state->key_len = key_len;
    state->selected_shard = -1;
    state->cur_block = NULL;
    state->exhausted = 0;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state->key);
        free(state);
        return NULL;
    }

    node->node_type = PLAN_SCAN_SEQ;  // 使用 SeqScan 作为基类型
    node->state = state;
    node->open = shard_scan_open;
    node->next = shard_scan_next;
    node->reset = shard_scan_reset;
    node->close = shard_scan_close;

    return node;
}