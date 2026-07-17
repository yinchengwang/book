#ifndef KBASE_INFRA_TOKENIZER_H
#define KBASE_INFRA_TOKENIZER_H

#include "infra/types.h"
#include "infra/model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 简单 WordPiece Tokenizer */
infra_status_t infra_tokenize(
    const char* text,
    const char** vocab, int vocab_size,
    int max_len,
    int* ids_out, int* len_out);

/* 从 GGUF 模型加载 vocab（tokenizer.ggml.model） */
infra_status_t infra_tokenizer_load_vocab(
    infra_model_t* model,
    char*** vocab_out, int* vocab_size_out);

void infra_tokenizer_free_vocab(char** vocab, int vocab_size);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_TOKENIZER_H */