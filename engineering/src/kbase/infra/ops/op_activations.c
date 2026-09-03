#include "infra/op.h"
#include <math.h>

static infra_status_t op_relu_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 1 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* C = outputs[0];
    if (!A || !C) return INFRA_ERR_PARAM;
    const float* a = (const float*)A->data;
    float* c = (float*)C->data;
    for (size_t i = 0; i < A->nelems; i++) {
        c[i] = a[i] > 0.0f ? a[i] : 0.0f;
    }
    (void)params;
    return INFRA_OK;
}

static infra_status_t op_gelu_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 1 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* C = outputs[0];
    if (!A || !C) return INFRA_ERR_PARAM;
    const float* a = (const float*)A->data;
    float* c = (float*)C->data;
    /* GELU tanh 近似: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))) */
    const float c1 = 0.7978845608f;  /* sqrt(2/pi) */
    const float c2 = 0.044715f;
    for (size_t i = 0; i < A->nelems; i++) {
        float x = a[i];
        float inner = c1 * (x + c2 * x * x * x);
        c[i] = 0.5f * x * (1.0f + tanhf(inner));
    }
    (void)params;
    return INFRA_OK;
}

static infra_op_t relu_op = {
    .name = "relu", .func = op_relu_impl,
    .min_inputs = 1, .max_inputs = 1,
    .min_outputs = 1, .max_outputs = 1,
    .description = "ReLU 激活",
};
static infra_op_t gelu_op = {
    .name = "gelu", .func = op_gelu_impl,
    .min_inputs = 1, .max_inputs = 1,
    .min_outputs = 1, .max_outputs = 1,
    .description = "GELU 激活（tanh 近似）",
};

void register_op_activations(void) {
    infra_op_register(&relu_op);
    infra_op_register(&gelu_op);
}