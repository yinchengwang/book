/**
 * @file spatial_functions_test.cpp
 * @brief 空间函数扩展测试
 */
#include <gtest/gtest.h>
#include "db/storage/spatial/spatial_functions.h"
#include <cstring>
#include <cmath>

/* ========================================================================
 * 几何对象创建测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, CreatePoint) {
    geometry_t *point = geom_create_point(1.0, 2.0);
    ASSERT_NE(nullptr, point);
    EXPECT_EQ(GEOM_POINT, point->type);
    EXPECT_EQ(0, point->srid);
    EXPECT_NE(nullptr, point->data);
    EXPECT_EQ(2 * sizeof(double), point->size);

    double *coords = (double *)point->data;
    EXPECT_DOUBLE_EQ(1.0, coords[0]);
    EXPECT_DOUBLE_EQ(2.0, coords[1]);

    geom_destroy(point);
}

TEST(SpatialFunctionsTest, CreatePoint3D) {
    geometry_t *point = geom_create_point_3d(1.0, 2.0, 3.0);
    ASSERT_NE(nullptr, point);
    EXPECT_EQ(GEOM_POINT, point->type);
    EXPECT_EQ(3 * sizeof(double), point->size);

    double *coords = (double *)point->data;
    EXPECT_DOUBLE_EQ(1.0, coords[0]);
    EXPECT_DOUBLE_EQ(2.0, coords[1]);
    EXPECT_DOUBLE_EQ(3.0, coords[2]);

    geom_destroy(point);
}

TEST(SpatialFunctionsTest, CreateLinestring) {
    double coords[] = {0.0, 0.0, 1.0, 1.0, 2.0, 0.0};
    geometry_t *line = geom_create_linestring(coords, 3);
    ASSERT_NE(nullptr, line);
    EXPECT_EQ(GEOM_LINESTRING, line->type);
    EXPECT_EQ(3 * 2 * sizeof(double), line->size);

    geom_destroy(line);
}

TEST(SpatialFunctionsTest, CreatePolygon) {
    double ring[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0};
    geometry_t *poly = geom_create_polygon(ring, 5);
    ASSERT_NE(nullptr, poly);
    EXPECT_EQ(GEOM_POLYGON, poly->type);
    EXPECT_EQ(5 * 2 * sizeof(double), poly->size);

    geom_destroy(poly);
}

/* ========================================================================
 * 空间谓词测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, PointContainsPoint) {
    geometry_t *outer = geom_create_point(1.0, 2.0);
    geometry_t *inner = geom_create_point(1.0, 2.0);

    EXPECT_TRUE(geom_contains(outer, inner));

    geom_destroy(outer);
    geom_destroy(inner);
}

TEST(SpatialFunctionsTest, PolygonContainsPoint) {
    /* 创建一个正方形多边形 */
    double ring[] = {0.0, 0.0, 2.0, 0.0, 2.0, 2.0, 0.0, 2.0, 0.0, 0.0};
    geometry_t *polygon = geom_create_polygon(ring, 5);

    /* 创建内部点 */
    geometry_t *inside_point = geom_create_point(1.0, 1.0);
    geometry_t *outside_point = geom_create_point(3.0, 3.0);

    EXPECT_TRUE(geom_contains(polygon, inside_point));
    EXPECT_FALSE(geom_contains(polygon, outside_point));

    geom_destroy(polygon);
    geom_destroy(inside_point);
    geom_destroy(outside_point);
}

TEST(SpatialFunctionsTest, PointIntersectsPoint) {
    geometry_t *a = geom_create_point(1.0, 2.0);
    geometry_t *b = geom_create_point(1.0, 2.0);
    geometry_t *c = geom_create_point(3.0, 4.0);

    EXPECT_TRUE(geom_intersects(a, b));
    EXPECT_FALSE(geom_intersects(a, c));

    geom_destroy(a);
    geom_destroy(b);
    geom_destroy(c);
}

TEST(SpatialFunctionsTest, PointWithinPolygon) {
    /* 创建一个正方形多边形 */
    double ring[] = {0.0, 0.0, 2.0, 0.0, 2.0, 2.0, 0.0, 2.0, 0.0, 0.0};
    geometry_t *polygon = geom_create_polygon(ring, 5);

    /* 创建内部点 */
    geometry_t *inside_point = geom_create_point(1.0, 1.0);
    geometry_t *outside_point = geom_create_point(3.0, 3.0);

    EXPECT_TRUE(geom_within(inside_point, polygon));
    EXPECT_FALSE(geom_within(outside_point, polygon));

    geom_destroy(polygon);
    geom_destroy(inside_point);
    geom_destroy(outside_point);
}

/* ========================================================================
 * 距离函数测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, PointDistance) {
    geometry_t *a = geom_create_point(0.0, 0.0);
    geometry_t *b = geom_create_point(3.0, 4.0);

    double dist = geom_distance(a, b);
    EXPECT_DOUBLE_EQ(5.0, dist);

    geom_destroy(a);
    geom_destroy(b);
}

TEST(SpatialFunctionsTest, PointDistanceZero) {
    geometry_t *a = geom_create_point(1.0, 2.0);
    geometry_t *b = geom_create_point(1.0, 2.0);

    double dist = geom_distance(a, b);
    EXPECT_DOUBLE_EQ(0.0, dist);

    geom_destroy(a);
    geom_destroy(b);
}

/* ========================================================================
 * 测量函数测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, PolygonArea) {
    /* 创建一个 2x2 的正方形 */
    double ring[] = {0.0, 0.0, 2.0, 0.0, 2.0, 2.0, 0.0, 2.0, 0.0, 0.0};
    geometry_t *polygon = geom_create_polygon(ring, 5);

    double area = geom_area(polygon);
    EXPECT_DOUBLE_EQ(4.0, area);

    geom_destroy(polygon);
}

TEST(SpatialFunctionsTest, LinestringLength) {
    /* 创建一个 3-4-5 三角形的边 */
    double coords[] = {0.0, 0.0, 3.0, 4.0};
    geometry_t *line = geom_create_linestring(coords, 2);

    double len = geom_length(line);
    EXPECT_DOUBLE_EQ(5.0, len);

    geom_destroy(line);
}

TEST(SpatialFunctionsTest, PolygonPerimeter) {
    /* 创建一个 2x2 的正方形 */
    double ring[] = {0.0, 0.0, 2.0, 0.0, 2.0, 2.0, 0.0, 2.0, 0.0, 0.0};
    geometry_t *polygon = geom_create_polygon(ring, 5);

    double perimeter = geom_perimeter(polygon);
    EXPECT_DOUBLE_EQ(8.0, perimeter);

    geom_destroy(polygon);
}

/* ========================================================================
 * 有效性检查测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, IsValid) {
    geometry_t *point = geom_create_point(1.0, 2.0);
    EXPECT_TRUE(geom_is_valid(point));

    double coords[] = {0.0, 0.0, 1.0, 1.0};
    geometry_t *line = geom_create_linestring(coords, 2);
    EXPECT_TRUE(geom_is_valid(line));

    double ring[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0};
    geometry_t *poly = geom_create_polygon(ring, 5);
    EXPECT_TRUE(geom_is_valid(poly));

    geom_destroy(point);
    geom_destroy(line);
    geom_destroy(poly);
}

/* ========================================================================
 * 维度测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, Dimension) {
    geometry_t *point = geom_create_point(1.0, 2.0);
    EXPECT_EQ(0, geom_dimension(point));

    double coords[] = {0.0, 0.0, 1.0, 1.0};
    geometry_t *line = geom_create_linestring(coords, 2);
    EXPECT_EQ(1, geom_dimension(line));

    double ring[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0};
    geometry_t *poly = geom_create_polygon(ring, 5);
    EXPECT_EQ(2, geom_dimension(poly));

    geom_destroy(point);
    geom_destroy(line);
    geom_destroy(poly);
}

TEST(SpatialFunctionsTest, NumPoints) {
    geometry_t *point = geom_create_point(1.0, 2.0);
    EXPECT_EQ(1, geom_num_points(point));

    double coords[] = {0.0, 0.0, 1.0, 1.0, 2.0, 0.0};
    geometry_t *line = geom_create_linestring(coords, 3);
    EXPECT_EQ(3, geom_num_points(line));

    double ring[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0};
    geometry_t *poly = geom_create_polygon(ring, 5);
    EXPECT_EQ(5, geom_num_points(poly));

    geom_destroy(point);
    geom_destroy(line);
    geom_destroy(poly);
}

/* ========================================================================
 * WKT 输出测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, ToWktPoint) {
    geometry_t *point = geom_create_point(1.0, 2.0);
    char *wkt = geom_to_wkt(point);
    ASSERT_NE(nullptr, wkt);
    EXPECT_STREQ("POINT(1.000000 2.000000)", wkt);

    free(wkt);
    geom_destroy(point);
}

TEST(SpatialFunctionsTest, ToWktPolygon) {
    double ring[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0};
    geometry_t *poly = geom_create_polygon(ring, 5);
    char *wkt = geom_to_wkt(poly);
    ASSERT_NE(nullptr, wkt);
    EXPECT_STREQ("POLYGON((0.000000 0.000000, 1.000000 0.000000, 1.000000 1.000000, 0.000000 1.000000, 0.000000 0.000000))", wkt);

    free(wkt);
    geom_destroy(poly);
}

/* ========================================================================
 * WKB 输入/输出测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, WkbRoundtrip) {
    geometry_t *original = geom_create_point(1.0, 2.0);

    size_t wkb_size;
    void *wkb = geom_to_wkb(original, &wkb_size);
    ASSERT_NE(nullptr, wkb);
    EXPECT_EQ(21u, wkb_size);

    geometry_t *restored = geom_from_wkb(wkb, wkb_size);
    ASSERT_NE(nullptr, restored);

    double *orig_coords = (double *)original->data;
    double *rest_coords = (double *)restored->data;
    EXPECT_DOUBLE_EQ(orig_coords[0], rest_coords[0]);
    EXPECT_DOUBLE_EQ(orig_coords[1], rest_coords[1]);

    free(wkb);
    geom_destroy(original);
    geom_destroy(restored);
}

/* ========================================================================
 * WKT 输入测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, FromWkt) {
    geometry_t *point = geom_from_wkt("POINT(3.0 4.0)");
    ASSERT_NE(nullptr, point);
    EXPECT_EQ(GEOM_POINT, point->type);

    double *coords = (double *)point->data;
    EXPECT_DOUBLE_EQ(3.0, coords[0]);
    EXPECT_DOUBLE_EQ(4.0, coords[1]);

    geom_destroy(point);
}

/* ========================================================================
 * 边界框测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, Envelope) {
    double ring[] = {0.0, 0.0, 2.0, 0.0, 2.0, 2.0, 0.0, 2.0, 0.0, 0.0};
    geometry_t *poly = geom_create_polygon(ring, 5);

    geometry_t *envelope = geom_envelope(poly);
    ASSERT_NE(nullptr, envelope);
    EXPECT_EQ(GEOM_POLYGON, envelope->type);

    double *coords = (double *)envelope->data;
    EXPECT_DOUBLE_EQ(0.0, coords[0]);  /* min_x */
    EXPECT_DOUBLE_EQ(0.0, coords[1]);  /* min_y */
    EXPECT_DOUBLE_EQ(2.0, coords[2]);  /* max_x */
    EXPECT_DOUBLE_EQ(0.0, coords[3]);  /* min_y */
    EXPECT_DOUBLE_EQ(2.0, coords[4]);  /* max_x */
    EXPECT_DOUBLE_EQ(2.0, coords[5]);  /* max_y */
    EXPECT_DOUBLE_EQ(0.0, coords[6]);  /* min_x */
    EXPECT_DOUBLE_EQ(2.0, coords[7]);  /* max_y */
    EXPECT_DOUBLE_EQ(0.0, coords[8]);  /* min_x */
    EXPECT_DOUBLE_EQ(0.0, coords[9]);  /* min_y */

    geom_destroy(poly);
    geom_destroy(envelope);
}

/* ========================================================================
 * 质心测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, Centroid) {
    /* 创建一个三角形 */
    double ring[] = {0.0, 0.0, 3.0, 0.0, 0.0, 3.0, 0.0, 0.0};
    geometry_t *triangle = geom_create_polygon(ring, 4);

    geometry_t *centroid = geom_centroid(triangle);
    ASSERT_NE(nullptr, centroid);
    EXPECT_EQ(GEOM_POINT, centroid->type);

    double *coords = (double *)centroid->data;
    /* 质心应为 (1.0, 1.0) */
    EXPECT_DOUBLE_EQ(1.0, coords[0]);
    EXPECT_DOUBLE_EQ(1.0, coords[1]);

    geom_destroy(triangle);
    geom_destroy(centroid);
}

/* ========================================================================
 * Buffer 测试
 * ======================================================================== */

TEST(SpatialFunctionsTest, Buffer) {
    geometry_t *point = geom_create_point(1.0, 1.0);

    geometry_t *buffered = geom_buffer(point, 0.5);
    ASSERT_NE(nullptr, buffered);
    EXPECT_EQ(GEOM_POLYGON, buffered->type);

    double *coords = (double *)buffered->data;
    /* Buffer 应创建一个正方形 */
    EXPECT_DOUBLE_EQ(0.5, coords[0]);   /* min_x */
    EXPECT_DOUBLE_EQ(0.5, coords[1]);   /* min_y */
    EXPECT_DOUBLE_EQ(1.5, coords[2]);   /* max_x */
    EXPECT_DOUBLE_EQ(0.5, coords[3]);   /* min_y */
    EXPECT_DOUBLE_EQ(1.5, coords[4]);   /* max_x */
    EXPECT_DOUBLE_EQ(1.5, coords[5]);   /* max_y */
    EXPECT_DOUBLE_EQ(0.5, coords[6]);   /* min_x */
    EXPECT_DOUBLE_EQ(1.5, coords[7]);   /* max_y */
    EXPECT_DOUBLE_EQ(0.5, coords[8]);   /* min_x */
    EXPECT_DOUBLE_EQ(0.5, coords[9]);   /* min_y */

    geom_destroy(point);
    geom_destroy(buffered);
}
