#include "infra/op.h"
#include <math.h>
#include <float.h>

static infra_status_t op_softmax_impl(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    if (num_inputs < 1 || num_outputs < 1) return INFRA_ERR_PARAM;
    infra_tensor_t* A = inputs[0];
    infra_tensor_t* C = outputs[0];
    if (!A || !C) return INFRA_ERR_PARAM;

    int64_t last_dim = A->dims[A->ndim - 1];
    int64_t outer = A->nelems / last_dim;
    const float* a = (const float*)A->data;
    float* c = (float*)C->data;

    for (int64_t i = 0; i < outer; i++) {
        const float* row = a + i * last_dim;
        float* crow = c + i * last_dim;
        float maxv = -FLT_MAX;
        for (int64_t j = 0; j < last_dim; j++) {
            if (row[j] > maxv) maxv = row[j];
        }
        float sum = 0.0f;
        for (int64_t j = 0; j < last_dim; j++) {
            crow[j] = expf(row[j] - maxv);
            sum += crow[j];
        }
        for (int64_t j = 0; j < last_dim; j++) {
            crow[j] /= sum;
        }
    }
    (void)params;
    return INFRA_OK;
}

static infra_op_t softmax_op = {
    .name = "softmax", .func = op_softmax_impl,
    .min_inputs = 1, .max_inputs = 1,
    .min_outputs = 1, .max_outputs = 1,
    .description = "Softmax（沿最后一维）",
};

void register_op_softmax(void) { infra_op_register(&softmax_op); }