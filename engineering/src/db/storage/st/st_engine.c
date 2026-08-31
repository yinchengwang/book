/**
 * @file st_engine.c
 * @brief 时空存储引擎实现
 *
 * 组合空间和时间维度，提供时空查询分析能力。
 * 支持轨迹查询、时空范围查询、时间聚合等。
 */
#include "db/st_engine.h"
#include "db/storage/spatial/rtree.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <dirent.h>

#ifdef _WIN32
#include <direct.h>
#include <errno.h>
#define mkdir(path) _mkdir(path)
#define unlink(path) _unlink(path)
#define rmdir(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define ST_ENGINE_NAME "st_engine"
#define ST_DATA_PREFIX "st_"

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 时空对象 */
typedef struct st_object_s {
    uint64_t id;           /**< 对象 ID */
    st_point_t position;   /**< 位置和时间 */
    double speed;          /**< 速度 */
    double heading;        /**< 航向角 */
} st_object_t;

/** 时空索引节点 */
typedef struct st_index_node_s {
    uint64_t object_id;
    st_point_t position;
    struct st_index_node_s *next;
} st_index_node_t;

/** 时空引擎全局状态 */
typedef struct st_engine_global_s {
    char data_dir[512];
    bool initialized;
} st_engine_global_t;

static st_engine_global_t g_st_engine = {
    .data_dir = {0},
    .initialized = false
};

/** 时空数据集头 */
typedef struct st_header_s {
    char name[256];
    uint64_t num_objects;
    int64_t start_time;
    int64_t end_time;
    bbox_t spatial_bounds;
} st_header_t;

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int get_dir_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s",
             g_st_engine.data_dir, ST_DATA_PREFIX, name);
    return 0;
}

static bbox_t compute_bounds_from_points(const st_point_t *points, size_t count) {
    bbox_t bounds = {0, 0, 0, 0};
    if (count == 0) return bounds;

    bounds.min_x = bounds.max_x = points[0].x;
    bounds.min_y = bounds.max_y = points[0].y;

    for (size_t i = 1; i < count; i++) {
        if (points[i].x < bounds.min_x) bounds.min_x = points[i].x;
        if (points[i].x > bounds.max_x) bounds.max_x = points[i].x;
        if (points[i].y < bounds.min_y) bounds.min_y = points[i].y;
        if (points[i].y > bounds.max_y) bounds.max_y = points[i].y;
    }
    return bounds;
}

st_point_t st_point_create(double x, double y, int64_t timestamp) {
    st_point_t p = {x, y, timestamp};
    return p;
}

st_bbox_t st_bbox_create(double min_x, double min_y, double max_x, double max_y,
                          int64_t start_time, int64_t end_time) {
    st_bbox_t bbox = {min_x, min_y, max_x, max_y, start_time, end_time};
    return bbox;
}

bool st_bbox_contains_point(const st_bbox_t *bbox, const st_point_t *point) {
    return (point->x >= bbox->min_x && point->x <= bbox->max_x &&
            point->y >= bbox->min_y && point->y <= bbox->max_y &&
            point->timestamp >= bbox->start_time && point->timestamp <= bbox->end_time);
}

double st_distance(const st_point_t *a, const st_point_t *b) {
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}

double st_combined_distance(const st_point_t *a, const st_point_t *b,
                             double spatial_weight, double temporal_weight) {
    double spatial_dist = st_distance(a, b);
    double temporal_dist = (double)llabs(a->timestamp - b->timestamp);
    return spatial_weight * spatial_dist + temporal_weight * temporal_dist;
}

/* ========================================================================
 * 生命周期
 * ======================================================================== */

int st_engine_init(const char *data_dir) {
    if (g_st_engine.initialized) return 0;

    if (data_dir) {
        strncpy(g_st_engine.data_dir, data_dir, sizeof(g_st_engine.data_dir) - 1);
    } else {
        strcpy(g_st_engine.data_dir, "./data/st");
    }

    /* 创建数据目录 */
#ifdef _WIN32
    mkdir(g_st_engine.data_dir);
#else
    mkdir(g_st_engine.data_dir, 0755);
#endif

    g_st_engine.initialized = true;
    LOG_INFO("ST engine initialized: %s", g_st_engine.data_dir);
    return 0;
}

int st_engine_shutdown(void) {
    g_st_engine.initialized = false;
    LOG_INFO("ST engine shutdown");
    return 0;
}

/* ========================================================================
 * 表操作
 * ======================================================================== */

static int st_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (!g_st_engine.initialized || !name) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

#ifdef _WIN32
    if (mkdir(dir_path) != 0 && errno != EEXIST) {
#else
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_ERROR("创建时空数据目录失败: %s", dir_path);
        return -1;
    }

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    st_header_t header = { .name = {0}, .num_objects = 0,
                           .start_time = 0, .end_time = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    LOG_INFO("ST table created: %s", name);
    return 0;
}

static void *st_engine_table_open(const char *name, AccessMode mode) {
    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(meta_path, "rb");
    if (!fp) return NULL;

    st_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    st_engine_db_t *db = (st_engine_db_t *)calloc(1, sizeof(st_engine_db_t));
    if (!db) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    get_dir_path(name, db->data_dir, sizeof(db->data_dir));
    db->mode = mode;
    db->num_objects = header.num_objects;
    db->start_time = header.start_time;
    db->end_time = header.end_time;
    db->spatial_bounds = header.spatial_bounds;

    /* 创建/加载空间索引 */
    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/spatial_index.bin", db->data_dir);
    db->spatial_index = rtree_load(index_path);
    if (!db->spatial_index) {
        db->spatial_index = rtree_create(16);
    }

    return db;
}

static int st_engine_table_close(void *rel) {
    if (!rel) return -1;
    st_engine_db_t *db = (st_engine_db_t *)rel;

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", db->data_dir);

    st_header_t header = {
        .num_objects = db->num_objects,
        .start_time = db->start_time,
        .end_time = db->end_time,
        .spatial_bounds = db->spatial_bounds
    };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 保存并释放空间索引 */
    if (db->spatial_index) {
        char index_path[512];
        snprintf(index_path, sizeof(index_path), "%s/spatial_index.bin", db->data_dir);
        rtree_save(db->spatial_index, index_path);
        rtree_free(db->spatial_index);
    }

    free(db);
    return 0;
}

static int st_engine_table_drop(const char *name) {
    if (!name) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    /* 删除目录及其内容（先删文件，再删目录） */
    DIR *d = opendir(dir_path);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char file_path[768];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            unlink(file_path);
        }
        closedir(d);
    }
    rmdir(dir_path);

    LOG_INFO("ST table dropped: %s", name);
    return 0;
}

static int st_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (!rel || !data) return -1;
    if (len < sizeof(st_object_t)) return -1;

    st_engine_db_t *db = (st_engine_db_t *)rel;
    const st_object_t *obj = (const st_object_t *)data;

    /* 追加到数据文件 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/objects.bin", db->data_dir);

    FILE *fp = fopen(data_path, "ab");
    if (!fp) return -1;

    fwrite(obj, sizeof(st_object_t), 1, fp);
    fclose(fp);

    /* 更新统计信息 */
    db->num_objects++;

    if (db->start_time == 0 || obj->position.timestamp < db->start_time) {
        db->start_time = obj->position.timestamp;
    }
    if (db->end_time == 0 || obj->position.timestamp > db->end_time) {
        db->end_time = obj->position.timestamp;
    }

    /* 更新空间边界 */
    if (db->num_objects == 1) {
        db->spatial_bounds.min_x = db->spatial_bounds.max_x = obj->position.x;
        db->spatial_bounds.min_y = db->spatial_bounds.max_y = obj->position.y;
    } else {
        if (obj->position.x < db->spatial_bounds.min_x)
            db->spatial_bounds.min_x = obj->position.x;
        if (obj->position.x > db->spatial_bounds.max_x)
            db->spatial_bounds.max_x = obj->position.x;
        if (obj->position.y < db->spatial_bounds.min_y)
            db->spatial_bounds.min_y = obj->position.y;
        if (obj->position.y > db->spatial_bounds.max_y)
            db->spatial_bounds.max_y = obj->position.y;
    }

    /* 插入到空间索引 */
    if (db->spatial_index) {
        bbox_t obj_bbox = {
            .min_x = obj->position.x,
            .min_y = obj->position.y,
            .max_x = obj->position.x,
            .max_y = obj->position.y
        };
        rtree_insert(db->spatial_index, obj->id, &obj_bbox);
    }

    return 0;
}

static int st_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (!stats || !name) return -1;

    memset(stats, 0, sizeof(storage_stats_t));

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(meta_path, "rb");
    if (fp) {
        st_header_t header;
        if (fread(&header, sizeof(header), 1, fp) == 1) {
            stats->num_objects = header.num_objects;
        }
        fclose(fp);
    }

    return 0;
}

/* ========================================================================
 * 时空查询实现
 * ======================================================================== */

/** 从文件加载所有对象 */
static int load_all_objects(st_engine_db_t *db, st_object_t **objects, size_t *count) {
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/objects.bin", db->data_dir);

    FILE *fp = fopen(data_path, "rb");
    if (!fp) {
        *objects = NULL;
        *count = 0;
        return 0;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *count = file_size / sizeof(st_object_t);
    if (*count == 0) {
        fclose(fp);
        *objects = NULL;
        return 0;
    }

    *objects = (st_object_t *)malloc(file_size);
    if (!*objects) {
        fclose(fp);
        return -1;
    }

    fread(*objects, sizeof(st_object_t), *count, fp);
    fclose(fp);

    return 0;
}

int st_engine_query_bbox_time(void *rel, const st_bbox_time_args_t *args,
                               st_query_result_t *results, uint32_t *num_results) {
    if (!rel || !args || !results || !num_results) return -1;

    st_engine_db_t *db = (st_engine_db_t *)rel;
    st_object_t *objects = NULL;
    size_t total = 0;

    if (load_all_objects(db, &objects, &total) != 0) return -1;

    uint32_t count = 0;
    uint32_t skip = args->offset;

    for (size_t i = 0; i < total && count < args->limit + skip; i++) {
        st_point_t pt = objects[i].position;
        if (pt.x >= args->bbox.min_x && pt.x <= args->bbox.max_x &&
            pt.y >= args->bbox.min_y && pt.y <= args->bbox.max_y &&
            pt.timestamp >= args->bbox.start_time && pt.timestamp <= args->bbox.end_time) {

            if (skip > 0) {
                skip--;
                continue;
            }

            results[count].id = objects[i].id;
            results[count].distance = 0;
            results[count].time_diff = 0;
            results[count].score = 0;
            count++;
        }
    }

    free(objects);
    *num_results = count;
    return 0;
}

int st_engine_nearest_time(void *rel, const st_point_t *point,
                            int64_t time_window, uint32_t k,
                            st_query_result_t *results, uint32_t *num_results) {
    if (!rel || !point || !results || !num_results) return -1;

    st_engine_db_t *db = (st_engine_db_t *)rel;
    st_object_t *objects = NULL;
    size_t total = 0;

    if (load_all_objects(db, &objects, &total) != 0) return -1;

    /* 过滤时间窗口内的所有候选对象 */
    int64_t min_time = point->timestamp - time_window;
    int64_t max_time = point->timestamp + time_window;

    /* 收集所有候选 */
    uint32_t capacity = 1024;
    st_query_result_t *candidates = (st_query_result_t *)malloc(sizeof(st_query_result_t) * capacity);
    if (!candidates) {
        free(objects);
        return -1;
    }

    uint32_t count = 0;
    for (size_t i = 0; i < total; i++) {
        st_point_t pt = objects[i].position;
        if (pt.timestamp >= min_time && pt.timestamp <= max_time) {
            if (count >= capacity) {
                capacity *= 2;
                st_query_result_t *new_cands = (st_query_result_t *)realloc(candidates, sizeof(st_query_result_t) * capacity);
                if (!new_cands) {
                    free(candidates);
                    free(objects);
                    return -1;
                }
                candidates = new_cands;
            }
            candidates[count].id = objects[i].id;
            candidates[count].distance = st_distance(point, &pt);
            candidates[count].time_diff = llabs(pt.timestamp - point->timestamp);
            candidates[count].score = st_combined_distance(point, &pt, 1.0, 0.001);
            count++;
        }
    }

    /* 按综合评分排序，取全局 top-k */
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (candidates[j].score < candidates[i].score) {
                st_query_result_t tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }

    uint32_t result_count = (count < k) ? count : k;
    for (uint32_t i = 0; i < result_count; i++) {
        results[i] = candidates[i];
    }

    free(candidates);
    free(objects);
    *num_results = result_count;
    return 0;
}

int st_engine_trajectory(void *rel, uint64_t object_id,
                          int64_t start_time, int64_t end_time,
                          st_trajectory_point_t *points, uint32_t *num_points) {
    if (!rel || !points || !num_points) return -1;

    st_engine_db_t *db = (st_engine_db_t *)rel;
    st_object_t *objects = NULL;
    size_t total = 0;

    if (load_all_objects(db, &objects, &total) != 0) return -1;

    uint32_t count = 0;
    for (size_t i = 0; i < total && count < *num_points; i++) {
        if (objects[i].id == object_id &&
            objects[i].position.timestamp >= start_time &&
            objects[i].position.timestamp <= end_time) {

            points[count].point = objects[i].position;
            points[count].speed = objects[i].speed;
            points[count].heading = objects[i].heading;
            count++;
        }
    }

    /* 按时间排序 */
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (points[j].point.timestamp < points[i].point.timestamp) {
                st_trajectory_point_t tmp = points[i];
                points[i] = points[j];
                points[j] = tmp;
            }
        }
    }

    free(objects);
    *num_points = count;
    return 0;
}

int st_engine_aggregate_time(void *rel, const bbox_t *bbox,
                              int64_t start_time, int64_t end_time,
                              int64_t bucket_ms,
                              uint64_t *count_results, uint32_t *num_buckets) {
    if (!rel || !bbox || !count_results || !num_buckets) return -1;

    st_engine_db_t *db = (st_engine_db_t *)rel;
    st_object_t *objects = NULL;
    size_t total = 0;

    if (load_all_objects(db, &objects, &total) != 0) return -1;

    /* 计算桶数量 */
    int64_t duration = end_time - start_time;
    uint32_t num_buckets_calc = (uint32_t)((duration + bucket_ms - 1) / bucket_ms);
    if (num_buckets_calc > 1000) num_buckets_calc = 1000; /* 限制最大桶数 */

    /* 初始化计数数组 */
    memset(count_results, 0, num_buckets_calc * sizeof(uint64_t));

    /* 统计每个时间桶的对象数量 */
    for (size_t i = 0; i < total; i++) {
        st_point_t pt = objects[i].position;
        if (pt.x >= bbox->min_x && pt.x <= bbox->max_x &&
            pt.y >= bbox->min_y && pt.y <= bbox->max_y &&
            pt.timestamp >= start_time && pt.timestamp <= end_time) {

            int64_t bucket_idx = (pt.timestamp - start_time) / bucket_ms;
            if (bucket_idx >= 0 && (uint32_t)bucket_idx < num_buckets_calc) {
                count_results[bucket_idx]++;
            }
        }
    }

    free(objects);
    *num_buckets = num_buckets_calc;
    return 0;
}

/* ========================================================================
 * storage_ops_t 适配层
 * ======================================================================== */

static storage_ops_t g_st_storage_ops = {
    .name = ST_ENGINE_NAME,
    .model = MODEL_SPATIAL,  /* 使用空间模型作为基础 */
    .init = st_engine_init,
    .shutdown = st_engine_shutdown,
    .table_create = st_engine_table_create,
    .table_open = st_engine_table_open,
    .table_close = st_engine_table_close,
    .table_drop = st_engine_table_drop,
    .tuple_insert = st_engine_tuple_insert,
    .get_stats = st_engine_get_stats,
};

const storage_ops_t *st_engine_get_ops(void) {
    return &g_st_storage_ops;
}
