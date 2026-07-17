#include "infra/op.h"
#include <string.h>

/* MatMul: C = A @ B
 * A: [M, K], B: [K, N], C: [M, N] */
static infra_status_t op_matmul_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 2 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* B = inputs[1];
    infra_tensor_t* C = outputs[0];

    if (!A || !B || !C) return INFRA_ERR_PARAM;
    if (A->ndim != 2 || B->ndim != 2 || C->ndim != 2) return INFRA_ERR_PARAM;
    if (A->dtype != INFRA_DTYPE_F32 || B->dtype != INFRA_DTYPE_F32) return INFRA_ERR_PARAM;

    int M = (int)A->dims[0];
    int K = (int)A->dims[1];
    int N = (int)B->dims[1];

    if (B->dims[0] != K || C->dims[0] != M || C->dims[1] != N) return INFRA_ERR_PARAM;

    const float* a = (const float*)A->data;
    const float* b = (const float*)B->data;
    float* c = (float*)C->data;

    memset(c, 0, C->nbytes);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += a[i * K + k] * b[k * N + j];
            }
            c[i * N + j] = sum;
        }
    }
    (void)params;
    return INFRA_OK;
}

static infra_op_t matmul_op = {
    .name = "matmul",
    .func = op_matmul_impl,
    .min_inputs = 2, .max_inputs = 2,
    .min_outputs = 1, .max_outputs = 1,
    .description = "矩阵乘法 C = A @ B",
};

/* 注册入口（由 op_registry.c 显式调用） */
void register_op_matmul(void) {
    infra_op_register(&matmul_op);
}