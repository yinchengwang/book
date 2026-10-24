#ifndef HNSW_DISTANCE_H
#define HNSW_DISTANCE_H

#include <algo-prod/distance/distance.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

float hnsw_compute_distance(const float *a, const float *b, int32_t dims, distance_metric_t metric);

#ifdef __cplusplus
}
#endif
#endif
