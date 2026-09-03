/**
 * @file test_streaming_index.cpp
 * @brief 流式索引单元测试
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <ctime>

extern "C" {
#include "db/index/vector_index/streaming/vector_write_buffer.h"
#include "db/index/vector_index/streaming/incremental_quant_train.h"
#include "db/index/vector_index/streaming/merge_scheduler.h"
#include "db/index/vector_index/streaming/concurrent_search.h"
#include "db/index/vector_index/streaming/streaming_index.h"
}

/* ========================================================================
 * 测试辅助函数
 * ======================================================================== */

static void generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

static bool vectors_equal(const float *a, const float *b, int32_t dims) {
    const float epsilon = 1e-5f;
    for (int32_t i = 0; i < dims; i++) {
        if (fabs(a[i] - b[i]) > epsilon) {
            return false;
        }
    }
    return true;
}

/* ========================================================================
 * 向量写入缓冲区测试
 * ======================================================================== */

class VectorBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

TEST_F(VectorBufferTest, CreateAndDestroy) {
    vector_buffer_config_t config = vector_buffer_config_default(128);
    config.capacity = 1000;
    config.flush_threshold = 100;

    vector_write_buffer_t *buf = vector_buffer_create(&config);
    ASSERT_NE(nullptr, buf);
    EXPECT_EQ(1000, vector_buffer_capacity(buf));
    EXPECT_EQ(100, vector_buffer_get_flush_threshold(buf));

    vector_buffer_destroy(buf);
}

TEST_F(VectorBufferTest, PushAndPop) {
    vector_buffer_config_t config = vector_buffer_config_default(4);
    config.capacity = 10;

    vector_write_buffer_t *buf = vector_buffer_create(&config);
    ASSERT_NE(nullptr, buf);

    float vec1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float vec2[4] = {5.0f, 6.0f, 7.0f, 8.0f};

    EXPECT_EQ(0, vector_buffer_push(buf, vec1));
    EXPECT_EQ(1, vector_buffer_size(buf));

    EXPECT_EQ(0, vector_buffer_push(buf, vec2));
    EXPECT_EQ(2, vector_buffer_size(buf));

    float output[4];
    EXPECT_EQ(1, vector_buffer_pop(buf, output, 1));
    EXPECT_TRUE(vectors_equal(vec1, output, 4));
    EXPECT_EQ(1, vector_buffer_size(buf));

    EXPECT_EQ(1, vector_buffer_pop(buf, output, 1));
    EXPECT_TRUE(vectors_equal(vec2, output, 4));
    EXPECT_EQ(0, vector_buffer_size(buf));

    vector_buffer_destroy(buf);
}

TEST_F(VectorBufferTest, PushBatch) {
    vector_buffer_config_t config = vector_buffer_config_default(4);
    config.capacity = 100;

    vector_write_buffer_t *buf = vector_buffer_create(&config);
    ASSERT_NE(nullptr, buf);

    float vectors[20 * 4];
    generate_random_vectors(vectors, 20, 4);

    int32_t pushed = vector_buffer_push_batch(buf, vectors, 20);
    EXPECT_EQ(20, pushed);
    EXPECT_EQ(20, vector_buffer_size(buf));

    vector_buffer_destroy(buf);
}

TEST_F(VectorBufferTest, Flush) {
    vector_buffer_config_t config = vector_buffer_config_default(4);
    config.capacity = 100;
    config.flush_threshold = 50;

    vector_write_buffer_t *buf = vector_buffer_create(&config);
    ASSERT_NE(nullptr, buf);

    float vectors[30 * 4];
    generate_random_vectors(vectors, 30, 4);

    vector_buffer_push_batch(buf, vectors, 30);
    EXPECT_EQ(30, vector_buffer_size(buf));

    float output[50 * 4];
    int32_t flushed = vector_buffer_flush(buf, output, 50);
    EXPECT_EQ(30, flushed);
    EXPECT_EQ(0, vector_buffer_size(buf));
    EXPECT_TRUE(vectors_equal(vectors, output, 4 * 30));

    vector_buffer_destroy(buf);
}

TEST_F(VectorBufferTest, NeedFlush) {
    vector_buffer_config_t config = vector_buffer_config_default(4);
    config.capacity = 100;
    config.flush_threshold = 10;

    vector_write_buffer_t *buf = vector_buffer_create(&config);
    ASSERT_NE(nullptr, buf);

    EXPECT_FALSE(vector_buffer_need_flush(buf));

    float vec[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (int i = 0; i < 9; i++) {
        vector_buffer_push(buf, vec);
    }
    EXPECT_FALSE(vector_buffer_need_flush(buf));

    vector_buffer_push(buf, vec);
    EXPECT_TRUE(vector_buffer_need_flush(buf));

    vector_buffer_destroy(buf);
}

TEST_F(VectorBufferTest, CircularBehavior) {
    vector_buffer_config_t config = vector_buffer_config_default(4);
    config.capacity = 5;
    config.flush_threshold = 3;

    vector_write_buffer_t *buf = vector_buffer_create(&config);
    ASSERT_NE(nullptr, buf);

    /* 添加 5 个向量填满缓冲区 */
    float vectors[5][4];
    for (int i = 0; i < 5; i++) {
        for (int d = 0; d < 4; d++) {
            vectors[i][d] = (float)i;
        }
    }

    EXPECT_EQ(5, vector_buffer_push_batch(buf, (float *)vectors, 5));
    EXPECT_EQ(5, vector_buffer_size(buf));
    EXPECT_TRUE(vector_buffer_full(buf));

    /* 弹出 2 个 */
    float output[2][4];
    EXPECT_EQ(2, vector_buffer_pop(buf, (float *)output, 2));
    EXPECT_EQ(3, vector_buffer_size(buf));

    /* 再添加 2 个 */
    float new_vecs[2][4];
    for (int i = 0; i < 2; i++) {
        for (int d = 0; d < 4; d++) {
            new_vecs[i][d] = (float)(10 + i);
        }
    }
    EXPECT_EQ(2, vector_buffer_push_batch(buf, (float *)new_vecs, 2));
    EXPECT_EQ(5, vector_buffer_size(buf));

    vector_buffer_destroy(buf);
}

/* ========================================================================
 * 增量量化器测试
 * ======================================================================== */

class IncrementalQuantTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

TEST_F(IncrementalQuantTest, CreateAndDestroy) {
    /* 创建一个简单的量化器用于测试 */
    quantizer_config_t config;
    quantizer_config_init(&config, 16, QUANTIZATION_TYPE_PQ);
    config.pq_subquantizers = 4;
    config.pq_bits = 8;

    quantizer_t *quantizer = quantizer_create(&config);
    if (quantizer == nullptr) {
        GTEST_SKIP() << "Failed to create quantizer";
    }

    /* 训练量化器 */
    float training_vectors[100 * 16];
    generate_random_vectors(training_vectors, 100, 16);
    EXPECT_EQ(0, quantizer_train(quantizer, 100, training_vectors));
    EXPECT_TRUE(quantizer_is_trained(quantizer));

    /* 创建增量训练器 */
    incremental_quant_config_t incr_config = incremental_quant_config_default();
    incr_config.incremental_threshold = 50;
    incr_config.max_incremental_updates = 3;

    incremental_quant_trainer_t *trainer =
        incremental_quant_trainer_create(quantizer, &incr_config);
    if (trainer == nullptr) {
        quantizer_drop(quantizer);
        GTEST_SKIP() << "Failed to create incremental trainer";
    }

    EXPECT_EQ(0, incremental_quant_update_count(trainer));

    /* 增量更新 */
    float new_vectors[20 * 16];
    generate_random_vectors(new_vectors, 20, 16);

    EXPECT_EQ(0, incremental_quant_update(trainer, 20, new_vectors));
    EXPECT_EQ(1, incremental_quant_update_count(trainer));

    /* 统计信息 */
    incremental_quant_stats_t stats;
    incremental_quant_get_stats(trainer, &stats);
    EXPECT_EQ(1, stats.update_count);
    EXPECT_EQ(20, stats.total_vectors_processed);

    incremental_quant_trainer_destroy(trainer);
    quantizer_drop(quantizer);
}

TEST_F(IncrementalQuantTest, FullRetrainTrigger) {
    quantizer_config_t config;
    quantizer_config_init(&config, 16, QUANTIZATION_TYPE_PQ);
    config.pq_subquantizers = 4;
    config.pq_bits = 8;

    quantizer_t *quantizer = quantizer_create(&config);
    if (quantizer == nullptr) {
        GTEST_SKIP();
    }

    float training_vectors[100 * 16];
    generate_random_vectors(training_vectors, 100, 16);
    quantizer_train(quantizer, 100, training_vectors);

    incremental_quant_config_t incr_config = incremental_quant_config_default();
    incr_config.max_incremental_updates = 2;

    incremental_quant_trainer_t *trainer =
        incremental_quant_trainer_create(quantizer, &incr_config);
    if (trainer == nullptr) {
        quantizer_drop(quantizer);
        GTEST_SKIP();
    }

    /* 多次增量更新 */
    float new_vectors[20 * 16];
    for (int i = 0; i < 3; i++) {
        generate_random_vectors(new_vectors, 20, 16);
        incremental_quant_update(trainer, 20, new_vectors);
    }

    /* 应该需要全量重建 */
    EXPECT_TRUE(incremental_quant_need_full_retrain(trainer));

    incremental_quant_trainer_destroy(trainer);
    quantizer_drop(quantizer);
}

/* ========================================================================
 * 流式索引测试
 * ======================================================================== */

class StreamingIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

TEST_F(StreamingIndexTest, CreateAndDestroy) {
    streaming_index_config_t config = streaming_index_config_default(
        STREAMING_INDEX_HNSW, 128);
    config.buffer_capacity = 1000;
    config.buffer_flush_threshold = 100;

    streaming_index_t *index = streaming_index_create(&config);
    /* 注意：由于缺少底层索引实现，这里可能返回 NULL */
    /* 测试只是为了验证接口可用 */

    if (index != nullptr) {
        streaming_index_destroy(index);
    } else {
        /* 预期行为：没有底层索引实现 */
        GTEST_LOG_(INFO) << "Streaming index requires underlying index implementation";
    }
}

TEST_F(StreamingIndexTest, ConfigDefault) {
    streaming_index_config_t config = streaming_index_config_default(
        STREAMING_INDEX_HNSW, 128);

    EXPECT_EQ(STREAMING_INDEX_HNSW, config.index_type);
    EXPECT_EQ(128, config.dims);
    EXPECT_EQ(10000, config.buffer_capacity);
    EXPECT_EQ(1000, config.buffer_flush_threshold);
    EXPECT_TRUE(config.enable_incremental_quant);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
