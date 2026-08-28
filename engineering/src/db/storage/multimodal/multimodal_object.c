/**
 * @file multimodal_object.c
 * @brief 多模态对象实现（C3-4）
 */
#include "db/multimodal_object.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

void mm_multimodal_object_init(mm_multimodal_object_t *obj) {
    if (!obj) return;
    memset(obj, 0, sizeof(*obj));
    obj->blob_id[0] = 0;  /* 默认无 blob */
    obj->has_blob = false;
    obj->n_vectors = 0;
    obj->metadata = NULL;
    obj->metadata_len = 0;
}

void mm_multimodal_object_free(mm_multimodal_object_t *obj) {
    if (!obj) return;
    for (int32_t i = 0; i < obj->n_vectors; ++i) {
        free(obj->vectors[i].data);
    }
    free(obj->metadata);
    memset(obj, 0, sizeof(*obj));
}

int mm_multimodal_add_vector(mm_multimodal_object_t *obj,
                             const char *name,
                             const float *vec, int32_t dim) {
    if (!obj || !name || !vec || dim <= 0) return -1;
    if (obj->n_vectors >= MM_MAX_NAMED_VECTORS) return -2;
    int32_t idx = obj->n_vectors;
    strncpy(obj->vectors[idx].name, name, sizeof(obj->vectors[idx].name) - 1);
    obj->vectors[idx].dim = dim;
    obj->vectors[idx].data = malloc(sizeof(float) * (size_t)dim);
    if (!obj->vectors[idx].data) return -3;
    memcpy(obj->vectors[idx].data, vec, sizeof(float) * (size_t)dim);
    obj->n_vectors++;
    return 0;
}