#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

extern "C" {
#include "algo-prod/Kmeans/kmeans.h"
}

namespace {

std::vector<double> sorted_centers(const double *centers, int n_clusters, int n_features) {
    std::vector<std::vector<double>> grouped;
    grouped.reserve(n_clusters);
    for (int cluster = 0; cluster < n_clusters; ++cluster) {
        grouped.emplace_back(
            centers + static_cast<size_t>(cluster) * n_features,
            centers + static_cast<size_t>(cluster + 1) * n_features);
    }

    std::sort(grouped.begin(), grouped.end(), [](const std::vector<double> &left, const std::vector<double> &right) {
        double left_sum = 0.0;
        double right_sum = 0.0;
        for (double value : left) {
            left_sum += value;
        }
        for (double value : right) {
            right_sum += value;
        }
        return left_sum < right_sum;
    });

    std::vector<double> flattened;
    flattened.reserve(static_cast<size_t>(n_clusters) * n_features);
    for (const auto &center : grouped) {
        flattened.insert(flattened.end(), center.begin(), center.end());
    }
    return flattened;
}

}  // namespace

TEST(KMeansTest, FitsTwoSeparatedClusters) {
    const std::vector<double> data = {
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        10.0, 10.0,
        10.0, 11.0,
        11.0, 10.0,
    };

    KMeansParams params = {};
    params.n_clusters = 2;
    params.n_init = 8;
    params.max_iter = 100;
    params.tol = 1e-8;
    params.random_state = 42;
    params.init = "k-means++";
    params.algorithm = "lloyd";
    params.n_samples = 6;
    params.n_features = 2;
    params.data = data.data();

    Kmeans(&params);

    ASSERT_EQ(params.success, 1);
    ASSERT_NE(params.cluster_centers, nullptr);
    ASSERT_NE(params.labels, nullptr);
    EXPECT_GT(params.n_iter, 0);
    EXPECT_LT(params.inertia, 3.0);

    EXPECT_EQ(params.labels[0], params.labels[1]);
    EXPECT_EQ(params.labels[1], params.labels[2]);
    EXPECT_EQ(params.labels[3], params.labels[4]);
    EXPECT_EQ(params.labels[4], params.labels[5]);
    EXPECT_NE(params.labels[0], params.labels[3]);

    const std::vector<double> centers = sorted_centers(params.cluster_centers, params.n_clusters, params.n_features);
    ASSERT_EQ(centers.size(), 4U);

    EXPECT_NEAR(centers[0], 1.0 / 3.0, 1e-6);
    EXPECT_NEAR(centers[1], 1.0 / 3.0, 1e-6);
    EXPECT_NEAR(centers[2], 31.0 / 3.0, 1e-6);
    EXPECT_NEAR(centers[3], 31.0 / 3.0, 1e-6);

    KmeansFree(&params);
}

TEST(KMeansTest, ClampsClusterCountToSampleCount) {
    const std::vector<double> data = {
        -2.0, -2.0,
        0.0, 0.0,
        3.0, 3.0,
    };

    KMeansParams params = {};
    params.n_clusters = 5;
    params.n_init = 4;
    params.max_iter = 50;
    params.tol = 1e-8;
    params.random_state = 7;
    params.init = "random";
    params.algorithm = "lloyd";
    params.n_samples = 3;
    params.n_features = 2;
    params.data = data.data();

    Kmeans(&params);

    ASSERT_EQ(params.success, 1);
    EXPECT_EQ(params.n_clusters, 3);
    ASSERT_NE(params.cluster_centers, nullptr);
    ASSERT_NE(params.labels, nullptr);

    std::vector<double> centers = sorted_centers(params.cluster_centers, params.n_clusters, params.n_features);
    ASSERT_EQ(centers.size(), 6U);
    EXPECT_NEAR(centers[0], -2.0, 1e-6);
    EXPECT_NEAR(centers[1], -2.0, 1e-6);
    EXPECT_NEAR(centers[2], 0.0, 1e-6);
    EXPECT_NEAR(centers[3], 0.0, 1e-6);
    EXPECT_NEAR(centers[4], 3.0, 1e-6);
    EXPECT_NEAR(centers[5], 3.0, 1e-6);

    KmeansFree(&params);
}
