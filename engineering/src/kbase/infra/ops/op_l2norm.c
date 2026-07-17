#include <math.h>

#include "infra/op.h"

static infra_status_t op_l2norm_impl(infra_tensor_t **inputs, int num_inputs, infra_tensor_t **outputs,
                                     int num_outputs, const void *params)
{
    if (!inputs || !outputs || num_inputs < 1 || num_outputs < 1) {
        return INFRA_ERR_PARAM;
    }

    infra_tensor_t *input = inputs[0];
    infra_tensor_t *output = outputs[0];
    if (!input || !output || !input->data || !output->data) {
        return INFRA_ERR_PARAM;
    }
    if ((input->ndim != 1 && input->ndim != 2) || output->ndim != input->ndim ||
        input->dtype != INFRA_DTYPE_F32 || output->dtype != INFRA_DTYPE_F32 ||
        output->nelems != input->nelems) {
        return INFRA_ERR_PARAM;
    }
    for (int i = 0; i < input->ndim; i++) {
        if (output->dims[i] != input->dims[i]) {
            return INFRA_ERR_PARAM;
        }
    }

    int64_t embedding_dim = input->dims[input->ndim - 1];
    if (embedding_dim <= 0) {
        return INFRA_ERR_PARAM;
    }
    int64_t row_count = (int64_t)input->nelems / embedding_dim;
    const float *input_data = (const float *)input->data;
    float *output_data = (float *)output->data;
    for (int64_t row_index = 0; row_index < row_count; row_index++) {
        const float *input_row = input_data + row_index * embedding_dim;
        float *output_row = output_data + row_index * embedding_dim;
        float squared_sum = 0.0f;
        for (int64_t column_index = 0; column_index < embedding_dim; column_index++) {
            squared_sum += input_row[column_index] * input_row[column_index];
        }

        float norm = sqrtf(squared_sum);
        if (norm == 0.0f) {
            for (int64_t column_index = 0; column_index < embedding_dim; column_index++) {
                output_row[column_index] = 0.0f;
            }
            continue;
        }
        for (int64_t column_index = 0; column_index < embedding_dim; column_index++) {
            output_row[column_index] = input_row[column_index] / norm;
        }
    }

    (void)params;
    return INFRA_OK;
}

static infra_op_t l2norm_op = {
    .name = "l2norm",
    .func = op_l2norm_impl,
    .min_inputs = 1,
    .max_inputs = 1,
    .min_outputs = 1,
    .max_outputs = 1,
    .description = "逐行执行 L2 归一化",
};

void register_op_l2norm(void)
{
    infra_op_register(&l2norm_op);
}
