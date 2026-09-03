#include "infra/tokenizer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char* str_dup(const char* s) { return strdup(s); }

static int find_vocab(const char** vocab, int n, const char* word) {
    for (int i = 0; i < n; i++) {
        if (strcmp(vocab[i], word) == 0) return i;
    }
    return -1;
}

infra_status_t infra_tokenize(
    const char* text, const char** vocab, int vocab_size,
    int max_len, int* ids_out, int* len_out)
{
    if (!text || !vocab || !ids_out || !len_out) return INFRA_ERR_PARAM;
    int len = 0;
    /* [CLS] */
    if (len < max_len) ids_out[len++] = 101;  /* "CLS" token id (BERT default) */
    /* Tokenize word by word */
    const char* p = text;
    char buf[64];
    while (*p && len < max_len - 1) {
        /* 跳过空白 */
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        /* 提取一个单词（简单实现：连续字母数字） */
        int n = 0;
        while (*p && !isspace((unsigned char)*p) && n < (int)sizeof(buf) - 1) {
            buf[n++] = *p++;
        }
        buf[n] = 0;
        /* 转换为小写 */
        for (int i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
        int id = find_vocab(vocab, vocab_size, buf);
        if (id < 0) id = 100;  /* [UNK] */
        ids_out[len++] = id;
    }
    /* [SEP] */
    if (len < max_len) ids_out[len++] = 102;
    *len_out = len;
    return INFRA_OK;
}

infra_status_t infra_tokenizer_load_vocab(
    infra_model_t* model,
    char*** vocab_out, int* vocab_size_out)
{
    if (!model || !model->gguf || !vocab_out || !vocab_size_out) return INFRA_ERR_PARAM;

    /* 从 GGUF 读取词表大小与原始指针 */
    int src_size = 0;
    char** src = infra_gguf_get_vocab(model->gguf, &src_size);
    if (!src || src_size <= 0) return INFRA_ERR_NOT_FOUND;

    /* 复制词表（调用方通过 infra_tokenizer_free_vocab 释放） */
    char** vocab = (char**)calloc((size_t)src_size, sizeof(char*));
    if (!vocab) return INFRA_ERR_MEMORY;

    for (int i = 0; i < src_size; i++) {
        vocab[i] = str_dup(src[i]);
        if (!vocab[i]) {
            /* 回滚已分配的内存 */
            for (int j = 0; j < i; j++) free(vocab[j]);
            free(vocab);
            return INFRA_ERR_MEMORY;
        }
    }

    *vocab_out = vocab;
    *vocab_size_out = src_size;
    return INFRA_OK;
}

void infra_tokenizer_free_vocab(char** vocab, int vocab_size) {
    if (!vocab) return;
    for (int i = 0; i < vocab_size; i++) free(vocab[i]);
    free(vocab);
}