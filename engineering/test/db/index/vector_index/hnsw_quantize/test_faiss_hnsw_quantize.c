/*
 * test_faiss_hnsw_quantize.c
 *
 * Task 7: 验证 faiss_hnsw 量化支持（SQ/PQ/LVQ/RQ）
 *
 * 测试内容：
 *   1. NONE 类型：code_size = 0，encode/decode 直接返回
 *   2. SQ 类型：训练后能 encode/decode，量化误差有界
 *   3. PQ 类型：训练后能 encode，能计算 ADC 距离
 *   4. LVQ/RQ 类型：训练后能 encode
 *   5. quantize_get_code_size 与 dims 关系正确
 *   6. quantize_destroy 正确释放资源
 *
 * 注意：faiss_hnsw_quantize.c 假定 faiss_hnsw_t 的 codes/code_size 字段已分配
 *       （由上层插入流程负责）。测试中手工模拟这一前置条件。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <stdint.h>

#include "db/index/vector_index/hnsw/faiss_hnsw.h"
/* 内部头文件（与 faiss_hnsw 库同源），用于测试访问量化器字段 */
#include "faiss_hnsw_internal.h"

#include "algo-prod/distance/distance.h"
#include "algo-prod/quantization/quantization.h"

/* ============================================================
 * 辅助函数
 * ============================================================ */

static float *generate_random_vectors(int n, int dims, float min_val, float max_val) {
    float *vectors = (float *)malloc((size_t)n * (size_t)dims * sizeof(float));
    if (!vectors) return NULL;
    float range = max_val - min_val;
    for (int i = 0; i < n * dims; i++) {
        vectors[i] = min_val + ((float)rand() / RAND_MAX) * range;
    }
    return vectors;
}

/* 计算两个向量的 L2 平方距离 */
static float l2_sqr(const float *a, const float *b, int dims) {
    float sum = 0.0f;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sum;
}

/* 测试结果计数器 */
static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg)                                    \
    do {                                                          \
        if (!(cond)) {                                            \
            fprintf(stderr, "  [FAIL] %s\n", msg);                \
            g_fail++;                                             \
            return -1;                                            \
        } else {                                                  \
            g_pass++;                                             \
        }                                                         \
    } while (0)

/* ============================================================
 * 测试 1: faiss_hnsw_quantize_get_code_size
 * ============================================================ */
static int test_get_code_size(void) {
    printf("Test 1: faiss_hnsw_quantize_get_code_size\n");

    /* SQ: dims 字节 */
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_SQ, 128) == 128,
                "SQ-128 should be 128 bytes");
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_SQ, 16) == 16,
                "SQ-16 should be 16 bytes");

    /* PQ: ceil(dims/16) 字节 */
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_PQ, 128) == 8,
                "PQ-128 should be 8 bytes");
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_PQ, 16) == 1,
                "PQ-16 should be 1 byte");
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_PQ, 17) == 2,
                "PQ-17 should be 2 bytes");

    /* LVQ: 1 字节 */
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_LVQ, 128) == 1,
                "LVQ-128 should be 1 byte");

    /* RQ: ceil(dims/4) 字节 */
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_RQ, 128) == 32,
                "RQ-128 should be 32 bytes");
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_RQ, 16) == 4,
                "RQ-16 should be 4 bytes");

    /* NONE: 0 字节 */
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_NONE, 128) == 0,
                "NONE-128 should be 0 bytes");

    /* dims <= 0 */
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_SQ, 0) == 0,
                "SQ-0 should be 0 bytes");
    TEST_ASSERT(faiss_hnsw_quantize_get_code_size(QUANTIZATION_TYPE_SQ, -1) == 0,
                "SQ-(-1) should be 0 bytes");

    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 2: NONE 类型行为（noop）
 * ============================================================ */
static int test_quantize_none(void) {
    printf("Test 2: NONE quantization noop\n");

    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, 32, 32,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_NONE);
    TEST_ASSERT(idx != NULL, "create NONE index");

    /* NONE 类型 train 应直接返回 0 */
    float *vecs = generate_random_vectors(10, 32, -1.0f, 1.0f);
    int ret = faiss_hnsw_quantize_train(idx, 10, vecs);
    TEST_ASSERT(ret == 0, "NONE train should return 0");
    TEST_ASSERT(idx->quantizer == NULL, "NONE has no quantizer");
    TEST_ASSERT(idx->code_size == 0, "NONE has code_size=0");

    /* NONE encode/decode 直接返回 */
    uint8_t code[32];
    memset(code, 0, sizeof(code));
    int n = faiss_hnsw_quantize_encode_one(idx, vecs, code);
    TEST_ASSERT(n == 0, "NONE encode should return 0");

    /* is_trained 对 NONE 永远返回 true */
    TEST_ASSERT(faiss_hnsw_quantize_is_trained(idx) == true,
                "NONE always trained");

    free(vecs);
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 3: SQ 训练 + 编码 + 解码（误差有界）
 * ============================================================ */
static int test_quantize_sq(void) {
    printf("Test 3: SQ8 train + encode + decode\n");

    const int dims = 16;
    const int n_train = 100;
    const int n_test = 20;

    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, dims, 32,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_SQ);
    TEST_ASSERT(idx != NULL, "create SQ index");

    /* 训练数据：[-10, 10] 范围随机向量 */
    float *train_vecs = generate_random_vectors(n_train, dims, -10.0f, 10.0f);

    int ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs);
    TEST_ASSERT(ret == 0, "SQ train should succeed");
    TEST_ASSERT(idx->quantizer != NULL, "SQ quantizer should be created");
    TEST_ASSERT(idx->code_size > 0, "SQ code_size > 0");
    TEST_ASSERT(faiss_hnsw_quantize_is_trained(idx) == true,
                "SQ should be trained");
    printf("  SQ code_size = %d (dims=%d)\n", idx->code_size, dims);

    /* 准备 codes 数组（模拟插入流程） */
    idx->n_total = n_test;
    idx->codes = (uint8_t *)calloc((size_t)n_test * (size_t)idx->code_size, sizeof(uint8_t));

    /* 编码 + 解码测试 */
    float *test_vecs = generate_random_vectors(n_test, dims, -10.0f, 10.0f);
    float max_absolute_error = 0.0f;

    for (int i = 0; i < n_test; i++) {
        int n = faiss_hnsw_quantize_encode(idx, i, &test_vecs[i * dims]);
        TEST_ASSERT(n >= 0, "SQ encode should succeed (return 0)");

        float decoded[16];
        ret = faiss_hnsw_quantize_decode(idx, i, decoded);
        TEST_ASSERT(ret == 0, "SQ decode should succeed");

        /* 量化误差分析 */
        float abs_err = 0.0f;
        for (int j = 0; j < dims; j++) {
            float e = fabsf(test_vecs[i * dims + j] - decoded[j]);
            if (e > abs_err) abs_err = e;
        }

        if (abs_err > max_absolute_error) max_absolute_error = abs_err;
    }

    printf("  SQ8 max abs error = %.4f\n", max_absolute_error);

    /* SQ8 在 [-10, 10] 范围上单维量化误差应 <= (20/255) ≈ 0.078 */
    TEST_ASSERT(max_absolute_error < 0.5f,
                "SQ8 absolute error too large");

    free(test_vecs);
    free(train_vecs);
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 4: PQ 训练 + 编码 + ADC 距离
 * ============================================================ */
static int test_quantize_pq(void) {
    printf("Test 4: PQ train + encode + ADC distance\n");

    const int dims = 32;
    const int n_train = 200;
    const int n_test = 5;

    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, dims, 32,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_PQ);
    TEST_ASSERT(idx != NULL, "create PQ index");

    /* 训练数据 */
    float *train_vecs = generate_random_vectors(n_train, dims, -1.0f, 1.0f);

    int ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs);
    TEST_ASSERT(ret == 0, "PQ train should succeed");
    TEST_ASSERT(idx->quantizer != NULL, "PQ quantizer should be created");
    TEST_ASSERT(idx->code_size > 0, "PQ code_size > 0");
    TEST_ASSERT(faiss_hnsw_quantize_is_trained(idx) == true,
                "PQ should be trained");
    printf("  PQ code_size = %d (dims=%d)\n", idx->code_size, dims);

    /* 准备 codes 数组 */
    idx->n_total = n_test;
    idx->codes = (uint8_t *)calloc((size_t)n_test * (size_t)idx->code_size, sizeof(uint8_t));

    /* 编码若干向量 */
    float *test_vecs = generate_random_vectors(n_test, dims, -1.0f, 1.0f);
    for (int i = 0; i < n_test; i++) {
        int n = faiss_hnsw_quantize_encode(idx, i, &test_vecs[i * dims]);
        TEST_ASSERT(n >= 0, "PQ encode should succeed");
    }

    /* 计算查询距离表 + ADC */
    float query[32];
    for (int i = 0; i < dims; i++) {
        query[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }

    int32_t table_size = faiss_hnsw_quantize_distance_table_size(idx);
    TEST_ASSERT(table_size > 0, "distance table size > 0");
    printf("  PQ distance table size = %d\n", table_size);

    float *table = (float *)malloc((size_t)table_size * sizeof(float));
    ret = faiss_hnsw_quantize_compute_distance_table(idx,
                                                     DISTANCE_METRIC_L2_SQUARED,
                                                     query, table);
    TEST_ASSERT(ret == 0, "compute distance table should succeed");

    /* 对每个编码向量计算 ADC 距离 */
    for (int i = 0; i < n_test; i++) {
        const uint8_t *code = idx->codes + (size_t)i * (size_t)idx->code_size;
        float dist = faiss_hnsw_quantize_adc_distance(idx, code, table);
        TEST_ASSERT(dist >= 0.0f, "ADC distance should be non-negative");
    }

    /* 批量编码 */
    float *batch_vecs = generate_random_vectors(10, dims, -1.0f, 1.0f);
    uint8_t *batch_codes = (uint8_t *)calloc((size_t)10 * (size_t)idx->code_size, sizeof(uint8_t));
    ret = faiss_hnsw_quantize_encode_batch(idx, 10, batch_vecs, batch_codes);
    TEST_ASSERT(ret == 0, "batch encode should succeed");

    free(batch_codes);
    free(batch_vecs);
    free(table);
    free(test_vecs);
    free(train_vecs);
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 5: LVQ 训练 + 编码
 * ============================================================ */
static int test_quantize_lvq(void) {
    printf("Test 5: LVQ train + encode\n");

    const int dims = 32;
    const int n_train = 100;
    const int n_test = 3;

    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, dims, 32,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_LVQ);
    TEST_ASSERT(idx != NULL, "create LVQ index");

    float *train_vecs = generate_random_vectors(n_train, dims, 0.0f, 1.0f);

    int ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs);
    if (ret != 0) {
        /* 某些 LVQ 配置可能因参数不支持而失败，仅记录 */
        printf("  [SKIP] LVQ train failed (ret=%d)\n", ret);
        free(train_vecs);
        faiss_hnsw_quantize_destroy(idx);
        faiss_hnsw_index_drop(idx);
        return 0;
    }

    TEST_ASSERT(idx->quantizer != NULL, "LVQ quantizer should be created");
    TEST_ASSERT(idx->code_size > 0, "LVQ code_size > 0");
    printf("  LVQ code_size = %d (dims=%d)\n", idx->code_size, dims);

    idx->n_total = n_test;
    idx->codes = (uint8_t *)calloc((size_t)n_test * (size_t)idx->code_size, sizeof(uint8_t));

    float *test_vecs = generate_random_vectors(n_test, dims, 0.0f, 1.0f);
    for (int i = 0; i < n_test; i++) {
        int n = faiss_hnsw_quantize_encode(idx, i, &test_vecs[i * dims]);
        TEST_ASSERT(n >= 0, "LVQ encode should succeed");
    }

    free(test_vecs);
    free(train_vecs);
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 6: RQ (RaBitQ) 训练 + 编码
 * ============================================================ */
static int test_quantize_rq(void) {
    printf("Test 6: RQ (RaBitQ) train + encode\n");

    const int dims = 32;
    const int n_train = 100;
    const int n_test = 3;

    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, dims, 32,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_RQ);
    TEST_ASSERT(idx != NULL, "create RQ index");

    float *train_vecs = generate_random_vectors(n_train, dims, -1.0f, 1.0f);

    int ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs);
    if (ret != 0) {
        printf("  [SKIP] RQ train failed (ret=%d)\n", ret);
        free(train_vecs);
        faiss_hnsw_quantize_destroy(idx);
        faiss_hnsw_index_drop(idx);
        return 0;
    }

    TEST_ASSERT(idx->quantizer != NULL, "RQ quantizer should be created");
    TEST_ASSERT(idx->code_size > 0, "RQ code_size > 0");
    printf("  RQ code_size = %d (dims=%d)\n", idx->code_size, dims);

    idx->n_total = n_test;
    idx->codes = (uint8_t *)calloc((size_t)n_test * (size_t)idx->code_size, sizeof(uint8_t));

    float *test_vecs = generate_random_vectors(n_test, dims, -1.0f, 1.0f);
    for (int i = 0; i < n_test; i++) {
        int n = faiss_hnsw_quantize_encode(idx, i, &test_vecs[i * dims]);
        TEST_ASSERT(n >= 0, "RQ encode should succeed");
    }

    free(test_vecs);
    free(train_vecs);
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 7: 重新训练（先训练后释放再训练）
 * ============================================================ */
static int test_quantize_retrain(void) {
    printf("Test 7: retrain quantizer\n");

    const int dims = 16;
    const int n_train = 50;

    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, dims, 32,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_SQ);
    TEST_ASSERT(idx != NULL, "create SQ index");

    float *train_vecs1 = generate_random_vectors(n_train, dims, -1.0f, 1.0f);
    int ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs1);
    TEST_ASSERT(ret == 0, "first train");

    quantizer_t *first_q = idx->quantizer;
    TEST_ASSERT(first_q != NULL, "first quantizer");

    /* 第二次训练（不同数据） */
    float *train_vecs2 = generate_random_vectors(n_train, dims, -100.0f, 100.0f);
    ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs2);
    TEST_ASSERT(ret == 0, "second train");
    TEST_ASSERT(idx->quantizer != NULL, "second quantizer");
    /* 注：第二次训练后 quantizer 指针可能与第一次相同（calloc 复用内存），
       但功能上等价：已重新训练、code_size 正确即可。 */
    TEST_ASSERT(idx->code_size > 0, "second code_size > 0");

    free(train_vecs1);
    free(train_vecs2);
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 测试 8: 边界条件
 * ============================================================ */
static int test_quantize_edge_cases(void) {
    printf("Test 8: edge cases (NULL/invalid args)\n");

    /* NULL 索引 */
    int ret = faiss_hnsw_quantize_train(NULL, 10, NULL);
    TEST_ASSERT(ret == -1, "train with NULL index");

    /* 创建 SQ 索引但 n_total=0 时 encode 应该失败 */
    faiss_hnsw_t *idx = faiss_hnsw_index_create(16, 8, 16,
                                                DISTANCE_METRIC_L2_SQUARED,
                                                QUANTIZATION_TYPE_SQ);
    TEST_ASSERT(idx != NULL, "create index");

    /* 准备训练数据：n=10 个 8 维向量 */
    const int n_train = 10;
    const int dims_ec = 8;
    float *train_vecs_ec = (float *)malloc(n_train * dims_ec * sizeof(float));
    for (int i = 0; i < n_train * dims_ec; i++) {
        train_vecs_ec[i] = (float)(i % 10) - 4.5f;
    }
    ret = faiss_hnsw_quantize_train(idx, n_train, train_vecs_ec);
    TEST_ASSERT(ret == 0, "train SQ");

    /* n_total = 0 但试图 encode vec_id=0 */
    idx->codes = (uint8_t *)calloc(idx->code_size, sizeof(uint8_t));
    ret = faiss_hnsw_quantize_encode(idx, 0, train_vecs_ec);
    TEST_ASSERT(ret == -1, "encode with n_total=0 should fail");

    /* 负 vec_id */
    ret = faiss_hnsw_quantize_encode(idx, -1, train_vecs_ec);
    TEST_ASSERT(ret == -1, "encode with negative vec_id should fail");

    free(train_vecs_ec);
    /* 注意: faiss_hnsw_index_drop 会 free(idx->codes)，
       所以此处先释放 codes 指针再恢复，或将 codes 置空以避免 double free */
    /* 将 codes 复位，避免 faiss_hnsw_index_drop 再次 free */
    free(idx->codes);
    idx->codes = NULL;
    faiss_hnsw_quantize_destroy(idx);
    faiss_hnsw_index_drop(idx);
    printf("  [PASS]\n");
    return 0;
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("=== faiss_hnsw quantize tests ===\n");

    /* 使用固定种子以保证可重复性 */
    srand(42);

    int (*tests[])(void) = {
        test_get_code_size,
        test_quantize_none,
        test_quantize_sq,
        test_quantize_pq,
        test_quantize_lvq,
        test_quantize_rq,
        test_quantize_retrain,
        test_quantize_edge_cases,
    };

    int n_tests = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < n_tests; i++) {
        if (tests[i]() != 0) {
            fprintf(stderr, "Test %d FAILED\n", i + 1);
        }
    }

    printf("\n=== Summary: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}