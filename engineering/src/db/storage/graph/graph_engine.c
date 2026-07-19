/**
 * @file graph_engine.c
 * @brief 图存储引擎实现
 */
#include "graph_engine.h"
#include "db/storage/graph/graph_csr.h"
#include "db/mm_pool.h"
#include "db/lock.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

#define GRAPH_ENGINE_NAME "graph_engine"
#define GRAPH_DATA_PREFIX "graph_"

/* 递归创建目录 */
static int mkpath(char *path) {
    if (path == NULL || strlen(path) == 0) return -1;

    char *p = strdup(path);
    if (p == NULL) return -1;

    char *cur = p;
    if (cur[0] == '.' && cur[1] == '/') {
        cur += 2;
    }

    for (char *sep = cur; *sep != '\0'; sep++) {
        if (*sep == '/') {
            *sep = '\0';
            if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
                free(p);
                return -1;
            }
            *sep = '/';
        }
    }

    if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }

    free(p);
    return 0;
}

static int ensure_dir(const char *path) {
    if (path == NULL) return -1;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }

    return mkpath((char *)path);
}

typedef struct graph_engine_global_s {
    char data_dir[512];
    bool initialized;
} graph_engine_global_t;

static graph_engine_global_t g_graph_engine = {
    .data_dir = {0},
    .initialized = false
};

typedef struct graph_engine_header_s {
    char name[256];
    uint64_t num_vertices;
    uint64_t num_edges;
} graph_engine_header_t;

static int get_graph_dir(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s",
             g_graph_engine.data_dir, GRAPH_DATA_PREFIX, name);
    return 0;
}

static int get_graph_db_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/%s.gdb",
             g_graph_engine.data_dir, GRAPH_DATA_PREFIX, name, name);
    return 0;
}

static int get_csr_dir(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/csr",
             g_graph_engine.data_dir, GRAPH_DATA_PREFIX, name);
    return 0;
}

static int graph_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (name == NULL) return -1;

    char dir_path[512];
    get_graph_dir(name, dir_path, sizeof(dir_path));

    /* 创建目录 */
    if (ensure_dir(dir_path) != 0) {
        return -1;
    }

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    graph_engine_header_t header = { .name = {0}, .num_vertices = 0, .num_edges = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    char db_path[512];
    get_graph_db_path(name, db_path, sizeof(db_path));
    graph_t *g = graph_create(db_path);
    if (g) graph_close(g);

    return 0;
}

static void *graph_engine_table_open(const char *name, AccessMode mode) {
    char db_path[512];
    get_graph_db_path(name, db_path, sizeof(db_path));

    graph_t *g = graph_open(db_path);
    if (g == NULL) return NULL;

    graph_engine_db_t *db = (graph_engine_db_t *)calloc(1, sizeof(graph_engine_db_t));
    if (db == NULL) {
        graph_close(g);
        return NULL;
    }

    db->graph = g;
    strncpy(db->name, name, sizeof(db->name) - 1);
    get_graph_dir(name, db->data_dir, sizeof(db->data_dir));
    db->mode = mode;

    /* 初始化内存池（使用全局图池） */
    db->mem_pool = g_graph_pool;
    db->use_mem_pool = (g_graph_pool != NULL);

    /* 初始化锁（默认禁用） */
    db->lockmgr = NULL;
    db->rwlock = NULL;
    db->use_lock = false;

    /* 初始化 CSR 存储 */
    db->use_csr = true;
    char csr_dir[512];
    get_csr_dir(name, csr_dir, sizeof(csr_dir));
    db->csr_storage = graph_csr_open(csr_dir);
    if (db->csr_storage == NULL) {
        /* 尝试创建新的 CSR 存储 */
        db->csr_storage = graph_csr_create(csr_dir, GRAPH_CSR_DEFAULT_MAX_VERTICES);
        if (db->csr_storage == NULL) {
            LOG_WARN("CSR 存储创建失败，将仅使用原有图引擎");
            db->use_csr = false;
        }
    }

    return db;
}

static int graph_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    /* 先获取统计信息（需要在关闭前） */
    graph_stats_t stats = {0};
    graph_get_stats(db->graph, &stats);

    /* 保存 CSR 存储 */
    if (db->use_csr && db->csr_storage != NULL) {
        graph_csr_save(db->csr_storage);
    }

    /* 释放 CSR 存储 */
    if (db->csr_storage != NULL) {
        graph_csr_destroy(db->csr_storage);
        db->csr_storage = NULL;
    }

    /* 刷新并关闭图 */
    graph_flush(db->graph);
    int ret = graph_close(db->graph);
    db->graph = NULL;  /* 避免悬空指针 */

    /* 保存元数据 */
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", db->data_dir);

    graph_engine_header_t header = {
        .num_vertices = stats.num_vertices,
        .num_edges = stats.num_edges
    };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 释放读写锁 */
    if (db->rwlock != NULL) {
        free(db->rwlock);
        db->rwlock = NULL;
    }

    free(db);
    return ret;
}

static int graph_engine_table_drop(const char *name) {
    char db_path[512];
    get_graph_db_path(name, db_path, sizeof(db_path));

    int ret = remove(db_path);

    /* 同时删除元数据文件 */
    char dir_path[512];
    get_graph_dir(name, dir_path, sizeof(dir_path));
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);
    remove(meta_path);

    return (ret == 0 || errno == ENOENT) ? 0 : -1;
}

static int graph_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (len < sizeof(uint8_t)) return -1;

    const uint8_t *ptr = (const uint8_t *)data;
    uint8_t type;  /* 0=vertex, 1=edge */
    memcpy(&type, ptr, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    if (type == 0) {
        /* 插入顶点 */
        if (graph_engine_add_vertex(rel, ptr, len - sizeof(uint8_t)) == GRAPH_INVALID_ID) {
            return -1;
        }
    } else if (type == 1) {
        /* 插入边 */
        if (graph_engine_add_edge(rel, ptr, len - sizeof(uint8_t)) == GRAPH_INVALID_ID) {
            return -1;
        }
    }

    return 0;
}

static int graph_engine_tuple_delete(void *rel, const void *key, size_t key_len) {
    if (rel == NULL || key == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    /* key 格式: type(1) + id(8) */
    if (key_len < sizeof(uint8_t) + sizeof(graph_vertex_id_t)) return -1;

    const uint8_t *ptr = (const uint8_t *)key;
    uint8_t type;
    memcpy(&type, ptr, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    graph_vertex_id_t id;
    memcpy(&id, ptr, sizeof(id));

    if (type == 0) {
        /* 删除顶点 */
        return graph_vertex_delete(db->graph, id);
    } else if (type == 1) {
        /* 删除边（需要边ID） */
        return graph_edge_delete(db->graph, id);
    }

    return -1;
}

static scan_desc_t *graph_engine_scan_begin(void *rel,
                                            const scan_key_t *keys, int nkeys,
                                            ScanDirection direction) {
    (void)rel;
    (void)keys;
    (void)nkeys;
    (void)direction;
    return NULL;
}

static int graph_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

static int graph_engine_scan_end(scan_desc_t *scan) {
    if (scan) free(scan);
    return 0;
}

static int graph_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    if (name == NULL) return 0;

    char db_path[512];
    get_graph_db_path(name, db_path, sizeof(db_path));

    graph_t *g = graph_open(db_path);
    if (g == NULL) return -1;

    graph_stats_t graph_stats;
    if (graph_get_stats(g, &graph_stats) == 0) {
        stats->num_objects = graph_stats.num_vertices + graph_stats.num_edges;
    }

    graph_close(g);
    return 0;
}

static const storage_ops_t g_graph_engine_ops = {
    .name = GRAPH_ENGINE_NAME,
    .model = MODEL_GRAPH,
    .init = graph_engine_init,
    .shutdown = graph_engine_shutdown,
    .table_create = graph_engine_table_create,
    .table_open = graph_engine_table_open,
    .table_close = graph_engine_table_close,
    .table_drop = graph_engine_table_drop,
    .tuple_insert = graph_engine_tuple_insert,
    .tuple_update = NULL,
    .tuple_delete = graph_engine_tuple_delete,
    .scan_begin = graph_engine_scan_begin,
    .scan_next = graph_engine_scan_next,
    .scan_end = graph_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = graph_engine_get_stats,
};

int graph_engine_init(const char *data_dir) {
    if (g_graph_engine.initialized) return 0;
    if (data_dir == NULL) return -1;
    strncpy(g_graph_engine.data_dir, data_dir, sizeof(g_graph_engine.data_dir) - 1);
    g_graph_engine.initialized = true;

    /* Task 2.11: 将图引擎注册到 storage_ops_t 全局表，
     * 让 storage_get_engine(MODEL_GRAPH) 可发现本引擎。 */
    storage_register_engine(MODEL_GRAPH, &g_graph_engine_ops);
    return 0;
}

int graph_engine_shutdown(void) {
    g_graph_engine.initialized = false;
    return 0;
}

const storage_ops_t *graph_engine_get_ops(void) {
    return &g_graph_engine_ops;
}

int graph_engine_create(const char *name, const storage_schema_t *schema) {
    return graph_engine_table_create(name, schema);
}

void *graph_engine_open(const char *name, AccessMode mode) {
    return graph_engine_table_open(name, mode);
}

int graph_engine_close(void *rel) {
    return graph_engine_table_close(rel);
}

int graph_engine_drop(const char *name) {
    return graph_engine_table_drop(name);
}

graph_vertex_id_t graph_engine_add_vertex(void *rel,
                                          const void *data, size_t len) {
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (len < sizeof(uint32_t)) return GRAPH_INVALID_ID;

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t label_len;
    memcpy(&label_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (label_len > len - sizeof(uint32_t)) return GRAPH_INVALID_ID;

    const char *label = (const char *)ptr;

    return graph_vertex_create(db->graph, label, NULL, 0);
}

graph_edge_id_t graph_engine_add_edge(void *rel,
                                      const void *data, size_t len) {
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (len < sizeof(uint64_t) * 2 + sizeof(uint32_t)) return GRAPH_INVALID_ID;

    const uint8_t *ptr = (const uint8_t *)data;
    graph_vertex_id_t src_id, dst_id;
    memcpy(&src_id, ptr, sizeof(src_id));
    ptr += sizeof(src_id);
    memcpy(&dst_id, ptr, sizeof(dst_id));
    ptr += sizeof(dst_id);

    uint32_t label_len;
    memcpy(&label_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    const char *label = (const char *)ptr;

    return graph_edge_create(db->graph, src_id, dst_id, label, NULL, 0);
}

int graph_engine_get_vertex(void *rel, const void *key, size_t key_len,
                            void **out_data, size_t *out_len) {
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (key_len != sizeof(graph_vertex_id_t)) return -1;

    graph_vertex_id_t vid;
    memcpy(&vid, key, sizeof(vid));

    graph_vertex_t *vertex;
    if (graph_vertex_get(db->graph, vid, &vertex) != 0) {
        return 1;
    }

    /* 序列化顶点数据：ID + 基本信息 */
    size_t buf_size = sizeof(graph_vertex_id_t) + sizeof(uint16_t) * 5;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    if (buf == NULL) {
        free(vertex);
        return -1;
    }

    size_t offset = 0;
    memcpy(buf + offset, &vertex->id, sizeof(vertex->id));
    offset += sizeof(vertex->id);
    memcpy(buf + offset, &vertex->n_labels, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(buf + offset, &vertex->n_props, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(buf + offset, &vertex->out_degree, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(buf + offset, &vertex->in_degree, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(buf + offset, &vertex->state, sizeof(uint16_t));

    *out_data = buf;
    *out_len = buf_size;
    free(vertex);
    return 0;
}

/* ========================================================================
 * CSR 存储 API 实现
 * ======================================================================== */

int graph_engine_enable_csr(void *rel, uint64_t max_vertices) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (db->csr_storage != NULL) {
        graph_csr_destroy(db->csr_storage);
    }

    char csr_dir[512];
    get_csr_dir(db->name, csr_dir, sizeof(csr_dir));

    if (max_vertices == 0) {
        max_vertices = GRAPH_CSR_DEFAULT_MAX_VERTICES;
    }

    db->csr_storage = graph_csr_create(csr_dir, max_vertices);
    if (db->csr_storage == NULL) {
        LOG_ERROR("创建 CSR 存储失败");
        return -1;
    }

    db->use_csr = true;
    LOG_INFO("CSR 存储已启用: max_vertices=%lu", max_vertices);
    return 0;
}

int graph_engine_csr_compact(void *rel) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (!db->use_csr || db->csr_storage == NULL) {
        return -1;
    }

    return graph_csr_compact(db->csr_storage);
}

bool graph_engine_csr_needs_compact(void *rel) {
    if (rel == NULL) return false;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (!db->use_csr || db->csr_storage == NULL) {
        return false;
    }

    return graph_csr_needs_compact(db->csr_storage);
}

const void *graph_engine_get_out_edges(void *rel, uint64_t src, uint32_t *out_count) {
    if (rel == NULL || out_count == NULL) return NULL;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (!db->use_csr || db->csr_storage == NULL) {
        *out_count = 0;
        return NULL;
    }

    return graph_csr_get_out_edges(db->csr_storage, src, out_count);
}

const void *graph_engine_get_in_edges(void *rel, uint64_t dst, uint32_t *out_count) {
    if (rel == NULL || out_count == NULL) return NULL;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (!db->use_csr || db->csr_storage == NULL) {
        *out_count = 0;
        return NULL;
    }

    return graph_csr_get_in_edges(db->csr_storage, dst, out_count);
}

int graph_engine_save_csr(void *rel) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (!db->use_csr || db->csr_storage == NULL) {
        return -1;
    }

    return graph_csr_save(db->csr_storage);
}

int graph_engine_load_csr(void *rel) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    /* 如果已有 CSR，先释放 */
    if (db->csr_storage != NULL) {
        graph_csr_destroy(db->csr_storage);
    }

    char csr_dir[512];
    get_csr_dir(db->name, csr_dir, sizeof(csr_dir));

    db->csr_storage = graph_csr_open(csr_dir);
    if (db->csr_storage == NULL) {
        LOG_WARN("CSR 存储文件不存在");
        db->use_csr = false;
        return -1;
    }

    db->use_csr = true;
    LOG_INFO("CSR 存储已加载");
    return 0;
}

void graph_engine_disable_csr(void *rel) {
    if (rel == NULL) return;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (db->csr_storage != NULL) {
        graph_csr_destroy(db->csr_storage);
        db->csr_storage = NULL;
    }

    db->use_csr = false;
    LOG_INFO("CSR 存储已禁用");
}

int graph_engine_stats(const char *name, storage_stats_t *stats) {
    return graph_engine_get_stats(name, stats);
}

/* ========================================================================
 * 内存池管理 API 实现
 * ======================================================================== */

int graph_engine_enable_mem_pool(void *rel, bool use_pool) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (use_pool && g_graph_pool != NULL) {
        db->mem_pool = g_graph_pool;
        db->use_mem_pool = true;
        LOG_INFO("图引擎内存池已启用");
    } else {
        db->mem_pool = NULL;
        db->use_mem_pool = false;
        LOG_INFO("图引擎内存池已禁用");
    }
    return 0;
}

int graph_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats) {
    if (rel == NULL || stats == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (db->mem_pool != NULL) {
        return mm_pool_get_stats(db->mem_pool, stats);
    }

    memset(stats, 0, sizeof(mm_pool_stats_t));
    return 0;
}

/* ========================================================================
 * 并发锁控制 API 实现
 * ======================================================================== */

/* 全局锁管理器用于图引擎 */
static lock_manager_t *g_graph_lockmgr = NULL;

/* 简单的读写锁实现（基于自旋锁，与向量引擎共享） */
typedef struct {
    volatile int readers;
    volatile int writers_waiting;
    volatile int writer_active;
} simple_rwlock_t;

static void rwlock_init(simple_rwlock_t *rwlock) {
    rwlock->readers = 0;
    rwlock->writers_waiting = 0;
    rwlock->writer_active = 0;
}

static void rwlock_read_lock(simple_rwlock_t *rwlock) {
    while (1) {
        while (rwlock->writer_active) { /* 等待写锁 */; }
        __sync_fetch_and_add(&rwlock->readers, 1);
        if (!rwlock->writer_active) return;
        __sync_fetch_and_sub(&rwlock->readers, 1);
    }
}

static void rwlock_read_unlock(simple_rwlock_t *rwlock) {
    __sync_fetch_and_sub(&rwlock->readers, 1);
}

static void rwlock_write_lock(simple_rwlock_t *rwlock) {
    __sync_fetch_and_add(&rwlock->writers_waiting, 1);
    while (1) {
        while (rwlock->readers > 0) { /* 等待读锁 */; }
        if (__sync_bool_compare_and_swap(&rwlock->writer_active, 0, 1)) {
            __sync_fetch_and_sub(&rwlock->writers_waiting, 1);
            return;
        }
    }
}

static void rwlock_write_unlock(simple_rwlock_t *rwlock) {
    __sync_fetch_and_and(&rwlock->writer_active, 0);
}

int graph_engine_enable_lock(void *rel, bool use_lock) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;

    if (use_lock) {
        if (g_graph_lockmgr == NULL) {
            g_graph_lockmgr = lock_mgr_create();
            if (g_graph_lockmgr == NULL) {
                LOG_ERROR("创建图引擎锁管理器失败");
                return -1;
            }
        }
        if (db->rwlock == NULL) {
            db->rwlock = calloc(1, sizeof(simple_rwlock_t));
            if (db->rwlock == NULL) return -1;
            rwlock_init((simple_rwlock_t *)db->rwlock);
        }
        db->lockmgr = g_graph_lockmgr;
        db->use_lock = true;
        LOG_INFO("图引擎并发锁已启用");
    } else {
        db->use_lock = false;
        db->lockmgr = NULL;
        if (db->rwlock != NULL) {
            free(db->rwlock);
            db->rwlock = NULL;
        }
        LOG_INFO("图引擎并发锁已禁用");
    }
    return 0;
}

int graph_engine_read_lock(void *rel) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        rwlock_read_lock((simple_rwlock_t *)db->rwlock);
    }
    return 0;
}

void graph_engine_read_unlock(void *rel) {
    if (rel == NULL) return;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        rwlock_read_unlock((simple_rwlock_t *)db->rwlock);
    }
}

int graph_engine_write_lock(void *rel, uint32_t timeout_ms) {
    if (rel == NULL) return -1;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        rwlock_write_lock((simple_rwlock_t *)db->rwlock);
    }
    (void)timeout_ms;
    return 0;
}

void graph_engine_write_unlock(void *rel) {
    if (rel == NULL) return;
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        rwlock_write_unlock((simple_rwlock_t *)db->rwlock);
    }
}
