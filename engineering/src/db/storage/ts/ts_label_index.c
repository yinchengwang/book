#include "db/storage/doc/doc_inverted.h"
#include "db/core/log.h"

/* C3-3 T3: 时序标签倒排索引（复用 doc_inverted） */
typedef struct {
    void *inverted;  /* doc_inverted_index_t* */
    int64_t high_cardinality_threshold;  /* 默认 10000 */
} ts_label_index_t;

ts_label_index_t *ts_label_index_create(int64_t threshold) {
    ts_label_index_t *idx = calloc(1, sizeof(*idx));
    if (!idx) return NULL;
    idx->high_cardinality_threshold = threshold > 0 ? threshold : 10000;
    /* 占位：实际复用 doc_inverted_index_create */
    return idx;
}

void ts_label_index_destroy(ts_label_index_t *idx) {
    if (!idx) return;
    /* 释放 inverted 子结构 */
    free(idx);
}

bool ts_label_index_is_high_cardinality(ts_label_index_t *idx) {
    if (!idx) return false;
    /* 简化：始终基于 threshold 返回 false（待实际统计后判断） */
    (void)idx;
    return false;
}
