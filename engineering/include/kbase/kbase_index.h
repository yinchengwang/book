#ifndef KBASE_KBASE_INDEX_H
#define KBASE_KBASE_INDEX_H

#include "infra/types.h"
#include "infra/model.h"
#include "infra/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 笔记条目 */
typedef struct {
    char* path;          /* 相对路径 */
    char* title;         /* 标题 */
    char* content;       /* 正文 */
    float* embedding;    /* 384 维向量 */
    int embedding_dim;
    int id;              /* 在 HNSW 中的 ID */
} kbase_note_t;

/* 索引 */
typedef struct kbase_index {
    kbase_note_t* notes;
    int num_notes;
    int capacity;
    char* notes_dir;
    void* hnsw;          /* faiss_hnsw_t* */
    int dim;
    int scan_error;      /* 扫描过程中累积的错误码（0=无错误） */
} kbase_index_t;

kbase_index_t* kbase_index_create(const char* notes_dir);
void kbase_index_destroy(kbase_index_t* idx);
int kbase_index_scan(kbase_index_t* idx);
infra_status_t kbase_index_build(kbase_index_t* idx, infra_model_t* model);
infra_status_t kbase_index_save(kbase_index_t* idx, const char* path);
infra_status_t kbase_index_load(kbase_index_t* idx, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_KBASE_INDEX_H */