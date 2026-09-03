#ifndef KBASE_KBASE_SEARCH_H
#define KBASE_KBASE_SEARCH_H

#include "kbase_index.h"
#include "infra/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    kbase_note_t* note;
    float score;
    int rank;
} kbase_result_t;

kbase_result_t* kbase_search(
    kbase_index_t* idx,
    infra_model_t* model,
    const char* query,
    int top_k,
    int* num_results);

void kbase_search_free(kbase_result_t* results, int num_results);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_KBASE_SEARCH_H */
