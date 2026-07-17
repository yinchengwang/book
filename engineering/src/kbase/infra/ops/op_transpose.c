#include "infra/op.h"
#include <string.h>

static infra_status_t op_transpose_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 1 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* C = outputs[0];
    if (!A || !C) return INFRA_ERR_PARAM;
    if (A->dtype != INFRA_DTYPE_F32 || C->dtype != INFRA_DTYPE_F32) return INFRA_ERR_PARAM;

    const op_transpose_params_t* p = (const op_transpose_params_t*)params;
    int a1 = p ? p->axis1 : 0;
    int a2 = p ? p->axis2 : 1;

    /* 仅支持 2D transpose 加速实现 */
    if (A->ndim != 2) return INFRA_ERR_PARAM;

    int M = (int)A->dims[0];
    int N = (int)A->dims[1];
    const float* a = (const float*)A->data;
    float* c = (float*)C->data;

    /* a1=0, a2=1 等价于完全转置 [M,N] -> [N,M] */
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            c[j * M + i] = a[i * N + j];
        }
    }
    return INFRA_OK;
}

static infra_op_t transpose_op = {
    .name = "transpose", .func = op_transpose_impl,
    .min_inputs = 1, .max_inputs = 1,
    .min_outputs = 1, .max_outputs = 1,
    .description = "2D 转置",
};

void register_op_transpose(void) { infra_op_register(&transpose_op); }