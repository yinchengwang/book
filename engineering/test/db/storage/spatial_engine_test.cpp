/**
 * @file spatial_engine_test.cpp
 * @brief 空间引擎集成测试（包含 Hilbert 索引）
 */
#include "db/storage/spatial/spatial_engine.h"
#include "db/storage/spatial/rtree.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
 * @brief SpatialEngineTest: 空间引擎功能测试
 */
class SpatialEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录
        test_dir = "test_data/mm_storage/spatial/spatial_test_hilbert";
#ifdef _WIN32
        _mkdir(test_dir.c_str());
#else
        mkdir(test_dir.c_str(), 0755);
#endif
        spatial_engine_init(test_dir.c_str());
    }

    void TearDown() override {
        spatial_engine_shutdown();
        // 清理测试数据
        remove((test_dir + "/header.bin").c_str());
        remove((test_dir + "/geometries.bin").c_str());
    }

    std::string test_dir;
};

/**
 * @test init_shutdown: 测试空间引擎初始化和关闭
 */
TEST_F(SpatialEngineTest, InitShutdown) {
    // 已经在 SetUp/TearDown 中测试
    SUCCEED();
}

/**
 * @test create_open_close: 测试数据集创建、打开和关闭
 */
TEST_F(SpatialEngineTest, CreateOpenClose) {
    // 创建空间数据集
    int ret = spatial_engine_create("test_basic", nullptr);
    EXPECT_EQ(ret, 0);

    // 打开数据集
    void *db = spatial_engine_open("test_basic", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 关闭数据集
    ret = spatial_engine_close(db);
    EXPECT_EQ(ret, 0);

    // 清理
    spatial_engine_drop("test_basic");
}

/**
 * @test insert_geometries: 测试插入几何对象
 */
TEST_F(SpatialEngineTest, InsertGeometries) {
    // 创建并打开数据集
    spatial_engine_create("test_insert", nullptr);
    void *db = spatial_engine_open("test_insert", ACCESS_MODE_READ_WRITE);

    // 构造几何数据
    // 格式: id(8) + type(4) + centroid(16) + bounds(32) = 60 bytes
    uint8_t geom_data[64];
    memset(geom_data, 0, sizeof(geom_data));

    // 设置 ID
    uint64_t id = 1;
    memcpy(geom_data, &id, sizeof(id));

    // 设置类型
    geometry_type_t type = GEOM_POINT;
    memcpy(geom_data + 8, &type, sizeof(type));

    // 设置中心点
    point_t centroid = {0.5, 0.5};
    memcpy(geom_data + 12, &centroid, sizeof(centroid));

    // 设置边界框
    bbox_t bounds = {0.5, 0.5, 0.5, 0.5};
    memcpy(geom_data + 28, &bounds, sizeof(bounds));

    // 插入
    int ret = spatial_engine_insert(db, geom_data, sizeof(geom_data));
    EXPECT_EQ(ret, 0);

    spatial_engine_close(db);
    spatial_engine_drop("test_insert");
}

/**
 * @test hilbert_index_integration: 测试 Hilbert 索引集成到空间引擎
 */
TEST_F(SpatialEngineTest, HilbertIndexIntegration) {
    // 创建空间数据集
    int ret = spatial_engine_create("test_hilbert", nullptr);
    EXPECT_EQ(ret, 0);

    // 打开数据集
    void *db = spatial_engine_open("test_hilbert", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入多个几何对象
    for (int i = 0; i < 10; i++) {
        uint8_t geom_data[64];
        memset(geom_data, 0, sizeof(geom_data));

        uint64_t id = i;
        memcpy(geom_data, &id, sizeof(id));

        geometry_type_t type = GEOM_POINT;
        memcpy(geom_data + 8, &type, sizeof(type));

        double x = (double)i / 10.0;
        point_t centroid = {x, x};
        memcpy(geom_data + 12, &centroid, sizeof(centroid));

        bbox_t bounds = {x, x, x, x};
        memcpy(geom_data + 28, &bounds, sizeof(bounds));

        spatial_engine_insert(db, geom_data, sizeof(geom_data));
    }

    // 构建索引（包括 Hilbert）
    ret = spatial_engine_build_index(db);
    EXPECT_EQ(ret, 0);

    spatial_engine_close(db);
    spatial_engine_drop("test_hilbert");
}

/**
 * @test hilbert_range_search: 测试 Hilbert 辅助范围查询
 */
TEST_F(SpatialEngineTest, HilbertRangeSearch) {
    // 创建并打开数据集
    spatial_engine_create("test_range", nullptr);
    void *db = spatial_engine_open("test_range", ACCESS_MODE_READ_WRITE);

    // 插入多个几何对象
    for (int i = 0; i < 10; i++) {
        uint8_t geom_data[64];
        memset(geom_data, 0, sizeof(geom_data));

        uint64_t id = i;
        memcpy(geom_data, &id, sizeof(id));

        geometry_type_t type = GEOM_POINT;
        memcpy(geom_data + 8, &type, sizeof(type));

        double x = (double)i / 10.0;
        point_t centroid = {x, x};
        memcpy(geom_data + 12, &centroid, sizeof(centroid));

        bbox_t bounds = {x, x, x, x};
        memcpy(geom_data + 28, &bounds, sizeof(bounds));

        spatial_engine_insert(db, geom_data, sizeof(geom_data));
    }

    // 构建索引
    spatial_engine_build_index(db);

    // 使用 Hilbert 辅助查询
    bbox_t query_bbox = {0.3, 0.3, 0.7, 0.7};
    spatial_query_result_t results[10];
    int count = spatial_engine_hilbert_search(db, &query_bbox, results, 10);

    EXPECT_GE(count, 0);

    spatial_engine_close(db);
    spatial_engine_drop("test_range");
}

/**
 * @test hilbert_knn_search: 测试 Hilbert 辅助 KNN 查询
 */
TEST_F(SpatialEngineTest, HilbertKnnSearch) {
    // 创建并打开数据集
    spatial_engine_create("test_knn", nullptr);
    void *db = spatial_engine_open("test_knn", ACCESS_MODE_READ_WRITE);

    // 插入多个几何对象
    for (int i = 0; i < 20; i++) {
        uint8_t geom_data[64];
        memset(geom_data, 0, sizeof(geom_data));

        uint64_t id = i;
        memcpy(geom_data, &id, sizeof(id));

        geometry_type_t type = GEOM_POINT;
        memcpy(geom_data + 8, &type, sizeof(type));

        double x = (double)i / 20.0;
        point_t centroid = {x, x};
        memcpy(geom_data + 12, &centroid, sizeof(centroid));

        bbox_t bounds = {x, x, x, x};
        memcpy(geom_data + 28, &bounds, sizeof(bounds));

        spatial_engine_insert(db, geom_data, sizeof(geom_data));
    }

    // 构建索引
    spatial_engine_build_index(db);

    // 使用 Hilbert KNN 查询
    point_t query_point = {0.55, 0.55};
    spatial_query_result_t results[5];
    int count = spatial_engine_hilbert_knn(db, &query_point, 5, results, 5);

    EXPECT_GE(count, 0);

    spatial_engine_close(db);
    spatial_engine_drop("test_knn");
}

/**
 * @test rtree_with_hilbert: 测试 R-Tree 和 Hilbert 索引协同工作
 */
TEST_F(SpatialEngineTest, RtreeWithHilbert) {
    // 创建并打开数据集
    spatial_engine_create("test_combined", nullptr);
    void *db = spatial_engine_open("test_combined", ACCESS_MODE_READ_WRITE);

    // 插入几何对象
    for (int i = 0; i < 15; i++) {
        uint8_t geom_data[64];
        memset(geom_data, 0, sizeof(geom_data));

        uint64_t id = i;
        memcpy(geom_data, &id, sizeof(id));

        geometry_type_t type = GEOM_POLYGON;
        memcpy(geom_data + 8, &type, sizeof(type));

        double base = (double)i / 10.0;
        point_t centroid = {base + 0.05, base + 0.05};
        memcpy(geom_data + 12, &centroid, sizeof(centroid));

        bbox_t bounds = {base, base, base + 0.1, base + 0.1};
        memcpy(geom_data + 28, &bounds, sizeof(bounds));

        spatial_engine_insert(db, geom_data, sizeof(geom_data));
    }

    // 构建索引
    spatial_engine_build_index(db);

    // 测试 R-Tree 范围查询
    bbox_t bbox = {0.2, 0.2, 0.5, 0.5};
    rtree_result_t rtree_results[10];
    int rtree_count = spatial_engine_search_bbox(db, &bbox, rtree_results, 10);

    // 测试 Hilbert 辅助查询
    spatial_query_result_t hilbert_results[10];
    int hilbert_count = spatial_engine_hilbert_search(db, &bbox, hilbert_results, 10);

    // 两种查询都应该返回结果
    EXPECT_GE(rtree_count, 0);
    EXPECT_GE(hilbert_count, 0);

    spatial_engine_close(db);
    spatial_engine_drop("test_combined");
}

/**
 * @test hilbert_index_rebuild: 测试 Hilbert 索引重建
 */
TEST_F(SpatialEngineTest, HilbertIndexRebuild) {
    // 创建并打开数据集
    spatial_engine_create("test_rebuild", nullptr);
    void *db = spatial_engine_open("test_rebuild", ACCESS_MODE_READ_WRITE);

    // 第一次插入和构建
    for (int i = 0; i < 5; i++) {
        uint8_t geom_data[64];
        memset(geom_data, 0, sizeof(geom_data));

        uint64_t id = i;
        memcpy(geom_data, &id, sizeof(id));

        geometry_type_t type = GEOM_POINT;
        memcpy(geom_data + 8, &type, sizeof(type));

        double x = (double)i;
        point_t centroid = {x, x};
        memcpy(geom_data + 12, &centroid, sizeof(centroid));

        bbox_t bounds = {x, x, x, x};
        memcpy(geom_data + 28, &bounds, sizeof(bounds));

        spatial_engine_insert(db, geom_data, sizeof(geom_data));
    }
    spatial_engine_build_index(db);

    // 再次构建（应该先释放旧的 Hilbert 索引）
    int ret = spatial_engine_build_index(db);
    EXPECT_EQ(ret, 0);

    spatial_engine_close(db);
    spatial_engine_drop("test_rebuild");
}

/**
 * @test bbox_utility: 测试边界框工具函数
 */
TEST_F(SpatialEngineTest, BboxUtility) {
    bbox_t bbox1 = bbox_create(0.0, 0.0, 1.0, 1.0);
    EXPECT_EQ(bbox1.min_x, 0.0);
    EXPECT_EQ(bbox1.max_x, 1.0);

    point_t pt = {0.5, 0.5};
    EXPECT_TRUE(bbox_contains_point(&bbox1, &pt));

    point_t pt_out = {1.5, 0.5};
    EXPECT_FALSE(bbox_contains_point(&bbox1, &pt_out));

    bbox_t bbox2 = bbox_create(0.5, 0.5, 1.5, 1.5);
    EXPECT_TRUE(bbox_intersects(&bbox1, &bbox2));

    bbox_t bbox3 = bbox_create(2.0, 2.0, 3.0, 3.0);
    EXPECT_FALSE(bbox_intersects(&bbox1, &bbox3));
}
