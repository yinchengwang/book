#include "pq.h"

#include <algo-prod/Kmeans/kmeans.h>

/* ──────────────────────────────────────────────────────────────────────────
 * 辅助：码本指�?
 * ────────────────────────────────────────────────────────────────────────── */

static float *pq_codebook_ptr(const quantizer_t *q, int32_t sub)
{
    return &q->codebooks[sub * q->max_codebook_size * q->subvector_dims];
}

/* ──────────────────────────────────────────────────────────────────────────
 * 生命周期
 * ────────────────────────────────────────────────────────────────────────── */

int32_t pq_init(quantizer_t *q)
{
    size_t count;

    q->subvector_dims    = q->config.dims / q->config.pq_subquantizers;
    q->max_codebook_size = 1 << q->config.pq_bits;
    q->codebook_size     = q->max_codebook_size;

    count = (size_t)q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims;
    q->codebooks = (float *)calloc(count, sizeof(float));
    return q->codebooks ? 0 : -1;
}

void pq_free(quantizer_t *q)
{
    free(q->codebooks);
    q->codebooks = NULL;
}

void pq_reset(quantizer_t *q)
{
    q->trained       = false;
    q->codebook_size = q->max_codebook_size;
    memset(q->codebooks,
           0,
           (size_t)q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims * sizeof(float));
}

/* ──────────────────────────────────────────────────────────────────────────
 * 训练：对每个子向量空间运�?K-Means，码心即码本
 * ────────────────────────────────────────────────────────────────────────── */

int32_t algo_pq_train(quantizer_t *q, int32_t n, const float *vectors)
{
    int32_t sub;

    q->codebook_size = q->max_codebook_size < n ? q->max_codebook_size : n;
    if (q->codebook_size <= 0) {
        return -1;
    }

    memset(q->codebooks,
           0,
           (size_t)q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims * sizeof(float));

    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        double *train_data;
        KMeansParams params;
        float *codebook;
        int32_t s;
        int32_t d;

        train_data = (double *)malloc((size_t)n * q->subvector_dims * sizeof(double));
        if (!train_data) {
            return -1;
        }

        for (s = 0; s < n; ++s) {
            const float *sv = &vectors[s * q->config.dims + sub * q->subvector_dims];
            for (d = 0; d < q->subvector_dims; ++d) {
                train_data[s * q->subvector_dims + d] = (double)sv[d];
            }
        }

        memset(&params, 0, sizeof(params));
        params.n_clusters   = q->codebook_size;
        params.n_init       = 8;
        params.max_iter     = 100;
        params.tol          = 1e-4;
        params.verbose      = 0;
        params.random_state = 42 + sub;
        params.init         = "k-means++";
        params.algorithm    = "lloyd";
        params.n_samples    = n;
        params.n_features   = q->subvector_dims;
        params.data         = train_data;

        Kmeans(&params);
        free(train_data);

        if (!params.success || !params.cluster_centers) {
            KmeansFree(&params);
            return -1;
        }

        codebook = pq_codebook_ptr(q, sub);
        for (s = 0; s < q->codebook_size; ++s) {
            for (d = 0; d < q->subvector_dims; ++d) {
                codebook[s * q->subvector_dims + d] =
                    (float)params.cluster_centers[s * q->subvector_dims + d];
            }
        }

        KmeansFree(&params);
    }

    q->trained = true;
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 编码：每个子向量找最近码�?
 * ────────────────────────────────────────────────────────────────────────── */

int32_t algo_pq_encode(const quantizer_t *q, const float *vector, uint8_t *code)
{
    int32_t sub;

    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        const float *sv       = &vector[sub * q->subvector_dims];
        const float *codebook = pq_codebook_ptr(q, sub);
        float best_dist       = FLT_MAX;
        int32_t best_code     = 0;
        int32_t c;

        for (c = 0; c < q->codebook_size; ++c) {
            float d = distance_l2sqr(sv, &codebook[c * q->subvector_dims], q->subvector_dims);
            if (d < best_dist) {
                best_dist = d;
                best_code = c;
            }
        }

        code[sub] = (uint8_t)best_code;
    }

    return 0;
}

int32_t algo_pq_encode_batch(const quantizer_t *q,
                              int32_t n,
                              const float *vectors,
                              uint8_t *codes)
{
    int32_t i;
    int32_t cs = algo_pq_code_size(q);

    for (i = 0; i < n; ++i) {
        if (algo_pq_encode(q, &vectors[i * q->config.dims], &codes[i * cs]) != 0) {
            return -1;
        }
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * ADC 距离：预计算每个子空间的距离表后查表累加
 * ────────────────────────────────────────────────────────────────────────── */

int32_t algo_pq_compute_distance_table(const quantizer_t *q,
                                        distance_metric_t metric,
                                        const float *query,
                                        float *table)
{
    int32_t sub;

    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        const float *sq       = &query[sub * q->subvector_dims];
        const float *codebook = pq_codebook_ptr(q, sub);
        int32_t c;

        for (c = 0; c < q->codebook_size; ++c) {
            table[sub * q->codebook_size + c] =
                distance_compute(metric, sq, &codebook[c * q->subvector_dims], q->subvector_dims);
        }
    }

    return 0;
}

float algo_pq_adc_distance(const quantizer_t *q,
                            const uint8_t *code,
                            const float *table)
{
    float dist = 0.0f;
    int32_t sub;

    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        dist += table[sub * q->codebook_size + code[sub]];
    }

    return dist;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 尺寸查询
 * ────────────────────────────────────────────────────────────────────────── */

int32_t algo_pq_code_size(const quantizer_t *q)
{
    return q->config.pq_subquantizers;
}

int32_t algo_pq_distance_table_size(const quantizer_t *q)
{
    return q->config.pq_subquantizers * q->codebook_size;
}

int32_t pq_model_float_count(const quantizer_t *q)
{
    return q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 模型导入 / 导出
 * ────────────────────────────────────────────────────────────────────────── */

int32_t pq_export_model(const quantizer_t *q,
                              float *codebooks,
                              int32_t count,
                              int32_t *trained_codebook_size)
{
    int32_t model_count = pq_model_float_count(q);

    if (model_count <= 0 || count < model_count) {
        return -1;
    }

    memcpy(codebooks, q->codebooks, (size_t)model_count * sizeof(float));
    *trained_codebook_size = q->codebook_size;
    return model_count;
}

int32_t pq_load_model(quantizer_t *q,
                            int32_t trained_codebook_size,
                            const float *codebooks,
                            int32_t count)
{
    int32_t model_count = pq_model_float_count(q);

    if (count < model_count || trained_codebook_size <= 0 ||
        trained_codebook_size > q->max_codebook_size) {
        return -1;
    }

    memcpy(q->codebooks, codebooks, (size_t)model_count * sizeof(float));
    q->codebook_size = trained_codebook_size;
    q->trained       = true;
    return 0;
}
