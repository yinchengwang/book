#ifndef KBASE_INFRA_MODEL_H
#define KBASE_INFRA_MODEL_H

#include "infra/types.h"
#include "infra/tensor.h"
#include "infra/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INFRA_MODEL_GGUF = 0,
    INFRA_MODEL_ONNX = 1,
} infra_model_format_t;

typedef enum {
    INFRA_ARCH_BERT = 0,
    INFRA_ARCH_LLAMA = 1,
    INFRA_ARCH_UNKNOWN = -1,
} infra_model_arch_t;

typedef struct infra_model_gguf infra_model_gguf_t;

typedef struct infra_model {
    infra_model_format_t format;
    infra_model_arch_t arch;
    int n_embd;
    int n_head;
    int n_layer;
    int n_ctx;
    int n_vocab;
    infra_memory_pool_t* pool;
    infra_model_gguf_t* gguf;
} infra_model_t;

/* GGUF 张量信息（与 model_gguf.c 内部结构对齐） */
typedef struct {
    char name[128];
    int ndim;
    int64_t dims[4];
    uint32_t dtype;
    uint64_t offset;
    uint64_t file_offset;
} infra_gguf_tensor_info_t;

/* GGUF 模型访问函数 */
char** infra_gguf_get_vocab(infra_model_gguf_t* gguf, int* vocab_size_out);

infra_model_t* infra_model_load(const char* path, infra_model_format_t fmt);
void infra_model_free(infra_model_t* model);
infra_status_t infra_model_validate(infra_model_t* model);
infra_tensor_t* infra_model_get_tensor(infra_model_t* model, const char* name);

/* GGUF 内部 API（被 model_gguf.c 实现） */
infra_status_t infra_gguf_load(infra_model_gguf_t* gguf, const char* path);
void infra_gguf_free(infra_model_gguf_t* gguf);
infra_gguf_tensor_info_t* infra_gguf_find_tensor(infra_model_gguf_t* gguf, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_MODEL_H */