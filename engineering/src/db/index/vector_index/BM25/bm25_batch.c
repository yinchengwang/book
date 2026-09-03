#include "bm25_private.h"

/* 预估批量文档中的唯一 term 数量。 */
static int32_t bm25_estimate_unique_terms(int32_t document_count, const bm25_document_input_t *documents)
{
    int32_t estimated = 0;
    int32_t i;

    for (i = 0; i < document_count; ++i) {
        if (documents[i].kind == BM25_INPUT_KIND_TOKENS) {
            estimated += documents[i].value.tokens.term_count;
        } else {
            /* 文本模式保守预估：平均每文档 50 个唯一 term。 */
            estimated += 50;
        }
    }
    return estimated < 64 ? 64 : estimated;
}

int32_t bm25_index_add_documents(bm25_t *index, int32_t document_count,
                                  const bm25_document_input_t *documents, int32_t *doc_ids)
{
    int32_t estimated_terms;
    int32_t saved_term_slot_count;
    int32_t saved_doc_count;
    int64_t saved_total_terms;
    int32_t i;

    if (!index || document_count < 0 || (document_count > 0 && !documents)) {
        return -1;
    }

    if (document_count == 0) {
        return 0;
    }

    /* 保存快照，用于失败回滚。 */
    saved_term_slot_count = index->term_slot_count;
    saved_doc_count = index->doc_count;
    saved_total_terms = index->total_terms;

    /* 预扩容：根据预估 term 数一次性扩容。 */
    estimated_terms = index->term_slot_count + bm25_estimate_unique_terms(document_count, documents);
    if (bm25_ensure_term_slot_capacity(index, estimated_terms) != 0 ||
        bm25_ensure_term_hash_table(index, estimated_terms) != 0 ||
        bm25_ensure_doc_capacity(index, index->doc_count + document_count) != 0) {
        return -1;
    }

    for (i = 0; i < document_count; ++i) {
        int32_t doc_id;

        if (documents[i].kind == BM25_INPUT_KIND_TOKENS) {
            doc_id = bm25_index_add_document(index,
                                              documents[i].value.tokens.term_count,
                                              documents[i].value.tokens.terms);
        } else if (documents[i].kind == BM25_INPUT_KIND_TEXT) {
            doc_id = bm25_index_add_text(index, documents[i].value.text);
        } else {
            goto rollback;
        }

        if (doc_id < 0) {
            goto rollback;
        }
        if (doc_ids) {
            doc_ids[i] = doc_id;
        }
    }

    /* 批量添加后统一重建所有 term_slot 的 posting filter。 */
    {
        int32_t slot_index;

        for (slot_index = 0; slot_index < index->term_slot_count; ++slot_index) {
            bm25_term_slot_t *slot = &index->term_slots[slot_index];

            if (slot->posting_count > 0) {
                bm25_ensure_posting_filters(index, slot);
            }
        }
    }

    return 0;

rollback:
    /* 撤销本次批量添加创建的 term_slot 和已分配的 docId。 */
    while (index->term_slot_count > saved_term_slot_count) {
        bm25_remove_term_slot(index, index->term_slot_count - 1);
    }
    /* 撤销已分配但尚未使用的 docId 计数。 */
    /* 注意：已成功插入的文档无法单独撤销 posting（复杂度太高），
     * 回滚要求调用方重新构建索引，这里只保证数据结构一致性。 */
    index->doc_count = saved_doc_count;
    index->total_terms = saved_total_terms;
    return -1;
}
