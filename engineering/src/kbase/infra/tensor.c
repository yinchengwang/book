#include "infra/tensor.h"
#include <stdlib.h>
#include <string.h>

size_t infra_sizeof_dtype(infra_dtype_t dtype) {
    switch (dtype) {
        case INFRA_DTYPE_F32: return 4;
        case INFRA_DTYPE_F16: return 2;
        case INFRA_DTYPE_Q8_0: return 0;  /* 块大小，特殊处理 */
        case INFRA_DTYPE_Q4_0: return 0;
        case INFRA_DTYPE_Q4_1: return 0;
        case INFRA_DTYPE_I32:  return 4;
        case INFRA_DTYPE_I64:  return 8;
        default: return 0;
    }
}

static size_t tensor_nelems(const int64_t* dims, int ndim) {
    size_t n = 1;
    for (int i = 0; i < ndim; i++) n *= (size_t)dims[i];
    return n;
}

infra_tensor_t* infra_tensor_create(const int64_t* dims, int ndim, infra_dtype_t dtype) {
    return infra_tensor_create_with_pool(NULL, dims, ndim, dtype);
}

infra_tensor_t* infra_tensor_create_with_pool(
    infra_memory_pool_t* pool, const int64_t* dims, int ndim, infra_dtype_t dtype)
{
    if (!dims || ndim <= 0 || ndim > INFRA_MAX_DIMS) return NULL;

    size_t esize = infra_sizeof_dtype(dtype);
    if (esize == 0) return NULL;  /* 量化类型不支持此方式创建 */

    infra_tensor_t* t = (infra_tensor_t*)calloc(1, sizeof(infra_tensor_t));
    if (!t) return NULL;

    t->dtype = dtype;
    t->ndim = ndim;
    t->nelems = tensor_nelems(dims, ndim);
    t->nbytes = t->nelems * esize;

    for (int i = 0; i < ndim; i++) t->dims[i] = dims[i];
    for (int i = ndim; i < INFRA_MAX_DIMS; i++) t->dims[i] = 0;

    /* 计算 strides（row-major） */
    int64_t stride = (int64_t)esize;
    for (int i = ndim - 1; i >= 0; i--) {
        t->strides[i] = stride;
        stride *= t->dims[i];
    }

    /* 从内存池或堆分配 */
    if (pool) {
        t->data = infra_memory_pool_alloc(pool, t->nbytes);
    } else {
        t->data = calloc(1, t->nbytes);
    }
    if (!t->data) {
        free(t);
        return NULL;
    }
    t->owns_data = 1;
    return t;
}

infra_tensor_t* infra_tensor_view(infra_tensor_t* src, const int64_t* dims, int ndim) {
    if (!src || !dims || ndim <= 0) return NULL;
    size_t nelems = tensor_nelems(dims, ndim);
    if (nelems != src->nelems) return NULL;

    infra_tensor_t* t = (infra_tensor_t*)calloc(1, sizeof(infra_tensor_t));
    if (!t) return NULL;

    t->dtype = src->dtype;
    t->ndim = ndim;
    t->nelems = src->nelems;
    t->nbytes = src->nbytes;
    for (int i = 0; i < ndim; i++) t->dims[i] = dims[i];
    /* 视图直接共享 strides 和 data */
    memcpy(t->strides, src->strides, sizeof(src->strides));
    t->data = src->data;
    t->owns_data = 0;  /* 视图不拥有数据 */
    t->name = src->name;
    return t;
}

void infra_tensor_free(infra_tensor_t* t) {
    if (!t) return;
    if (t->owns_data && t->data) free(t->data);
    free(t);
}

void infra_tensor_free_all(infra_tensor_t** tensors, int n) {
    if (!tensors) return;
    for (int i = 0; i < n; i++) {
        infra_tensor_free(tensors[i]);
        tensors[i] = NULL;  /* 防止误用 */
    }
    free(tensors);  /* 释放数组本身 */
}

infra_status_t infra_tensor_reshape(infra_tensor_t* t, const int64_t* new_dims, int new_ndim) {
    if (!t || !new_dims || new_ndim <= 0 || new_ndim > INFRA_MAX_DIMS) {
        return INFRA_ERR_PARAM;
    }
    size_t nelems = tensor_nelems(new_dims, new_ndim);
    if (nelems != t->nelems) return INFRA_ERR_PARAM;
    if (!infra_tensor_is_contiguous(t)) return INFRA_ERR_PARAM;

    t->ndim = new_ndim;
    for (int i = 0; i < new_ndim; i++) t->dims[i] = new_dims[i];
    /* 重新计算 strides */
    int64_t stride = (int64_t)infra_sizeof_dtype(t->dtype);
    for (int i = new_ndim - 1; i >= 0; i--) {
        t->strides[i] = stride;
        stride *= t->dims[i];
    }
    return INFRA_OK;
}

infra_status_t infra_tensor_copy(infra_tensor_t* dst, const infra_tensor_t* src) {
    if (!dst || !src) return INFRA_ERR_PARAM;
    if (dst->nelems != src->nelems) return INFRA_ERR_PARAM;
    if (dst->dtype != src->dtype) return INFRA_ERR_PARAM;

    if (infra_tensor_is_contiguous(dst) && infra_tensor_is_contiguous(src)) {
        memcpy(dst->data, src->data, src->nbytes);
        return INFRA_OK;
    }
    /* TODO: strided copy */
    return INFRA_ERR_OP;
}

int infra_tensor_is_contiguous(const infra_tensor_t* t) {
    if (!t) return 0;
    int64_t expected = (int64_t)infra_sizeof_dtype(t->dtype);
    for (int i = t->ndim - 1; i >= 0; i--) {
        if (t->strides[i] != expected) return 0;
        expected *= t->dims[i];
    }
    return 1;
}