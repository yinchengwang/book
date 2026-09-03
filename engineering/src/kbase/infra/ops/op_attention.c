#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "infra/op.h"

static infra_status_t validate_attention_tensors(const infra_tensor_t *query, const infra_tensor_t *key,
                                                 const infra_tensor_t *value, const infra_tensor_t *output)
{
    if (!query || !key || !value || !output || !query->data || !key->data || !value->data || !output->data) {
        return INFRA_ERR_PARAM;
    }
    if (query->ndim != 2 || key->ndim != 2 || value->ndim != 2 || output->ndim != 2) {
        return INFRA_ERR_PARAM;
    }
    if (query->dtype != INFRA_DTYPE_F32 || key->dtype != INFRA_DTYPE_F32 || value->dtype != INFRA_DTYPE_F32 ||
        output->dtype != INFRA_DTYPE_F32) {
        return INFRA_ERR_PARAM;
    }
    if (query->dims[0] <= 0 || query->dims[1] <= 0 || key->dims[0] != query->dims[0] ||
        key->dims[1] != query->dims[1] || value->dims[0] != query->dims[0] || value->dims[1] != query->dims[1] ||
        output->dims[0] != query->dims[0] || output->dims[1] != query->dims[1]) {
        return INFRA_ERR_PARAM;
    }
    return INFRA_OK;
}

static infra_status_t op_attention_impl(infra_tensor_t **inputs, int num_inputs, infra_tensor_t **outputs,
                                        int num_outputs, const void *params)
{
    if (!inputs || !outputs || num_inputs < 3 || num_outputs < 1) {
        return INFRA_ERR_PARAM;
    }

    infra_tensor_t *query = inputs[0];
    infra_tensor_t *key = inputs[1];
    infra_tensor_t *value = inputs[2];
    infra_tensor_t *output = outputs[0];
    infra_status_t status = validate_attention_tensors(query, key, value, output);
    if (status != INFRA_OK) {
        return status;
    }

    const op_attention_params_t *attention_params = (const op_attention_params_t *)params;
    int num_heads = attention_params ? attention_params->num_heads : 1;
    int64_t sequence_length = query->dims[0];
    int64_t embedding_dim = query->dims[1];
    if (num_heads <= 0 || embedding_dim % num_heads != 0) {
        return INFRA_ERR_PARAM;
    }

    int64_t head_dim = embedding_dim / num_heads;
    if ((uint64_t)sequence_length > SIZE_MAX / (uint64_t)sequence_length ||
        (size_t)sequence_length * (size_t)sequence_length > SIZE_MAX / sizeof(float)) {
        return INFRA_ERR_MEMORY;
    }
    float scale = attention_params && attention_params->scale > 0.0f
                      ? attention_params->scale
                      : 1.0f / sqrtf((float)head_dim);
    size_t attention_element_count = (size_t)sequence_length * (size_t)sequence_length;
    float *attention_weights = (float *)malloc(attention_element_count * sizeof(float));
    if (!attention_weights) {
        return INFRA_ERR_MEMORY;
    }

    const float *query_data = (const float *)query->data;
    const float *key_data = (const float *)key->data;
    const float *value_data = (const float *)value->data;
    float *output_data = (float *)output->data;

    for (int head_index = 0; head_index < num_heads; head_index++) {
        int64_t head_offset = (int64_t)head_index * head_dim;
        for (int64_t query_index = 0; query_index < sequence_length; query_index++) {
            float maximum_score = -FLT_MAX;
            for (int64_t key_index = 0; key_index < sequence_length; key_index++) {
                float score = 0.0f;
                for (int64_t dimension_index = 0; dimension_index < head_dim; dimension_index++) {
                    int64_t query_offset = query_index * embedding_dim + head_offset + dimension_index;
                    int64_t key_offset = key_index * embedding_dim + head_offset + dimension_index;
                    score += query_data[query_offset] * key_data[key_offset];
                }
                score *= scale;
                attention_weights[query_index * sequence_length + key_index] = score;
                if (score > maximum_score) {
                    maximum_score = score;
                }
            }

            float exponential_sum = 0.0f;
            for (int64_t key_index = 0; key_index < sequence_length; key_index++) {
                size_t weight_offset = (size_t)query_index * (size_t)sequence_length + (size_t)key_index;
                float weight = expf(attention_weights[weight_offset] - maximum_score);
                attention_weights[weight_offset] = weight;
                exponential_sum += weight;
            }
            for (int64_t key_index = 0; key_index < sequence_length; key_index++) {
                size_t weight_offset = (size_t)query_index * (size_t)sequence_length + (size_t)key_index;
                attention_weights[weight_offset] /= exponential_sum;
            }
        }

        for (int64_t query_index = 0; query_index < sequence_length; query_index++) {
            for (int64_t dimension_index = 0; dimension_index < head_dim; dimension_index++) {
                float result = 0.0f;
                for (int64_t value_index = 0; value_index < sequence_length; value_index++) {
                    size_t weight_offset = (size_t)query_index * (size_t)sequence_length + (size_t)value_index;
                    int64_t value_offset = value_index * embedding_dim + head_offset + dimension_index;
                    result += attention_weights[weight_offset] * value_data[value_offset];
                }
                output_data[query_index * embedding_dim + head_offset + dimension_index] = result;
            }
        }
    }

    free(attention_weights);
    return INFRA_OK;
}

static infra_op_t attention_op = {
    .name = "attention",
    .func = op_attention_impl,
    .min_inputs = 3,
    .max_inputs = 3,
    .min_outputs = 1,
    .max_outputs = 1,
    .description = "无 Mask 的多头注意力",
};

void register_op_attention(void)
{
    infra_op_register(&attention_op);
}
