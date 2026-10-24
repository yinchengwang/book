/**
 * @file spatial_functions.c
 * @brief 空间函数扩展实现
 *
 * Phase12 - 实现空间函数，追赶 PostGIS 水平。
 */
#include "db/storage/spatial/spatial_functions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 几何对象创建
 * ======================================================================== */

geometry_t *geom_create_point(double x, double y) {
    geometry_t *geom = (geometry_t *)calloc(1, sizeof(geometry_t));
    if (!geom) return NULL;

    geom->type = GEOM_POINT;
    geom->srid = 0;

    /* 存储坐标 (x, y) */
    double *coords = (double *)malloc(2 * sizeof(double));
    if (!coords) {
        free(geom);
        return NULL;
    }

    coords[0] = x;
    coords[1] = y;
    geom->data = coords;
    geom->size = 2 * sizeof(double);

    return geom;
}

geometry_t *geom_create_point_3d(double x, double y, double z) {
    geometry_t *geom = (geometry_t *)calloc(1, sizeof(geometry_t));
    if (!geom) return NULL;

    geom->type = GEOM_POINT;
    geom->srid = 0;

    /* 存储坐标 (x, y, z) */
    double *coords = (double *)malloc(3 * sizeof(double));
    if (!coords) {
        free(geom);
        return NULL;
    }

    coords[0] = x;
    coords[1] = y;
    coords[2] = z;
    geom->data = coords;
    geom->size = 3 * sizeof(double);

    return geom;
}

geometry_t *geom_create_linestring(double *coords, size_t num_points) {
    if (!coords || num_points < 2) return NULL;

    geometry_t *geom = (geometry_t *)calloc(1, sizeof(geometry_t));
    if (!geom) return NULL;

    geom->type = GEOM_LINESTRING;
    geom->srid = 0;

    size_t data_size = num_points * 2 * sizeof(double);
    double *data = (double *)malloc(data_size);
    if (!data) {
        free(geom);
        return NULL;
    }

    memcpy(data, coords, data_size);
    geom->data = data;
    geom->size = data_size;

    return geom;
}

geometry_t *geom_create_polygon(double *ring, size_t num_points) {
    if (!ring || num_points < 3) return NULL;

    geometry_t *geom = (geometry_t *)calloc(1, sizeof(geometry_t));
    if (!geom) return NULL;

    geom->type = GEOM_POLYGON;
    geom->srid = 0;

    size_t data_size = num_points * 2 * sizeof(double);
    double *data = (double *)malloc(data_size);
    if (!data) {
        free(geom);
        return NULL;
    }

    memcpy(data, ring, data_size);
    geom->data = data;
    geom->size = data_size;

    return geom;
}

geometry_t *geom_from_wkb(const void *wkb, size_t size) {
    if (!wkb || size < 5) return NULL;  /* 最小 WKB: 1 byte order + 4 type */

    const unsigned char *bytes = (const unsigned char *)wkb;

    /* 读取字节序 */
    unsigned char byte_order = bytes[0];

    /* 读取几何类型 (uint32) */
    uint32_t geom_type;
    if (byte_order == 0) {  /* Little-endian */
        geom_type = (uint32_t)bytes[1] | ((uint32_t)bytes[2] << 8) |
                   ((uint32_t)bytes[3] << 16) | ((uint32_t)bytes[4] << 24);
    } else {  /* Big-endian */
        geom_type = ((uint32_t)bytes[1] << 24) | ((uint32_t)bytes[2] << 16) |
                   ((uint32_t)bytes[3] << 8) | (uint32_t)bytes[4];
    }

    /* 只处理 Point 类型 (1) */
    if ((geom_type & 0xFF) != 1) {
        return NULL;  /* 暂不支持其他类型 */
    }

    if (size < 21) return NULL;  /* Point: 1 + 4 + 8 + 8 = 21 bytes */

    double x, y;
    if (byte_order == 0) {
        uint64_t ux, uy;
        memcpy(&ux, bytes + 5, 8);
        memcpy(&uy, bytes + 13, 8);
        x = *(double *)&ux;
        y = *(double *)&uy;
    } else {
        /* Big-endian: 需要字节交换 */
        uint64_t ux = 0, uy = 0;
        for (int i = 0; i < 8; i++) {
            ux = (ux << 8) | bytes[5 + i];
            uy = (uy << 8) | bytes[13 + i];
        }
        x = *(double *)&ux;
        y = *(double *)&uy;
    }

    return geom_create_point(x, y);
}

geometry_t *geom_from_wkt(const char *wkt) {
    if (!wkt) return NULL;

    /* 简化实现：只支持 POINT(x y) 格式 */
    if (strncmp(wkt, "POINT(", 6) == 0) {
        const char *start = wkt + 6;
        const char *end = strchr(start, ')');
        if (!end) return NULL;

        /* 解析 x y */
        double x, y;
        if (sscanf(start, "%lf %lf", &x, &y) == 2) {
            return geom_create_point(x, y);
        }
    }

    return NULL;
}

/* ========================================================================
 * 几何对象销毁
 * ======================================================================== */

void geom_destroy(geometry_t *geom) {
    if (!geom) return;

    if (geom->data) {
        free(geom->data);
        geom->data = NULL;
    }

    free(geom);
}

/* ========================================================================
 * 空间谓词（简化实现）
 * ======================================================================== */

bool geom_contains(const geometry_t *a, const geometry_t *b) {
    if (!a || !b) return false;

    /* 简化实现：基于点包含 */
    if (a->type == GEOM_POINT && b->type == GEOM_POINT) {
        if (!a->data || !b->data) return false;

        const double *pa = (const double *)a->data;
        const double *pb = (const double *)b->data;

        /* 点包含点：坐标相同 */
        return (pa[0] == pb[0] && pa[1] == pb[1]);
    }

    if (a->type == GEOM_POLYGON && b->type == GEOM_POINT) {
        if (!a->data || !b->data) return false;

        const double *polygon = (const double *)a->data;
        size_t num_points = a->size / (2 * sizeof(double));
        const double *point = (const double *)b->data;

        /* 射线法判断点是否在多边形内 */
        bool inside = false;
        for (size_t i = 0, j = num_points - 1; i < num_points; j = i++) {
            double xi = polygon[i * 2], yi = polygon[i * 2 + 1];
            double xj = polygon[j * 2], yj = polygon[j * 2 + 1];

            if (((yi > point[1]) != (yj > point[1])) &&
                (point[0] < (xj - xi) * (point[1] - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        return inside;
    }

    return false;
}

bool geom_intersects(const geometry_t *a, const geometry_t *b) {
    if (!a || !b) return false;

    /* 简化实现：基于 bbox 相交 */
    if (a->type == GEOM_POINT && b->type == GEOM_POINT) {
        if (!a->data || !b->data) return false;

        const double *pa = (const double *)a->data;
        const double *pb = (const double *)b->data;

        return (pa[0] == pb[0] && pa[1] == pb[1]);
    }

    /* 其他情况返回 false */
    return false;
}

bool geom_within(const geometry_t *a, const geometry_t *b) {
    /* within 是 contains 的反向 */
    return geom_contains(b, a);
}

bool geom_touches(const geometry_t *a, const geometry_t *b) {
    if (!a || !b) return false;

    /* 简化实现：点接触 */
    if (a->type == GEOM_POINT && b->type == GEOM_POINT) {
        return geom_intersects(a, b);
    }

    return false;
}

bool geom_crosses(const geometry_t *a, const geometry_t *b) {
    /* 简化实现：暂不支持 */
    (void)a;
    (void)b;
    return false;
}

bool geom_disjoint(const geometry_t *a, const geometry_t *b) {
    return !geom_intersects(a, b);
}

bool geom_overlaps(const geometry_t *a, const geometry_t *b) {
    /* 简化实现：暂不支持 */
    (void)a;
    (void)b;
    return false;
}

/* ========================================================================
 * 距离函数
 * ======================================================================== */

double geom_distance(const geometry_t *a, const geometry_t *b) {
    if (!a || !b) return -1.0;

    if (a->type == GEOM_POINT && b->type == GEOM_POINT) {
        if (!a->data || !b->data) return -1.0;

        const double *pa = (const double *)a->data;
        const double *pb = (const double *)b->data;

        double dx = pa[0] - pb[0];
        double dy = pa[1] - pb[1];
        return sqrt(dx * dx + dy * dy);
    }

    return -1.0;
}

double geom_distance_spheroid(const geometry_t *a, const geometry_t *b) {
    /* 简化实现：使用平面距离 */
    return geom_distance(a, b);
}

/* ========================================================================
 * 空间操作（简化实现）
 * ======================================================================== */

geometry_t *geom_buffer(const geometry_t *geom, double distance) {
    if (!geom || distance <= 0) return NULL;

    /* 简化实现：返回扩展的 bbox 作为多边形 */
    if (geom->type == GEOM_POINT && geom->data) {
        const double *coords = (const double *)geom->data;
        double x = coords[0], y = coords[1];

        /* 创建一个正方形作为 buffer 近似 */
        double ring[10] = {
            x - distance, y - distance,
            x + distance, y - distance,
            x + distance, y + distance,
            x - distance, y + distance,
            x - distance, y - distance  /* 闭合 */
        };
        return geom_create_polygon(ring, 5);
    }

    return NULL;
}

geometry_t *geom_intersection(const geometry_t *a, const geometry_t *b) {
    /* 简化实现：暂不支持 */
    (void)a;
    (void)b;
    return NULL;
}

geometry_t *geom_union(const geometry_t *a, const geometry_t *b) {
    /* 简化实现：暂不支持 */
    (void)a;
    (void)b;
    return NULL;
}

geometry_t *geom_difference(const geometry_t *a, const geometry_t *b) {
    /* 简化实现：暂不支持 */
    (void)a;
    (void)b;
    return NULL;
}

geometry_t *geom_centroid(const geometry_t *geom) {
    if (!geom || !geom->data) return NULL;

    if (geom->type == GEOM_POINT) {
        /* 点的质心就是它自己 */
        const double *coords = (const double *)geom->data;
        return geom_create_point(coords[0], coords[1]);
    }

    if (geom->type == GEOM_POLYGON) {
        /* 计算多边形质心 */
        const double *polygon = (const double *)geom->data;
        size_t num_points = geom->size / (2 * sizeof(double));

        /* 排除闭合点（最后一个点与第一个点相同） */
        size_t vertex_count = num_points - 1;
        if (vertex_count < 3) vertex_count = num_points;  /* 退化情况 */

        double cx = 0, cy = 0;
        for (size_t i = 0; i < vertex_count; i++) {
            cx += polygon[i * 2];
            cy += polygon[i * 2 + 1];
        }
        cx /= vertex_count;
        cy /= vertex_count;

        return geom_create_point(cx, cy);
    }

    return NULL;
}

geometry_t *geom_boundary(const geometry_t *geom) {
    /* 简化实现：暂不支持 */
    (void)geom;
    return NULL;
}

geometry_t *geom_envelope(const geometry_t *geom) {
    if (!geom || !geom->data) return NULL;

    if (geom->type == GEOM_POINT) {
        /* 点的 envelope 是它自己 */
        const double *coords = (const double *)geom->data;
        return geom_create_point(coords[0], coords[1]);
    }

    if (geom->type == GEOM_POLYGON || geom->type == GEOM_LINESTRING) {
        /* 计算 bounding box */
        const double *data = (const double *)geom->data;
        size_t num_points = geom->size / (2 * sizeof(double));

        double min_x = data[0], max_x = data[0];
        double min_y = data[1], max_y = data[1];

        for (size_t i = 1; i < num_points; i++) {
            if (data[i * 2] < min_x) min_x = data[i * 2];
            if (data[i * 2] > max_x) max_x = data[i * 2];
            if (data[i * 2 + 1] < min_y) min_y = data[i * 2 + 1];
            if (data[i * 2 + 1] > max_y) max_y = data[i * 2 + 1];
        }

        /* 返回矩形多边形 */
        double ring[10] = {
            min_x, min_y,
            max_x, min_y,
            max_x, max_y,
            min_x, max_y,
            min_x, min_y  /* 闭合 */
        };
        return geom_create_polygon(ring, 5);
    }

    return NULL;
}

/* ========================================================================
 * 测量函数
 * ======================================================================== */

double geom_area(const geometry_t *geom) {
    if (!geom || !geom->data) return 0.0;

    if (geom->type == GEOM_POLYGON) {
        /* Shoelace formula */
        const double *polygon = (const double *)geom->data;
        size_t num_points = geom->size / (2 * sizeof(double));

        double area = 0;
        for (size_t i = 0; i < num_points - 1; i++) {
            area += polygon[i * 2] * polygon[(i + 1) * 2 + 1];
            area -= polygon[(i + 1) * 2] * polygon[i * 2 + 1];
        }

        return fabs(area) / 2.0;
    }

    return 0.0;
}

double geom_length(const geometry_t *geom) {
    if (!geom || !geom->data) return 0.0;

    if (geom->type == GEOM_LINESTRING) {
        const double *coords = (const double *)geom->data;
        size_t num_points = geom->size / (2 * sizeof(double));

        double len = 0;
        for (size_t i = 0; i < num_points - 1; i++) {
            double dx = coords[(i + 1) * 2] - coords[i * 2];
            double dy = coords[(i + 1) * 2 + 1] - coords[i * 2 + 1];
            len += sqrt(dx * dx + dy * dy);
        }

        return len;
    }

    return 0.0;
}

double geom_perimeter(const geometry_t *geom) {
    if (!geom || !geom->data) return 0.0;

    if (geom->type == GEOM_POLYGON) {
        const double *polygon = (const double *)geom->data;
        size_t num_points = geom->size / (2 * sizeof(double));

        double perimeter = 0;
        for (size_t i = 0; i < num_points - 1; i++) {
            double dx = polygon[(i + 1) * 2] - polygon[i * 2];
            double dy = polygon[(i + 1) * 2 + 1] - polygon[i * 2 + 1];
            perimeter += sqrt(dx * dx + dy * dy);
        }

        return perimeter;
    }

    return 0.0;
}

/* ========================================================================
 * 有效性检查
 * ======================================================================== */

bool geom_is_valid(const geometry_t *geom) {
    if (!geom) return false;

    switch (geom->type) {
        case GEOM_POINT:
            return geom->data != NULL && geom->size >= 2 * sizeof(double);
        case GEOM_LINESTRING:
            return geom->data != NULL && geom->size >= 4 * sizeof(double);
        case GEOM_POLYGON:
            return geom->data != NULL && geom->size >= 6 * sizeof(double);
        default:
            return false;
    }
}

bool geom_is_simple(const geometry_t *geom) {
    /* 简化实现：所有几何体都返回 true */
    (void)geom;
    return true;
}

/* ========================================================================
 * 维度
 * ======================================================================== */

int geom_dimension(const geometry_t *geom) {
    if (!geom) return -1;

    switch (geom->type) {
        case GEOM_POINT:
            return 0;
        case GEOM_LINESTRING:
            return 1;
        case GEOM_POLYGON:
            return 2;
        default:
            return -1;
    }
}

int geom_num_points(const geometry_t *geom) {
    if (!geom || !geom->data) return 0;

    switch (geom->type) {
        case GEOM_POINT:
            return 1;
        case GEOM_LINESTRING:
        case GEOM_POLYGON:
            return (int)(geom->size / (2 * sizeof(double)));
        default:
            return 0;
    }
}

int geom_num_rings(const geometry_t *geom) {
    if (!geom) return 0;

    if (geom->type == GEOM_POLYGON) {
        return 1;  /* 简化实现：只返回外环 */
    }

    return 0;
}

/* ========================================================================
 * 输出
 * ======================================================================== */

char *geom_to_wkt(const geometry_t *geom) {
    if (!geom || !geom->data) return NULL;

    char *wkt = NULL;

    switch (geom->type) {
        case GEOM_POINT: {
            const double *coords = (const double *)geom->data;
            wkt = (char *)malloc(64);
            if (wkt) {
                snprintf(wkt, 64, "POINT(%.6f %.6f)", coords[0], coords[1]);
            }
            break;
        }
        case GEOM_LINESTRING: {
            const double *coords = (const double *)geom->data;
            size_t num_points = geom->size / (2 * sizeof(double));

            wkt = (char *)malloc(num_points * 40 + 32);
            if (wkt) {
                char *p = wkt;
                p += sprintf(p, "LINESTRING(");
                for (size_t i = 0; i < num_points; i++) {
                    if (i > 0) p += sprintf(p, ", ");
                    p += sprintf(p, "%.6f %.6f", coords[i * 2], coords[i * 2 + 1]);
                }
                sprintf(p, ")");
            }
            break;
        }
        case GEOM_POLYGON: {
            const double *coords = (const double *)geom->data;
            size_t num_points = geom->size / (2 * sizeof(double));

            wkt = (char *)malloc(num_points * 40 + 32);
            if (wkt) {
                char *p = wkt;
                p += sprintf(p, "POLYGON((");
                for (size_t i = 0; i < num_points; i++) {
                    if (i > 0) p += sprintf(p, ", ");
                    p += sprintf(p, "%.6f %.6f", coords[i * 2], coords[i * 2 + 1]);
                }
                sprintf(p, "))");
            }
            break;
        }
    }

    return wkt;
}

void *geom_to_wkb(const geometry_t *geom, size_t *out_size) {
    if (!geom || !geom->data || !out_size) return NULL;

    unsigned char *wkb = NULL;

    switch (geom->type) {
        case GEOM_POINT: {
            /* WKB Point: 1 byte order + 4 type + 8 x + 8 y = 21 bytes */
            wkb = (unsigned char *)malloc(21);
            if (wkb) {
                const double *coords = (const double *)geom->data;

                wkb[0] = 0;  /* Little-endian */

                /* Type = 1 (Point) */
                uint32_t type = 1;
                memcpy(wkb + 1, &type, 4);

                /* Coordinates */
                memcpy(wkb + 5, &coords[0], 8);
                memcpy(wkb + 13, &coords[1], 8);

                *out_size = 21;
            }
            break;
        }
        default:
            /* 暂不支持其他类型 */
            break;
    }

    return wkb;
}
