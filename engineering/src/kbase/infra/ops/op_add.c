#include "infra/op.h"
#include <string.h>

/* 逐元素加法（支持 broadcast，如 [M,N] + [N]） */
static infra_status_t op_add_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 2 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* B = inputs[1];
    infra_tensor_t* C = outputs[0];
    if (!A || !B || !C) return INFRA_ERR_PARAM;
    if (A->dtype != INFRA_DTYPE_F32 || C->dtype != INFRA_DTYPE_F32) return INFRA_ERR_PARAM;

    size_t n = C->nelems;
    if (A->nelems == n && B->nelems == n) {
        /* 无 broadcast */
        const float* a = (const float*)A->data;
        const float* b = (const float*)B->data;
        float* c = (float*)C->data;
        for (size_t i = 0; i < n; i++) c[i] = a[i] + b[i];
    } else if (B->nelems == 1) {
        /* B 是标量 */
        const float* a = (const float*)A->data;
        const float bs = *(const float*)B->data;
        float* c = (float*)C->data;
        for (size_t i = 0; i < n; i++) c[i] = a[i] + bs;
    } else if (A->ndim == 2 && B->ndim == 1) {
        /* broadcast: [M,N] + [N] */
        int M = (int)A->dims[0], N = (int)A->dims[1];
        const float* a = (const float*)A->data;
        const float* b = (const float*)B->data;
        float* c = (float*)C->data;
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                c[i * N + j] = a[i * N + j] + b[j];
            }
        }
    } else {
        return INFRA_ERR_PARAM;
    }
    (void)params;
    return INFRA_OK;
}

static infra_op_t add_op = {
    .name = "add",
    .func = op_add_impl,
    .min_inputs = 2, .max_inputs = 2,
    .min_outputs = 1, .max_outputs = 1,
    .description = "逐元素加法（支持 broadcast）",
};

void register_op_add(void) { infra_op_register(&add_op); }