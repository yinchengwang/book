#ifndef KBASE_INFRA_TENSOR_H
#define KBASE_INFRA_TENSOR_H

#include "infra/types.h"
#include "infra/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INFRA_MAX_DIMS 4

/* 张量结构（row-major 连续存储） */
typedef struct infra_tensor {
    infra_dtype_t dtype;                          /* 数据类型 */
    int32_t ndim;                                  /* 维度数 */
    int64_t dims[INFRA_MAX_DIMS];                 /* 各维度尺寸 */
    int64_t strides[INFRA_MAX_DIMS];              /* 各维度步长（字节） */
    size_t nelems;                                 /* 元素总数 */
    size_t nbytes;                                 /* 数据字节数 */
    void* data;                                    /* 数据指针 */
    int owns_data;                                 /* 是否拥有数据所有权 */
    const char* name;                              /* 名称（调试用） */
} infra_tensor_t;

/* 创建张量（分配数据） */
infra_tensor_t* infra_tensor_create(const int64_t* dims, int ndim, infra_dtype_t dtype);

/* 创建张量（可选内存池，NULL 则用 malloc） */
infra_tensor_t* infra_tensor_create_with_pool(
    infra_memory_pool_t* pool, const int64_t* dims, int ndim, infra_dtype_t dtype);

/* 创建视图（不分配数据） */
infra_tensor_t* infra_tensor_view(infra_tensor_t* src, const int64_t* dims, int ndim);

/* 释放张量 */
void infra_tensor_free(infra_tensor_t* t);

/* 释放张量数组 */
void infra_tensor_free_all(infra_tensor_t** tensors, int n);

/* 重塑形状 */
infra_status_t infra_tensor_reshape(infra_tensor_t* t, const int64_t* new_dims, int new_ndim);

/* 复制数据 */
infra_status_t infra_tensor_copy(infra_tensor_t* dst, const infra_tensor_t* src);

/* 检查是否连续 */
int infra_tensor_is_contiguous(const infra_tensor_t* t);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_TENSOR_H */
