#include "infra/model.h"
#include "model_gguf_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

infra_model_t* infra_model_load(const char* path, infra_model_format_t fmt) {
    if (!path) return NULL;
    infra_model_t* m = (infra_model_t*)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->format = fmt;

    if (fmt == INFRA_MODEL_GGUF) {
        m->gguf = (infra_model_gguf_t*)calloc(1, sizeof(*m->gguf));
        if (!m->gguf) { free(m); return NULL; }
        if (infra_gguf_load(m->gguf, path) != INFRA_OK) {
            infra_gguf_free(m->gguf); free(m); return NULL;
        }
        m->n_embd = m->gguf->n_embd;
        m->n_head = m->gguf->n_head;
        m->n_layer = m->gguf->n_layer;
        m->n_ctx = m->gguf->n_ctx;
        m->arch = (strcmp(m->gguf->arch_str, "bert") == 0) ? INFRA_ARCH_BERT : INFRA_ARCH_UNKNOWN;
        m->pool = infra_memory_pool_create(64 * 1024 * 1024, 256 * 1024 * 1024);
        if (!m->pool) { infra_gguf_free(m->gguf); free(m); return NULL; }
    } else {
        free(m);
        return NULL;
    }
    return m;
}

void infra_model_free(infra_model_t* model) {
    if (!model) return;
    if (model->gguf) infra_gguf_free(model->gguf);
    if (model->pool) infra_memory_pool_destroy(model->pool);
    free(model);
}

infra_status_t infra_model_validate(infra_model_t* model) {
    if (!model) return INFRA_ERR_PARAM;
    if (model->n_embd <= 0 || model->n_layer <= 0) return INFRA_ERR_FORMAT;
    return INFRA_OK;
}

infra_tensor_t* infra_model_get_tensor(infra_model_t* model, const char* name) {
    if (!model || !name || !model->gguf) return NULL;
    infra_gguf_tensor_info_t* info = infra_gguf_find_tensor(model->gguf, name);
    if (!info) return NULL;

    size_t esize = 4;
    if (info->dtype == 1) esize = 2;  /* F16 */
    int64_t n = 1;
    for (int d = 0; d < info->ndim; d++) n *= info->dims[d];
    size_t nb = (size_t)n * esize;

    infra_tensor_t* t = (infra_tensor_t*)calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->dtype = (esize == 4) ? INFRA_DTYPE_F32 : INFRA_DTYPE_F16;
    t->ndim = info->ndim;
    t->nelems = (size_t)n;
    t->nbytes = nb;
    for (int d = 0; d < t->ndim; d++) t->dims[d] = info->dims[d];

    fseek(model->gguf->fp, (long)info->file_offset, SEEK_SET);
    /* 张量数据用 malloc（arena pool 只能用 reset 批量释放，不能单个 free） */
    t->data = malloc(nb);
    if (!t->data) { free(t); return NULL; }
    if (fread(t->data, 1, nb, model->gguf->fp) != nb) {
        free(t->data); free(t); return NULL;
    }
    t->owns_data = 1;
    t->name = info->name;
    return t;
}