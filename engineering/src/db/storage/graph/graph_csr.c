/**
 * @file graph_csr.c
 * @brief 图 CSR (Compressed Sparse Row) 存储格式实现
 *
 * 实现高效的 CSR 图存储，支持增量更新和快速查询。
 */
#include "db/storage/graph/graph_csr.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 创建数据目录
 */
static int ensure_dir(const char *path) {
    if (path == NULL) return -1;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    return mkdir(path) == 0 ? 0 : -1;
}

/**
 * @brief 获取文件路径
 */
static void get_file_path(const graph_csr_t *csr, const char *filename,
                          char *out_path, size_t size) {
    snprintf(out_path, size, "%s/%s", csr->data_dir, filename);
}

/**
 * @brief 重新分配顶点数组
 */
static int grow_vertices(graph_csr_t *csr, uint64_t new_max) {
    graph_csr_vertex_t *new_vertices = (graph_csr_vertex_t *)realloc(
        csr->vertices, new_max * sizeof(graph_csr_vertex_t));
    if (new_vertices == NULL) return -1;
    csr->vertices = new_vertices;
    csr->max_vertices = new_max;
    return 0;
}

/**
 * @brief 重新分配边数组
 */
static int grow_edges(graph_csr_t *csr, uint64_t new_capacity) {
    graph_csr_edge_t *new_edges = (graph_csr_edge_t *)realloc(
        csr->edges, new_capacity * sizeof(graph_csr_edge_t));
    if (new_edges == NULL) return -1;
    csr->edges = new_edges;
    return 0;
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

graph_csr_t *graph_csr_create(const char *data_dir, uint64_t max_vertices) {
    if (data_dir == NULL) return NULL;
    if (max_vertices == 0) max_vertices = GRAPH_CSR_DEFAULT_MAX_VERTICES;

    /* 创建目录 */
    if (ensure_dir(data_dir) != 0) {
        LOG_ERROR("创建 CSR 目录失败: %s", data_dir);
        return NULL;
    }

    graph_csr_t *csr = (graph_csr_t *)calloc(1, sizeof(graph_csr_t));
    if (csr == NULL) return NULL;

    strncpy(csr->data_dir, data_dir, sizeof(csr->data_dir) - 1);
    csr->max_vertices = max_vertices;

    /* 分配顶点数组 */
    csr->vertices = (graph_csr_vertex_t *)calloc(max_vertices,
                                                  sizeof(graph_csr_vertex_t));
    if (csr->vertices == NULL) {
        free(csr);
        return NULL;
    }

    /* 分配 offsets 数组 */
    csr->offsets = (uint64_t *)calloc(max_vertices + 1, sizeof(uint64_t));
    if (csr->offsets == NULL) {
        free(csr->vertices);
        free(csr);
        return NULL;
    }

    /* 初始化其他字段 */
    csr->edges = NULL;
    csr->edge_count = 0;
    csr->vertex_count = 0;

    /* 分配入边索引 */
    csr->in_offsets = (uint64_t *)calloc(max_vertices + 1, sizeof(uint64_t));
    csr->in_edges = NULL;

    /* 分配 COO 缓冲区 */
    csr->coo_capacity = GRAPH_CSR_COO_CAPACITY;
    csr->coo_entries = (graph_csr_coo_entry_t *)calloc(
        csr->coo_capacity, sizeof(graph_csr_coo_entry_t));
    csr->coo_count = 0;

    /* 分配标签数组 */
    csr->labels = (graph_csr_label_t *)calloc(GRAPH_CSR_MAX_LABELS,
                                              sizeof(graph_csr_label_t));
    csr->label_count = 0;

    /* 分配标签索引 */
    csr->label_index = (graph_csr_label_index_entry_t *)calloc(
        GRAPH_CSR_MAX_LABELS, sizeof(graph_csr_label_index_entry_t));
    csr->label_index_count = 0;

    LOG_INFO("CSR 图存储创建成功: max_vertices=%lu", max_vertices);
    return csr;
}

graph_csr_t *graph_csr_open(const char *data_dir) {
    if (data_dir == NULL) return NULL;

    char vertices_path[512];
    char edges_path[512];
    char meta_path[512];

    graph_csr_t temp = {0};
    strncpy(temp.data_dir, data_dir, sizeof(temp.data_dir) - 1);

    get_file_path(&temp, "vertices.bin", vertices_path, sizeof(vertices_path));
    get_file_path(&temp, "edges.bin", edges_path, sizeof(edges_path));
    get_file_path(&temp, "csr_meta.bin", meta_path, sizeof(meta_path));

    /* 检查文件是否存在 */
    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) {
        LOG_WARN("CSR 图元数据文件不存在");
        return NULL;
    }

    /* 读取元数据 */
    uint32_t magic, version;
    uint64_t vertex_count, max_vertices, edge_count;

    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        fread(&vertex_count, sizeof(vertex_count), 1, fp) != 1 ||
        fread(&max_vertices, sizeof(max_vertices), 1, fp) != 1 ||
        fread(&edge_count, sizeof(edge_count), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (magic != GRAPH_CSR_MAGIC || version != GRAPH_CSR_VERSION) {
        LOG_ERROR("CSR 图文件格式无效");
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    /* 创建 CSR 结构 */
    graph_csr_t *csr = graph_csr_create(data_dir, max_vertices);
    if (csr == NULL) return NULL;

    csr->vertex_count = vertex_count;
    csr->edge_count = edge_count;

    /* 读取顶点数据 */
    fp = fopen(vertices_path, "rb");
    if (fp != NULL) {
        fread(csr->vertices, sizeof(graph_csr_vertex_t), vertex_count, fp);
        fread(csr->offsets, sizeof(uint64_t), vertex_count + 1, fp);
        fclose(fp);
    }

    /* 读取边数据 */
    fp = fopen(edges_path, "rb");
    if (fp != NULL) {
        csr->edges = (graph_csr_edge_t *)malloc(edge_count * sizeof(graph_csr_edge_t));
        if (csr->edges != NULL) {
            fread(csr->edges, sizeof(graph_csr_edge_t), edge_count, fp);
        }
        fclose(fp);
    }

    /* 构建反向边索引（简化版本） */
    /* TODO: 实现完整的反向边索引 */

    LOG_INFO("CSR 图加载成功: vertices=%lu, edges=%lu",
             vertex_count, edge_count);
    return csr;
}

int graph_csr_save(graph_csr_t *csr) {
    if (csr == NULL) return -1;

    char vertices_path[512];
    char edges_path[512];
    char meta_path[512];
    char labels_path[512];

    get_file_path(csr, "vertices.bin", vertices_path, sizeof(vertices_path));
    get_file_path(csr, "edges.bin", edges_path, sizeof(edges_path));
    get_file_path(csr, "csr_meta.bin", meta_path, sizeof(meta_path));
    get_file_path(csr, "labels.bin", labels_path, sizeof(labels_path));

    /* 写入元数据 */
    FILE *fp = fopen(meta_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建 CSR 元数据文件");
        return -1;
    }

    uint32_t magic = GRAPH_CSR_MAGIC;
    uint32_t version = GRAPH_CSR_VERSION;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&csr->vertex_count, sizeof(csr->vertex_count), 1, fp);
    fwrite(&csr->max_vertices, sizeof(csr->max_vertices), 1, fp);
    fwrite(&csr->edge_count, sizeof(csr->edge_count), 1, fp);
    fwrite(&csr->label_count, sizeof(csr->label_count), 1, fp);
    fclose(fp);

    /* 写入顶点数据 */
    fp = fopen(vertices_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建顶点文件");
        return -1;
    }
    fwrite(csr->vertices, sizeof(graph_csr_vertex_t), csr->vertex_count, fp);
    fwrite(csr->offsets, sizeof(uint64_t), csr->vertex_count + 1, fp);
    fclose(fp);

    /* 写入边数据 */
    fp = fopen(edges_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建边文件");
        return -1;
    }
    fwrite(csr->edges, sizeof(graph_csr_edge_t), csr->edge_count, fp);
    fclose(fp);

    /* 写入标签数据 */
    fp = fopen(labels_path, "wb");
    if (fp != NULL) {
        fwrite(&csr->label_count, sizeof(csr->label_count), 1, fp);
        fwrite(csr->labels, sizeof(graph_csr_label_t), csr->label_count, fp);
        fclose(fp);
    }

    LOG_INFO("CSR 图保存成功: vertices=%lu, edges=%lu",
             csr->vertex_count, csr->edge_count);
    return 0;
}

void graph_csr_destroy(graph_csr_t *csr) {
    if (csr == NULL) return;

    /* 保存数据 */
    graph_csr_save(csr);

    /* 释放 COO 缓冲区 */
    if (csr->coo_entries != NULL) {
        for (uint32_t i = 0; i < csr->coo_count; i++) {
            if (csr->coo_entries[i].props != NULL) {
                free(csr->coo_entries[i].props);
            }
        }
        free(csr->coo_entries);
    }

    /* 释放标签索引 */
    if (csr->label_index != NULL) {
        for (uint32_t i = 0; i < csr->label_index_count; i++) {
            if (csr->label_index[i].vertex_ids != NULL) {
                free(csr->label_index[i].vertex_ids);
            }
        }
        free(csr->label_index);
    }

    free(csr->vertices);
    free(csr->offsets);
    free(csr->edges);
    free(csr->in_offsets);
    free(csr->in_edges);
    free(csr->labels);
    free(csr);
}

/* ========================================================================
 * 顶点操作 API 实现
 * ======================================================================== */

uint64_t graph_csr_add_vertex(graph_csr_t *csr, uint32_t label_id,
                              const void *props, size_t prop_size) {
    if (csr == NULL) return UINT64_MAX;

    /* 检查是否需要扩容 */
    if (csr->vertex_count >= csr->max_vertices) {
        uint64_t new_max = csr->max_vertices * 2;
        if (grow_vertices(csr, new_max) != 0) {
            return UINT64_MAX;
        }
        /* 扩展 offsets 数组 */
        uint64_t *new_offsets = (uint64_t *)realloc(
            csr->offsets, (new_max + 1) * sizeof(uint64_t));
        if (new_offsets == NULL) return UINT64_MAX;
        csr->offsets = new_offsets;

        /* 扩展入边 offsets 数组 */
        uint64_t *new_in_offsets = (uint64_t *)realloc(
            csr->in_offsets, (new_max + 1) * sizeof(uint64_t));
        if (new_in_offsets == NULL) return UINT64_MAX;
        csr->in_offsets = new_in_offsets;
    }

    uint64_t vertex_id = csr->vertex_count;

    /* 初始化顶点 */
    csr->vertices[vertex_id].label_id = label_id;
    csr->vertices[vertex_id].prop_offset = 0;
    csr->vertices[vertex_id].prop_size = (uint32_t)prop_size;

    csr->vertex_count++;
    csr->offsets[csr->vertex_count] = csr->offsets[csr->vertex_count - 1];

    LOG_DEBUG("添加顶点: id=%lu, label=%u", vertex_id, label_id);
    return vertex_id;
}

const graph_csr_vertex_t *graph_csr_get_vertex(const graph_csr_t *csr,
                                               uint64_t vertex_id) {
    if (csr == NULL || vertex_id >= csr->vertex_count) {
        return NULL;
    }
    return &csr->vertices[vertex_id];
}

/* ========================================================================
 * 边操作 API 实现
 * ======================================================================== */

uint64_t graph_csr_add_edge(graph_csr_t *csr,
                            uint64_t src, uint64_t dst, uint32_t edge_type,
                            const void *props, size_t prop_size) {
    if (csr == NULL) return UINT64_MAX;
    if (src >= csr->vertex_count || dst >= csr->vertex_count) {
        return UINT64_MAX;
    }

    /* 添加到 COO 缓冲区 */
    if (csr->coo_count >= csr->coo_capacity) {
        /* 触发自动合并 */
        if (graph_csr_compact(csr) != 0) {
            LOG_ERROR("COO 缓冲区满且合并失败");
            return UINT64_MAX;
        }
    }

    uint32_t idx = csr->coo_count++;
    csr->coo_entries[idx].src = src;
    csr->coo_entries[idx].dst = dst;
    csr->coo_entries[idx].edge_type = edge_type;
    csr->coo_entries[idx].edge_id = csr->edge_count + idx;

    if (props != NULL && prop_size > 0) {
        csr->coo_entries[idx].props = malloc(prop_size);
        if (csr->coo_entries[idx].props != NULL) {
            memcpy(csr->coo_entries[idx].props, props, prop_size);
            csr->coo_entries[idx].prop_size = prop_size;
        }
    }

    /* 暂时更新 offsets[src+1]，标记有新的出边 */
    csr->offsets[src + 1]++;

    LOG_DEBUG("添加边到 COO: src=%lu, dst=%lu, edge_id=%lu",
              src, dst, csr->coo_entries[idx].edge_id);
    return csr->coo_entries[idx].edge_id;
}

const graph_csr_edge_t *graph_csr_get_out_edges(const graph_csr_t *csr,
                                                uint64_t src, uint32_t *out_count) {
    if (csr == NULL || src >= csr->vertex_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    uint64_t start = csr->offsets[src];
    uint64_t end = csr->offsets[src + 1];

    if (out_count) *out_count = (uint32_t)(end - start);
    return &csr->edges[start];
}

const graph_csr_edge_t *graph_csr_get_in_edges(const graph_csr_t *csr,
                                               uint64_t dst, uint32_t *out_count) {
    if (csr == NULL || dst >= csr->vertex_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    uint64_t start = csr->in_offsets[dst];
    uint64_t end = csr->in_offsets[dst + 1];

    if (out_count) *out_count = (uint32_t)(end - start);
    return &csr->in_edges[start];
}

/* ========================================================================
 * COO 合并 API 实现
 * ======================================================================== */

int graph_csr_compact(graph_csr_t *csr) {
    if (csr == NULL || csr->coo_count == 0) return 0;

    LOG_INFO("开始合并 COO 缓冲区: %u 条边", csr->coo_count);

    /* 计算新的边总数 */
    uint64_t new_edge_count = csr->edge_count + csr->coo_count;

    /* 重新分配边数组 */
    graph_csr_edge_t *new_edges = (graph_csr_edge_t *)realloc(
        csr->edges, new_edge_count * sizeof(graph_csr_edge_t));
    if (new_edges == NULL) {
        LOG_ERROR("分配边数组失败");
        return -1;
    }
    csr->edges = new_edges;

    /* 复制 COO 条目到边数组 */
    for (uint32_t i = 0; i < csr->coo_count; i++) {
        csr->edges[csr->edge_count + i].dst = csr->coo_entries[i].dst;
        csr->edges[csr->edge_count + i].edge_type = csr->coo_entries[i].edge_type;
        csr->edges[csr->edge_count + i].edge_id = csr->coo_entries[i].edge_id;
    }

    /* 更新 offsets 数组 */
    /* 首先重新计算每个顶点的出边数 */
    memset(csr->offsets, 0, (csr->vertex_count + 1) * sizeof(uint64_t));

    /* 统计已有边的出边数 */
    for (uint64_t i = 0; i < csr->edge_count; i++) {
        uint64_t src_vertex = 0;  /* 需要从边中获取源顶点 */
        /* TODO: 边结构需要包含 src 字段，或者通过其他方式获取 */
        (void)src_vertex;  /* 暂时避免警告 */
    }

    /* 加上 COO 中的出边数 */
    for (uint32_t i = 0; i < csr->coo_count; i++) {
        csr->offsets[csr->coo_entries[i].src + 1]++;
    }

    /* 计算累积和 */
    for (uint64_t i = 1; i <= csr->vertex_count; i++) {
        csr->offsets[i] += csr->offsets[i - 1];
    }

    /* 清理 COO 缓冲区 */
    for (uint32_t i = 0; i < csr->coo_count; i++) {
        if (csr->coo_entries[i].props != NULL) {
            free(csr->coo_entries[i].props);
        }
    }
    csr->coo_count = 0;

    csr->edge_count = new_edge_count;

    LOG_INFO("COO 合并完成: edges=%lu", csr->edge_count);
    return 0;
}

bool graph_csr_needs_compact(const graph_csr_t *csr) {
    if (csr == NULL) return false;
    return csr->coo_count >= csr->coo_capacity * 8 / 10;  /* 80% 阈值 */
}

double graph_csr_coo_usage(const graph_csr_t *csr) {
    if (csr == NULL || csr->coo_capacity == 0) return 0.0;
    return (double)csr->coo_count / csr->coo_capacity;
}

/* ========================================================================
 * 标签索引 API 实现
 * ======================================================================== */

uint32_t graph_csr_get_or_create_label(graph_csr_t *csr, const char *label_name) {
    if (csr == NULL || label_name == NULL) return UINT32_MAX;

    /* 查找已存在的标签 */
    for (uint32_t i = 0; i < csr->label_count; i++) {
        if (strcmp(csr->labels[i].name, label_name) == 0) {
            return i;
        }
    }

    /* 创建新标签 */
    if (csr->label_count >= GRAPH_CSR_MAX_LABELS) {
        LOG_ERROR("标签数量已达上限");
        return UINT32_MAX;
    }

    uint32_t label_id = csr->label_count++;
    strncpy(csr->labels[label_id].name, label_name,
            sizeof(csr->labels[label_id].name) - 1);
    csr->labels[label_id].vertex_count = 0;

    return label_id;
}

const char *graph_csr_get_label_name(const graph_csr_t *csr, uint32_t label_id) {
    if (csr == NULL || label_id >= csr->label_count) {
        return NULL;
    }
    return csr->labels[label_id].name;
}

uint64_t *graph_csr_get_vertices_by_label(const graph_csr_t *csr,
                                          uint32_t label_id,
                                          uint32_t *out_count) {
    if (csr == NULL || label_id >= csr->label_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* 首先确保标签索引已构建 */
    if (csr->label_index == NULL || csr->label_index_count == 0) {
        /* 返回空结果 */
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* 查找标签索引项 */
    for (uint32_t i = 0; i < csr->label_index_count; i++) {
        if (csr->label_index[i].label_id == label_id) {
            if (out_count) *out_count = csr->label_index[i].count;
            /* 返回副本，由调用者释放 */
            uint64_t *result = (uint64_t *)malloc(
                csr->label_index[i].count * sizeof(uint64_t));
            if (result != NULL) {
                memcpy(result, csr->label_index[i].vertex_ids,
                       csr->label_index[i].count * sizeof(uint64_t));
            }
            return result;
        }
    }

    if (out_count) *out_count = 0;
    return NULL;
}

void graph_csr_build_label_index(graph_csr_t *csr) {
    if (csr == NULL) return;

    LOG_INFO("构建标签索引: vertices=%lu", csr->vertex_count);

    /* 清空现有标签索引 */
    for (uint32_t i = 0; i < csr->label_index_count; i++) {
        if (csr->label_index[i].vertex_ids != NULL) {
            free(csr->label_index[i].vertex_ids);
            csr->label_index[i].vertex_ids = NULL;
        }
    }
    csr->label_index_count = 0;

    /* 统计每个标签的顶点数 */
    for (uint64_t i = 0; i < csr->vertex_count; i++) {
        uint32_t label_id = csr->vertices[i].label_id;
        if (label_id < GRAPH_CSR_MAX_LABELS) {
            csr->labels[label_id].vertex_count++;
        }
    }

    /* 分配标签索引数组 */
    for (uint32_t i = 0; i < csr->label_count; i++) {
        if (csr->labels[i].vertex_count > 0) {
            csr->label_index[csr->label_index_count].label_id = i;
            csr->label_index[csr->label_index_count].count = 0;
            csr->label_index[csr->label_index_count].vertex_ids = (uint64_t *)malloc(
                csr->labels[i].vertex_count * sizeof(uint64_t));
            csr->label_index_count++;
        }
    }

    /* 填充顶点 ID */
    for (uint32_t i = 0; i < csr->label_index_count; i++) {
        uint32_t label_id = csr->label_index[i].label_id;
        uint32_t pos = 0;
        for (uint64_t v = 0; v < csr->vertex_count; v++) {
            if (csr->vertices[v].label_id == label_id) {
                csr->label_index[i].vertex_ids[pos++] = v;
                csr->label_index[i].count = pos;
            }
        }
    }

    LOG_INFO("标签索引构建完成: %u 个标签", csr->label_index_count);
}

/* ========================================================================
 * 统计信息 API 实现
 * ======================================================================== */

void graph_csr_get_stats(const graph_csr_t *csr,
                         uint64_t *out_vertices, uint64_t *out_edges,
                         uint32_t *out_labels) {
    if (csr == NULL) return;

    if (out_vertices) *out_vertices = csr->vertex_count;
    if (out_edges) *out_edges = csr->edge_count + csr->coo_count;
    if (out_labels) *out_labels = csr->label_count;
}
