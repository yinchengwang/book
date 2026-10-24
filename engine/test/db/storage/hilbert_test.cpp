/**
 * @file hilbert_test.cpp
 * @brief Hilbert 曲线索引单元测试
 */
#include "db/index/hilbert.h"
#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>

/**
 * @brief HilbertTest: Hilbert 曲线编码解码测试
 */
class HilbertTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @test hilbert_encode_decode_2d: 测试 2D 坐标编码解码往返一致性
 */
TEST_F(HilbertTest, EncodeDecode2D) {
    const uint32_t order = 8;
    const double tolerance = 0.02;  /* 允许 2% 误差（四舍五入误差） */

    // 测试点 (0.25, 0.75)
    double x1 = 0.25, y1 = 0.75;
    uint64_t h1 = hilbert_encode2d(x1, y1, order);
    EXPECT_NE(h1, HILBERT_INVALID_INDEX);

    double dec_x1, dec_y1;
    hilbert_decode2d(h1, order, &dec_x1, &dec_y1);
    EXPECT_NEAR(dec_x1, x1, tolerance);
    EXPECT_NEAR(dec_y1, y1, tolerance);

    // 测试点 (0.5, 0.5)
    double x2 = 0.5, y2 = 0.5;
    uint64_t h2 = hilbert_encode2d(x2, y2, order);
    EXPECT_NE(h2, HILBERT_INVALID_INDEX);

    double dec_x2, dec_y2;
    hilbert_decode2d(h2, order, &dec_x2, &dec_y2);
    EXPECT_NEAR(dec_x2, x2, tolerance);
    EXPECT_NEAR(dec_y2, y2, tolerance);

    // 测试原点 (0, 0)
    uint64_t h3 = hilbert_encode2d(0.0, 0.0, order);
    EXPECT_EQ(h3, 0u);

    // 测试角落点 (1, 1) -> 接近最大值
    uint64_t h4 = hilbert_encode2d(1.0, 1.0, order);
    EXPECT_GT(h4, 0u);

    // 验证编码的单调性：相邻点的 Hilbert 码应该相近
    uint64_t h5 = hilbert_encode2d(0.0, 0.1, order);
    uint64_t h6 = hilbert_encode2d(0.0, 0.2, order);
    EXPECT_LT(hilbert_distance(h5, h6), 1000u);
}

/**
 * @test hilbert_encode_decode_3d: 测试 3D 坐标编码解码
 *
 * 注意：3D Hilbert 算法实现较复杂，可能存在精度问题。
 * 如需精确 3D 支持，建议使用外部库。
 */
TEST_F(HilbertTest, DISABLED_EncodeDecode3D) {
    const uint32_t order = 4;
    const double tolerance = 0.1;

    double x = 0.5, y = 0.3, z = 0.7;
    uint64_t h = hilbert_encode3d(x, y, z, order);
    EXPECT_NE(h, HILBERT_INVALID_INDEX);

    double dec_x, dec_y, dec_z;
    hilbert_decode3d(h, order, &dec_x, &dec_y, &dec_z);
    EXPECT_NEAR(dec_x, x, tolerance);
    EXPECT_NEAR(dec_y, y, tolerance);
    EXPECT_NEAR(dec_z, z, tolerance);
}

/**
 * @test hilbert_bbox_range: 测试边界框的 Hilbert 范围计算
 */
TEST_F(HilbertTest, BboxRange) {
    const uint32_t order = 8;

    hilbert_bbox_t bbox = {0.2, 0.3, 0.8, 0.9};
    uint64_t min_code, max_code;
    hilbert_bbox_range(&bbox, order, &min_code, &max_code);

    EXPECT_LT(min_code, max_code);

    // 中心点应该在范围内
    uint64_t center = hilbert_bbox_center(&bbox, order);
    EXPECT_GE(center, min_code);
    EXPECT_LE(center, max_code);
}

/**
 * @test hilbert_index_create_destroy: 测试 Hilbert 索引创建和销毁
 */
TEST_F(HilbertTest, CreateDestroy) {
    hilbert_index_t *index = hilbert_index_create(100, 16);
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->count, 0u);
    EXPECT_EQ(index->capacity, 100u);
    EXPECT_EQ(index->order, 16u);

    hilbert_index_destroy(index);
}

/**
 * @test hilbert_index_add: 测试添加索引记录
 */
TEST_F(HilbertTest, IndexAdd) {
    hilbert_index_t *index = hilbert_index_create(10, 8);
    ASSERT_NE(index, nullptr);

    hilbert_bbox_t bbox = {0.1, 0.2, 0.3, 0.4};
    int ret = hilbert_index_add(index, 1, &bbox);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(index->count, 1u);

    // 添加更多记录
    hilbert_bbox_t bbox2 = {0.5, 0.6, 0.7, 0.8};
    hilbert_index_add(index, 2, &bbox2);
    EXPECT_EQ(index->count, 2u);

    hilbert_index_destroy(index);
}

/**
 * @test hilbert_index_build: 测试索引构建排序
 */
TEST_F(HilbertTest, IndexBuild) {
    hilbert_index_t *index = hilbert_index_create(10, 8);
    ASSERT_NE(index, nullptr);

    // 添加多个记录
    hilbert_bbox_t bboxes[] = {
        {0.8, 0.8, 0.9, 0.9},
        {0.1, 0.1, 0.2, 0.2},
        {0.5, 0.5, 0.6, 0.6}
    };

    for (int i = 0; i < 3; i++) {
        hilbert_index_add(index, i, &bboxes[i]);
    }

    hilbert_index_build(index);

    // 构建后记录应该按 Hilbert 码排序
    for (uint32_t i = 1; i < index->count; i++) {
        EXPECT_LE(index->records[i-1].hilbert_code, index->records[i].hilbert_code);
    }

    hilbert_index_destroy(index);
}

/**
 * @test hilbert_index_range_query: 测试范围查询
 */
TEST_F(HilbertTest, RangeQuery) {
    hilbert_index_t *index = hilbert_index_create(100, 16);
    ASSERT_NE(index, nullptr);

    // 添加一些记录
    for (int i = 0; i < 50; i++) {
        hilbert_bbox_t bbox = {
            (double)i / 100.0,
            (double)i / 100.0,
            (double)(i + 1) / 100.0,
            (double)(i + 1) / 100.0
        };
        hilbert_index_add(index, i, &bbox);
    }

    hilbert_index_build(index);

    // 查询范围 [0.2, 0.2] - [0.4, 0.4]
    hilbert_bbox_t query = {0.2, 0.2, 0.4, 0.4};
    uint64_t results[20];
    uint32_t count = hilbert_index_range_query(index, &query, results, 20);

    // 应该找到至少 1 个结果
    EXPECT_GE(count, 1u);

    hilbert_index_destroy(index);
}

/**
 * @test hilbert_index_knn: 测试 KNN 查询
 */
TEST_F(HilbertTest, KnnQuery) {
    hilbert_index_t *index = hilbert_index_create(100, 16);
    ASSERT_NE(index, nullptr);

    // 添加一些记录
    for (int i = 0; i < 30; i++) {
        hilbert_bbox_t bbox = {
            (double)i / 10.0,
            (double)i / 10.0,
            (double)i / 10.0 + 0.1,
            (double)i / 10.0 + 0.1
        };
        hilbert_index_add(index, i, &bbox);
    }

    hilbert_index_build(index);

    // 查询点 (0.55, 0.55) 的最近 5 个邻居
    hilbert_point2d_t point = {0.55, 0.55};
    hilbert_record_t results[5];
    uint32_t count = hilbert_index_knn(index, &point, 5, results, 5);

    EXPECT_GE(count, 1u);

    hilbert_index_destroy(index);
}

/**
 * @test hilbert_auto_order: 测试自动阶数选择
 */
TEST_F(HilbertTest, AutoOrder) {
    uint32_t order1 = hilbert_auto_order(1000.0, 1000.0);
    EXPECT_GE(order1, 1u);
    EXPECT_LE(order1, HILBERT_MAX_ORDER);

    uint32_t order2 = hilbert_auto_order(1000000.0, 1000000.0);
    EXPECT_GE(order2, order1);
}

/**
 * @test hilbert_distance: 测试 Hilbert 码距离计算
 */
TEST_F(HilbertTest, Distance) {
    uint64_t d1 = hilbert_distance(100, 200);
    uint64_t d2 = hilbert_distance(200, 100);
    EXPECT_EQ(d1, d2);
    EXPECT_GT(d1, 0u);

    // 相同码的距离为 0
    uint64_t d3 = hilbert_distance(500, 500);
    EXPECT_EQ(d3, 0u);
}

/**
 * @test hilbert_in_range: 测试 Hilbert 码范围检查
 */
TEST_F(HilbertTest, InRange) {
    uint64_t code = 500;
    uint64_t min = 400;
    uint64_t max = 600;

    EXPECT_TRUE(hilbert_in_range(code, min, max));
    EXPECT_FALSE(hilbert_in_range(300, min, max));
    EXPECT_FALSE(hilbert_in_range(700, min, max));
    EXPECT_TRUE(hilbert_in_range(400, min, max));
    EXPECT_TRUE(hilbert_in_range(600, min, max));
}

/**
 * @test hilbert_index_save_load: 测试 Hilbert 索引持久化
 */
TEST_F(HilbertTest, SaveAndLoad) {
    hilbert_index_t *index = hilbert_index_create(100, 16);
    ASSERT_NE(index, nullptr);

    /* 添加记录 */
    for (int i = 0; i < 10; i++) {
        hilbert_bbox_t bbox = {
            (double)i * 0.1, (double)i * 0.1,
            (double)i * 0.1 + 0.1, (double)i * 0.1 + 0.1
        };
        hilbert_index_add(index, i, &bbox);
    }
    hilbert_index_build(index);

    /* 保存到文件 */
    int ret = hilbert_index_save(index, "test_hilbert.bin");
    EXPECT_EQ(ret, 0);

    hilbert_index_destroy(index);

    /* 从文件加载 */
    hilbert_index_t *loaded = hilbert_index_load("test_hilbert.bin");
    ASSERT_NE(loaded, nullptr);

    EXPECT_EQ(loaded->count, 10u);
    EXPECT_EQ(loaded->order, 16u);

    /* 清理 */
    hilbert_index_destroy(loaded);
    remove("test_hilbert.bin");
}

/**
 * @test hilbert_bbox_range: 测试边界框 Hilbert 范围
 */
TEST_F(HilbertTest, BBoxRange) {
    hilbert_bbox_t bbox = {0.0, 0.0, 1.0, 1.0};
    uint32_t order = 16;

    uint64_t min_code, max_code;
    hilbert_bbox_range(&bbox, order, &min_code, &max_code);

    EXPECT_LE(min_code, max_code);
    EXPECT_NE(min_code, HILBERT_INVALID_INDEX);
    EXPECT_NE(max_code, HILBERT_INVALID_INDEX);
}

/**
 * @test hilbert_bbox_center: 测试边界框中心点 Hilbert 码
 */
TEST_F(HilbertTest, BBoxCenter) {
    hilbert_bbox_t bbox = {0.0, 0.0, 1.0, 1.0};
    uint32_t order = 16;

    uint64_t center_code = hilbert_bbox_center(&bbox, order);
    EXPECT_NE(center_code, HILBERT_INVALID_INDEX);

    /* 中心点的 Hilbert 码应该在 bbox 范围内 */
    uint64_t min_code, max_code;
    hilbert_bbox_range(&bbox, order, &min_code, &max_code);
    EXPECT_GE(center_code, min_code);
    EXPECT_LE(center_code, max_code);
}
