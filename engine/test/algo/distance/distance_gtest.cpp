#include <gtest/gtest.h>

#include <cmath>

extern "C" {
#include <algo-prod/distance/distance.h>
}

namespace {

float reference_l2sqr(const float *lhs, const float *rhs, int32_t dims) {
    float result = 0.0f;

    for (int32_t i = 0; i < dims; ++i) {
        const float diff = lhs[i] - rhs[i];
        result += diff * diff;
    }
    return result;
}

float reference_inner_product_distance(const float *lhs, const float *rhs, int32_t dims) {
    float dot = 0.0f;

    for (int32_t i = 0; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
    }
    return -dot;
}

float reference_cosine_distance(const float *lhs, const float *rhs, int32_t dims) {
    float dot = 0.0f;
    float lhs_norm = 0.0f;
    float rhs_norm = 0.0f;

    for (int32_t i = 0; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    if (lhs_norm <= 0.0f && rhs_norm <= 0.0f) {
        return 0.0f;
    }
    if (lhs_norm <= 0.0f || rhs_norm <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - dot / (std::sqrt(lhs_norm) * std::sqrt(rhs_norm));
}

float reference_hamming_distance(const float *lhs, const float *rhs, int32_t dims) {
    float result = 0.0f;

    for (int32_t i = 0; i < dims; ++i) {
        if (std::fabs(lhs[i] - rhs[i]) > 1e-6f) {
            result += 1.0f;
        }
    }
    return result;
}

}  // namespace

TEST(DistanceTest, ComputesAllMetricsAgainstReferenceImplementation) {
    const float lhs[] = {1.0f, -2.5f, 3.25f, 4.0f, -1.5f, 0.75f, 2.25f};
    const float rhs[] = {0.5f, -1.0f, 2.0f, 5.0f, -3.0f, 1.25f, -0.75f};
    constexpr int32_t dims = static_cast<int32_t>(sizeof(lhs) / sizeof(lhs[0]));

    EXPECT_NEAR(distance_compute(DISTANCE_METRIC_L2_SQUARED, lhs, rhs, dims),
                reference_l2sqr(lhs, rhs, dims),
                1e-5f);
    EXPECT_NEAR(distance_compute(DISTANCE_METRIC_INNER_PRODUCT, lhs, rhs, dims),
                reference_inner_product_distance(lhs, rhs, dims),
                1e-5f);
    EXPECT_NEAR(distance_compute(DISTANCE_METRIC_COSINE, lhs, rhs, dims),
                reference_cosine_distance(lhs, rhs, dims),
                1e-5f);
    EXPECT_NEAR(distance_compute(DISTANCE_METRIC_HAMMING, lhs, rhs, dims),
                reference_hamming_distance(lhs, rhs, dims),
                1e-5f);
}

TEST(DistanceTest, IndexedAndQueryWrappersMatchDirectDistance) {
    const float vectors[] = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
        0.5f, 1.5f, 2.5f, 3.5f, 4.5f,
        -1.0f, -0.5f, 0.0f, 0.5f, 1.0f,
    };
    const float query[] = {1.25f, 1.75f, 2.25f, 2.75f, 3.25f};
    const float *vec0 = &vectors[0];
    const float *vec1 = &vectors[5];
    const float *vec2 = &vectors[10];
    constexpr int32_t dims = 5;

    EXPECT_NEAR(distance_compute_indexed(DISTANCE_METRIC_L2_SQUARED, vectors, dims, 0, 1),
                distance_compute(DISTANCE_METRIC_L2_SQUARED, vec0, vec1, dims),
                1e-5f);
    EXPECT_NEAR(distance_compute_from_query(DISTANCE_METRIC_L2_SQUARED, query, vectors, dims, 2),
                distance_compute(DISTANCE_METRIC_L2_SQUARED, query, vec2, dims),
                1e-5f);
}

TEST(DistanceTest, Batch4FromQueryMatchesSingleCallsWithTailDimensions) {
    const float vectors[] = {
        1.0f, 0.0f, 2.0f, -1.0f, 3.0f, 4.0f, -2.0f,
        0.5f, 1.0f, 2.5f, -0.5f, 2.0f, 3.0f, -1.5f,
        -1.0f, 2.0f, 1.0f, 0.0f, -0.5f, 1.5f, 2.5f,
        3.0f, -1.0f, 0.5f, 2.0f, 1.0f, -0.5f, 4.0f,
    };
    const float query[] = {0.25f, 1.5f, 1.75f, -0.75f, 1.25f, 2.0f, -1.0f};
    float dis1 = 0.0f;
    float dis2 = 0.0f;
    float dis3 = 0.0f;
    float dis4 = 0.0f;
    constexpr int32_t dims = 7;

    distance_compute_batch4_from_query(DISTANCE_METRIC_L2_SQUARED,
                                            query,
                                            vectors,
                                            dims,
                                            0,
                                            1,
                                            2,
                                            3,
                                            &dis1,
                                            &dis2,
                                            &dis3,
                                            &dis4);

    EXPECT_NEAR(dis1, distance_compute_from_query(DISTANCE_METRIC_L2_SQUARED, query, vectors, dims, 0), 1e-5f);
    EXPECT_NEAR(dis2, distance_compute_from_query(DISTANCE_METRIC_L2_SQUARED, query, vectors, dims, 1), 1e-5f);
    EXPECT_NEAR(dis3, distance_compute_from_query(DISTANCE_METRIC_L2_SQUARED, query, vectors, dims, 2), 1e-5f);
    EXPECT_NEAR(dis4, distance_compute_from_query(DISTANCE_METRIC_L2_SQUARED, query, vectors, dims, 3), 1e-5f);
}

TEST(DistanceTest, Batch4FromQuerySupportsCosineAndInnerProductMetrics) {
    const float vectors[] = {
        1.0f, 0.0f, 2.0f, -1.0f, 3.0f,
        0.5f, 1.0f, 2.5f, -0.5f, 2.0f,
        -1.0f, 2.0f, 1.0f, 0.0f, -0.5f,
        3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
    };
    const float query[] = {0.25f, 1.5f, 1.75f, -0.75f, 1.25f};
    float cosine1 = 0.0f;
    float cosine2 = 0.0f;
    float cosine3 = 0.0f;
    float cosine4 = 0.0f;
    float ip1 = 0.0f;
    float ip2 = 0.0f;
    float ip3 = 0.0f;
    float ip4 = 0.0f;
    constexpr int32_t dims = 5;

    distance_compute_batch4_from_query(DISTANCE_METRIC_COSINE,
                                            query,
                                            vectors,
                                            dims,
                                            0,
                                            1,
                                            2,
                                            3,
                                            &cosine1,
                                            &cosine2,
                                            &cosine3,
                                            &cosine4);
    distance_compute_batch4_from_query(DISTANCE_METRIC_INNER_PRODUCT,
                                            query,
                                            vectors,
                                            dims,
                                            0,
                                            1,
                                            2,
                                            3,
                                            &ip1,
                                            &ip2,
                                            &ip3,
                                            &ip4);

    EXPECT_NEAR(cosine1, distance_compute_from_query(DISTANCE_METRIC_COSINE, query, vectors, dims, 0), 1e-5f);
    EXPECT_NEAR(cosine2, distance_compute_from_query(DISTANCE_METRIC_COSINE, query, vectors, dims, 1), 1e-5f);
    EXPECT_NEAR(cosine3, distance_compute_from_query(DISTANCE_METRIC_COSINE, query, vectors, dims, 2), 1e-5f);
    EXPECT_NEAR(cosine4, distance_compute_from_query(DISTANCE_METRIC_COSINE, query, vectors, dims, 3), 1e-5f);
    EXPECT_NEAR(ip1, distance_compute_from_query(DISTANCE_METRIC_INNER_PRODUCT, query, vectors, dims, 0), 1e-5f);
    EXPECT_NEAR(ip2, distance_compute_from_query(DISTANCE_METRIC_INNER_PRODUCT, query, vectors, dims, 1), 1e-5f);
    EXPECT_NEAR(ip3, distance_compute_from_query(DISTANCE_METRIC_INNER_PRODUCT, query, vectors, dims, 2), 1e-5f);
    EXPECT_NEAR(ip4, distance_compute_from_query(DISTANCE_METRIC_INNER_PRODUCT, query, vectors, dims, 3), 1e-5f);
}

TEST(DistanceTest, InvalidInputsReturnZeroDistanceOrLeaveOutputsUntouched) {
    float dis1 = 11.0f;
    float dis2 = 12.0f;
    float dis3 = 13.0f;
    float dis4 = 14.0f;

    EXPECT_FLOAT_EQ(distance_compute(static_cast<distance_metric_t>(-1), nullptr, nullptr, 0), 0.0f);
    EXPECT_FLOAT_EQ(distance_l2sqr(nullptr, nullptr, 8), 0.0f);

    distance_compute_batch4_from_query(DISTANCE_METRIC_L2_SQUARED,
                                            nullptr,
                                            nullptr,
                                            0,
                                            0,
                                            1,
                                            2,
                                            3,
                                            nullptr,
                                            &dis2,
                                            &dis3,
                                            &dis4);

    EXPECT_FLOAT_EQ(dis1, 11.0f);
    EXPECT_FLOAT_EQ(dis2, 12.0f);
    EXPECT_FLOAT_EQ(dis3, 13.0f);
    EXPECT_FLOAT_EQ(dis4, 14.0f);

    distance_compute_batch4_from_query(DISTANCE_METRIC_L2_SQUARED,
                                            nullptr,
                                            nullptr,
                                            0,
                                            0,
                                            1,
                                            2,
                                            3,
                                            &dis1,
                                            &dis2,
                                            &dis3,
                                            &dis4);

    EXPECT_FLOAT_EQ(dis1, 0.0f);
    EXPECT_FLOAT_EQ(dis2, 0.0f);
    EXPECT_FLOAT_EQ(dis3, 0.0f);
    EXPECT_FLOAT_EQ(dis4, 0.0f);
}