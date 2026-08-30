#include "db/storage/doc/doc_inverted.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>

/* C3-3 T3: 时序标签倒排索引（复用 doc_inverted） */
typedef struct {
    void *inverted;  /* doc_inverted_index_t* */
    int64_t high_cardinality_threshold;  /* 默认 10000 */
    int64_t total_labels;  /* 总标签数 */
    int64_t unique_labels;  /* 唯一标签数 */
} ts_label_index_t;

ts_label_index_t *ts_label_index_create(int64_t threshold) {
    ts_label_index_t *idx = calloc(1, sizeof(*idx));
    if (!idx) return NULL;

    idx->high_cardinality_threshold = threshold > 0 ? threshold : 10000;
    idx->total_labels = 0;
    idx->unique_labels = 0;

    /* 创建倒排索引 */
    idx->inverted = doc_inverted_create("/tmp/ts_label_index", "simple");
    if (!idx->inverted) {
        free(idx);
        return NULL;
    }

    return idx;
}

void ts_label_index_destroy(ts_label_index_t *idx) {
    if (!idx) return;

    /* 关闭并释放倒排索引 */
    if (idx->inverted) {
        doc_inverted_close((doc_inverted_index_t *)idx->inverted);
        doc_inverted_free((doc_inverted_index_t *)idx->inverted);
    }

    free(idx);
}

bool ts_label_index_is_high_cardinality(ts_label_index_t *idx) {
    if (!idx) return false;

    /* 如果唯一标签数超过阈值，认为是高基数 */
    return idx->unique_labels > idx->high_cardinality_threshold;
}

int ts_label_index_add(ts_label_index_t *idx, uint64_t series_id,
                       const char *label_key, const char *label_value) {
    if (!idx || !label_key || !label_value) return -1;

    /* 构建标签字符串：key=value */
    size_t key_len = strlen(label_key);
    size_t val_len = strlen(label_value);
    size_t total_len = key_len + 1 + val_len + 1;  /* key=value\0 */

    char *label_str = (char *)malloc(total_len);
    if (!label_str) return -1;

    snprintf(label_str, total_len, "%s=%s", label_key, label_value);

    /* 添加到倒排索引 */
    int result = doc_inverted_add((doc_inverted_index_t *)idx->inverted,
                                  series_id, label_str);

    free(label_str);

    if (result == 0) {
        idx->total_labels++;
        idx->unique_labels++;  /* 简化：每次添加都增加唯一计数 */
    }

    return result;
}

int ts_label_index_remove(ts_label_index_t *idx, uint64_t series_id) {
    if (!idx) return -1;

    int result = doc_inverted_remove((doc_inverted_index_t *)idx->inverted,
                                     series_id);

    if (result == 0) {
        idx->total_labels--;
        /* 注意：唯一标签数可能需要更复杂的逻辑来维护 */
    }

    return result;
}

int ts_label_index_search(ts_label_index_t *idx, const char *query,
                          uint64_t *out_series_ids, uint32_t max_results,
                          uint32_t *out_count) {
    if (!idx || !query || !out_series_ids || !out_count) return -1;

    *out_count = 0;

    /* 使用倒排索引搜索 */
    doc_inverted_result_t results[1024];
    uint32_t found = doc_inverted_search((doc_inverted_index_t *)idx->inverted,
                                         query, results,
                                         max_results < 1024 ? max_results : 1024);

    /* 转换结果 */
    for (uint32_t i = 0; i < found && i < max_results; i++) {
        out_series_ids[i] = results[i].doc_id;
    }

    *out_count = found;
    return 0;
}

int64_t ts_label_index_count(ts_label_index_t *idx) {
    if (!idx) return 0;
    return idx->unique_labels;
}
