#include <stdint.h>
#include <string.h>

#include "infra/op.h"

static infra_status_t op_gather_impl(infra_tensor_t **inputs, int num_inputs, infra_tensor_t **outputs,
                                     int num_outputs, const void *params)
{
    if (!inputs || !outputs || num_inputs < 2 || num_outputs < 1) {
        return INFRA_ERR_PARAM;
    }

    infra_tensor_t *weight = inputs[0];
    infra_tensor_t *indices = inputs[1];
    infra_tensor_t *output = outputs[0];
    if (!weight || !indices || !output || !weight->data || !indices->data || !output->data) {
        return INFRA_ERR_PARAM;
    }
    if (weight->ndim != 2 || indices->ndim != 1 || output->ndim != 2) {
        return INFRA_ERR_PARAM;
    }
    if (weight->dtype != INFRA_DTYPE_F32 || indices->dtype != INFRA_DTYPE_I32 || output->dtype != INFRA_DTYPE_F32) {
        return INFRA_ERR_PARAM;
    }

    int64_t vocab_size = weight->dims[0];
    int64_t embedding_dim = weight->dims[1];
    int64_t sequence_length = indices->dims[0];
    if (vocab_size <= 0 || embedding_dim <= 0 || sequence_length <= 0 || output->dims[0] != sequence_length ||
        output->dims[1] != embedding_dim) {
        return INFRA_ERR_PARAM;
    }

    const float *weight_data = (const float *)weight->data;
    const int32_t *indices_data = (const int32_t *)indices->data;
    float *output_data = (float *)output->data;
    size_t row_bytes = (size_t)embedding_dim * sizeof(float);
    for (int64_t i = 0; i < sequence_length; i++) {
        int32_t index = indices_data[i];
        if (index < 0 || (int64_t)index >= vocab_size) {
            return INFRA_ERR_PARAM;
        }
        memcpy(output_data + i * embedding_dim, weight_data + (int64_t)index * embedding_dim, row_bytes);
    }

    (void)params;
    return INFRA_OK;
}

static infra_op_t gather_op = {
    .name = "gather",
    .func = op_gather_impl,
    .min_inputs = 2,
    .max_inputs = 2,
    .min_outputs = 1,
    .max_outputs = 1,
    .description = "按 Token ID 查找嵌入向量",
};

void register_op_gather(void)
{
    infra_op_register(&gather_op);
}
