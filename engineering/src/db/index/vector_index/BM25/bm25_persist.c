#include "bm25_private.h"

#include <stdio.h>

#define BM25_FILE_MAGIC   0x35324D42   /* "BM25" 小端序。 */
#define BM25_FILE_VERSION 1

/* TODO: CRC32 文件完整性校验，后续版本实现。 */

/* 安全读写工具。 */
static int bm25_fwrite(const void *data, size_t size, size_t count, FILE *fp)
{
    if (!fp || !data || size == 0 || count == 0) {
        return 0;
    }
    return fwrite(data, size, count, fp) == count ? 0 : -1;
}

static int bm25_fread(void *data, size_t size, size_t count, FILE *fp)
{
    if (!fp || !data || size == 0 || count == 0) {
        return 0;
    }
    return fread(data, size, count, fp) == count ? 0 : -1;
}

/* 按 term 逐个写入（term 数据 + postings + filters 连续存放），加载时按顺序恢复。 */
int32_t bm25_save(const bm25_t *index, const char *path)
{
    FILE   *fp;
    int32_t slot_index;
    int32_t doc_index;

    if (!index || !path) {
        return -1;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        return -1;
    }

    /* ── Header ── */
    if (bm25_fwrite(&(int32_t){BM25_FILE_MAGIC}, sizeof(int32_t), 1, fp) != 0 ||
        bm25_fwrite(&(int32_t){BM25_FILE_VERSION}, sizeof(int32_t), 1, fp) != 0 ||
        bm25_fwrite(&index->params.k1, sizeof(float), 1, fp) != 0 ||
        bm25_fwrite(&index->params.b, sizeof(float), 1, fp) != 0 ||
        bm25_fwrite(&index->term_slot_count, sizeof(int32_t), 1, fp) != 0 ||
        bm25_fwrite(&index->doc_count, sizeof(int32_t), 1, fp) != 0 ||
        bm25_fwrite(&index->total_terms, sizeof(int64_t), 1, fp) != 0 ||
        bm25_fwrite(&index->free_doc_count, sizeof(int32_t), 1, fp) != 0) {
        fclose(fp);
        return -1;
    }

    /* ── Terms + Postings (每 term 一块) ── */
    for (slot_index = 0; slot_index < index->term_slot_count; ++slot_index) {
        const bm25_term_slot_t *slot = &index->term_slots[slot_index];
        uint16_t              term_len;
        int32_t               posting_index;

        if (!slot->term) {
            term_len = 0;
            bm25_fwrite(&term_len, sizeof(uint16_t), 1, fp);
            bm25_fwrite(&(int32_t){0}, sizeof(int32_t), 1, fp); /* doc_freq */
            bm25_fwrite(&(int32_t){0}, sizeof(int32_t), 1, fp); /* total_term_freq */
            bm25_fwrite(&(uint64_t){0}, sizeof(uint64_t), 1, fp); /* hash */
            bm25_fwrite(&(int32_t){0}, sizeof(int32_t), 1, fp); /* posting_count */
            continue;
        }

        term_len = (uint16_t)strlen(slot->term);
        bm25_fwrite(&term_len, sizeof(uint16_t), 1, fp);
        bm25_fwrite(slot->term, 1, term_len, fp);
        bm25_fwrite(&slot->doc_freq, sizeof(int32_t), 1, fp);
        bm25_fwrite(&slot->total_term_freq, sizeof(int32_t), 1, fp);
        bm25_fwrite(&slot->hash_value, sizeof(uint64_t), 1, fp);
        bm25_fwrite(&slot->posting_count, sizeof(int32_t), 1, fp);

        for (posting_index = 0; posting_index < slot->posting_count; ++posting_index) {
            bm25_fwrite(&slot->postings[posting_index].doc_id, sizeof(int32_t), 1, fp);
            bm25_fwrite(&slot->postings[posting_index].term_freq, sizeof(int32_t), 1, fp);
        }
    }

    /* ── Doc长度 ── */
    for (doc_index = 0; doc_index < index->doc_count; ++doc_index) {
        bm25_fwrite(&index->doc_lengths[doc_index], sizeof(int32_t), 1, fp);
    }

    /* ── 正排索引 ── */
    for (doc_index = 0; doc_index < index->doc_count; ++doc_index) {
        const bm25_doc_forward_list_t *fw = &index->doc_forwards[doc_index];

        bm25_fwrite(&fw->count, sizeof(int32_t), 1, fp);
        if (fw->count > 0) {
            int32_t item_index;

            for (item_index = 0; item_index < fw->count; ++item_index) {
                bm25_fwrite(&fw->items[item_index].term_slot_index, sizeof(int32_t), 1, fp);
                bm25_fwrite(&fw->items[item_index].token_hash, sizeof(uint64_t), 1, fp);
            }
        }
    }

    /* ── DocId 复用池 ── */
    for (doc_index = 0; doc_index < index->free_doc_count; ++doc_index) {
        bm25_fwrite(&index->free_doc_ids[doc_index], sizeof(int32_t), 1, fp);
    }

    fclose(fp);
    return 0;
}

bm25_t *bm25_index_load(const char *path)
{
    FILE   *fp;
    bm25_t *index;
    int32_t magic;
    int32_t version;
    int32_t term_slot_count;
    int32_t doc_count;
    int64_t total_terms;
    int32_t free_doc_count;
    int32_t slot_index;
    int32_t doc_index;
    bm25_params_t params;

    if (!path) {
        return NULL;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    /* ── Header ── */
    bm25_set_default_params(&params);
    if (bm25_fread(&magic, sizeof(int32_t), 1, fp) != 0 || magic != (int32_t)BM25_FILE_MAGIC ||
        bm25_fread(&version, sizeof(int32_t), 1, fp) != 0 || version != BM25_FILE_VERSION ||
        bm25_fread(&params.k1, sizeof(float), 1, fp) != 0 ||
        bm25_fread(&params.b, sizeof(float), 1, fp) != 0 ||
        bm25_fread(&term_slot_count, sizeof(int32_t), 1, fp) != 0 ||
        bm25_fread(&doc_count, sizeof(int32_t), 1, fp) != 0 ||
        bm25_fread(&total_terms, sizeof(int64_t), 1, fp) != 0 ||
        bm25_fread(&free_doc_count, sizeof(int32_t), 1, fp) != 0) {
        fclose(fp);
        return NULL;
    }

    index = bm25_index_create_with_params(&params);
    if (!index) {
        fclose(fp);
        return NULL;
    }

    /* ── Terms + Postings ── */
    {
        char     buffer[256];
        uint16_t term_len;
        int32_t  doc_freq;
        int32_t  total_term_freq;
        uint64_t hash_value;
        int32_t  posting_count;
        int32_t  posting_index;

        for (slot_index = 0; slot_index < term_slot_count; ++slot_index) {
            if (bm25_fread(&term_len, sizeof(uint16_t), 1, fp) != 0) {
                bm25_index_drop(index);
                fclose(fp);
                return NULL;
            }

            if (term_len == 0) {
                bm25_fread(&doc_freq, sizeof(int32_t), 1, fp);
                bm25_fread(&total_term_freq, sizeof(int32_t), 1, fp);
                bm25_fread(&hash_value, sizeof(uint64_t), 1, fp);
                bm25_fread(&posting_count, sizeof(int32_t), 1, fp);
                continue;
            }

            if (term_len >= (int32_t)sizeof(buffer) ||
                bm25_fread(buffer, 1, term_len, fp) != 0) {
                bm25_index_drop(index);
                fclose(fp);
                return NULL;
            }
            buffer[term_len] = '\0';

            if (bm25_fread(&doc_freq, sizeof(int32_t), 1, fp) != 0 ||
                bm25_fread(&total_term_freq, sizeof(int32_t), 1, fp) != 0 ||
                bm25_fread(&hash_value, sizeof(uint64_t), 1, fp) != 0 ||
                bm25_fread(&posting_count, sizeof(int32_t), 1, fp) != 0) {
                bm25_index_drop(index);
                fclose(fp);
                return NULL;
            }

            {
                int32_t inserted = bm25_insert_term_slot(index, buffer, hash_value);
                if (inserted < 0) {
                    bm25_index_drop(index);
                    fclose(fp);
                    return NULL;
                }

            {
                bm25_term_slot_t *slot = bm25_get_term_slot(index, inserted);

                slot->doc_freq        = doc_freq;
                slot->total_term_freq = total_term_freq;

                if (posting_count > 0) {
                    if (bm25_reserve_postings(slot, posting_count) != 0) {
                        bm25_index_drop(index);
                        fclose(fp);
                        return NULL;
                    }

                    for (posting_index = 0; posting_index < posting_count; ++posting_index) {
                        if (bm25_fread(&slot->postings[posting_index].doc_id,
                                       sizeof(int32_t), 1, fp) != 0 ||
                            bm25_fread(&slot->postings[posting_index].term_freq,
                                       sizeof(int32_t), 1, fp) != 0) {
                            bm25_index_drop(index);
                            fclose(fp);
                            return NULL;
                        }
                    }
                    slot->posting_count = posting_count;
                    /* 标记 dirty，延迟到搜索时重建。
                     * 此时 index->doc_count 尚未设置，立即重建会导致 IDF 全为 0。 */
                    slot->posting_filter_count = -1;
                }
            }
            }
        }
    }

    /* ── Doc 长度 + 正排索引 ── */
    if (doc_count > 0) {
        if (bm25_ensure_doc_capacity(index, doc_count) != 0) {
            bm25_index_drop(index);
            fclose(fp);
            return NULL;
        }

        for (doc_index = 0; doc_index < doc_count; ++doc_index) {
            if (bm25_fread(&index->doc_lengths[doc_index], sizeof(int32_t), 1, fp) != 0) {
                bm25_index_drop(index);
                fclose(fp);
                return NULL;
            }
        }

        for (doc_index = 0; doc_index < doc_count; ++doc_index) {
            int32_t fw_count;

            if (bm25_fread(&fw_count, sizeof(int32_t), 1, fp) != 0) {
                bm25_index_drop(index);
                fclose(fp);
                return NULL;
            }

            if (fw_count > 0) {
                int32_t item_index;

                for (item_index = 0; item_index < fw_count; ++item_index) {
                    int32_t  fw_slot;
                    uint64_t fw_hash;

                    if (bm25_fread(&fw_slot, sizeof(int32_t), 1, fp) != 0 ||
                        bm25_fread(&fw_hash, sizeof(uint64_t), 1, fp) != 0) {
                        bm25_index_drop(index);
                        fclose(fp);
                        return NULL;
                    }
                    bm25_doc_forward_add_token(index, doc_index, fw_slot, fw_hash);
                }
            }
        }
    }

    index->doc_count  = doc_count;
    index->total_terms = total_terms;


    /* ── DocId 复用池 ── */
    if (free_doc_count > 0) {
        int32_t i;

        for (i = 0; i < free_doc_count; ++i) {
            int32_t free_id;

            if (bm25_fread(&free_id, sizeof(int32_t), 1, fp) != 0) {
                bm25_index_drop(index);
                fclose(fp);
                return NULL;
            }
            bm25_free_doc_id(index, free_id);
        }
    }

    fclose(fp);
    return index;
}
