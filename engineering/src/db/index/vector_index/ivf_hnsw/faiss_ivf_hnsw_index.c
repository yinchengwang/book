/*
 * HNSW-IVF 索引生命周期管理。
 */

#include "faiss_ivf_hnsw_private.h"

static void set_default_params(faiss_ivf_hnsw_params_t *params) {
    if (params->nlist <= 0) params->nlist = 100;
    if (params->nlist2 <= 0) params->nlist2 = 50;
    if (params->nprobe <= 0) params->nprobe = 10;
    if (params->max_assignments <= 0) params->max_assignments = FAISS_IVF_HNSW_DEFAULT_MAX_ASSIGNMENTS;
    if (params->M <= 0) params->M = FAISS_IVF_HNSW_DEFAULT_M;
    if (params->ef_construction <= 0) params->ef_construction = FAISS_IVF_HNSW_DEFAULT_EF_CONSTRUCTION;
    if (params->ef_search <= 0) params->ef_search = FAISS_IVF_HNSW_DEFAULT_EF_SEARCH;
    if (params->train_max_vectors <= 0) params->train_max_vectors = 4096;
    if (params->kmeans_max_iter <= 0) params->kmeans_max_iter = 25;
    if (params->kmeans_tol <= 0) params->kmeans_tol = 1e-4;
    if (params->random_state <= 0) params->random_state = 42;
}

static int params_are_valid(const faiss_ivf_hnsw_params_t *params) {
    if (!params) return 0;
    if (params->dims <= 0) return 0;
    if (params->nlist <= 0) return 0;
    if (params->nlist2 <= 0) return 0;
    if (params->nprobe <= 0) return 0;
    if (params->M <= 0) return 0;
    if (params->ef_construction <= 0) return 0;
    if (!distance_metric_is_valid(params->metric)) return 0;
    if (!quantization_type_is_valid(params->quantization_type)) return 0;
    return 1;
}

faiss_ivf_hnsw_t *faiss_ivf_hnsw_index_create(const faiss_ivf_hnsw_params_t *params_in) {
    faiss_ivf_hnsw_params_t default_params;
    memset(&default_params, 0, sizeof(default_params));
    set_default_params(&default_params);

    const faiss_ivf_hnsw_params_t *params = params_in ? params_in : &default_params;
    if (!params_are_valid(params)) return NULL;

    faiss_ivf_hnsw_t *index = (faiss_ivf_hnsw_t *)calloc(1, sizeof(faiss_ivf_hnsw_t));
    if (!index) return NULL;

    index->dims = params->dims;
    index->nlist = params->nlist;
    index->nlist2 = params->nlist2;
    index->nprobe = params->nprobe;
    index->max_assignments = params->max_assignments;
    index->M = params->M;
    index->ef_construction = params->ef_construction;
    index->ef_search = params->ef_search > 0 ? params->ef_search : FAISS_IVF_HNSW_DEFAULT_EF_SEARCH;
    index->metric = params->metric;
    index->quantization_type = params->quantization_type;
    index->total_bucket_count = params->nlist * params->nlist2;
    index->trained = false;
    index->n_total = 0;

    faiss_ivf_hnsw_set_default_training_params(&index->training_params);
    index->training_params.max_iter = params->kmeans_max_iter;
    index->training_params.tol = params->kmeans_tol;
    index->training_params.random_state = params->random_state;

    index->primary_centroids = (float *)malloc((size_t)params->nlist * params->dims * sizeof(float));
    index->secondary_centroids = (float *)calloc((size_t)index->total_bucket_count * params->dims, sizeof(float));
    index->secondary_counts = (int32_t *)calloc((size_t)params->nlist, sizeof(int32_t));

    if (!index->primary_centroids || !index->secondary_centroids || !index->secondary_counts) goto FAIL;

    index->invlists = faiss_inverted_lists_create((size_t)index->total_bucket_count, 0);
    if (!index->invlists) goto FAIL;

    faiss_direct_map_init(&index->direct_map, 1, 0);

    index->hnsw_levels = (int32_t *)calloc((size_t)params->nlist, sizeof(int32_t));
    index->hnsw_offsets = (int32_t *)calloc((size_t)params->nlist, sizeof(int32_t));
    index->hnsw_nbs = (int32_t *)malloc(1 * sizeof(int32_t));
    index->hnsw_nbs_size = 1;
    index->hnsw_entry_point = -1;
    index->hnsw_max_level = -1;

    if (!index->hnsw_levels || !index->hnsw_offsets || !index->hnsw_nbs) goto FAIL;
    index->hnsw_levels_size = (uint32_t)params->nlist;
    index->hnsw_offsets_size = (uint32_t)params->nlist;

    if (hnsw_set_default_probas(index) != 0) goto FAIL;

    index->assignments = (faiss_ivf_hnsw_assign_entry_t *)malloc(1 * sizeof(faiss_ivf_hnsw_assign_entry_t));
    if (!index->assignments) goto FAIL;

    if (params->quantization_type != QUANTIZATION_TYPE_NONE) {
        quantizer_config_init(&index->quantizer_config, params->dims, params->quantization_type);
        index->quantizer_config.pq_subquantizers = params->pq_m > 0 ? params->pq_m : 8;
        index->quantizer_config.pq_bits = params->pq_bits > 0 ? params->pq_bits : 8;
        index->quantization_params.pq_m = index->quantizer_config.pq_subquantizers;
        index->quantization_params.pq_bits = index->quantizer_config.pq_bits;
        index->quantizer = quantizer_create(&index->quantizer_config);
        if (!index->quantizer) goto FAIL;
    }

    return index;

FAIL:
    free(index->primary_centroids);
    free(index->secondary_centroids);
    free(index->secondary_counts);
    faiss_inverted_lists_drop(index->invlists);
    faiss_direct_map_drop(&index->direct_map);
    free(index->hnsw_levels);
    free(index->hnsw_offsets);
    free(index->hnsw_nbs);
    free(index->hnsw_assign_probas);
    free(index->hnsw_cum_nneighbor_per_level);
    free(index->assignments);
    quantizer_drop(index->quantizer);
    free(index);
    return NULL;
}

void faiss_ivf_hnsw_index_drop(faiss_ivf_hnsw_t *index) {
    if (!index) return;
    free(index->vectors);
    free(index->codes);
    free(index->primary_centroids);
    free(index->secondary_centroids);
    free(index->secondary_counts);
    quantizer_drop(index->quantizer);
    faiss_inverted_lists_drop(index->invlists);
    faiss_direct_map_drop(&index->direct_map);
    free(index->hnsw_levels);
    free(index->hnsw_offsets);
    free(index->hnsw_nbs);
    free(index->hnsw_assign_probas);
    free(index->hnsw_cum_nneighbor_per_level);
    free(index->assignments);
    free(index);
}

void faiss_ivf_hnsw_index_reset(faiss_ivf_hnsw_t *index) {
    if (!index) return;
    free(index->vectors);
    free(index->codes);
    index->vectors = NULL;
    index->codes = NULL;
    index->n_total = 0;
    index->code_size = 0;
    faiss_inverted_lists_reset(index->invlists);
    faiss_direct_map_clear(&index->direct_map);
    memset(index->secondary_counts, 0, (size_t)index->nlist * sizeof(int32_t));
    if (index->quantizer) quantizer_reset(index->quantizer);
    free(index->assignments);
    index->assignments = (faiss_ivf_hnsw_assign_entry_t *)malloc(1 * sizeof(faiss_ivf_hnsw_assign_entry_t));
}

bool faiss_ivf_hnsw_index_is_trained(const faiss_ivf_hnsw_t *index) { return index ? index->trained : false; }
int32_t faiss_ivf_hnsw_index_size(const faiss_ivf_hnsw_t *index) { return index ? index->n_total : -1; }
int32_t faiss_ivf_hnsw_index_nlist(const faiss_ivf_hnsw_t *index) { return index ? index->nlist : -1; }
quantization_type_t faiss_ivf_hnsw_index_quantization_type(const faiss_ivf_hnsw_t *index) { return index ? index->quantization_type : QUANTIZATION_TYPE_NONE; }

int32_t faiss_ivf_hnsw_index_memory_size(const faiss_ivf_hnsw_t *index) {
    if (!index) return -1;
    int32_t size = sizeof(faiss_ivf_hnsw_t);
    size += index->nlist * index->dims * sizeof(float);
    size += index->total_bucket_count * index->dims * sizeof(float);
    size += index->nlist * sizeof(int32_t);
    if (index->vectors) size += index->n_total * index->dims * sizeof(float);
    if (index->codes) size += index->n_total * index->code_size * sizeof(uint8_t);
    size += index->hnsw_levels_size * sizeof(int32_t);
    size += index->hnsw_offsets_size * sizeof(int32_t);
    size += index->hnsw_nbs_size * sizeof(int32_t);
    size += index->hnsw_assign_probas_size * sizeof(float);
    size += index->hnsw_cum_nneighbor_per_level_size * sizeof(int32_t);
    size += (int32_t)(index->n_total * index->max_assignments * sizeof(faiss_ivf_hnsw_assign_entry_t));
    return size;
}

int32_t faiss_ivf_hnsw_index_set_nprobe(faiss_ivf_hnsw_t *index, int32_t nprobe) { if (!index || nprobe <= 0) return -1; index->nprobe = nprobe; return 0; }
int32_t faiss_ivf_hnsw_index_get_nprobe(const faiss_ivf_hnsw_t *index) { return index ? index->nprobe : -1; }
int32_t faiss_ivf_hnsw_index_set_ef_search(faiss_ivf_hnsw_t *index, int32_t ef_search) { if (!index || ef_search <= 0) return -1; index->ef_search = ef_search; return 0; }
int32_t faiss_ivf_hnsw_index_get_ef_search(const faiss_ivf_hnsw_t *index) { return index ? index->ef_search : -1; }
int32_t faiss_ivf_hnsw_index_set_max_assignments(faiss_ivf_hnsw_t *index, int32_t max_assignments) { if (!index || max_assignments <= 0 || max_assignments > 100) return -1; index->max_assignments = max_assignments; return 0; }
int32_t faiss_ivf_hnsw_index_get_max_assignments(const faiss_ivf_hnsw_t *index) { return index ? index->max_assignments : -1; }

int32_t faiss_ivf_hnsw_index_set_quantization_params(faiss_ivf_hnsw_t *index, const faiss_ivf_hnsw_quantization_params_t *params) {
    if (!index || !faiss_ivf_hnsw_quantization_params_are_valid(index, params)) return -1;
    if (index->trained || index->n_total > 0) return -1;
    index->quantization_params = *params;
    index->quantizer_config.pq_subquantizers = params->pq_m;
    index->quantizer_config.pq_bits = params->pq_bits;
    if (!quantizer_config_validate(&index->quantizer_config)) return -1;
    return faiss_ivf_hnsw_rebuild_quantizer(index);
}

int32_t faiss_ivf_hnsw_index_get_quantization_params(faiss_ivf_hnsw_t *index, faiss_ivf_hnsw_quantization_params_t *params) {
    if (!index || !params || index->quantization_type == QUANTIZATION_TYPE_NONE) return -1;
    *params = index->quantization_params; return 0;
}

int32_t faiss_ivf_hnsw_index_set_training_params(faiss_ivf_hnsw_t *index, const faiss_ivf_hnsw_training_params_t *params) {
    if (!index || !faiss_ivf_hnsw_training_params_are_valid(params)) return -1;
    index->training_params = *params; return 0;
}

int32_t faiss_ivf_hnsw_index_get_training_params(const faiss_ivf_hnsw_t *index, faiss_ivf_hnsw_training_params_t *params) {
    if (!index || !params) return -1;
    *params = index->training_params; return 0;
}
