/**
 * @file spatial_geo.c
 * @brief 空间几何对象实现
 */

#include "db/storage/spatial/spatial_geo.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

#define PI 3.14159265358979323846

/* ========================================================================
 * 几何对象创建和销毁
 * ======================================================================== */

SpatialGeometry *spatial_point_create(double x, double y) {
    SpatialGeometry *geom = (SpatialGeometry *)calloc(1, sizeof(SpatialGeometry));
    if (!geom) return NULL;

    geom->type = SPATIAL_GEOM_POINT;
    geom->geom.point.x = x;
    geom->geom.point.y = y;
    geom->bbox = bbox_create(x, y, x, y);

    return geom;
}

SpatialGeometry *spatial_line_create(const SpatialCoord *points, size_t num_points) {
    if (!points || num_points < 2) return NULL;

    SpatialGeometry *geom = (SpatialGeometry *)calloc(1, sizeof(SpatialGeometry));
    if (!geom) return NULL;

    geom->type = SPATIAL_GEOM_LINESTRING;
    geom->geom.line.points = (SpatialCoord *)malloc(num_points * sizeof(SpatialCoord));
    if (!geom->geom.line.points) {
        free(geom);
        return NULL;
    }
    memcpy(geom->geom.line.points, points, num_points * sizeof(SpatialCoord));
    geom->geom.line.num_points = num_points;

    /* 计算边界框 */
    double min_x = points[0].x, max_x = points[0].x;
    double min_y = points[0].y, max_y = points[0].y;
    for (size_t i = 1; i < num_points; i++) {
        if (points[i].x < min_x) min_x = points[i].x;
        if (points[i].x > max_x) max_x = points[i].x;
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }
    geom->bbox = bbox_create(min_x, min_y, max_x, max_y);

    return geom;
}

SpatialGeometry *spatial_polygon_create(const SpatialCoord *exterior,
                                        size_t num_exterior,
                                        const SpatialCoord **interiors,
                                        size_t num_interiors) {
    if (!exterior || num_exterior < 3) return NULL;

    SpatialGeometry *geom = (SpatialGeometry *)calloc(1, sizeof(SpatialGeometry));
    if (!geom) return NULL;

    geom->type = SPATIAL_GEOM_POLYGON;

    /* 外环 */
    geom->geom.polygon.exterior = (SpatialRing *)malloc(sizeof(SpatialRing));
    if (!geom->geom.polygon.exterior) {
        free(geom);
        return NULL;
    }
    geom->geom.polygon.exterior->points = (SpatialCoord *)malloc(num_exterior * sizeof(SpatialCoord));
    memcpy(geom->geom.polygon.exterior->points, exterior, num_exterior * sizeof(SpatialCoord));
    geom->geom.polygon.exterior->num_points = num_exterior;

    /* 内环 */
    if (interiors && num_interiors > 0) {
        geom->geom.polygon.interiors = (SpatialRing **)malloc(num_interiors * sizeof(SpatialRing *));
        geom->geom.polygon.num_interiors = num_interiors;
        for (size_t i = 0; i < num_interiors; i++) {
            geom->geom.polygon.interiors[i] = (SpatialRing *)malloc(sizeof(SpatialRing));
            size_t n = 0;
            /* 计算内环点数 */
            const SpatialCoord *ip = interiors[i];
            while (n < 1024 && (ip[n].x != 0 || ip[n].y != 0 || n == 0)) n++;
            geom->geom.polygon.interiors[i]->points = (SpatialCoord *)malloc(n * sizeof(SpatialCoord));
            memcpy(geom->geom.polygon.interiors[i]->points, interiors[i], n * sizeof(SpatialCoord));
            geom->geom.polygon.interiors[i]->num_points = n;
        }
    }

    /* 计算边界框 */
    double min_x = exterior[0].x, max_x = exterior[0].x;
    double min_y = exterior[0].y, max_y = exterior[0].y;
    for (size_t i = 1; i < num_exterior; i++) {
        if (exterior[i].x < min_x) min_x = exterior[i].x;
        if (exterior[i].x > max_x) max_x = exterior[i].x;
        if (exterior[i].y < min_y) min_y = exterior[i].y;
        if (exterior[i].y > max_y) max_y = exterior[i].y;
    }
    geom->bbox = bbox_create(min_x, min_y, max_x, max_y);

    return geom;
}

SpatialGeometry *spatial_geometry_clone(const SpatialGeometry *geom) {
    if (!geom) return NULL;

    switch (geom->type) {
        case SPATIAL_GEOM_POINT:
            return spatial_point_create(geom->geom.point.x, geom->geom.point.y);
        case SPATIAL_GEOM_LINESTRING:
            return spatial_line_create(geom->geom.line.points, geom->geom.line.num_points);
        default:
            return NULL;
    }
}

void spatial_geometry_destroy(SpatialGeometry *geom) {
    if (!geom) return;

    switch (geom->type) {
        case SPATIAL_GEOM_LINESTRING:
            free(geom->geom.line.points);
            break;
        case SPATIAL_GEOM_POLYGON:
            free(geom->geom.polygon.exterior->points);
            free(geom->geom.polygon.exterior);
            for (size_t i = 0; i < geom->geom.polygon.num_interiors; i++) {
                free(geom->geom.polygon.interiors[i]->points);
                free(geom->geom.polygon.interiors[i]);
            }
            free(geom->geom.polygon.interiors);
            break;
        default:
            break;
    }
    free(geom);
}

bbox_t spatial_geometry_bbox(const SpatialGeometry *geom) {
    return geom ? geom->bbox : bbox_create(0, 0, 0, 0);
}

/* ========================================================================
 * WKT 解析
 * ======================================================================== */

static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static const char *parse_keyword(const char *p, const char *keyword) {
    p = skip_ws(p);
    size_t len = strlen(keyword);
    if (strncasecmp(p, keyword, len) == 0) {
        return p + len;
    }
    return NULL;
}

static SpatialCoord parse_coord(const char **p) {
    SpatialCoord coord = {0, 0};
    *p = skip_ws(*p);
    coord.x = strtod(*p, (char **)p);
    *p = skip_ws(*p);
    if (**p == ',') (*p)++;
    *p = skip_ws(*p);
    coord.y = strtod(*p, (char **)p);
    return coord;
}

static SpatialGeometry *parse_point(const char *p) {
    p = parse_keyword(p, "POINT");
    if (!p) return NULL;
    p = skip_ws(p);
    if (*p == '(') p++;
    p = skip_ws(p);

    SpatialCoord coord = parse_coord(&p);

    p = skip_ws(p);
    if (*p == ')') p++;

    return spatial_point_create(coord.x, coord.y);
}

static SpatialGeometry *parse_linestring(const char *p) {
    p = parse_keyword(p, "LINESTRING");
    if (!p) return NULL;
    p = skip_ws(p);
    if (*p == '(') p++;
    p = skip_ws(p);

    SpatialCoord points[1024];
    size_t num_points = 0;

    while (*p && *p != ')' && num_points < 1024) {
        points[num_points++] = parse_coord(&p);
        p = skip_ws(p);
        if (*p == ',') p++;
        p = skip_ws(p);
    }

    if (*p == ')') p++;

    return spatial_line_create(points, num_points);
}

static SpatialGeometry *parse_polygon(const char *p) {
    p = parse_keyword(p, "POLYGON");
    if (!p) return NULL;
    p = skip_ws(p);
    if (*p == '(') p++;
    p = skip_ws(p);

    SpatialCoord exterior[1024];
    size_t num_exterior = 0;

    /* 解析外环 */
    while (*p && *p != ')' && *p != '(' && num_exterior < 1024) {
        exterior[num_exterior++] = parse_coord(&p);
        p = skip_ws(p);
        if (*p == ',') p++;
        p = skip_ws(p);
    }

    if (*p == ')') p++;

    return spatial_polygon_create(exterior, num_exterior, NULL, 0);
}

SpatialGeometry *spatial_wkt_parse(const char *wkt, SpatialWktError *out_error) {
    if (!wkt) {
        if (out_error) *out_error = SPATIAL_WKT_INVALID_FORMAT;
        return NULL;
    }

    const char *p = skip_ws(wkt);
    SpatialGeometry *geom = NULL;

    if (strncasecmp(p, "POINT", 5) == 0) {
        geom = parse_point(p);
    } else if (strncasecmp(p, "LINESTRING", 10) == 0) {
        geom = parse_linestring(p);
    } else if (strncasecmp(p, "POLYGON", 7) == 0) {
        geom = parse_polygon(p);
    } else {
        if (out_error) *out_error = SPATIAL_WKT_UNSUPPORTED_TYPE;
        return NULL;
    }

    if (!geom && out_error) {
        *out_error = SPATIAL_WKT_PARSE_ERROR;
    } else if (out_error) {
        *out_error = SPATIAL_WKT_OK;
    }

    return geom;
}

const char *spatial_wkt_error_str(SpatialWktError error) {
    switch (error) {
        case SPATIAL_WKT_OK: return "OK";
        case SPATIAL_WKT_INVALID_FORMAT: return "Invalid format";
        case SPATIAL_WKT_UNSUPPORTED_TYPE: return "Unsupported geometry type";
        case SPATIAL_WKT_PARSE_ERROR: return "Parse error";
        default: return "Unknown error";
    }
}

char *spatial_wkt_serialize(const SpatialGeometry *geom, size_t *out_len) {
    return spatial_wkt_serialize_precision(geom, -1);
}

char *spatial_wkt_serialize_precision(const SpatialGeometry *geom, int precision) {
    if (!geom) return NULL;

    char buf[4096];
    char fmt[32];
    if (precision >= 0) {
        snprintf(fmt, sizeof(fmt), " %%.%df", precision);
    } else {
        snprintf(fmt, sizeof(fmt), " %%.10g");
    }

    char *p = buf;
    p += sprintf(p, "%s(", geom->type == SPATIAL_GEOM_POINT ? "POINT" :
                 geom->type == SPATIAL_GEOM_LINESTRING ? "LINESTRING" :
                 geom->type == SPATIAL_GEOM_POLYGON ? "POLYGON" : "GEOMETRY");

    switch (geom->type) {
        case SPATIAL_GEOM_POINT:
            p += sprintf(p, fmt + 1, geom->geom.point.x);
            *p++ = ' ';
            p += sprintf(p, fmt + 1, geom->geom.point.y);
            break;
        case SPATIAL_GEOM_LINESTRING:
            for (size_t i = 0; i < geom->geom.line.num_points; i++) {
                if (i > 0) *p++ = ',';
                p += sprintf(p, fmt, geom->geom.line.points[i].x);
                *p++ = ' ';
                p += sprintf(p, fmt, geom->geom.line.points[i].y);
            }
            break;
        case SPATIAL_GEOM_POLYGON:
            for (size_t i = 0; i < geom->geom.polygon.exterior->num_points; i++) {
                if (i > 0) *p++ = ',';
                p += sprintf(p, fmt, geom->geom.polygon.exterior->points[i].x);
                *p++ = ' ';
                p += sprintf(p, fmt, geom->geom.polygon.exterior->points[i].y);
            }
            break;
        default:
            break;
    }

    *p++ = ')';
    *p = '\0';

    return strdup(buf);
}

/* ========================================================================
 * 距离计算
 * ======================================================================== */

double spatial_distance_point_point(const SpatialCoord *p1, const SpatialCoord *p2) {
    double dx = p2->x - p1->x;
    double dy = p2->y - p1->y;
    return sqrt(dx * dx + dy * dy);
}

static double point_to_segment_distance(const SpatialCoord *p,
                                        const SpatialCoord *a,
                                        const SpatialCoord *b) {
    double dx = b->x - a->x;
    double dy = b->y - a->y;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-10) {
        return spatial_distance_point_point(p, a);
    }

    double t = ((p->x - a->x) * dx + (p->y - a->y) * dy) / len_sq;
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    SpatialCoord proj = {a->x + t * dx, a->y + t * dy};
    return spatial_distance_point_point(p, &proj);
}

double spatial_distance_point_line(const SpatialCoord *p, const SpatialLine *line) {
    if (!p || !line || line->num_points < 2) return 0;

    double min_dist = INFINITY;
    for (size_t i = 0; i < line->num_points - 1; i++) {
        double d = point_to_segment_distance(p, &line->points[i], &line->points[i + 1]);
        if (d < min_dist) min_dist = d;
    }
    return min_dist;
}

double spatial_distance_point_polygon(const SpatialCoord *p, const SpatialPolygon *poly) {
    if (!p || !poly || !poly->exterior) return 0;

    if (spatial_point_in_polygon(p, poly)) {
        return 0;
    }

    /* 构造临时线段用于距离计算 */
    SpatialLine tmp_line;
    tmp_line.points = poly->exterior->points;
    tmp_line.num_points = poly->exterior->num_points;
    return spatial_distance_point_line(p, &tmp_line);
}

double spatial_distance(const SpatialGeometry *a, const SpatialGeometry *b) {
    if (!a || !b) return 0;

    if (a->type == SPATIAL_GEOM_POINT && b->type == SPATIAL_GEOM_POINT) {
        return spatial_distance_point_point(&a->geom.point, &b->geom.point);
    }
    if (a->type == SPATIAL_GEOM_POINT && b->type == SPATIAL_GEOM_LINESTRING) {
        return spatial_distance_point_line(&a->geom.point, &b->geom.line);
    }
    if (a->type == SPATIAL_GEOM_LINESTRING && b->type == SPATIAL_GEOM_POINT) {
        return spatial_distance_point_line(&b->geom.point, &a->geom.line);
    }
    if (a->type == SPATIAL_GEOM_POINT && b->type == SPATIAL_GEOM_POLYGON) {
        return spatial_distance_point_polygon(&a->geom.point, &b->geom.polygon);
    }
    if (a->type == SPATIAL_GEOM_POLYGON && b->type == SPATIAL_GEOM_POINT) {
        return spatial_distance_point_polygon(&b->geom.point, &a->geom.polygon);
    }

    /* 默认使用边界框距离 */
    return 0;
}

double spatial_distance_sphere(const SpatialCoord *p1, const SpatialCoord *p2,
                               double radius) {
    if (!p1 || !p2) return 0;
    if (radius <= 0) radius = 6371000;  /* 地球半径 */

    double lat1 = p1->y * PI / 180.0;
    double lat2 = p2->y * PI / 180.0;
    double dlat = (p2->y - p1->y) * PI / 180.0;
    double dlon = (p2->x - p1->x) * PI / 180.0;

    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return radius * c;
}

/* ========================================================================
 * 空间关系判断
 * ======================================================================== */

static bool point_in_ring(const SpatialCoord *p, const SpatialCoord *ring, size_t n) {
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((ring[i].y > p->y) != (ring[j].y > p->y)) &&
            (p->x < (ring[j].x - ring[i].x) * (p->y - ring[i].y) /
                     (ring[j].y - ring[i].y) + ring[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

bool spatial_point_in_polygon(const SpatialCoord *p, const SpatialPolygon *poly) {
    if (!p || !poly || !poly->exterior) return false;

    if (!point_in_ring(p, poly->exterior->points, poly->exterior->num_points)) {
        return false;
    }

    /* 检查是否在孔洞内 */
    for (size_t i = 0; i < poly->num_interiors; i++) {
        if (point_in_ring(p, poly->interiors[i]->points, poly->interiors[i]->num_points)) {
            return false;
        }
    }

    return true;
}

bool spatial_point_in_bbox(const SpatialCoord *p, const bbox_t *bbox) {
    return p && bbox &&
           p->x >= bbox->min_x && p->x <= bbox->max_x &&
           p->y >= bbox->min_y && p->y <= bbox->max_y;
}

bool spatial_bbox_intersects(const bbox_t *a, const bbox_t *b) {
    return a && b &&
           !(a->max_x < b->min_x || a->min_x > b->max_x ||
             a->max_y < b->min_y || a->min_y > b->max_y);
}

bool spatial_check_relation(const SpatialGeometry *a,
                           const SpatialGeometry *b,
                           SpatialRelation relation) {
    switch (relation) {
        case SPATIAL_REL_INTERSECTS:
            return spatial_intersects(a, b);
        case SPATIAL_REL_CONTAINS:
            return spatial_contains(a, b);
        case SPATIAL_REL_WITHIN:
            return spatial_within(a, b);
        default:
            return false;
    }
}

bool spatial_intersects(const SpatialGeometry *a, const SpatialGeometry *b) {
    if (!a || !b) return false;
    return spatial_bbox_intersects(&a->bbox, &b->bbox);
}

bool spatial_contains(const SpatialGeometry *container, const SpatialGeometry *item) {
    if (!container || !item) return false;

    if (!spatial_bbox_intersects(&container->bbox, &item->bbox)) {
        return false;
    }

    /* 简化实现：只处理点包含 */
    if (item->type == SPATIAL_GEOM_POINT && container->type == SPATIAL_GEOM_POLYGON) {
        return spatial_point_in_polygon(&item->geom.point, &container->geom.polygon);
    }

    return false;
}

bool spatial_within(const SpatialGeometry *inner, const SpatialGeometry *outer) {
    return spatial_contains(outer, inner);
}

SpatialGeometry *spatial_buffer(const SpatialGeometry *geom, double distance) {
    if (!geom || distance <= 0) return NULL;

    /* 简化实现：返回边界框扩展 */
    bbox_t buf_bbox = geom->bbox;
    buf_bbox.min_x -= distance;
    buf_bbox.min_y -= distance;
    buf_bbox.max_x += distance;
    buf_bbox.max_y += distance;

    SpatialCoord points[5];
    points[0] = (SpatialCoord){buf_bbox.min_x, buf_bbox.min_y};
    points[1] = (SpatialCoord){buf_bbox.max_x, buf_bbox.min_y};
    points[2] = (SpatialCoord){buf_bbox.max_x, buf_bbox.max_y};
    points[3] = (SpatialCoord){buf_bbox.min_x, buf_bbox.max_y};
    points[4] = points[0];

    return spatial_polygon_create(points, 5, NULL, 0);
}

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

double spatial_sql_distance(const char *wkt1, const char *wkt2) {
    SpatialWktError err;
    SpatialGeometry *g1 = spatial_wkt_parse(wkt1, &err);
    SpatialGeometry *g2 = spatial_wkt_parse(wkt2, &err);
    if (!g1 || !g2) {
        spatial_geometry_destroy(g1);
        spatial_geometry_destroy(g2);
        return 0;
    }
    double d = spatial_distance(g1, g2);
    spatial_geometry_destroy(g1);
    spatial_geometry_destroy(g2);
    return d;
}

double spatial_sql_distance_sphere(double x1, double y1, double x2, double y2,
                                   double radius) {
    SpatialCoord p1 = {x1, y1};
    SpatialCoord p2 = {x2, y2};
    return spatial_distance_sphere(&p1, &p2, radius);
}

bool spatial_sql_contains(const char *wkt1, const char *wkt2) {
    SpatialWktError err;
    SpatialGeometry *g1 = spatial_wkt_parse(wkt1, &err);
    SpatialGeometry *g2 = spatial_wkt_parse(wkt2, &err);
    if (!g1 || !g2) {
        spatial_geometry_destroy(g1);
        spatial_geometry_destroy(g2);
        return false;
    }
    bool result = spatial_contains(g1, g2);
    spatial_geometry_destroy(g1);
    spatial_geometry_destroy(g2);
    return result;
}

bool spatial_sql_within(const char *wkt1, const char *wkt2) {
    return spatial_sql_contains(wkt2, wkt1);
}

bool spatial_sql_intersects(const char *wkt1, const char *wkt2) {
    SpatialWktError err;
    SpatialGeometry *g1 = spatial_wkt_parse(wkt1, &err);
    SpatialGeometry *g2 = spatial_wkt_parse(wkt2, &err);
    if (!g1 || !g2) {
        spatial_geometry_destroy(g1);
        spatial_geometry_destroy(g2);
        return false;
    }
    bool result = spatial_intersects(g1, g2);
    spatial_geometry_destroy(g1);
    spatial_geometry_destroy(g2);
    return result;
}

char *spatial_sql_buffer(const char *wkt, double distance) {
    SpatialWktError err;
    SpatialGeometry *geom = spatial_wkt_parse(wkt, &err);
    if (!geom) return strdup("");
    SpatialGeometry *buf = spatial_buffer(geom, distance);
    char *result = spatial_wkt_serialize(buf, NULL);
    spatial_geometry_destroy(geom);
    spatial_geometry_destroy(buf);
    return result ? result : strdup("");
}
