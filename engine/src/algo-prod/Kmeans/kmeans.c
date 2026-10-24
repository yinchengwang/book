#include "algo-prod/Kmeans/kmeans.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>

static unsigned int kmeans_seed(int random_state)
{
    if (random_state >= 0) {
        return (unsigned int)random_state;
    }
    return 5489u;
}

static unsigned int kmeans_rand(unsigned int *seed)
{
    unsigned int x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;
    return x;
}

static int kmeans_rand_int(unsigned int *seed, int upper)
{
    if (upper <= 0) {
        return 0;
    }
    return (int)(kmeans_rand(seed) % (unsigned int)upper);
}

static double squared_distance(const double *x, const double *y, int n_features)
{
    double result = 0.0;
    int i;
    for (i = 0; i < n_features; ++i) {
        double diff = x[i] - y[i];
        result += diff * diff;
    }
    return result;
}

static void copy_centers(double *dest, const double *src, int n_clusters, int n_features)
{
    memcpy(dest, src, sizeof(double) * (size_t)n_clusters * (size_t)n_features);
}

static void initialize_random_centers(KMeansParams *params, double *centers, unsigned int *seed)
{
    int i;
    int first = kmeans_rand_int(seed, params->n_samples);
    int *chosen = (int *)malloc(sizeof(int) * params->n_clusters);
    if (!chosen) {
        return;
    }

    for (i = 0; i < params->n_clusters; ++i) {
        chosen[i] = -1;
    }

    chosen[0] = first;
    copy_centers(centers, params->data + (size_t)first * params->n_features, 1, params->n_features);

    for (i = 1; i < params->n_clusters; ++i) {
        int index;
        int valid;
        do {
            index = kmeans_rand_int(seed, params->n_samples);
            valid = 1;
            for (int j = 0; j < i; ++j) {
                if (chosen[j] == index) {
                    valid = 0;
                    break;
                }
            }
        } while (!valid);
        chosen[i] = index;
        copy_centers(centers + (size_t)i * params->n_features,
                     params->data + (size_t)index * params->n_features,
                     1,
                     params->n_features);
    }

    free(chosen);
}

static int initialize_kmeanspp_centers(KMeansParams *params, double *centers, unsigned int *seed)
{
    int i, j;
    int first = kmeans_rand_int(seed, params->n_samples);
    copy_centers(centers, params->data + (size_t)first * params->n_features, 1, params->n_features);

    double *distances = (double *)malloc(sizeof(double) * (size_t)params->n_samples);
    if (!distances) {
        return 0;
    }

    for (i = 1; i < params->n_clusters; ++i) {
        double total = 0.0;
        for (j = 0; j < params->n_samples; ++j) {
            double d = squared_distance(params->data + (size_t)j * params->n_features,
                                        centers + (size_t)(i - 1) * params->n_features,
                                        params->n_features);
            if (i == 1) {
                distances[j] = d;
            } else if (d < distances[j]) {
                distances[j] = d;
            }
            total += distances[j];
        }

        if (total <= 0.0) {
            initialize_random_centers(params, centers + (size_t)i * params->n_features, seed);
            continue;
        }

        double threshold = (double)kmeans_rand(seed) / (double)UINT_MAX * total;
        double cumulative = 0.0;
        int selected = params->n_samples - 1;
        for (j = 0; j < params->n_samples; ++j) {
            cumulative += distances[j];
            if (cumulative >= threshold) {
                selected = j;
                break;
            }
        }

        copy_centers(centers + (size_t)i * params->n_features,
                     params->data + (size_t)selected * params->n_features,
                     1,
                     params->n_features);
    }

    free(distances);
    return 1;
}

static void assign_labels(KMeansParams *params,
                          const double *centers,
                          int *labels,
                          double *distances,
                          double *inertia)
{
    int i, j;
    double total_inertia = 0.0;
    for (i = 0; i < params->n_samples; ++i) {
        const double *point = params->data + (size_t)i * params->n_features;
        double best_distance = -1.0;
        int best_label = 0;

        for (j = 0; j < params->n_clusters; ++j) {
            double d = squared_distance(point,
                                        centers + (size_t)j * params->n_features,
                                        params->n_features);
            if (j == 0 || d < best_distance) {
                best_distance = d;
                best_label = j;
            }
        }

        labels[i] = best_label;
        distances[i] = best_distance;
        total_inertia += best_distance;
    }

    *inertia = total_inertia;
}

static void recompute_centers(KMeansParams *params,
                              const int *labels,
                              const double *centers,
                              double *next_centers,
                              int *counts,
                              unsigned int *seed)
{
    int k;
    int i;
    int feature_count = params->n_clusters * params->n_features;

    for (i = 0; i < feature_count; ++i) {
        next_centers[i] = 0.0;
    }

    for (k = 0; k < params->n_clusters; ++k) {
        counts[k] = 0;
    }

    for (i = 0; i < params->n_samples; ++i) {
        int label = labels[i];
        const double *point = params->data + (size_t)i * params->n_features;
        double *center = next_centers + (size_t)label * params->n_features;
        int j;
        for (j = 0; j < params->n_features; ++j) {
            center[j] += point[j];
        }
        counts[label] += 1;
    }

    for (k = 0; k < params->n_clusters; ++k) {
        if (counts[k] == 0) {
            int index = kmeans_rand_int(seed, params->n_samples);
            copy_centers(next_centers + (size_t)k * params->n_features,
                         params->data + (size_t)index * params->n_features,
                         1,
                         params->n_features);
            continue;
        }

        double scale = 1.0 / (double)counts[k];
        double *center = next_centers + (size_t)k * params->n_features;
        for (i = 0; i < params->n_features; ++i) {
            center[i] *= scale;
        }
    }

    (void)centers;
}

static double compute_max_center_shift(const double *centers,
                                       const double *next_centers,
                                       int n_clusters,
                                       int n_features)
{
    double max_shift = 0.0;
    int i, j;
    for (i = 0; i < n_clusters; ++i) {
        const double *current = centers + (size_t)i * n_features;
        const double *next = next_centers + (size_t)i * n_features;
        double shift = 0.0;
        for (j = 0; j < n_features; ++j) {
            double diff = current[j] - next[j];
            shift += diff * diff;
        }
        if (shift > max_shift) {
            max_shift = shift;
        }
    }
    return sqrt(max_shift);
}

static void copy_int_array(int *dest, const int *src, int length)
{
    memcpy(dest, src, sizeof(int) * (size_t)length);
}

void Kmeans(KMeansParams *params)
{
    if (!params || !params->data || params->n_samples <= 0 || params->n_features <= 0 || params->n_clusters <= 0) {
        if (params) {
            params->success = 0;
        }
        return;
    }

    if (params->n_clusters > params->n_samples) {
        params->n_clusters = params->n_samples;
    }

    if (params->n_init <= 0) {
        params->n_init = 10;
    }
    if (params->max_iter <= 0) {
        params->max_iter = 300;
    }
    if (params->tol <= 0.0) {
        params->tol = 1e-4;
    }
    if (!params->init) {
        params->init = "k-means++";
    }
    if (!params->algorithm) {
        params->algorithm = "lloyd";
    }

    params->success = 0;
    params->inertia = 0.0;
    params->n_iter = 0;

    if (params->cluster_centers) {
        free(params->cluster_centers);
        params->cluster_centers = NULL;
    }
    if (params->labels) {
        free(params->labels);
        params->labels = NULL;
    }

    int n_samples = params->n_samples;
    int n_features = params->n_features;
    int n_clusters = params->n_clusters;
    size_t centers_size = (size_t)n_clusters * (size_t)n_features;

    double *best_centers = (double *)malloc(sizeof(double) * centers_size);
    int *best_labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
    if (!best_centers || !best_labels) {
        /* 任意一个分配失败时均安全释放：free(NULL) 是合法的。
         * 此分支前尚未分配 centers/next_centers/distances/labels/counts，
         * params->cluster_centers/params->labels 已在前面显式释放并置 NULL。 */
        free(best_centers);
        free(best_labels);
        params->success = 0;
        return;
    }

    double *centers = (double *)malloc(sizeof(double) * centers_size);
    double *next_centers = (double *)malloc(sizeof(double) * centers_size);
    double *distances = (double *)malloc(sizeof(double) * (size_t)n_samples);
    int *labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
    int *counts = (int *)malloc(sizeof(int) * (size_t)n_clusters);
    if (!centers || !next_centers || !distances || !labels || !counts) {
        /* 任意一个分配失败时安全释放所有已分配的内存（含上面已成功的 best_*）。
         * free(NULL) 安全，无副作用。 */
        free(best_centers);
        free(best_labels);
        free(centers);
        free(next_centers);
        free(distances);
        free(labels);
        free(counts);
        params->success = 0;
        return;
    }

    double best_inertia = INFINITY;
    int best_iter = 0;
    unsigned int base_seed = kmeans_seed(params->random_state);

    for (int attempt = 0; attempt < params->n_init; ++attempt) {
        unsigned int seed = base_seed + (unsigned int)attempt * 1009u;
        if (strcmp(params->init, "random") == 0) {
            initialize_random_centers(params, centers, &seed);
        } else {
            if (!initialize_kmeanspp_centers(params, centers, &seed)) {
                initialize_random_centers(params, centers, &seed);
            }
        }

        int iteration;
        double inertia = 0.0;

        for (iteration = 0; iteration < params->max_iter; ++iteration) {
            assign_labels(params, centers, labels, distances, &inertia);
            recompute_centers(params, labels, centers, next_centers, counts, &seed);

            double shift = compute_max_center_shift(centers, next_centers, n_clusters, n_features);
            copy_centers(centers, next_centers, n_clusters, n_features);

            if (shift <= params->tol) {
                iteration += 1;
                break;
            }
        }

        if (iteration == params->max_iter) {
            assign_labels(params, centers, labels, distances, &inertia);
        }

        if (params->verbose) {
            printf("Kmeans attempt %d inertia=%.6f iterations=%d\n", attempt + 1, inertia, iteration);
        }

        if (inertia < best_inertia) {
            best_inertia = inertia;
            best_iter = iteration;
            copy_centers(best_centers, centers, n_clusters, n_features);
            copy_int_array(best_labels, labels, n_samples);
        }
    }

    params->cluster_centers = (double *)malloc(sizeof(double) * centers_size);
    params->labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
    if (!params->cluster_centers || !params->labels) {
        /* 任意一个分配失败时安全释放所有已分配的内存。
         * free(NULL) 是合法的，所以逐项 free 不会崩溃。
         * 注意：已成功分配的资源指针全部需要 free 一遍。 */
        free(params->cluster_centers);
        free(params->labels);
        free(best_centers);
        free(best_labels);
        free(centers);
        free(next_centers);
        free(distances);
        free(labels);
        free(counts);
        params->cluster_centers = NULL;
        params->labels = NULL;
        params->success = 0;
        return;
    }

    copy_centers(params->cluster_centers, best_centers, n_clusters, n_features);
    copy_int_array(params->labels, best_labels, n_samples);
    params->inertia = best_inertia;
    params->n_iter = best_iter;
    params->success = 1;

    free(best_centers);
    free(best_labels);
    free(centers);
    free(next_centers);
    free(distances);
    free(labels);
    free(counts);
}

void KmeansFree(KMeansParams *params)
{
    if (!params) {
        return;
    }
    free(params->cluster_centers);
    free(params->labels);
    params->cluster_centers = NULL;
    params->labels = NULL;
    params->inertia = 0.0;
    params->n_iter = 0;
    params->success = 0;
}

