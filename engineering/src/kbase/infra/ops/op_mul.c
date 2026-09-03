#include "infra/op.h"

static infra_status_t op_mul_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 2 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* B = inputs[1];
    infra_tensor_t* C = outputs[0];
    if (!A || !B || !C) return INFRA_ERR_PARAM;

    size_t n = C->nelems;
    if (A->nelems == n && B->nelems == n) {
        const float* a = (const float*)A->data;
        const float* b = (const float*)B->data;
        float* c = (float*)C->data;
        for (size_t i = 0; i < n; i++) c[i] = a[i] * b[i];
    } else if (B->nelems == 1) {
        const float* a = (const float*)A->data;
        const float bs = *(const float*)B->data;
        float* c = (float*)C->data;
        for (size_t i = 0; i < n; i++) c[i] = a[i] * bs;
    } else {
        return INFRA_ERR_PARAM;
    }
    (void)params;
    return INFRA_OK;
}

static infra_op_t mul_op = {
    .name = "mul",
    .func = op_mul_impl,
    .min_inputs = 2, .max_inputs = 2,
    .min_outputs = 1, .max_outputs = 1,
    .description = "逐元素乘法（支持 broadcast）",
};

void register_op_mul(void) { infra_op_register(&mul_op); }