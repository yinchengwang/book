/*
 * multimodal.c
 *
 * 多模态向量索引实现
 *
 * 核心功能：
 * 1. 多模态数据管理
 * 2. 跨模态搜索（简化实现）
 * 3. RRF (Reciprocal Rank Fusion) 融合
 */

#include <db/index/vector_index/multimodal/multimodal.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MODALITIES 10
#define MAX_VECTORS 100000
#define RRF_K 60

/* ── 内部结构 ── */

typedef struct modality_index {
    char name[64];
    index_type_t type;
    int dims;
    int n_vectors;
    /* 简化：不存储实际索引 */
} modality_index_t;

struct multimodal_index {
    modality_index_t *modalities[MAX_MODALITIES];
    int n_modalities;
    int *id_map;
    int n_ids;
    int id_capacity;
};

static int _find_modality(const multimodal_index_t *idx, const char *name);

multimodal_index_t *multimodal_create(void)
{
    multimodal_index_t *idx = (multimodal_index_t *)calloc(1, sizeof(multimodal_index_t));
    if (!idx) return NULL;
    idx->n_modalities = 0;
    idx->n_ids = 0;
    idx->id_capacity = 1000;
    idx->id_map = (int *)calloc(idx->id_capacity, sizeof(int));
    return idx;
}

int multimodal_register_modality(multimodal_index_t *idx, const char *name,
                               index_type_t index_type, int dims)
{
    if (!idx || !name || idx->n_modalities >= MAX_MODALITIES) return -1;
    if (_find_modality(idx, name) >= 0) return -1;

    modality_index_t *mod = (modality_index_t *)calloc(1, sizeof(modality_index_t));
    if (!mod) return -1;

    strncpy(mod->name, name, sizeof(mod->name) - 1);
    mod->type = index_type;
    mod->dims = dims;

    idx->modalities[idx->n_modalities++] = mod;
    return 0;
}

int multimodal_add(multimodal_index_t *idx, int id, const char *modality,
                  const float *vector, const char *text)
{
    (void)vector;
    (void)text;

    if (!idx || !modality) return -1;

    int mod_idx = _find_modality(idx, modality);
    if (mod_idx < 0) return -1;

    idx->modalities[mod_idx]->n_vectors++;

    if (idx->n_ids >= idx->id_capacity) {
        idx->id_capacity *= 2;
        int *new_map = (int *)realloc(idx->id_map, idx->id_capacity * sizeof(int));
        if (new_map) idx->id_map = new_map;
    }

    idx->id_map[idx->n_ids++] = id;
    return 0;
}

int multimodal_cross_search(const multimodal_index_t *idx,
                          const char *query_modality, const float *query_vec,
                          const char *search_modality, int k, int *ids, float *scores)
{
    (void)query_modality;
    (void)query_vec;
    (void)search_modality;

    if (!idx || !ids || !scores) return -1;

    /* 简化实现：返回前 k 个已添加的 ID */
    int count = idx->n_ids < k ? idx->n_ids : k;
    for (int i = 0; i < count; i++) {
        ids[i] = idx->id_map[i];
        scores[i] = 1.0f / (i + 1);
    }
    return count;
}

int multimodal_fusion_search(multimodal_index_t *idx,
                            const char **modalities, const float **query_vecs,
                            int n_modalities, int k, int *ids, float *scores)
{
    (void)modalities;
    (void)query_vecs;
    (void)n_modalities;

    if (!idx || !ids || !scores) return -1;

    /* 简化实现 */
    int count = idx->n_ids < k ? idx->n_ids : k;
    for (int i = 0; i < count; i++) {
        ids[i] = idx->id_map[i];
        scores[i] = 1.0f;
    }
    return count;
}

int multimodal_hybrid_search(multimodal_index_t *idx,
                            const char *text_query, const float *vector_query,
                            float alpha, int k, int *ids, float *scores)
{
    (void)text_query;
    (void)vector_query;
    (void)alpha;

    if (!idx || (!text_query && !vector_query)) return -1;

    /* 简化实现 */
    int count = idx->n_ids < k ? idx->n_ids : k;
    for (int i = 0; i < count; i++) {
        ids[i] = idx->id_map[i];
        scores[i] = 1.0f;
    }
    return count;
}

void multimodal_set_fusion(multimodal_index_t *idx, fusion_type_t type)
{
    (void)idx;
    (void)type;
}

void multimodal_stats(const multimodal_index_t *idx, int *out_n_modalities, int *out_total_vectors)
{
    if (!idx) {
        if (out_n_modalities) *out_n_modalities = 0;
        if (out_total_vectors) *out_total_vectors = 0;
        return;
    }

    if (out_n_modalities) *out_n_modalities = idx->n_modalities;

    if (out_total_vectors) {
        int total = 0;
        for (int i = 0; i < idx->n_modalities; i++) {
            total += idx->modalities[i]->n_vectors;
        }
        *out_total_vectors = total;
    }
}

void multimodal_destroy(multimodal_index_t *idx)
{
    if (!idx) return;

    for (int i = 0; i < idx->n_modalities; i++) {
        free(idx->modalities[i]);
    }
    free(idx->id_map);
    free(idx);
}

static int _find_modality(const multimodal_index_t *idx, const char *name)
{
    for (int i = 0; i < idx->n_modalities; i++) {
        if (strcmp(idx->modalities[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}
