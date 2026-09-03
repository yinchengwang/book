#include "kbase_search.h"
#include "infra/infra.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* HNSW 搜索函数指针（已废弃，改用直接调用） */
typedef int32_t (*hnsw_search_fn)(void*, const float*, int32_t, int32_t, float*, int32_t*);

kbase_result_t* kbase_search(
    kbase_index_t* idx, infra_model_t* model,
    const char* query, int top_k, int* num_results)
{
    if (!idx || !model || !query || !num_results) return NULL;
    if (!idx->hnsw || top_k <= 0 || idx->num_notes == 0) {
        *num_results = 0;
        return NULL;
    }
    if (top_k > idx->num_notes) top_k = idx->num_notes;

    /* 1. Embedding 查询文本 */
    const char* texts[1] = { query };
    float* q_emb = NULL;
    int dim = 0;
    if (infra_embed(model, texts, 1, &q_emb, &dim) != INFRA_OK) {
        *num_results = 0;
        return NULL;
    }

    /* 2. HNSW 搜索 */
    float* dists = (float*)calloc((size_t)top_k, sizeof(float));
    int32_t* ids = (int32_t*)calloc((size_t)top_k, sizeof(int32_t));
    if (!dists || !ids) {
        free(dists); free(ids);
        infra_embed_free(q_emb, 1);
        *num_results = 0;
        return NULL;
    }

    faiss_hnsw_t* hnsw = (faiss_hnsw_t*)idx->hnsw;
    int32_t found = faiss_hnsw_index_search(hnsw, q_emb, top_k, 50, dists, ids);

    /* 3. 构建结果 */
    kbase_result_t* results = (kbase_result_t*)calloc((size_t)top_k, sizeof(kbase_result_t));
    if (!results) {
        free(dists); free(ids); infra_embed_free(q_emb, 1);
        *num_results = 0; return NULL;
    }

    int n = 0;
    for (int i = 0; i < found && i < top_k; i++) {
        if (ids[i] >= 0 && ids[i] < idx->num_notes) {
            results[n].note = &idx->notes[ids[i]];
            results[n].score = dists[i];
            results[n].rank = n;
            n++;
        }
    }

    infra_embed_free(q_emb, 1);
    free(dists); free(ids);
    *num_results = n;
    return results;
}

void kbase_search_free(kbase_result_t* results, int num_results) {
    (void)num_results;
    free(results);
}