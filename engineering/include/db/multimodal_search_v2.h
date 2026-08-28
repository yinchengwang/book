/**
 * @file multimodal_search_v2.h
 * @brief 多模态检索 v2 接口
 */
#ifndef DB_MULTIMODAL_SEARCH_V2_H
#define DB_MULTIMODAL_SEARCH_V2_H

#include "db/multimodal_object.h"

#ifdef __cplusplus
extern "C" {
#endif

int mm_multimodal_search_v2(const mm_multimodal_object_t *query,
                           void *const *collections,
                           int n_spaces,
                           int top_k,
                           const char *filter_json,
                           int32_t *out_ids,
                           double *out_scores);

#ifdef __cplusplus
}
#endif

#endif