// engineering/src/db/executor/operators/hashjoin_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Combine two VectorBlocks into a single new block
 */
static VectorBlock *combine_blocks(const VectorBlock *a, const VectorBlock *b) {
    if (!a && !b) return NULL;
    if (!a) return b;
    if (!b) return a;

    int total_rows = a->num_rows + b->num_rows;
    VectorBlock *combined = vector_block_create(total_rows, a->num_columns);
    if (!combined) return NULL;

    vector_block_set_num_rows(combined, total_rows);

    // Copy column types
    for (int i = 0; i < a->num_columns; i++) {
        vector_block_set_column_type(combined, i, vector_block_get_column_type(a, i));
    }

    // Copy column data
    for (int i = 0; i < a->num_columns; i++) {
        int col_size = a->column_sizes[i];
        void *combined_data = malloc(total_rows * col_size);
        if (!combined_data) {
            vector_block_destroy(combined);
            return NULL;
        }
        // Copy block A's data
        memcpy(combined_data, a->columns[i], a->num_rows * col_size);
        // Copy block B's data
        memcpy((char *)combined_data + a->num_rows * col_size, b->columns[i], b->num_rows * col_size);
        combined->columns[i] = combined_data;
        combined->column_sizes[i] = col_size;
    }

    // Copy null bitmaps (OR them together for respective rows)
    int a_words = (a->num_rows + 63) / 64;
    int b_words = (b->num_rows + 63) / 64;
    int combined_words = (total_rows + 63) / 64;
    memset(combined->null_bitmap, 0, combined_words * sizeof(uint64_t));
    for (int i = 0; i < a_words && i < combined_words; i++) {
        combined->null_bitmap[i] = a->null_bitmap[i];
    }
    for (int i = 0; i < b_words && (a_words + i) < combined_words; i++) {
        combined->null_bitmap[a_words + i] = b->null_bitmap[i];
    }

    return combined;
}

int hashjoin_open(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state) return -1;

    state->exhausted = 0;
    state->build_done = 0;
    state->cur_block = NULL;
    return 0;
}

VectorBlock *hashjoin_next(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state || state->exhausted) return NULL;

    vecx_hashjoin_t *hj = state->hj;
    if (!hj) return NULL;

    // 阶段1: build 侧 - 从左子节点读取所有块并构建哈希表
    if (!state->build_done) {
        while (1) {
            VectorBlock *build_block = exec_next(node->left);
            if (!build_block) break;
            int ret = vecx_hashjoin_add_build(hj, build_block);
            if (ret != 0) {
                state->exhausted = 1;
                return NULL;
            }
        }
        state->build_done = 1;
    }

    // 阶段2: probe 侧 - 循环读取所有 probe 块并累积结果
    if (!state->cur_block) {
        // First call - start accumulating probe results
        while (1) {
            VectorBlock *probe_block = exec_next(node->right);
            if (!probe_block) {
                break;
            }

            VectorBlock *output = NULL;
            int n = vecx_hashjoin_probe(hj, probe_block, &output);
            if (n < 0) {
                state->exhausted = 1;
                return NULL;
            }

            if (n > 0 && output) {
                if (!state->cur_block) {
                    state->cur_block = output;
                } else {
                    VectorBlock *merged = combine_blocks(state->cur_block, output);
                    vector_block_destroy(output);
                    if (!merged) {
                        state->exhausted = 1;
                        vector_block_destroy(state->cur_block);
                        state->cur_block = NULL;
                        return NULL;
                    }
                    vector_block_destroy(state->cur_block);
                    state->cur_block = merged;
                }
            }
        }
        state->exhausted = 1;

        VectorBlock *result = state->cur_block;
        state->cur_block = NULL;
        return result;
    }

    // Should not reach here since we set exhausted=1 and return result
    state->exhausted = 1;
    return NULL;
}

void hashjoin_reset(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state) return;

    state->exhausted = 0;
    state->build_done = 0;
    if (state->cur_block) {
        vector_block_destroy(state->cur_block);
        state->cur_block = NULL;
    }
    if (node->left && node->left->reset) {
        node->left->reset(node->left);
    }
    if (node->right && node->right->reset) {
        node->right->reset(node->right);
    }
}

void hashjoin_close(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state) return;

    if (state->cur_block) {
        vector_block_destroy(state->cur_block);
        state->cur_block = NULL;
    }
    if (state->hj) {
        vecx_hashjoin_destroy(state->hj);
        state->hj = NULL;
    }
}

ExecNode *exec_create_hashjoin(int build_key_col, int probe_key_col) {
    HashJoinState *state = (HashJoinState *)calloc(1, sizeof(HashJoinState));
    if (!state) return NULL;

    state->hj = vecx_hashjoin_create(build_key_col, probe_key_col);
    if (!state->hj) {
        free(state);
        return NULL;
    }

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        vecx_hashjoin_destroy(state->hj);
        free(state);
        return NULL;
    }

    node->node_type = PLAN_JOIN_HASH;
    node->state = state;
    node->open = hashjoin_open;
    node->next = hashjoin_next;
    node->reset = hashjoin_reset;
    node->close = hashjoin_close;

    return node;
}
