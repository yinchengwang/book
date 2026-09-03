/* diskann quantization helpers */

#include "diskann_private.h"

/* encoding helpers */

int diskann_maybe_encode_vector(diskann_t *index, int32_t id)
{
    if (!diskann_pq_ready(index) || !index->codes || id < 0 || id >= index->n_total) {
        return 0;
    }

    return quantizer_encode(index->quantizer,
                                 &index->vectors[id * index->dims],
                                 &index->codes[id * index->pq_code_size]);
}

int diskann_encode_all_vectors(diskann_t *index)
{
    int32_t i;

    if (!diskann_pq_ready(index)) {
        return 0;
    }
    if (diskann_ensure_code_storage(index) != 0) {
        return -1;
    }

    for (i = 0; i < index->n_total; ++i) {
        if (quantizer_encode(index->quantizer,
                                  &index->vectors[i * index->dims],
                                  &index->codes[i * index->pq_code_size]) != 0) {
            return -1;
        }
    }
    return 0;
}

/* quantizer lifecycle helpers */

int diskann_ensure_quantizer(diskann_t *index)
{
    if (!diskann_pq_enabled(index)) {
        quantizer_drop(index->quantizer);
        index->quantizer = NULL;
        index->pq_trained_codebook_size = 0;
        return 0;
    }

    if (!index->quantizer) {
        index->quantizer = quantizer_create(&index->quantizer_config);
        if (!index->quantizer) {
            return -1;
        }
    }

    return 0;
}

int diskann_gather_training_vectors(const diskann_t *index, float **vectors_out, int32_t *count_out)
{
    float *samples;
    int32_t sample_count;
    int32_t *active_ids;
    int32_t active_count;
    int32_t i;

    if (!index || !vectors_out || !count_out) {
        return -1;
    }

    *vectors_out = NULL;
    *count_out = 0;
    if (index->active_count <= 0) {
        return -1;
    }
    if (diskann_collect_active_ids(index, &active_ids, &active_count) != 0) {
        return -1;
    }

    sample_count = diskann_min_i32(active_count, diskann_max_i32(index->quantization_params.train_max_vectors, 1));
    samples = (float *)malloc((size_t)sample_count * (size_t)index->dims * sizeof(float));
    if (!samples) {
        free(active_ids);
        return -1;
    }

    for (i = 0; i < sample_count; ++i) {
        int32_t source_idx = (int32_t)(((int64_t)i * (int64_t)active_count) / (int64_t)sample_count);
        if (source_idx >= active_count) {
            source_idx = active_count - 1;
        }
        memcpy(&samples[i * index->dims],
               &index->vectors[active_ids[source_idx] * index->dims],
               (size_t)index->dims * sizeof(float));
    }

    free(active_ids);
    *vectors_out = samples;
    *count_out = sample_count;
    return 0;
}

/* search-time pq helper */

float diskann_candidate_distance_from_query(const diskann_t *index,
                                                  const float *query,
                                                  const float *distance_table,
                                                  int32_t id)
{
    if (diskann_pq_ready(index) && distance_table && index->codes) {
        return quantizer_adc_distance(index->quantizer,
                                           &index->codes[id * index->pq_code_size],
                                           distance_table);
    }

    return diskann_exact_distance_from_query(index, query, id);
}
