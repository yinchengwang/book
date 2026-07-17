#include "infra/op.h"
#include <math.h>

static infra_status_t op_layernorm_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 1 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* C = outputs[0];
    if (!A || !C) return INFRA_ERR_PARAM;

    const op_norm_params_t* p = (const op_norm_params_t*)params;
    float eps = (p ? p->eps : 1e-5f);

    int64_t last_dim = A->dims[A->ndim - 1];
    int64_t outer = A->nelems / last_dim;
    const float* a = (const float*)A->data;
    float* c = (float*)C->data;

    /* 可选 gamma/beta（从第 2、3 输入读取） */
    const float* gamma = NULL;
    const float* beta = NULL;
    if (num_inputs >= 3) {
        gamma = (const float*)inputs[1]->data;
        beta = (const float*)inputs[2]->data;
    }

    for (int64_t i = 0; i < outer; i++) {
        const float* row = a + i * last_dim;
        float* crow = c + i * last_dim;
        float mean = 0.0f;
        for (int64_t j = 0; j < last_dim; j++) mean += row[j];
        mean /= (float)last_dim;
        float var = 0.0f;
        for (int64_t j = 0; j < last_dim; j++) {
            float d = row[j] - mean;
            var += d * d;
        }
        var /= (float)last_dim;
        float invstd = 1.0f / sqrtf(var + eps);
        for (int64_t j = 0; j < last_dim; j++) {
            float x = (row[j] - mean) * invstd;
            if (gamma) x *= gamma[j];
            if (beta) x += beta[j];
            crow[j] = x;
        }
    }
    return INFRA_OK;
}

static infra_op_t layernorm_op = {
    .name = "layernorm", .func = op_layernorm_impl,
    .min_inputs = 1, .max_inputs = 3,
    .min_outputs = 1, .max_outputs = 1,
    .description = "Layer Normalization",
};

void register_op_norm(void) { infra_op_register(&layernorm_op); }