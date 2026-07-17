/**
 * @file spatial_engine.c
 * @brief 空间存储引擎实现
 */
#include "db/storage/spatial/spatial_engine.h"  /* 使用绝对路径 */
#include "db/storage/spatial/rtree.h"  /* 后包含，使用已定义的类型 */
#include "db/index/hilbert.h"          /* Hilbert 曲线索引 */
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Windows/macOS/Linux 跨平台兼容 */
#ifdef _WIN32
    #include <direct.h>
    #include <errno.h>
    #define mkdir(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#define SPATIAL_ENGINE_NAME "spatial_engine"
#define SPATIAL_DATA_PREFIX "spatial_"

typedef struct spatial_engine_global_s {
    char data_dir[512];
    bool initialized;
} spatial_engine_global_t;

static spatial_engine_global_t g_spatial_engine = {
    .data_dir = {0},
    .initialized = false
};

typedef struct spatial_header_s {
    char name[256];
    uint64_t num_geometries;
} spatial_header_t;

static int get_dir_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s",
             g_spatial_engine.data_dir, SPATIAL_DATA_PREFIX, name);
    return 0;
}

static int spatial_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (name == NULL) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    /* 创建目录（兼容 Windows/macOS/Linux） */
#ifdef _WIN32
    if (mkdir(dir_path) != 0 && errno != EEXIST) {
#else
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_ERROR("创建空间数据目录失败: %s", dir_path);
        return -1;
    }

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    spatial_header_t header = { .name = {0}, .num_geometries = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }
    return 0;
}

static void *spatial_engine_table_open(const char *name, AccessMode mode) {
    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return NULL;

    spatial_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    spatial_engine_db_t *db = (spatial_engine_db_t *)calloc(1, sizeof(spatial_engine_db_t));
    if (db == NULL) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    get_dir_path(name, db->data_dir, sizeof(db->data_dir));
    db->mode = mode;
    db->num_geometries = header.num_geometries;
    db->rtree_index = NULL;
    db->index_built = false;

    return db;
}

static int spatial_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", db->data_dir);

    spatial_header_t header = { .num_geometries = db->num_geometries };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 释放 R-Tree 索引 */
    if (db->rtree_index != NULL) {
        rtree_free((rtree_t *)db->rtree_index);
        db->rtree_index = NULL;
    }

    /* 释放 Hilbert 辅助索引 */
    if (db->hilbert_index != NULL) {
        hilbert_index_destroy((hilbert_index_t *)db->hilbert_index);
        db->hilbert_index = NULL;
    }

    free(db);
    return 0;
}

static int spatial_engine_table_drop(const char *name) {
    (void)name;
    return 0;
}

static int spatial_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;
    if (len < sizeof(uint32_t) * 3) return -1;

    /* 解析几何数据获取 ID 和边界框 */
    const uint8_t *ptr = (const uint8_t *)data;
    uint64_t id;
    memcpy(&id, ptr, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    geometry_type_t type;
    memcpy(&type, ptr, sizeof(geometry_type_t));
    ptr += sizeof(geometry_type_t);

    point_t centroid;
    memcpy(&centroid, ptr, sizeof(point_t));
    ptr += sizeof(point_t);

    bbox_t bounds;
    memcpy(&bounds, ptr, sizeof(bbox_t));

    /* 写入数据文件 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/geometries.bin", db->data_dir);

    FILE *fp = fopen(data_path, "ab");
    if (fp == NULL) {
        fp = fopen(data_path, "wb");
        if (fp == NULL) return -1;
    }

    fwrite(data, 1, len, fp);
    fclose(fp);

    db->num_geometries++;

    /* 如果索引已构建，同时更新索引 */
    if (db->rtree_index != NULL && db->index_built) {
        rtree_insert((rtree_t *)db->rtree_index, id, &bounds);
    }

    return 0;
}

static scan_desc_t *spatial_engine_scan_begin(void *rel,
                                              const scan_key_t *keys, int nkeys,
                                              ScanDirection direction) {
    (void)keys;
    (void)nkeys;
    (void)direction;

    if (rel == NULL) return NULL;
    return NULL;
}

static int spatial_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

static int spatial_engine_scan_end(scan_desc_t *scan) {
    if (scan) free(scan);
    return 0;
}

static int spatial_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    if (name == NULL) return 0;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return -1;

    spatial_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    stats->num_objects = header.num_geometries;
    return 0;
}

static const storage_ops_t g_spatial_engine_ops = {
    .name = SPATIAL_ENGINE_NAME,
    .model = MODEL_SPATIAL,
    .init = spatial_engine_init,
    .shutdown = spatial_engine_shutdown,
    .table_create = spatial_engine_table_create,
    .table_open = spatial_engine_table_open,
    .table_close = spatial_engine_table_close,
    .table_drop = spatial_engine_table_drop,
    .tuple_insert = spatial_engine_tuple_insert,
    .tuple_update = NULL,
    .tuple_delete = NULL,
    .scan_begin = spatial_engine_scan_begin,
    .scan_next = spatial_engine_scan_next,
    .scan_end = spatial_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = spatial_engine_get_stats,
};

int spatial_engine_init(const char *data_dir) {
    if (g_spatial_engine.initialized) return 0;
    if (data_dir == NULL) return -1;
    strncpy(g_spatial_engine.data_dir, data_dir, sizeof(g_spatial_engine.data_dir) - 1);
    g_spatial_engine.initialized = true;
    return 0;
}

int spatial_engine_shutdown(void) {
    g_spatial_engine.initialized = false;
    return 0;
}

const storage_ops_t *spatial_engine_get_ops(void) {
    return &g_spatial_engine_ops;
}

int spatial_engine_create(const char *name, const storage_schema_t *schema) {
    return spatial_engine_table_create(name, schema);
}

void *spatial_engine_open(const char *name, AccessMode mode) {
    return spatial_engine_table_open(name, mode);
}

int spatial_engine_close(void *rel) {
    return spatial_engine_table_close(rel);
}

int spatial_engine_drop(const char *name) {
    return spatial_engine_table_drop(name);
}

int spatial_engine_insert(void *rel, const void *data, size_t len) {
    return spatial_engine_tuple_insert(rel, data, len);
}

int spatial_engine_stats(const char *name, storage_stats_t *stats) {
    return spatial_engine_get_stats(name, stats);
}

/* ========================================================================
 * 空间查询实现（暴力扫描）
 * ======================================================================== */

/* 几何对象数据结构（与 insert 格式对应） */
typedef struct geometry_record_s {
    uint64_t id;
    geometry_type_t type;
    point_t centroid;
    bbox_t bounds;
    uint8_t data[256];
} geometry_record_t;

static double distance_point_to_point(const point_t *a, const point_t *b) {
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}

int spatial_engine_query_bbox(void *rel, const spatial_query_args_t *args,
                               spatial_query_result_t *results, uint32_t *num_results) {
    if (rel == NULL || args == NULL || results == NULL || num_results == NULL) {
        return -1;
    }

    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;
    uint32_t count = 0;
    uint32_t max_results = args->limit > 0 ? args->limit : 100;

    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/geometries.bin", db->data_dir);

    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) {
        *num_results = 0;
        return 0;  /* 文件不存在时返回空结果 */
    }

    geometry_record_t rec;
    while (count < max_results && fread(&rec, sizeof(rec), 1, fp) == 1) {
        /* 跳过 offset */
        if (rec.id < args->offset) {
            continue;
        }

        /* 检查边界框是否相交 */
        if (bbox_intersects(&rec.bounds, &args->bbox)) {
            results[count].data = malloc(sizeof(rec));
            if (results[count].data) {
                memcpy(results[count].data, &rec, sizeof(rec));
                results[count].len = sizeof(rec);
                results[count].id = rec.id;
                results[count].distance = 0;
                count++;
            }
        }
    }

    fclose(fp);
    *num_results = count;
    return 0;
}

int spatial_engine_nearest(void *rel, const point_t *point, uint32_t k,
                            spatial_query_result_t *results, uint32_t *num_results) {
    if (rel == NULL || point == NULL || results == NULL || num_results == NULL) {
        return -1;
    }

    if (k == 0) {
        *num_results = 0;
        return 0;
    }

    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;
    uint32_t count = 0;

    /* 分配临时数组存储所有结果 */
    typedef struct {
        geometry_record_t rec;
        double dist;
    } temp_result_t;

    temp_result_t *temp_results = calloc(db->num_geometries, sizeof(temp_result_t));
    if (temp_results == NULL) {
        return -1;
    }

    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/geometries.bin", db->data_dir);

    FILE *fp = fopen(data_path, "rb");
    if (fp != NULL) {
        geometry_record_t rec;
        while (fread(&rec, sizeof(rec), 1, fp) == 1) {
            temp_results[count].rec = rec;
            temp_results[count].dist = distance_point_to_point(&rec.centroid, point);
            count++;
        }
        fclose(fp);
    }

    /* 使用简单选择排序找出 top-k */
    uint32_t n = count;
    for (uint32_t i = 0; i < n - 1 && i < k; i++) {
        uint32_t min_idx = i;
        for (uint32_t j = i + 1; j < n; j++) {
            if (temp_results[j].dist < temp_results[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            temp_result_t tmp = temp_results[i];
            temp_results[i] = temp_results[min_idx];
            temp_results[min_idx] = tmp;
        }
    }

    /* 复制 top-k 结果 */
    uint32_t result_count = (n < k) ? n : k;
    for (uint32_t i = 0; i < result_count; i++) {
        results[i].data = malloc(sizeof(geometry_record_t));
        if (results[i].data) {
            memcpy(results[i].data, &temp_results[i].rec, sizeof(geometry_record_t));
            results[i].len = sizeof(geometry_record_t);
            results[i].id = temp_results[i].rec.id;
            results[i].distance = temp_results[i].dist;
        }
    }

    free(temp_results);
    *num_results = result_count;
    return 0;
}

/* ========================================================================
 * R-Tree 索引 API 实现
 * ======================================================================== */

/**
 * @brief 获取索引文件路径
 */
static int get_index_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/rtree_index.bin",
             g_spatial_engine.data_dir, SPATIAL_DATA_PREFIX, name);
    return 0;
}

/**
 * @brief 从几何数据文件加载数据构建 R-Tree
 */
static int build_rtree_from_file(spatial_engine_db_t *db) {
    if (db->num_geometries == 0) return 0;

    rtree_t *rtree = rtree_create(16);
    if (!rtree) return -1;

    /* 创建 Hilbert 辅助索引 */
    hilbert_index_t *hilbert = hilbert_index_create(db->num_geometries + 100,
                                                     HILBERT_DEFAULT_ORDER);
    if (!hilbert) {
        rtree_free(rtree);
        return -1;
    }

    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/geometries.bin", db->data_dir);

    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) {
        rtree_free(rtree);
        hilbert_index_destroy(hilbert);
        return -1;
    }

    /* 统计边界框范围用于 Hilbert 阶数自适应 */
    hilbert_bbox_t data_bbox = { .min_x = 0, .min_y = 0, .max_x = 1, .max_y = 1 };
    bool first = true;

    geometry_record_t rec;
    while (fread(&rec, sizeof(rec), 1, fp) == 1) {
        rtree_insert(rtree, rec.id, &rec.bounds);

        /* 收集 Hilbert 索引数据 */
        hilbert_bbox_t hbbox;
        hbbox.min_x = rec.bounds.min_x;
        hbbox.min_y = rec.bounds.min_y;
        hbbox.max_x = rec.bounds.max_x;
        hbbox.max_y = rec.bounds.max_y;
        hilbert_index_add(hilbert, rec.id, &hbbox);

        /* 更新边界框统计 */
        if (first) {
            data_bbox.min_x = rec.bounds.min_x;
            data_bbox.min_y = rec.bounds.min_y;
            data_bbox.max_x = rec.bounds.max_x;
            data_bbox.max_y = rec.bounds.max_y;
            first = false;
        } else {
            if (rec.bounds.min_x < data_bbox.min_x) data_bbox.min_x = rec.bounds.min_x;
            if (rec.bounds.min_y < data_bbox.min_y) data_bbox.min_y = rec.bounds.min_y;
            if (rec.bounds.max_x > data_bbox.max_x) data_bbox.max_x = rec.bounds.max_x;
            if (rec.bounds.max_y > data_bbox.max_y) data_bbox.max_y = rec.bounds.max_y;
        }
    }

    fclose(fp);

    /* 构建 Hilbert 索引（排序） */
    hilbert->data_bbox = data_bbox;
    hilbert_index_build(hilbert);

    db->rtree_index = rtree;
    db->index_built = true;
    db->hilbert_index = hilbert;
    db->hilbert_built = true;
    return 0;
}

int spatial_engine_build_index(void *rel) {
    if (rel == NULL) return -1;
    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    if (db->num_geometries == 0) {
        return 0;
    }

    /* 如果已有索引，先释放 */
    if (db->rtree_index != NULL) {
        rtree_free((rtree_t *)db->rtree_index);
        db->rtree_index = NULL;
    }

    /* 从文件加载数据构建 R-Tree */
    return build_rtree_from_file(db);
}

int spatial_engine_search_bbox(void *rel, const bbox_t *bbox,
                               void *results, int max_results) {
    if (rel == NULL || bbox == NULL || results == NULL || max_results <= 0) {
        return -1;
    }

    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    /* 如果索引未构建，先构建 */
    if (db->rtree_index == NULL || !db->index_built) {
        if (spatial_engine_build_index(rel) != 0) {
            return -1;
        }
    }

    if (db->rtree_index == NULL) {
        return 0;
    }

    rtree_result_t *rtree_results = (rtree_result_t *)results;
    return rtree_search((rtree_t *)db->rtree_index, bbox, rtree_results, max_results);
}

int spatial_engine_search_nearest(void *rel, const point_t *point, int k,
                                   void *results, int max_results) {
    if (rel == NULL || point == NULL || results == NULL || k <= 0) {
        return -1;
    }

    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    /* 如果索引未构建，先构建 */
    if (db->rtree_index == NULL || !db->index_built) {
        if (spatial_engine_build_index(rel) != 0) {
            return -1;
        }
    }

    if (db->rtree_index == NULL) {
        return 0;
    }

    rtree_result_t *rtree_results = (rtree_result_t *)results;
    return rtree_knn((rtree_t *)db->rtree_index, point, k, rtree_results, max_results);
}

int spatial_engine_save_index(void *rel, const char *path) {
    if (rel == NULL) return -1;
    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    if (db->rtree_index == NULL) {
        return -1;
    }

    char index_path[512];
    if (path != NULL && strlen(path) > 0) {
        strncpy(index_path, path, sizeof(index_path) - 1);
    } else {
        get_index_path(db->name, index_path, sizeof(index_path));
    }

    return rtree_save((rtree_t *)db->rtree_index, index_path);
}

int spatial_engine_load_index(void *rel, const char *path) {
    if (rel == NULL) return -1;
    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    /* 如果已有索引，先释放 */
    if (db->rtree_index != NULL) {
        rtree_free((rtree_t *)db->rtree_index);
        db->rtree_index = NULL;
    }

    char index_path[512];
    if (path != NULL && strlen(path) > 0) {
        strncpy(index_path, path, sizeof(index_path) - 1);
    } else {
        get_index_path(db->name, index_path, sizeof(index_path));
    }

    rtree_t *rtree = rtree_load(index_path);
    if (rtree == NULL) {
        return -1;
    }

    db->rtree_index = rtree;
    db->index_built = true;
    return 0;
}

int spatial_engine_drop_index(void *rel) {
    if (rel == NULL) return -1;
    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    if (db->rtree_index != NULL) {
        rtree_free((rtree_t *)db->rtree_index);
        db->rtree_index = NULL;
    }
    db->index_built = false;

    /* 释放 Hilbert 索引 */
    if (db->hilbert_index != NULL) {
        hilbert_index_destroy((hilbert_index_t *)db->hilbert_index);
        db->hilbert_index = NULL;
    }
    db->hilbert_built = false;

    /* 删除索引文件 */
    char index_path[512];
    get_index_path(db->name, index_path, sizeof(index_path));
    remove(index_path);

    return 0;
}

/* ========================================================================
 * Hilbert 辅助索引查询优化
 * ======================================================================== */

/**
 * @brief 使用 Hilbert 辅助索引进行范围查询
 *
 * 通过 Hilbert 码的局部性，减少 R-Tree 查询范围。
 *
 * @param rel 空间引擎句柄
 * @param bbox 查询边界框
 * @param results 结果数组
 * @param max_results 最大结果数
 * @return 匹配数量
 */
int spatial_engine_hilbert_search(void *rel, const bbox_t *bbox,
                                  spatial_query_result_t *results,
                                  uint32_t max_results) {
    if (rel == NULL || bbox == NULL || results == NULL || max_results == 0) {
        return -1;
    }

    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    /* 如果 Hilbert 索引未构建，先构建 */
    if (db->hilbert_index == NULL || !db->hilbert_built) {
        if (spatial_engine_build_index(rel) != 0) {
            return -1;
        }
    }

    if (db->hilbert_index == NULL) {
        return 0;
    }

    hilbert_index_t *hilbert = (hilbert_index_t *)db->hilbert_index;

    /* 转换 bbox 到 Hilbert 范围 */
    hilbert_bbox_t query_bbox = {
        .min_x = bbox->min_x,
        .min_y = bbox->min_y,
        .max_x = bbox->max_x,
        .max_y = bbox->max_y
    };

    uint64_t *hilbert_results = calloc(max_results, sizeof(uint64_t));
    if (!hilbert_results) return -1;

    /* Hilbert 范围查询 */
    uint32_t count = hilbert_index_range_query(hilbert, &query_bbox,
                                                hilbert_results, max_results);

    /* 验证候选结果（检查真实边界框相交） */
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < count && valid_count < max_results; i++) {
        /* 从 Hilbert 结果获取 ID，然后读取几何数据验证 */
        /* 这里简化处理，直接使用 Hilbert 结果 */
        results[valid_count].id = hilbert_results[i];
        results[valid_count].distance = 0;
        results[valid_count].data = NULL;
        results[valid_count].len = 0;
        valid_count++;
    }

    free(hilbert_results);
    return valid_count;
}

/**
 * @brief 使用 Hilbert 辅助索引进行 KNN 查询
 *
 * 通过 Hilbert 码的螺旋搜索，找到最近的 k 个邻居。
 *
 * @param rel 空间引擎句柄
 * @param point 查询点
 * @param k 返回数量
 * @param results 结果数组
 * @param max_results 数组容量
 * @return 找到的数量
 */
int spatial_engine_hilbert_knn(void *rel, const point_t *point, int k,
                               spatial_query_result_t *results,
                               uint32_t max_results) {
    if (rel == NULL || point == NULL || results == NULL || k <= 0) {
        return -1;
    }

    spatial_engine_db_t *db = (spatial_engine_db_t *)rel;

    /* 如果 Hilbert 索引未构建，先构建 */
    if (db->hilbert_index == NULL || !db->hilbert_built) {
        if (spatial_engine_build_index(rel) != 0) {
            return -1;
        }
    }

    if (db->hilbert_index == NULL) {
        return 0;
    }

    hilbert_index_t *hilbert = (hilbert_index_t *)db->hilbert_index;

    /* 转换点 */
    hilbert_point2d_t query_point = { .x = point->x, .y = point->y };

    /* Hilbert KNN 查询 */
    hilbert_record_t *hilbert_results = calloc(k, sizeof(hilbert_record_t));
    if (!hilbert_results) return -1;

    uint32_t count = hilbert_index_knn(hilbert, &query_point, k,
                                       hilbert_results, k);

    /* 转换结果 */
    for (uint32_t i = 0; i < count && i < max_results; i++) {
        results[i].id = hilbert_results[i].item_id;
        /* 距离需要从几何数据计算，这里简化处理 */
        results[i].distance = 0;
        results[i].data = NULL;
        results[i].len = 0;
    }

    free(hilbert_results);
    return count;
}
