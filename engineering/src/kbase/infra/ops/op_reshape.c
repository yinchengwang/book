#include "infra/op.h"
#include <string.h>

static infra_status_t op_reshape_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 1 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* C = outputs[0];
    if (!A || !C) return INFRA_ERR_PARAM;
    /* C 已经预分配为新形状；做数据复制以避免生命周期冲突 */
    if (C->nbytes != A->nbytes) return INFRA_ERR_PARAM;
    memcpy(C->data, A->data, A->nbytes);
    (void)params;
    return INFRA_OK;
}

static infra_op_t reshape_op = {
    .name = "reshape", .func = op_reshape_impl,
    .min_inputs = 1, .max_inputs = 1,
    .min_outputs = 1, .max_outputs = 1,
    .description = "Reshape 视图（数据拷贝）",
};

void register_op_reshape(void) { infra_op_register(&reshape_op); }