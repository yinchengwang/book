/**
 * @file columnar_engine.c
 * @brief 列式存储引擎实现
 *
 * 提供列式存储、向量化聚合、压缩等能力。
 * 适用于 OLAP 场景的高效分析查询。
 */
#include "columnar_engine.h"
#include "columnar_engine_internal.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

/* ========================================================================
 * 静态变量
 * ======================================================================== */

static char g_data_dir[512] = {0};
static bool g_initialized = false;

/* ========================================================================
 * 生命周期
 * ======================================================================== */

int columnar_engine_init(const char *data_dir) {
    if (g_initialized) return 0;
    if (data_dir) {
        strncpy(g_data_dir, data_dir, sizeof(g_data_dir) - 1);
    } else {
        strcpy(g_data_dir, "./data/columnar");
    }
    g_initialized = true;
    LOG_INFO("Columnar engine initialized");
    return 0;
}

int columnar_engine_shutdown(void) {
    g_initialized = false;
    LOG_INFO("Columnar engine shutdown");
    return 0;
}

/* ========================================================================
 * 表操作
 * ======================================================================== */

int columnar_table_create(const char *name, const char **column_names, int32_t num_columns) {
    if (!g_initialized || !name || !column_names || num_columns <= 0) return -1;

    columnar_table_internal_t *table = (columnar_table_internal_t *)calloc(1, sizeof(columnar_table_internal_t));
    if (!table) return -1;

    strncpy(table->name, name, sizeof(table->name) - 1);
    table->num_columns = num_columns;
    table->columns = (column_desc_t *)calloc(num_columns, sizeof(column_desc_t));
    if (!table->columns) {
        free(table);
        return -1;
    }

    for (int i = 0; i < num_columns; i++) {
        strncpy(table->columns[i].name, column_names[i], sizeof(table->columns[i].name) - 1);
        table->columns[i].type_oid = 0;
        table->columns[i].chunks = NULL;
        table->columns[i].total_count = 0;
    }

    // 序列化到文件（使用偏移量而非指针）
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.table", g_data_dir, name);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(table, sizeof(columnar_table_internal_t), 1, f);
        fclose(f);
    }

    // 序列化数据文件
    columnar_serialize_table(table, NULL);

    free(table->columns);
    free(table);
    LOG_INFO("Columnar table created: %s, columns: %d", name, num_columns);
    return 0;
}

void *columnar_table_open(const char *name) {
    if (!g_initialized || !name) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.table", g_data_dir, name);

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    columnar_table_internal_t *table = (columnar_table_internal_t *)malloc(sizeof(columnar_table_internal_t));
    if (!table) {
        fclose(f);
        return NULL;
    }

    fread(table, sizeof(columnar_table_internal_t), 1, f);
    fclose(f);
    return table;
}

int columnar_table_close(void *table) {
    if (table) {
        columnar_table_internal_t *t = (columnar_table_internal_t *)table;
        for (int i = 0; i < t->num_columns; i++) {
            // 清理列数据
        }
        free(t->columns);
        free(t);
    }
    return 0;
}

int columnar_table_drop(const char *name) {
    if (!name) return -1;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.table", g_data_dir, name);
    remove(path);
    LOG_INFO("Columnar table dropped: %s", name);
    return 0;
}

/* ========================================================================
 * 列操作
 * ======================================================================== */

int columnar_column_append(void *table, const char *col_name, const void *data, size_t len) {
    if (!table || !col_name || !data) return -1;

    columnar_table_internal_t *t = (columnar_table_internal_t *)table;
    int col_idx = -1;

    for (int i = 0; i < t->num_columns; i++) {
        if (strcmp(t->columns[i].name, col_name) == 0) {
            col_idx = i;
            break;
        }
    }

    if (col_idx < 0) return -1;

    column_desc_t *col = &t->columns[col_idx];

    if (!col->chunks) {
        col->chunks = create_chunk(col_name, len);
        if (!col->chunks) return -1;
    }

    if (append_to_chunk(col->chunks, data) != 0) return -1;
    col->total_count++;
    t->row_count++;

    return 0;
}

int columnar_column_append_batch(void *table, const char *col_name, const void **data, size_t *lens, size_t count) {
    if (!table || !col_name || !data || !lens || count == 0) return -1;

    for (size_t i = 0; i < count; i++) {
        if (columnar_column_append(table, col_name, data[i], lens[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

/* ========================================================================
 * 向量化聚合
 * ======================================================================== */

int64_t columnar_agg_count(void *table) {
    if (!table) return 0;
    columnar_table_internal_t *t = (columnar_table_internal_t *)table;
    return t->row_count;
}

int columnar_agg_sum_int64(void *table, const char *col_name, int64_t *result) {
    if (!table || !col_name || !result) return -1;

    columnar_table_internal_t *t = (columnar_table_internal_t *)table;
    int col_idx = -1;

    for (int i = 0; i < t->num_columns; i++) {
        if (strcmp(t->columns[i].name, col_name) == 0) {
            col_idx = i;
            break;
        }
    }

    if (col_idx < 0) return -1;

    column_desc_t *col = &t->columns[col_idx];
    if (!col->chunks) {
        *result = 0;
        return 0;
    }

    int64_t sum = 0;
    column_chunk_t *chunk = col->chunks;
    while (chunk) {
        int64_t *values = (int64_t *)chunk->data;
        for (size_t i = 0; i < chunk->count; i++) {
            sum += values[i];
        }
        chunk = chunk->next;
    }

    *result = sum;
    return 0;
}

int columnar_agg_avg_double(void *table, const char *col_name, double *result) {
    if (!table || !col_name || !result) return -1;

    columnar_table_internal_t *t = (columnar_table_internal_t *)table;
    int col_idx = -1;

    for (int i = 0; i < t->num_columns; i++) {
        if (strcmp(t->columns[i].name, col_name) == 0) {
            col_idx = i;
            break;
        }
    }

    if (col_idx < 0) return -1;

    column_desc_t *col = &t->columns[col_idx];
    if (!col->chunks || col->total_count == 0) {
        *result = 0.0;
        return 0;
    }

    double sum = 0.0;
    column_chunk_t *chunk = col->chunks;
    while (chunk) {
        double *values = (double *)chunk->data;
        for (size_t i = 0; i < chunk->count; i++) {
            sum += values[i];
        }
        chunk = chunk->next;
    }

    *result = sum / col->total_count;
    return 0;
}

/* ========================================================================
 * 序列化/反序列化（偏移量代替指针）
 * ======================================================================== */

/**
 * @brief 序列化表到文件（使用偏移量而非指针）
 * @param table 表对象
 * @param file_offset 输出文件的写入偏移量
 * @return 0成功，-1失败
 */
static int columnar_serialize_table(void *table, size_t *file_offset) {
    if (!table) return -1;
    columnar_table_internal_t *t = (columnar_table_internal_t *)table;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.data", g_data_dir, t->name);

    FILE *f = fopen(path, "ab"); /* 追加模式 */
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    size_t base_offset = ftell(f);

    /* 写入每个列块的数据，记录偏移量 */
    for (int i = 0; i < t->num_columns; i++) {
        column_desc_t *col = &t->columns[i];
        column_chunk_t *chunk = col->chunks;
        while (chunk) {
            chunk->file_offset = ftell(f);
            /* 写入数据 */
            size_t write_size = chunk->compressed_size > 0 ? chunk->compressed_size : chunk->count * chunk->element_size;
            if (write_size > 0) {
                fwrite(chunk->data, 1, write_size, f);
            }
            chunk = chunk->next;
        }
    }

    fclose(f);
    if (file_offset) *file_offset = base_offset;
    return 0;
}

/**
 * @brief 从文件反序列化表（将偏移量转换为指针）
 * @param name 表名
 * @return 0成功，-1失败
 */
static int columnar_deserialize_table(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.table", g_data_dir, name);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    /* 反序列化表结构 */
    columnar_table_internal_t *table = (columnar_table_internal_t *)malloc(sizeof(columnar_table_internal_t));
    if (!table) { fclose(f); return -1; }
    fread(table, sizeof(columnar_table_internal_t), 1, f);
    fclose(f);

    /* 反序列化数据文件 */
    snprintf(path, sizeof(path), "%s/%s.data", g_data_dir, name);
    f = fopen(path, "rb");
    if (f) {
        for (int i = 0; i < table->num_columns; i++) {
            column_desc_t *col = &table->columns[i];
            column_chunk_t *chunk = col->chunks;
            while (chunk) {
                if (chunk->file_offset > 0) {
                    fseek(f, (long)chunk->file_offset, SEEK_SET);
                    size_t read_size = chunk->compressed_size > 0 ? chunk->compressed_size : chunk->count * chunk->element_size;
                    chunk->data = malloc(read_size > 0 ? read_size : 1);
                    if (chunk->data && read_size > 0) {
                        fread(chunk->data, 1, read_size, f);
                    }
                    if (chunk->compressed_size > 0) {
                        columnar_decompress_chunk(chunk);
                    }
                }
                chunk = chunk->next;
            }
        }
        fclose(f);
    }

    free(table->columns);
    free(table);
    return 0;
}

/* ========================================================================
 * 压缩
 * ======================================================================== */

int columnar_compress(void *table, ColumnarCompressionType type) {
    if (!table) return -1;
    columnar_table_internal_t *t = (columnar_table_internal_t *)table;

    if (type == COL_COMPRESS_NONE) return 0;

    for (int i = 0; i < t->num_columns; i++) {
        column_chunk_t *chunk = t->columns[i].chunks;
        while (chunk) {
            if (type == COL_COMPRESS_RLE || type == COL_COMPRESS_ZSTD) {
                if (columnar_compress_chunk(chunk) != 0) return -1;
            }
            chunk = chunk->next;
        }
    }

    LOG_INFO("Compression type %d applied", type);
    return 0;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

int columnar_get_stats(const char *name, storage_stats_t *stats) {
    if (!stats) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    stats->num_objects = 1;
    return 0;
}

/* ========================================================================
 * storage_ops_t 适配层
 * ======================================================================== */

static int col_table_create(const char *name, const storage_schema_t *schema) {
    if (!schema || !schema->num_columns) return -1;
    const char **cols = (const char **)malloc(sizeof(const char *) * schema->num_columns);
    if (!cols) return -1;
    for (int i = 0; i < schema->num_columns; i++) {
        cols[i] = schema->columns[i].name;
    }
    int ret = columnar_table_create(name, cols, schema->num_columns);
    free(cols);
    return ret;
}

static void *col_table_open(const char *name, AccessMode mode) {
    (void)mode;
    return columnar_table_open(name);
}

static int col_table_close(void *rel) {
    return columnar_table_close(rel);
}

static int col_table_drop(const char *name) {
    return columnar_table_drop(name);
}

static int col_tuple_insert(void *rel, const void *data, size_t len) {
    // 列存不支持直接元组插入，需要通过列接口
    (void)rel; (void)data; (void)len;
    return -1;
}

static int col_get_stats(const char *name, storage_stats_t *stats) {
    return columnar_get_stats(name, stats);
}

/* ========================================================================
 * 引擎 ops 表
 * ======================================================================== */

static storage_ops_t g_columnar_storage_ops = {
    .name = "columnar_engine",
    .model = MODEL_COLUMNAR,
    .init = columnar_engine_init,
    .shutdown = columnar_engine_shutdown,
    .table_create = col_table_create,
    .table_open = col_table_open,
    .table_close = col_table_close,
    .table_drop = col_table_drop,
    .tuple_insert = col_tuple_insert,
    .get_stats = col_get_stats,
};

const storage_ops_t *columnar_engine_get_ops(void) {
    return &g_columnar_storage_ops;
}
