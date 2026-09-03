#include "infra/model.h"
#include "model_gguf_internal.h"
#include <stdlib.h>
#include <string.h>

#define GGUF_MAGIC   0x46554747  /* "GGUF" little-endian */
#define GGUF_VERSION 3

enum {
    GGUF_TYPE_UINT32  = 4,
    GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_STRING  = 8,
    GGUF_TYPE_ARRAY   = 9,
};

static int read_u32(FILE* fp, uint32_t* out) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) return -1;
    *out = (uint32_t)buf[0] | ((uint32_t)buf[1]<<8) |
           ((uint32_t)buf[2]<<16) | ((uint32_t)buf[3]<<24);
    return 0;
}
static int read_u64(FILE* fp, uint64_t* out) {
    uint8_t buf[8];
    if (fread(buf, 1, 8, fp) != 8) return -1;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= ((uint64_t)buf[i]) << (i * 8);
    *out = v;
    return 0;
}
static int read_i64(FILE* fp, int64_t* out) { return read_u64(fp, (uint64_t*)out); }

infra_status_t infra_gguf_load(infra_model_gguf_t* gguf, const char* path) {
    if (!gguf || !path) return INFRA_ERR_PARAM;
    memset(gguf, 0, sizeof(*gguf));
    gguf->fp = fopen(path, "rb");
    if (!gguf->fp) return INFRA_ERR_LOAD;

    /* Header */
    uint32_t magic, version;
    if (read_u32(gguf->fp, &magic) || magic != GGUF_MAGIC) return INFRA_ERR_FORMAT;
    if (read_u32(gguf->fp, &version) || version != GGUF_VERSION) return INFRA_ERR_FORMAT;
    if (read_u64(gguf->fp, &gguf->tensor_count)) return INFRA_ERR_FORMAT;
    if (read_u64(gguf->fp, &gguf->metadata_kv_count)) return INFRA_ERR_FORMAT;

    /* Metadata KV */
    for (uint64_t i = 0; i < gguf->metadata_kv_count; i++) {
        uint64_t key_len;
        if (read_u64(gguf->fp, &key_len)) return INFRA_ERR_FORMAT;
        char key[256];
        if (key_len >= sizeof(key)) {
            fseek(gguf->fp, (long)key_len, SEEK_CUR);
        } else {
            if (fread(key, 1, (size_t)key_len, gguf->fp) != key_len) return INFRA_ERR_FORMAT;
            key[key_len] = 0;
        }
        uint32_t vtype;
        if (read_u32(gguf->fp, &vtype)) return INFRA_ERR_FORMAT;

        if (vtype == GGUF_TYPE_STRING) {
            uint64_t slen;
            if (read_u64(gguf->fp, &slen)) return INFRA_ERR_FORMAT;
            char val[256];
            size_t rl = slen < sizeof(val) - 1 ? (size_t)slen : sizeof(val) - 1;
            if (fread(val, 1, rl, gguf->fp) != rl) return INFRA_ERR_FORMAT;
            val[rl] = 0;
            if (slen > rl) fseek(gguf->fp, (long)(slen - rl), SEEK_CUR);
            if (strcmp(key, "general.architecture") == 0)
                strncpy(gguf->arch_str, val, sizeof(gguf->arch_str) - 1);
        } else if (vtype == GGUF_TYPE_UINT32 || vtype == GGUF_TYPE_INT32) {
            uint32_t v;
            if (read_u32(gguf->fp, &v)) return INFRA_ERR_FORMAT;
            if (strcmp(key, "bert.embedding_length") == 0) gguf->n_embd = (int)v;
            else if (strcmp(key, "bert.attention.head_count") == 0) gguf->n_head = (int)v;
            else if (strcmp(key, "bert.block_count") == 0) gguf->n_layer = (int)v;
            else if (strcmp(key, "bert.context_length") == 0) gguf->n_ctx = (int)v;
        } else if (vtype == GGUF_TYPE_ARRAY) {
            uint32_t etype;
            uint64_t alen;
            if (read_u32(gguf->fp, &etype)) return INFRA_ERR_FORMAT;
            if (read_u64(gguf->fp, &alen)) return INFRA_ERR_FORMAT;
            if (etype == GGUF_TYPE_STRING) {
                /* 检查是否是 tokenizer.ggml.tokens */
                int is_vocab = (strcmp(key, "tokenizer.ggml.tokens") == 0);
                if (is_vocab && alen > 0) {
                    gguf->vocab = (char**)calloc((size_t)alen, sizeof(char*));
                    if (!gguf->vocab) return INFRA_ERR_MEMORY;
                    gguf->vocab_size = (int)alen;
                }
                for (uint64_t j = 0; j < alen; j++) {
                    uint64_t sl;
                    if (read_u64(gguf->fp, &sl)) return INFRA_ERR_FORMAT;
                    if (is_vocab && gguf->vocab) {
                        /* 读取词表字符串 */
                        gguf->vocab[j] = (char*)malloc((size_t)sl + 1);
                        if (!gguf->vocab[j]) return INFRA_ERR_MEMORY;
                        if (fread(gguf->vocab[j], 1, (size_t)sl, gguf->fp) != sl) return INFRA_ERR_FORMAT;
                        gguf->vocab[j][sl] = 0;
                    } else {
                        fseek(gguf->fp, (long)sl, SEEK_CUR);
                    }
                }
            } else {
                fseek(gguf->fp, (long)(alen * 4), SEEK_CUR);
            }
        } else {
            return INFRA_ERR_FORMAT;
        }
    }

    /* Tensor info */
    if (gguf->tensor_count > 0) {
        gguf->tensors = (infra_gguf_tensor_info_t*)calloc(
            (size_t)gguf->tensor_count, sizeof(*gguf->tensors));
        if (!gguf->tensors) return INFRA_ERR_MEMORY;
    }
    for (uint64_t i = 0; i < gguf->tensor_count; i++) {
        uint64_t name_len;
        if (read_u64(gguf->fp, &name_len)) return INFRA_ERR_FORMAT;
        if (name_len >= sizeof(gguf->tensors[i].name)) return INFRA_ERR_FORMAT;
        if (fread(gguf->tensors[i].name, 1, (size_t)name_len, gguf->fp) != name_len)
            return INFRA_ERR_FORMAT;
        gguf->tensors[i].name[name_len] = 0;
        if (read_u32(gguf->fp, (uint32_t*)&gguf->tensors[i].ndim)) return INFRA_ERR_FORMAT;
        for (int d = 0; d < gguf->tensors[i].ndim; d++) {
            if (read_i64(gguf->fp, &gguf->tensors[i].dims[d])) return INFRA_ERR_FORMAT;
        }
        if (read_u32(gguf->fp, &gguf->tensors[i].dtype)) return INFRA_ERR_FORMAT;
        if (read_u64(gguf->fp, &gguf->tensors[i].offset)) return INFRA_ERR_FORMAT;
    }

    gguf->data_start = (uint64_t)ftell(gguf->fp);
    if (gguf->data_start % 32 != 0) {
        fseek(gguf->fp, (long)(32 - (gguf->data_start % 32)), SEEK_CUR);
        gguf->data_start = (uint64_t)ftell(gguf->fp);
    }
    for (uint64_t i = 0; i < gguf->tensor_count; i++) {
        gguf->tensors[i].file_offset = gguf->data_start + gguf->tensors[i].offset;
    }
    return INFRA_OK;
}

void infra_gguf_free(infra_model_gguf_t* gguf) {
    if (!gguf) return;
    if (gguf->fp) fclose(gguf->fp);
    free(gguf->tensors);
    if (gguf->vocab) {
        for (int i = 0; i < gguf->vocab_size; i++) free(gguf->vocab[i]);
        free(gguf->vocab);
    }
    free(gguf);
}

infra_gguf_tensor_info_t* infra_gguf_find_tensor(infra_model_gguf_t* gguf, const char* name) {
    if (!gguf || !name) return NULL;
    for (uint64_t i = 0; i < gguf->tensor_count; i++) {
        if (strcmp(gguf->tensors[i].name, name) == 0)
            return &gguf->tensors[i];
    }
    return NULL;
}

char** infra_gguf_get_vocab(infra_model_gguf_t* gguf, int* vocab_size_out) {
    if (!gguf || !vocab_size_out) return NULL;
    *vocab_size_out = gguf->vocab_size;
    return gguf->vocab;
}