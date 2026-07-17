#include "bm25_private.h"

static int32_t bm25_posting_find_index(const bm25_term_slot_t *slot, int32_t doc_id)
{
    int32_t low;
    int32_t high;

    if (!slot || slot->posting_count <= 0) {
        return -1;
    }

    low = 0;
    high = slot->posting_count - 1;
    while (low <= high) {
        int32_t mid = low + (high - low) / 2;

        if (slot->postings[mid].doc_id < doc_id) {
            low = mid + 1;
        } else if (slot->postings[mid].doc_id > doc_id) {
            high = mid - 1;
        } else {
            return mid;
        }
    }
    return -1;
}

int32_t bm25_delete_document(bm25_t *index, int32_t doc_id)
{
    const bm25_doc_forward_list_t *forward;
    int32_t removed_token_count;
    int32_t i;

    if (!index || doc_id < 0 || doc_id >= index->doc_count) {
        return -1;
    }

    forward = bm25_doc_forward_get_tokens(index, doc_id);
    if (!forward || forward->count == 0) {
        /* 正排为空：文档无 token，仅回收 docId 和统计。 */
        index->doc_lengths[doc_id] = 0;
        bm25_free_doc_id(index, doc_id);
        return 0;
    }

    removed_token_count = 0;
    for (i = 0; i < forward->count; ++i) {
        int32_t term_slot_index = forward->items[i].term_slot_index;
        bm25_term_slot_t *slot;
        int32_t posting_index;
        int32_t term_freq;

        if (term_slot_index < 0 || term_slot_index >= index->term_slot_count) {
            continue;
        }
        slot = &index->term_slots[term_slot_index];

        posting_index = bm25_posting_find_index(slot, doc_id);
        if (posting_index < 0) {
            continue;
        }

        term_freq = slot->postings[posting_index].term_freq;
        if (bm25_posting_remove_at(slot, posting_index) != 0) {
            continue;
        }

        slot->doc_freq -= 1;
        slot->total_term_freq -= term_freq;
        removed_token_count += 1;

        /* 标记 filter 为 dirty，后续搜索/保存时延迟重建。 */
        slot->posting_filter_count = -1;
    }

    index->total_terms -= index->doc_lengths[doc_id];
    index->doc_lengths[doc_id] = 0;
    bm25_doc_forward_release(index, doc_id);
    bm25_free_doc_id(index, doc_id);

    return removed_token_count;
}
