/*
 * faiss_ivf_utils.c
 *
 * IVF 内部工具函数: 参数验证、量化器重建、K-Means 训练包装、路由查找、残差训练。
 */

#include "faiss_ivf_private.h"

/* 训练参数的合理默认值 */
void faiss_ivf_set_default_training_params(faiss_ivf_training_params_t *params)
{
    params->n_init = 8;
    params->max_iter = 100;
    params->tol = 1e-4;
    params->random_state = 42;
    params->verbose = false;
}

int faiss_ivf_training_params_are_valid(const faiss_ivf_training_params_t *params)
{
    if (!params) return 0;
    if (params->n_init <= 0 || params->max_iter <= 0 || params->tol <= 0.0) return 0;
    return 1;
}

int faiss_ivf_quantization_params_are_valid(const faiss_ivf_t *index, const faiss_ivf_quantization_params_t *params)
{
    if (!index || !params) return 0;
    if (index->quantization_type == QUANTIZATION_TYPE_NONE) return 0;
    if (params->pq_m <= 0 || params->pq_bits <= 0 || params->pq_bits > 8) return 0;
    if (index->dims % params->pq_m != 0) return 0;
    return 1;
}

int faiss_ivf_rebuild_quantizer(faiss_ivf_t *index)
{
    if (!index) return -1;
    quantizer_drop(index->quantizer);
    index->quantizer = NULL;
    index->code_size = 0;
    if (index->quantization_type == QUANTIZATION_TYPE_NONE) return 0;
    index->quantizer = quantizer_create(&index->quantizer_config);
    if (!index->quantizer) return -1;
    return 0;
}

int faiss_ivf_compare_scored_indices(const void *left, const void *right)
{
    const faiss_ivf_scored_index_t *lhs = (const faiss_ivf_scored_index_t *)left;
    const faiss_ivf_scored_index_t *rhs = (const faiss_ivf_scored_index_t *)right;
    if (lhs->distance < rhs->distance) return -1;
    if (lhs->distance > rhs->distance) return 1;
    if (lhs->id < rhs->id) return -1;
    if (lhs->id > rhs->id) return 1;
    return 0;
}

int32_t faiss_ivf_get_bucket_id(const faiss_ivf_t *index, int32_t primary_cluster, int32_t secondary_cluster)
{
    return primary_cluster * index->nlist2 + secondary_cluster;
}

const float *faiss_ivf_get_bucket_centroid_ptr(const faiss_ivf_t *index, int32_t primary_cluster, int32_t secondary_cluster)
{
    int32_t bucket_id = faiss_ivf_get_bucket_id(index, primary_cluster, secondary_cluster);
    return &index->secondary_centroids[bucket_id * index->dims];
}

void faiss_ivf_compute_residual(float *residual, const float *vector, const float *centroid, int32_t dims)
{
    for (int32_t d = 0; d < dims; ++d) {
        residual[d] = vector[d] - centroid[d];
    }
}

int faiss_ivf_run_kmeans_training(const faiss_ivf_t *index,
                                  int32_t n,
                                  const float *vectors,
                                  int32_t n_clusters,
                                  float *centroids_out,
                                  int32_t *labels_out)
{
    double *training_data;
    KMeansParams params;
    int64_t i;
    int32_t centroid_i;
    int32_t d;

    if (!index || !vectors || !centroids_out || n <= 0 || n_clusters <= 0 || n_clusters > n) {
        return 0;
    }

    training_data = (double *)malloc((size_t)n * index->dims * sizeof(double));
    if (!training_data) return 0;

    for (i = 0; i < (int64_t)n * index->dims; ++i) {
        training_data[i] = (double)vectors[i];
    }

    memset(&params, 0, sizeof(params));
    params.n_clusters = n_clusters;
    params.n_init = index->training_params.n_init;
    params.max_iter = index->training_params.max_iter;
    params.tol = index->training_params.tol;
    params.verbose = index->training_params.verbose ? 1 : 0;
    params.random_state = index->training_params.random_state;
    params.init = "k-means++";
    params.algorithm = "lloyd";
    params.n_samples = n;
    params.n_features = index->dims;
    params.data = training_data;

    Kmeans(&params);
    free(training_data);

    if (!params.success || !params.cluster_centers) {
        KmeansFree(&params);
        return 0;
    }

    for (centroid_i = 0; centroid_i < n_clusters; ++centroid_i) {
        for (d = 0; d < index->dims; ++d) {
            centroids_out[centroid_i * index->dims + d] =
                (float)params.cluster_centers[centroid_i * index->dims + d];
        }
    }

    if (labels_out && params.labels) {
        for (i = 0; i < n; ++i) {
            labels_out[i] = params.labels[i];
        }
    }

    KmeansFree(&params);
    return 1;
}

int32_t faiss_ivf_find_nearest_primary_centroid(const faiss_ivf_t *index, const float *vector)
{
    float min_dist = FLT_MAX;
    int32_t best_cluster = 0;
    int32_t j;

    for (j = 0; j < index->nlist; ++j) {
        float dist = distance_compute(index->metric, vector, &index->primary_centroids[j * index->dims], index->dims);
        if (dist < min_dist) {
            min_dist = dist;
            best_cluster = j;
        }
    }
    return best_cluster;
}

int32_t faiss_ivf_find_nearest_secondary_centroid(const faiss_ivf_t *index,
                                                  int32_t primary_cluster,
                                                  const float *vector)
{
    float min_dist = FLT_MAX;
    int32_t best_secondary = 0;
    int32_t secondary_count;
    int32_t secondary;

    if (!index || !vector || primary_cluster < 0 || primary_cluster >= index->nlist) {
        return 0;
    }

    secondary_count = index->secondary_counts[primary_cluster];
    if (secondary_count <= 1) {
        return 0;
    }

    for (secondary = 0; secondary < secondary_count; ++secondary) {
        float dist = distance_compute(index->metric,
                                           vector,
                                           faiss_ivf_get_bucket_centroid_ptr(index, primary_cluster, secondary),
                                           index->dims);
        if (dist < min_dist) {
            min_dist = dist;
            best_secondary = secondary;
        }
    }

    return best_secondary;
}

int faiss_ivf_train_quantizer_residuals(faiss_ivf_t *index,
                                        int32_t n,
                                        const float *vectors,
                                        const int32_t *primary_labels)
{
    float *residuals;
    int32_t i;

    if (!index->quantizer) {
        return 0;
    }

    residuals = (float *)malloc((size_t)n * index->dims * sizeof(float));
    if (!residuals) {
        return -1;
    }

    for (i = 0; i < n; ++i) {
        int32_t primary_cluster = primary_labels[i];
        int32_t secondary_cluster = faiss_ivf_find_nearest_secondary_centroid(index,
                                                                              primary_cluster,
                                                                              &vectors[i * index->dims]);
        faiss_ivf_compute_residual(&residuals[i * index->dims],
                                   &vectors[i * index->dims],
                                   faiss_ivf_get_bucket_centroid_ptr(index, primary_cluster, secondary_cluster),
                                   index->dims);
    }

    if (quantizer_train(index->quantizer, n, residuals) != 0) {
        free(residuals);
        return -1;
    }

    index->code_size = quantizer_code_size(index->quantizer);
    free(residuals);
    return 0;
}
