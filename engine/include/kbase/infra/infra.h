#ifndef KBASE_INFRA_H
#define KBASE_INFRA_H

#include "infra/types.h"
#include "infra/tensor.h"
#include "infra/model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化算子注册表（必须在使用前调用一次） */
infra_status_t infra_init(void);

/* Embedding 推理 */
infra_status_t infra_embed(
    infra_model_t* model,
    const char** texts, int num_texts,
    float** embeddings_out, int* dim_out);

void infra_embed_free(float* embeddings, int num_texts);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_H */