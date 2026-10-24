#ifndef KMEANS_H
#define KMEANS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KMeansParams {
    int n_clusters;
    int n_init;
    int max_iter;
    double tol;
    int verbose;
    int random_state;
    const char *init;
    const char *algorithm;
    int n_samples;
    int n_features;
    const double *data;
    double *cluster_centers;
    int *labels;
    double inertia;
    int n_iter;
    int success;
} KMeansParams;

void Kmeans(KMeansParams *params);
void KmeansFree(KMeansParams *params);

#ifdef __cplusplus
}
#endif

#endif /* KMEANS_H */