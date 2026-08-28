/**
 * @file multimodal_object.h
 * @brief 多模态对象结构（C3-4）
 */
#ifndef DB_MULTIMODAL_OBJECT_H
#define DB_MULTIMODAL_OBJECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM_MAX_NAMED_VECTORS 8

typedef struct {
    char name[64];          /* 向量空间名（clip/siglip/text_bge 等） */
    int32_t dim;
    int32_t index_type;     /* vector_index_type_t 枚举 */
} mm_named_vector_schema_t;

typedef struct {
    uint64_t id;
    uint8_t blob_id[32];   /* C3-1 Blob 引用（0 = 无 Blob） */
    bool has_blob;

    int32_t n_vectors;     /* 已填充向量数 */
    struct {
        char name[64];
        int32_t dim;
        float *data;       /* 堆分配，调用方管理 */
    } vectors[MM_MAX_NAMED_VECTORS];

    void *metadata;        /* 元数据（JSON 字符串或自定义） */
    size_t metadata_len;
} mm_multimodal_object_t;

void mm_multimodal_object_init(mm_multimodal_object_t *obj);
void mm_multimodal_object_free(mm_multimodal_object_t *obj);

int mm_multimodal_add_vector(mm_multimodal_object_t *obj,
                             const char *name,
                             const float *vec, int32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* DB_MULTIMODAL_OBJECT_H */