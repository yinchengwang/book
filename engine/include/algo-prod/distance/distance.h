/*
 * distance.h
 *
 * ĺéčˇçŚťçťä¸ćĽĺŁă?
 * HNSWăIVFăDiskANN ĺéĺć¨Ąĺé˝éčżčżéĺąč˝ä¸ĺčˇçŚťĺşŚéçĺŽç°ĺˇŽĺźă?
 */

#ifndef DISTANCE_DISTANCE_H
#define DISTANCE_DISTANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum distance_metric {
    DISTANCE_METRIC_L2_SQUARED = 0,
    DISTANCE_METRIC_COSINE = 1,
    DISTANCE_METRIC_INNER_PRODUCT = 2,
    DISTANCE_METRIC_HAMMING = 3,
} distance_metric_t;

int distance_metric_is_valid(distance_metric_t metric);
float distance_compute(distance_metric_t metric, const float *lhs, const float *rhs, int32_t dims);
float distance_compute_indexed(distance_metric_t metric,
                              const float *vectors,
                              int32_t dims,
                              int32_t id1,
                              int32_t id2);
float distance_compute_from_query(distance_metric_t metric,
                                  const float *query,
                                  const float *vectors,
                                  int32_t dims,
                                  int32_t id);
void distance_compute_batch4_from_query(distance_metric_t metric,
                                        const float *query,
                                        const float *vectors,
                                        int32_t dims,
                                        int32_t id1,
                                        int32_t id2,
                                        int32_t id3,
                                        int32_t id4,
                                        float *dis1,
                                        float *dis2,
                                        float *dis3,
                                        float *dis4);
float distance_l2sqr(const float *lhs, const float *rhs, int32_t dims);
float distance_l2sqr_indexed(const float *vectors, int32_t dims, int32_t id1, int32_t id2);
float distance_l2sqr_from_query(const float *query, const float *vectors, int32_t dims, int32_t id);
void distance_l2sqr_batch4_from_query(const float *query,
                                      const float *vectors,
                                      int32_t dims,
                                      int32_t id1,
                                      int32_t id2,
                                      int32_t id3,
                                      int32_t id4,
                                      float *dis1,
                                      float *dis2,
                                      float *dis3,
                                      float *dis4);

#ifdef __cplusplus
}
#endif

#endif