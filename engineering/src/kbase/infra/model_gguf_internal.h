#ifndef KBASE_INFRA_MODEL_GGUF_INTERNAL_H
#define KBASE_INFRA_MODEL_GGUF_INTERNAL_H

#include "infra/model.h"
#include <stdio.h>

struct infra_model_gguf {
    FILE* fp;
    uint64_t metadata_kv_count;
    uint64_t tensor_count;
    char arch_str[64];
    int n_embd, n_head, n_layer, n_ctx, n_vocab;
    infra_gguf_tensor_info_t* tensors;
    uint64_t data_start;
    /* 词表 */
    char** vocab;         /* 词表字符串数组 */
    int vocab_size;       /* 词表大小 */
};

#endif /* KBASE_INFRA_MODEL_GGUF_INTERNAL_H */