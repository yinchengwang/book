#ifndef QUANTIZATION_PQ_H
#define QUANTIZATION_PQ_H

#include "quantization_private.h"

/* ── 内部 PQ 函数，仅�?quantization.c 调用 ── */

int32_t pq_init(quantizer_t *q);
void    pq_free(quantizer_t *q);
void    pq_reset(quantizer_t *q);

int32_t pq_train(quantizer_t *q, int32_t n, const float *vectors);

int32_t pq_encode(const quantizer_t *q, const float *vector, uint8_t *code);
int32_t pq_encode_batch(const quantizer_t *q, int32_t n, const float *vectors, uint8_t *codes);

int32_t pq_compute_distance_table(const quantizer_t *q,
                                        distance_metric_t metric,
                                        const float *query,
                                        float *table);
float   pq_adc_distance(const quantizer_t *q,
                              const uint8_t *code,
                              const float *table);

int32_t pq_code_size(const quantizer_t *q);
int32_t pq_distance_table_size(const quantizer_t *q);
int32_t pq_model_float_count(const quantizer_t *q);
int32_t pq_export_model(const quantizer_t *q,
                              float *codebooks,
                              int32_t count,
                              int32_t *trained_codebook_size);
int32_t pq_load_model(quantizer_t *q,
                            int32_t trained_codebook_size,
                            const float *codebooks,
                            int32_t count);

#endif /* QUANTIZATION_PQ_H */
