/**
 * @file hilbert.c
 * @brief Hilbert 曲线空间填充曲线实现
 *
 * 使用查找表法实现 Hilbert 曲线的编码和解码。
 * 基于 PostgreSQL PostGIS 扩展的参考实现。
 */
#include "db/index/hilbert.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

/* ========================================================================
 * 查找表法 Hilbert 编码/解码
 *
 * 算法原理：
 * - Hilbert 曲线将 2D 空间映射到 1D 索引
 * - 每个 order 级别的 Hilbert 曲线由 4 个 (order-1) 子曲线组成
 * - 子曲线通过旋转/镜像变换连接
 * ======================================================================== */

/* 2D 旋转模式：rot[ry][rx] = (new_rx, new_ry) */
static const uint8_t ROT[2][2][2] = {
    /* ry=0 */ { {0, 1}, {1, 0} },  /* ry=0, rx=0: (0,0); ry=0, rx=1: (1,0) */
    /* ry=1 */ { {0, 0}, {1, 1} }   /* ry=1, rx=0: (0,0); ry=1, rx=1: (1,1) */
};

/* 2D 坐标交换标志：swap[ry][rx] */
static const uint8_t SWAP[2][2] = {
    /* ry=0 */ {1, 1},  /* ry=0: always swap */
    /* ry=1 */ {0, 0}   /* ry=1: never swap */
};

/* 2D 翻转标志：flip[ry][rx] */
static const uint8_t FLIP[2][2] = {
    /* ry=0 */ {0, 1},  /* ry=0, rx=0: no flip; ry=0, rx=1: flip */
    /* ry=1 */ {0, 0}   /* ry=1: never flip */
};

/* 2D 方向：dir[ry][rx] = d offset factor */
static const uint8_t DIR[2][2] = {
    {0, 3},  /* ry=0: (0,0)->0, (1,0)->3 */
    {1, 2}   /* ry=1: (0,0)->1, (1,0)->2 */
};

/* 3D 旋转查找表 */
static const uint8_t ROT3D[8][3] = {
    {0, 0, 0},  /* 000: no change */
    {1, 0, 0},  /* 001 */
    {0, 1, 0},  /* 010 */
    {1, 1, 0},  /* 011 */
    {0, 0, 1},  /* 100 */
    {1, 0, 1},  /* 101 */
    {0, 1, 1},  /* 110 */
    {1, 1, 1}   /* 111 */
};

/* ========================================================================
 * 2D Hilbert 编码/解码实现
 * ======================================================================== */

uint64_t hilbert_encode2d(double x, double y, uint32_t order) {
    if (order == 0 || order > HILBERT_MAX_ORDER) {
        return HILBERT_INVALID_INDEX;
    }

    uint32_t n = 1U << order;
    uint32_t ix = (uint32_t)(fmax(0.0, fmin(1.0, x)) * (n - 1));
    uint32_t iy = (uint32_t)(fmax(0.0, fmin(1.0, y)) * (n - 1));

    uint64_t d = 0;
    uint32_t rx, ry;
    uint32_t level = order;

    while (level > 0) {
        level--;

        rx = (ix >> level) & 1;
        ry = (iy >> level) & 1;

        d += (uint64_t)(1U << (2 * level)) * DIR[ry][rx];

        /* 应用变换 */
        if (ry == 0) {
            if (rx == 1) {
                ix ^= ((1U << level) - 1);
                iy ^= ((1U << level) - 1);
            }
            /* 交换 ix 和 iy */
            uint32_t t = ix;
            ix = iy;
            iy = t;
        }
    }

    return d;
}

void hilbert_decode2d(uint64_t h, uint32_t order,
                      double *out_x, double *out_y) {
    if (order == 0 || order > HILBERT_MAX_ORDER) {
        if (out_x) *out_x = 0;
        if (out_y) *out_y = 0;
        return;
    }

    uint32_t x = 0, y = 0;
    uint32_t rx, ry;
    uint32_t level;

    for (level = 0; level < order; level++) {
        /* 从 d 中提取 2 bits */
        uint32_t bits = (h >> (2 * level)) & 3;

        /* 确定 rx, ry */
        if (bits == 0) { rx = 0; ry = 0; }
        else if (bits == 1) { rx = 0; ry = 1; }
        else if (bits == 2) { rx = 1; ry = 1; }
        else { rx = 1; ry = 0; }

        /* 添加当前位的贡献 */
        x |= (rx << level);
        y |= (ry << level);

        /* 逆变换 */
        if (ry == 0) {
            uint32_t t = x;
            x = y;
            y = t;
            if (rx == 1) {
                x ^= ((1U << level) - 1);
                y ^= ((1U << level) - 1);
            }
        }
    }

    if (out_x) *out_x = (double)x / ((1U << order) - 1);
    if (out_y) *out_y = (double)y / ((1U << order) - 1);
}

/* ========================================================================
 * 3D Hilbert 编码/解码实现
 * ======================================================================== */

uint64_t hilbert_encode3d(double x, double y, double z, uint32_t order) {
    if (order == 0 || order > HILBERT_MAX_ORDER) {
        return HILBERT_INVALID_INDEX;
    }

    uint32_t n = 1U << order;
    uint32_t ix = (uint32_t)(fmax(0.0, fmin(1.0, x)) * (n - 1));
    uint32_t iy = (uint32_t)(fmax(0.0, fmin(1.0, y)) * (n - 1));
    uint32_t iz = (uint32_t)(fmax(0.0, fmin(1.0, z)) * (n - 1));

    uint64_t d = 0;
    uint32_t rx, ry, rz;

    for (uint32_t level = 0; level < order; level++) {
        uint32_t s = 1U << level;

        rx = (ix >> level) & 1;
        ry = (iy >> level) & 1;
        rz = (iz >> level) & 1;

        /* c 格式: rz | ry | rx (bit2 | bit1 | bit0) */
        uint32_t c = (rz << 2) | (ry << 1) | rx;
        d += (uint64_t)s * s * s * c;

        /* 3D Hilbert 旋转 */
        if (rz == 0) {
            if (rx == 1) {
                ix ^= (s - 1);
                iy ^= (s - 1);
            }
            uint32_t t = ix;
            ix = iy;
            iy = t;
        }
        if (ry == 0) {
            if (rx == 1) {
                ix ^= (s - 1);
                iz ^= (s - 1);
            }
            uint32_t t = ix;
            ix = iz;
            iz = t;
        }
    }

    return d;
}

void hilbert_decode3d(uint64_t h, uint32_t order,
                      double *out_x, double *out_y, double *out_z) {
    if (order == 0 || order > HILBERT_MAX_ORDER) {
        if (out_x) *out_x = 0;
        if (out_y) *out_y = 0;
        if (out_z) *out_z = 0;
        return;
    }

    uint32_t x = 0, y = 0, z = 0;
    uint32_t rx, ry, rz;

    for (uint32_t level = 0; level < order; level++) {
        uint32_t s = 1U << level;
        uint32_t bits = (h >> (3 * level)) & 7;

        rx = bits & 1;
        ry = (bits >> 1) & 1;
        rz = (bits >> 2) & 1;

        x |= (rx << level);
        y |= (ry << level);
        z |= (rz << level);

        /* 3D 逆旋转 */
        if (rz == 0) {
            if (rx == 1) {
                x ^= (s - 1);
                y ^= (s - 1);
            }
            uint32_t t = x;
            x = y;
            y = t;
        }
        if (ry == 0) {
            if (rx == 1) {
                x ^= (s - 1);
                z ^= (s - 1);
            }
            uint32_t t = x;
            x = z;
            z = t;
        }
    }

    if (out_x) *out_x = (double)x / ((1U << order) - 1);
    if (out_y) *out_y = (double)y / ((1U << order) - 1);
    if (out_z) *out_z = (double)z / ((1U << order) - 1);
}

/* ========================================================================
 * BBox 相关 API
 * ======================================================================== */

void hilbert_bbox_range(const hilbert_bbox_t *bbox, uint32_t order,
                        uint64_t *out_min, uint64_t *out_max) {
    if (bbox == NULL || out_min == NULL || out_max == NULL) return;

    uint64_t corners[4] = {
        hilbert_encode2d(bbox->min_x, bbox->min_y, order),
        hilbert_encode2d(bbox->min_x, bbox->max_y, order),
        hilbert_encode2d(bbox->max_x, bbox->min_y, order),
        hilbert_encode2d(bbox->max_x, bbox->max_y, order)
    };

    *out_min = corners[0];
    *out_max = corners[0];

    for (int i = 1; i < 4; i++) {
        if (corners[i] < *out_min) *out_min = corners[i];
        if (corners[i] > *out_max) *out_max = corners[i];
    }
}

uint64_t hilbert_bbox_center(const hilbert_bbox_t *bbox, uint32_t order) {
    if (bbox == NULL) return HILBERT_INVALID_INDEX;

    double cx = (bbox->min_x + bbox->max_x) / 2.0;
    double cy = (bbox->min_y + bbox->max_y) / 2.0;

    return hilbert_encode2d(cx, cy, order);
}

/* ========================================================================
 * Hilbert 索引管理
 * ======================================================================== */

hilbert_index_t *hilbert_index_create(uint32_t capacity, uint32_t order) {
    if (capacity == 0) capacity = 1024;
    if (order == 0) order = HILBERT_DEFAULT_ORDER;
    if (order > HILBERT_MAX_ORDER) order = HILBERT_MAX_ORDER;

    hilbert_index_t *index = (hilbert_index_t *)calloc(1, sizeof(hilbert_index_t));
    if (index == NULL) return NULL;

    index->records = (hilbert_record_t *)calloc(capacity, sizeof(hilbert_record_t));
    if (index->records == NULL) {
        free(index);
        return NULL;
    }

    index->capacity = capacity;
    index->count = 0;
    index->order = order;

    index->data_bbox.min_x = 1e100;
    index->data_bbox.min_y = 1e100;
    index->data_bbox.max_x = -1e100;
    index->data_bbox.max_y = -1e100;

    return index;
}

void hilbert_index_destroy(hilbert_index_t *index) {
    if (index == NULL) return;
    free(index->records);
    free(index);
}

int hilbert_index_add(hilbert_index_t *index, uint64_t item_id,
                      const hilbert_bbox_t *bbox) {
    if (index == NULL || bbox == NULL) return -1;

    if (index->count >= index->capacity) {
        uint32_t new_cap = index->capacity * 2;
        hilbert_record_t *new_records = (hilbert_record_t *)realloc(
            index->records, new_cap * sizeof(hilbert_record_t));
        if (new_records == NULL) return -1;
        index->records = new_records;
        index->capacity = new_cap;
    }

    uint64_t h = hilbert_bbox_center(bbox, index->order);

    index->records[index->count].hilbert_code = h;
    index->records[index->count].item_id = item_id;
    index->records[index->count].bbox = *bbox;
    index->count++;

    if (bbox->min_x < index->data_bbox.min_x) index->data_bbox.min_x = bbox->min_x;
    if (bbox->min_y < index->data_bbox.min_y) index->data_bbox.min_y = bbox->min_y;
    if (bbox->max_x > index->data_bbox.max_x) index->data_bbox.max_x = bbox->max_x;
    if (bbox->max_y > index->data_bbox.max_y) index->data_bbox.max_y = bbox->max_y;

    return 0;
}

static int hilbert_record_compare(const void *a, const void *b) {
    const hilbert_record_t *ra = (const hilbert_record_t *)a;
    const hilbert_record_t *rb = (const hilbert_record_t *)b;
    if (ra->hilbert_code < rb->hilbert_code) return -1;
    if (ra->hilbert_code > rb->hilbert_code) return 1;
    return 0;
}

void hilbert_index_build(hilbert_index_t *index) {
    if (index == NULL || index->count == 0) return;
    qsort(index->records, index->count, sizeof(hilbert_record_t),
          hilbert_record_compare);
}

uint32_t hilbert_index_range_query(const hilbert_index_t *index,
                                   const hilbert_bbox_t *query,
                                   uint64_t *results,
                                   uint32_t max_results) {
    if (index == NULL || query == NULL || results == NULL || max_results == 0) {
        return 0;
    }

    uint64_t min_h, max_h;
    hilbert_bbox_range(query, index->order, &min_h, &max_h);

    uint32_t found = 0;

    for (uint32_t i = 0; i < index->count && found < max_results; i++) {
        const hilbert_record_t *rec = &index->records[i];

        if (rec->hilbert_code < min_h || rec->hilbert_code > max_h) {
            continue;
        }

        if (rec->bbox.max_x < query->min_x || rec->bbox.min_x > query->max_x ||
            rec->bbox.max_y < query->min_y || rec->bbox.min_y > query->max_y) {
            continue;
        }

        results[found++] = rec->item_id;
    }

    return found;
}

static double bbox_center_distance_sq(const hilbert_point2d_t *point,
                                      const hilbert_bbox_t *bbox) {
    double cx = (bbox->min_x + bbox->max_x) / 2.0;
    double cy = (bbox->min_y + bbox->max_y) / 2.0;
    double dx = point->x - cx;
    double dy = point->y - cy;
    return dx * dx + dy * dy;
}

uint32_t hilbert_index_knn(const hilbert_index_t *index,
                           const hilbert_point2d_t *point, int k,
                           hilbert_record_t *results,
                           uint32_t max_results) {
    if (index == NULL || point == NULL || k <= 0 || results == NULL) {
        return 0;
    }

    if (index->count == 0) return 0;

    uint32_t n = (uint32_t)(k < (int)max_results ? k : (int)max_results);

    typedef struct {
        uint32_t idx;
        double dist;
    } dist_item_t;

    dist_item_t *items = (dist_item_t *)malloc(index->count * sizeof(dist_item_t));
    if (items == NULL) return 0;

    for (uint32_t i = 0; i < index->count; i++) {
        items[i].idx = i;
        items[i].dist = bbox_center_distance_sq(point, &index->records[i].bbox);
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t min_idx = i;
        for (uint32_t j = i + 1; j < index->count; j++) {
            if (items[j].dist < items[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            dist_item_t tmp = items[i];
            items[i] = items[min_idx];
            items[min_idx] = tmp;
        }
        results[i] = index->records[items[i].idx];
    }

    free(items);
    return n;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

uint32_t hilbert_auto_order(double width, double height) {
    double max_dim = width > height ? width : height;
    if (max_dim <= 0) return HILBERT_DEFAULT_ORDER;

    uint32_t order = 1;
    double range = 2.0;

    while (range < max_dim && order < HILBERT_MAX_ORDER) {
        order++;
        range *= 2.0;
    }

    return order;
}

uint64_t hilbert_distance(uint64_t code1, uint64_t code2) {
    if (code1 > code2) {
        return code1 - code2;
    }
    return code2 - code1;
}

bool hilbert_in_range(uint64_t code, uint64_t min, uint64_t max) {
    return code >= min && code <= max;
}

/* ========================================================================
 * 持久化实现
 * ======================================================================== */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t capacity;
    uint32_t order;
    hilbert_bbox_t data_bbox;
} hilbert_index_header_t;

int hilbert_index_save(const hilbert_index_t *index, const char *path) {
    if (index == NULL || path == NULL) return -1;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }

    /* 写入文件头 */
    hilbert_index_header_t header = {
        .magic = HILBERT_INDEX_MAGIC,
        .version = HILBERT_INDEX_VERSION,
        .count = index->count,
        .capacity = index->capacity,
        .order = index->order,
        .data_bbox = index->data_bbox
    };

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    /* 写入记录数据 */
    if (index->count > 0 && index->records != NULL) {
        if (fwrite(index->records, sizeof(hilbert_record_t), index->count, fp)
            != index->count) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

hilbert_index_t *hilbert_index_load(const char *path) {
    if (path == NULL) return NULL;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    /* 读取文件头 */
    hilbert_index_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 验证魔数和版本 */
    if (header.magic != HILBERT_INDEX_MAGIC ||
        header.version != HILBERT_INDEX_VERSION) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    hilbert_index_t *index = hilbert_index_create(header.capacity, header.order);
    if (index == NULL) {
        fclose(fp);
        return NULL;
    }

    /* 恢复数据 */
    index->count = header.count;
    index->data_bbox = header.data_bbox;

    /* 读取记录 */
    if (header.count > 0) {
        index->records = (hilbert_record_t *)malloc(
            header.count * sizeof(hilbert_record_t));
        if (index->records == NULL) {
            hilbert_index_destroy(index);
            fclose(fp);
            return NULL;
        }

        if (fread(index->records, sizeof(hilbert_record_t), header.count, fp)
            != header.count) {
            hilbert_index_destroy(index);
            fclose(fp);
            return NULL;
        }
    }

    fclose(fp);
    return index;
}
