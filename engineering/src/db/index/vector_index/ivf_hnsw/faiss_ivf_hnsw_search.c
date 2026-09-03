/*
 * HNSW-IVF 搜索实现。
 */

#include "faiss_ivf_hnsw_private.h"

static inline void heap_push(int32_t k, int32_t *result_ids, float *result_dists,
                            int32_t *num_results, int32_t vec_id, float dist) {
    if (dist < result_dists[0] || *num_results < k) {
        if (*num_results < k) {
            result_ids[*num_results] = vec_id;
            result_dists[*num_results] = dist;
            (*num_results)++;
            for (int32_t p = *num_results - 1; p > 0; p--) {
                if (result_dists[p] > result_dists[p - 1]) {
                    float tmp_d = result_dists[p]; result_dists[p] = result_dists[p - 1]; result_dists[p - 1] = tmp_d;
                    int32_t tmp_id = result_ids[p]; result_ids[p] = result_ids[p - 1]; result_ids[p - 1] = tmp_id;
                }
            }
        } else {
            result_ids[0] = vec_id;
            result_dists[0] = dist;
            for (int32_t p = 1; p < k; p++) {
                if (result_dists[p] > result_dists[0]) {
                    float tmp_d = result_dists[0]; result_dists[0] = result_dists[p]; result_dists[p] = tmp_d;
                    int32_t tmp_id = result_ids[0]; result_ids[0] = result_ids[p]; result_ids[p] = tmp_id;
                }
            }
        }
    }
}

int32_t faiss_ivf_hnsw_index_search_with_count(faiss_ivf_hnsw_t *index, const float *query,
                                                 int32_t k, float *distances, int32_t *labels,
                                                 int32_t *hit_count) {
    if (!index || !index->trained || !query || k <= 0 || !distances || !labels || !hit_count) return -1;
    *hit_count = 0;

    if (index->n_total <= 0) {
        for (int32_t i = 0; i < k; i++) { distances[i] = FLT_MAX; labels[i] = -1; }
        return 0;
    }

    faiss_ivf_hnsw_scored_index_t *primary_clusters =
        (faiss_ivf_hnsw_scored_index_t *)malloc((size_t)index->nlist * sizeof(faiss_ivf_hnsw_scored_index_t));
    if (!primary_clusters) return -1;

    int32_t nprobe = index->nprobe;
    if (nprobe > index->nlist) nprobe = index->nlist;

    if (faiss_ivf_hnsw_coarse_search(index, query, nprobe, primary_clusters) != 0) {
        for (int32_t i = 0; i < index->nlist; i++) {
            primary_clusters[i].id = i;
            primary_clusters[i].distance = distance_compute(index->metric, query,
                &index->primary_centroids[(size_t)i * index->dims], index->dims);
        }
        qsort(primary_clusters, (size_t)index->nlist, sizeof(faiss_ivf_hnsw_scored_index_t),
              faiss_ivf_hnsw_compare_scored_indices);
    }

    int32_t max_buckets = nprobe * index->nlist2;
    faiss_ivf_hnsw_scored_index_t *bucket_queue =
        (faiss_ivf_hnsw_scored_index_t *)malloc((size_t)max_buckets * sizeof(faiss_ivf_hnsw_scored_index_t));
    if (!bucket_queue) { free(primary_clusters); return -1; }

    int32_t bucket_count = 0;
    for (int32_t p = 0; p < nprobe; p++) {
        int32_t cluster = primary_clusters[p].id;
        int32_t secondary_count = index->secondary_counts[cluster];
        if (secondary_count <= 0) continue;
        for (int32_t s = 0; s < secondary_count; s++) {
            int32_t bucket_id = faiss_ivf_hnsw_get_bucket_id(index, cluster, s);
            float centroid_dist = distance_compute(index->metric, query,
                faiss_ivf_hnsw_get_bucket_centroid_ptr(index, cluster, s), index->dims);
            bucket_queue[bucket_count].id = bucket_id;
            bucket_queue[bucket_count].distance = centroid_dist;
            bucket_count++;
        }
    }

    qsort(bucket_queue, (size_t)bucket_count, sizeof(faiss_ivf_hnsw_scored_index_t),
          faiss_ivf_hnsw_compare_scored_indices);
    free(primary_clusters);

    int32_t *expanded_buckets = (int32_t *)malloc((size_t)bucket_count * sizeof(int32_t));
    int32_t expanded_count = 0;
    if (expanded_buckets) {
        for (int32_t i = 0; i < bucket_count; i++) expanded_buckets[expanded_count++] = bucket_queue[i].id;
    }

    int32_t num_results = 0;
    int32_t *result_ids = (int32_t *)malloc((size_t)k * sizeof(int32_t));
    float *result_dists = (float *)malloc((size_t)k * sizeof(float));
    if (!result_ids || !result_dists) {
        free(result_ids); free(result_dists); free(expanded_buckets); return -1;
    }
    for (int32_t i = 0; i < k; i++) { result_ids[i] = -1; result_dists[i] = FLT_MAX; }

    float *query_residual = NULL;
    float *distance_table = NULL;
    if (index->quantizer) {
        query_residual = (float *)malloc((size_t)index->dims * sizeof(float));
        distance_table = (float *)malloc((size_t)quantizer_distance_table_size(index->quantizer) * sizeof(float));
        if (!query_residual || !distance_table) {
            free(query_residual); free(distance_table); free(result_ids); free(result_dists); free(expanded_buckets); return -1;
        }
    }

    bool *seen = (bool *)calloc((size_t)index->n_total, sizeof(bool));

    for (int32_t bi = 0; bi < expanded_count; bi++) {
        int32_t bucket_id = expanded_buckets[bi];
        if (index->quantizer) {
            int32_t primary_cluster = bucket_id / index->nlist2;
            int32_t secondary = bucket_id % index->nlist2;
            faiss_ivf_hnsw_compute_residual(query_residual, query,
                faiss_ivf_hnsw_get_bucket_centroid_ptr(index, primary_cluster, secondary), index->dims);
            quantizer_compute_distance_table(index->quantizer, index->metric, query_residual, distance_table);
        }

        size_t list_size = faiss_inverted_lists_list_size(index->invlists, (size_t)bucket_id);
        const int32_t *list_ids = faiss_inverted_lists_get_ids(index->invlists, (size_t)bucket_id);

        for (size_t idx = 0; idx < list_size; idx++) {
            int32_t vec_id = list_ids[idx];
            if (vec_id < 0) continue;
            if (seen && seen[vec_id]) continue;

            float dist;
            if (index->quantizer) {
                dist = quantizer_adc_distance(index->quantizer, &index->codes[(size_t)vec_id * index->code_size], distance_table);
            } else {
                dist = distance_compute_from_query(index->metric, query, index->vectors, index->dims, vec_id);
            }

            heap_push(k, result_ids, result_dists, &num_results, vec_id, dist);
            if (seen) seen[vec_id] = true;
        }
    }

    *hit_count = num_results;
    for (int32_t out = 0; out < num_results; out++) {
        int32_t best_idx = out;
        for (int32_t j = out + 1; j < num_results; j++) {
            if (result_dists[j] < result_dists[best_idx]) best_idx = j;
        }
        if (best_idx != out) {
            float tmp_d = result_dists[out]; result_dists[out] = result_dists[best_idx]; result_dists[best_idx] = tmp_d;
            int32_t tmp_id = result_ids[out]; result_ids[out] = result_ids[best_idx]; result_ids[best_idx] = tmp_id;
        }
    }

    for (int32_t i = 0; i < num_results; i++) { distances[i] = result_dists[i]; labels[i] = result_ids[i]; }
    for (int32_t i = num_results; i < k; i++) { distances[i] = FLT_MAX; labels[i] = -1; }

    if (seen) free(seen);
    free(query_residual); free(distance_table); free(result_ids); free(result_dists); free(expanded_buckets);
    return 0;
}

int32_t faiss_ivf_hnsw_index_search(faiss_ivf_hnsw_t *index, const float *query,
                                      int32_t k, float *distances, int32_t *labels) {
    int32_t hit_count;
    return faiss_ivf_hnsw_index_search_with_count(index, query, k, distances, labels, &hit_count);
}
