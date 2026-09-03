/**
 * @file multisource.c
 * @brief Multi-source replication implementation
 *
 * Manages multiple replica sources for a node, allowing a single node
 * to replicate from multiple upstream sources.
 */

#include "db/consistency/multisource.h"
#include "db/consistency/replication_consensus.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Multi-source Replication API Implementation
 * ============================================================ */

int rc_add_source(replication_consensus_t *rc, const replica_node_t *node)
{
    if (rc == NULL || node == NULL) {
        return -1;
    }

    if (node->node_id <= 0) {
        return -1;
    }

    if (rc->source_count >= MAX_REPLICA_SOURCES) {
        return -1;
    }

    /* 检查是否已存在 */
    for (int i = 0; i < rc->source_count; i++) {
        if (rc->sources[i].node_id == node->node_id) {
            /* 更新已存在的节点 */
            rc->sources[i] = *node;
            return 0;
        }
    }

    /* 添加新节点 */
    rc->sources[rc->source_count++] = *node;
    return 0;
}

int rc_remove_source(replication_consensus_t *rc, int node_id)
{
    if (rc == NULL || node_id <= 0) {
        return -1;
    }

    for (int i = 0; i < rc->source_count; i++) {
        if (rc->sources[i].node_id == node_id) {
            /* 移动后面的元素 */
            for (int j = i; j < rc->source_count - 1; j++) {
                rc->sources[j] = rc->sources[j + 1];
            }
            rc->source_count--;
            memset(&rc->sources[rc->source_count], 0, sizeof(replica_node_t));
            return 0;
        }
    }

    return -1;
}

int rc_get_sources(replication_consensus_t *rc, replica_node_t *nodes, int *count)
{
    if (rc == NULL || nodes == NULL || count == NULL) {
        return -1;
    }

    int max_count = *count;
    *count = rc->source_count;

    if (max_count < rc->source_count) {
        /* 缓冲区不够，只复制能容纳的数量 */
        for (int i = 0; i < max_count; i++) {
            nodes[i] = rc->sources[i];
        }
        return -1;
    }

    for (int i = 0; i < rc->source_count; i++) {
        nodes[i] = rc->sources[i];
    }

    return 0;
}

int rc_sync_all(replication_consensus_t *rc)
{
    if (rc == NULL) {
        return -1;
    }

    /* 同步所有源节点 */
    for (int i = 0; i < rc->source_count; i++) {
        replica_node_t *source = &rc->sources[i];

        /*
         * 简化实现：更新源节点状态
         * 实际实现中需要与每个源节点建立连接并同步 WAL
         */
        if (source->state == REPL_STATE_DISCONNECTED) {
            /* 尝试重新连接 */
            source->state = REPL_STATE_CONNECTING;
        } else if (source->state == REPL_STATE_STREAMING ||
                   source->state == REPL_STATE_NORMAL) {
            /* 正常的源节点，状态已经是同步的 */
            ;
        }
    }

    return 0;
}
