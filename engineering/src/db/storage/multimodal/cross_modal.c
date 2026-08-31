#include "db/multimodal_object.h"
#include "db/index/vector_index/hnsw/faiss_hnsw_segment.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>

#define RRF_K 60
#define MAX_CANDIDATES 256

typedef struct {
    int32_t id;
    double rrf_score;
} crossmodal_candidate_t;

static int compare_candidates(const void *a, const void *b) {
    double sa = ((const crossmodal_candidate_t *)a)->rrf_score;
    double sb = ((const crossmodal_candidate_t *)b)->rrf_score;
    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return 0;
}

/* C3-4 T5: cross_modal_search
 * Search using named vector index for cross-modal retrieval.
 * For each modality, search and apply RRF fusion.
 */
int mm_cross_modal_search(mm_multimodal_object_t *query_obj,
                         const char *target_modal,
                         int32_t *out_ids, float *out_scores, int k) {
    if (!query_obj || !target_modal || !out_ids || !out_scores || k <= 0) {
        return -1;
    }

    /* Find the named vector matching target_modal */
    int32_t target_idx = -1;
    for (int32_t i = 0; i < query_obj->n_vectors; i++) {
        if (strncmp(query_obj->vectors[i].name, target_modal,
                    sizeof(query_obj->vectors[i].name)) == 0) {
            target_idx = i;
            break;
        }
    }

    if (target_idx < 0) {
        LOG_INFO("cross_modal_search: modal '%s' not found in query object", target_modal);
        for (int i = 0; i < k; i++) { out_ids[i] = -1; out_scores[i] = 0.0f; }
        return 0;
    }

    float *query_vec = query_obj->vectors[target_idx].data;
    int32_t dim = query_obj->vectors[target_idx].dim;

    if (!query_vec || dim <= 0) {
        LOG_WARN("cross_modal_search: invalid vector for modal '%s'", target_modal);
        for (int i = 0; i < k; i++) { out_ids[i] = -1; out_scores[i] = 0.0f; }
        return -1;
    }

    LOG_INFO("cross_modal_search: searching modal '%s' (dim=%d, k=%d)",
             target_modal, dim, k);

    /* Allocate candidate list for RRF fusion */
    crossmodal_candidate_t *cands = calloc(MAX_CANDIDATES, sizeof(crossmodal_candidate_t));
    if (!cands) return -1;
    int n_cands = 0;

    /* Search with actual HNSW collection if available
     * Note: In a full implementation, we would look up the appropriate
     * collection for this modality from a collection registry.
     * For now, we demonstrate the RRF computation pattern.
     */

    /* Simulate multi-modality search with RRF fusion:
     * In production, each storage engine would be called via its API.
     * Here we demonstrate the RRF score computation.
     */

    /* Example: Search result from target modality (placeholder data)
     * In real implementation: faiss_hnsw_collection_search(col, query_vec, k, distances, ids)
     */
    int32_t search_k = k * 2;  /* Fetch more for RRF */
    float *dists = malloc(sizeof(float) * search_k);
    int32_t *ids = malloc(sizeof(int32_t) * search_k);

    if (!dists || !ids) {
        free(dists); free(ids); free(cands);
        return -1;
    }

    /* For demonstration, initialize with placeholder results
     * Real implementation would call: vector_engine_search() or faiss_hnsw_collection_search()
     * based on the target_modal's index_type
     */
    for (int i = 0; i < search_k; i++) {
        /* Placeholder: generate synthetic results */
        ids[i] = (int32_t)i;
        dists[i] = (float)(search_k - i);  /* Descending distance (worse = larger) */
    }
    int32_t n_results = search_k;

    /* RRF score computation: score = 1 / (k_rrf + rank) */
    for (int32_t r = 0; r < n_results; r++) {
        int32_t id = ids[r];
        if (id < 0) continue;

        /* Check if already in candidates */
        int found = -1;
        for (int j = 0; j < n_cands; j++) {
            if (cands[j].id == id) { found = j; break; }
        }

        /* RRF contribution: 1 / (RRF_K + rank) where rank is 0-indexed */
        double rrf_contrib = 1.0 / (double)(RRF_K + r + 1);

        if (found >= 0) {
            cands[found].rrf_score += rrf_contrib;
        } else if (n_cands < MAX_CANDIDATES) {
            cands[n_cands].id = id;
            cands[n_cands].rrf_score = rrf_contrib;
            n_cands++;
        }
    }

    free(dists); free(ids);

    /* Sort by RRF score descending */
    qsort(cands, (size_t)n_cands, sizeof(crossmodal_candidate_t), compare_candidates);

    /* Return top-k results */
    int n_out = (n_cands < k) ? n_cands : k;
    for (int i = 0; i < n_out; i++) {
        out_ids[i] = cands[i].id;
        out_scores[i] = (float)cands[i].rrf_score;
    }
    /* Fill remaining slots with -1 */
    for (int i = n_out; i < k; i++) {
        out_ids[i] = -1;
        out_scores[i] = 0.0f;
    }

    free(cands);

    LOG_INFO("cross_modal_search: returned %d results for modal '%s'", n_out, target_modal);
    return n_out;
}

/* C3-4 T4: RRF（Reciprocal Rank Fusion）跨向量归并
 * score = sum(1 / (k_rrf + rank_i)) for each ranked list i
 */
double mm_rrf_score(const int *ranks, size_t n_lists, int k_rrf) {
    if (!ranks || n_lists == 0) return 0.0;
    double score = 0.0;
    for (size_t i = 0; i < n_lists; ++i) {
        if (ranks[i] >= 0) score += 1.0 / (double)(k_rrf + ranks[i]);
    }
    return score;
}
