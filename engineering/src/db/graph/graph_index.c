/**
 * @file graph_index.c
 * @brief 图索引实现
 */

#include "db/graph/graph_index.h"
#include "db/graph/graph_property.h"
#include "db/mm_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 标签索引实现
 * ======================================================================== */

GraphLabelIndex *graph_label_index_create(void *mem_pool)
{
    GraphLabelIndex *index = (GraphLabelIndex *)malloc(sizeof(GraphLabelIndex));
    if (!index) return NULL;

    index->vertex_index = NULL;
    index->edge_index = NULL;
    index->mem_pool = mem_pool;
    return index;
}

void graph_label_index_destroy(GraphLabelIndex *index)
{
    if (!index) return;

    /* 释放顶点索引 */
    GraphLabelIndexEntry *v, *vtmp;
    HASH_ITER(hh, index->vertex_index, v, vtmp) {
        free(v->vertex_ids);
        HASH_DEL(index->vertex_index, v);
        free(v);
    }

    /* 释放边索引 */
    GraphEdgeLabelIndexEntry *e, *etmp;
    HASH_ITER(hh, index->edge_index, e, etmp) {
        free(e->edge_ids);
        HASH_DEL(index->edge_index, e);
        free(e);
    }

    free(index);
}

int graph_label_index_add_vertex(GraphLabelIndex *index,
                                graph_vertex_id_t vid,
                                graph_label_id_t label)
{
    GraphLabelIndexEntry *entry;
    HASH_FIND(hh, index->vertex_index, &label, sizeof(graph_label_id_t), entry);

    if (!entry) {
        entry = (GraphLabelIndexEntry *)malloc(sizeof(GraphLabelIndexEntry));
        entry->label_id = label;
        entry->vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * 64);
        entry->num_vertices = 0;
        entry->capacity = 64;
        HASH_ADD(hh, index->vertex_index, label_id, sizeof(graph_label_id_t), entry);
    }

    if (entry->num_vertices >= entry->capacity) {
        entry->capacity *= 2;
        entry->vertex_ids = (graph_vertex_id_t *)realloc(
            entry->vertex_ids, sizeof(graph_vertex_id_t) * entry->capacity);
    }

    entry->vertex_ids[entry->num_vertices++] = vid;
    return 0;
}

int graph_label_index_remove_vertex(GraphLabelIndex *index,
                                   graph_vertex_id_t vid,
                                   graph_label_id_t label)
{
    GraphLabelIndexEntry *entry;
    HASH_FIND(hh, index->vertex_index, &label, sizeof(graph_label_id_t), entry);

    if (!entry) return -1;

    for (size_t i = 0; i < entry->num_vertices; i++) {
        if (entry->vertex_ids[i] == vid) {
            for (size_t j = i; j < entry->num_vertices - 1; j++) {
                entry->vertex_ids[j] = entry->vertex_ids[j + 1];
            }
            entry->num_vertices--;
            return 0;
        }
    }

    return -1;
}

int graph_label_index_add_edge(GraphLabelIndex *index,
                              graph_edge_id_t eid,
                              graph_label_id_t rel_type)
{
    GraphEdgeLabelIndexEntry *entry;
    HASH_FIND(hh, index->edge_index, &rel_type, sizeof(graph_label_id_t), entry);

    if (!entry) {
        entry = (GraphEdgeLabelIndexEntry *)malloc(sizeof(GraphEdgeLabelIndexEntry));
        entry->rel_type = rel_type;
        entry->edge_ids = (graph_edge_id_t *)malloc(sizeof(graph_edge_id_t) * 64);
        entry->num_edges = 0;
        entry->capacity = 64;
        HASH_ADD(hh, index->edge_index, rel_type, sizeof(graph_label_id_t), entry);
    }

    if (entry->num_edges >= entry->capacity) {
        entry->capacity *= 2;
        entry->edge_ids = (graph_edge_id_t *)realloc(
            entry->edge_ids, sizeof(graph_edge_id_t) * entry->capacity);
    }

    entry->edge_ids[entry->num_edges++] = eid;
    return 0;
}

int graph_label_index_remove_edge(GraphLabelIndex *index,
                                 graph_edge_id_t eid,
                                 graph_label_id_t rel_type)
{
    GraphEdgeLabelIndexEntry *entry;
    HASH_FIND(hh, index->edge_index, &rel_type, sizeof(graph_label_id_t), entry);

    if (!entry) return -1;

    for (size_t i = 0; i < entry->num_edges; i++) {
        if (entry->edge_ids[i] == eid) {
            for (size_t j = i; j < entry->num_edges - 1; j++) {
                entry->edge_ids[j] = entry->edge_ids[j + 1];
            }
            entry->num_edges--;
            return 0;
        }
    }

    return -1;
}

graph_vertex_id_t *graph_label_index_get_vertices(GraphLabelIndex *index,
                                                  graph_label_id_t label,
                                                  size_t *out_count)
{
    *out_count = 0;

    GraphLabelIndexEntry *entry;
    HASH_FIND(hh, index->vertex_index, &label, sizeof(graph_label_id_t), entry);

    if (!entry) return NULL;

    *out_count = entry->num_vertices;
    graph_vertex_id_t *result = (graph_vertex_id_t *)malloc(
        sizeof(graph_vertex_id_t) * entry->num_vertices);
    memcpy(result, entry->vertex_ids, sizeof(graph_vertex_id_t) * entry->num_vertices);

    return result;
}

graph_edge_id_t *graph_label_index_get_edges(GraphLabelIndex *index,
                                             graph_label_id_t rel_type,
                                             size_t *out_count)
{
    *out_count = 0;

    GraphEdgeLabelIndexEntry *entry;
    HASH_FIND(hh, index->edge_index, &rel_type, sizeof(graph_label_id_t), entry);

    if (!entry) return NULL;

    *out_count = entry->num_edges;
    graph_edge_id_t *result = (graph_edge_id_t *)malloc(
        sizeof(graph_edge_id_t) * entry->num_edges);
    memcpy(result, entry->edge_ids, sizeof(graph_edge_id_t) * entry->num_edges);

    return result;
}

size_t graph_label_index_vertex_count(GraphLabelIndex *index, graph_label_id_t label)
{
    GraphLabelIndexEntry *entry;
    HASH_FIND(hh, index->vertex_index, &label, sizeof(graph_label_id_t), entry);
    return entry ? entry->num_vertices : 0;
}

size_t graph_label_index_edge_count(GraphLabelIndex *index, graph_label_id_t rel_type)
{
    GraphEdgeLabelIndexEntry *entry;
    HASH_FIND(hh, index->edge_index, &rel_type, sizeof(graph_label_id_t), entry);
    return entry ? entry->num_edges : 0;
}

/* ========================================================================
 * 属性索引管理器实现
 * ======================================================================== */

GraphPropIndexMgr *graph_prop_index_mgr_create(void *mem_pool)
{
    GraphPropIndexMgr *mgr = (GraphPropIndexMgr *)malloc(sizeof(GraphPropIndexMgr));
    if (!mgr) return NULL;

    mgr->indexes = NULL;
    mgr->num_indexes = 0;
    mgr->mem_pool = mem_pool;
    return mgr;
}

void graph_prop_index_mgr_destroy(GraphPropIndexMgr *mgr)
{
    if (!mgr) return;

    for (size_t i = 0; i < mgr->num_indexes; i++) {
        free(mgr->indexes[i]);
    }
    free(mgr->indexes);
    free(mgr);
}

int graph_prop_index_create(GraphPropIndexMgr *mgr,
                           const char *index_name,
                           const char *prop_name,
                           GraphIndexType type,
                           bool is_vertex_index)
{
    GraphPropIndex *index = (GraphPropIndex *)malloc(sizeof(GraphPropIndex));
    if (!index) return -1;

    strncpy(index->index_name, index_name, sizeof(index->index_name) - 1);
    strncpy(index->prop_name, prop_name, sizeof(index->prop_name) - 1);
    index->type = type;
    index->is_vertex_index = is_vertex_index;
    index->value_index = NULL;
    index->range_indexes = NULL;
    index->num_range_indexes = 0;
    index->mem_pool = mgr->mem_pool;

    mgr->indexes = (GraphPropIndex **)realloc(
        mgr->indexes, sizeof(GraphPropIndex *) * (mgr->num_indexes + 1));
    mgr->indexes[mgr->num_indexes++] = index;

    return 0;
}

int graph_prop_index_drop(GraphPropIndexMgr *mgr, const char *index_name)
{
    for (size_t i = 0; i < mgr->num_indexes; i++) {
        if (strcmp(mgr->indexes[i]->index_name, index_name) == 0) {
            free(mgr->indexes[i]);

            for (size_t j = i; j < mgr->num_indexes - 1; j++) {
                mgr->indexes[j] = mgr->indexes[j + 1];
            }
            mgr->num_indexes--;

            return 0;
        }
    }
    return -1;
}

GraphPropIndex *graph_prop_index_get(GraphPropIndexMgr *mgr, const char *index_name)
{
    for (size_t i = 0; i < mgr->num_indexes; i++) {
        if (strcmp(mgr->indexes[i]->index_name, index_name) == 0) {
            return mgr->indexes[i];
        }
    }
    return NULL;
}

static int compare_prop_values(const void *a, const void *b)
{
    const GraphPropValueEntry *ea = (const GraphPropValueEntry *)a;
    const GraphPropValueEntry *eb = (const GraphPropValueEntry *)b;
    return graph_prop_value_compare(&ea->value, &eb->value);
}

int graph_prop_index_add_vertex(GraphPropIndexMgr *mgr,
                               const char *index_name,
                               graph_vertex_id_t vid,
                               const GraphPropValue *value)
{
    GraphPropIndex *index = graph_prop_index_get(mgr, index_name);
    if (!index || !index->is_vertex_index) return -1;

    /* 创建值索引项 */
    GraphPropValueEntry *entry = (GraphPropValueEntry *)malloc(sizeof(GraphPropValueEntry));
    graph_prop_value_copy_to(value, &entry->value);
    entry->vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * 16);
    entry->vertex_ids[0] = vid;
    entry->num_vertices = 1;
    entry->capacity = 16;

    /* 添加到哈希表（使用字符串值作为 key） */
    char *value_str = graph_prop_value_to_string(value);
    HASH_ADD_KEYPTR(hh, index->value_index, value_str, strlen(value_str), entry);
    free(value_str);

    return 0;
}

int graph_prop_index_add_edge(GraphPropIndexMgr *mgr,
                             const char *index_name,
                             graph_edge_id_t eid,
                             const GraphPropValue *value)
{
    GraphPropIndex *index = graph_prop_index_get(mgr, index_name);
    if (!index || index->is_vertex_index) return -1;

    /* 创建值索引项 */
    GraphPropValueEntry *entry = (GraphPropValueEntry *)malloc(sizeof(GraphPropValueEntry));
    graph_prop_value_copy_to(value, &entry->value);
    entry->edge_ids = (graph_edge_id_t *)malloc(sizeof(graph_edge_id_t) * 16);
    entry->edge_ids[0] = eid;
    entry->num_vertices = 1;  /* 复用 num_vertices 字段存储边数 */
    entry->capacity = 16;

    char *value_str = graph_prop_value_to_string(value);
    HASH_ADD_KEYPTR(hh, index->value_index, value_str, strlen(value_str), entry);
    free(value_str);

    return 0;
}

int graph_prop_index_remove_vertex(GraphPropIndexMgr *mgr,
                                  const char *index_name,
                                  graph_vertex_id_t vid)
{
    (void)mgr; (void)index_name; (void)vid;
    /* 简化实现 */
    return 0;
}

int graph_prop_index_remove_edge(GraphPropIndexMgr *mgr,
                               const char *index_name,
                               graph_edge_id_t eid)
{
    (void)mgr; (void)index_name; (void)eid;
    /* 简化实现 */
    return 0;
}

graph_vertex_id_t *graph_prop_index_lookup(GraphPropIndexMgr *mgr,
                                          const char *index_name,
                                          const GraphPropValue *value,
                                          size_t *out_count)
{
    *out_count = 0;

    GraphPropIndex *index = graph_prop_index_get(mgr, index_name);
    if (!index) return NULL;

    char *value_str = graph_prop_value_to_string(value);
    GraphPropValueEntry *entry;
    HASH_FIND(hh, index->value_index, value_str, strlen(value_str), entry);
    free(value_str);

    if (!entry) return NULL;

    if (index->is_vertex_index) {
        *out_count = entry->num_vertices;
        graph_vertex_id_t *result = (graph_vertex_id_t *)malloc(
            sizeof(graph_vertex_id_t) * entry->num_vertices);
        memcpy(result, entry->vertex_ids, sizeof(graph_vertex_id_t) * entry->num_vertices);
        return result;
    } else {
        /* 边索引返回边 ID */
        *out_count = entry->num_vertices;
        graph_vertex_id_t *result = (graph_vertex_id_t *)malloc(
            sizeof(graph_vertex_id_t) * entry->num_vertices);
        for (size_t i = 0; i < entry->num_vertices; i++) {
            result[i] = entry->edge_ids[i];
        }
        return result;
    }
}

graph_vertex_id_t *graph_prop_index_range_ge(GraphPropIndexMgr *mgr,
                                            const char *index_name,
                                            const GraphPropValue *min_value,
                                            size_t *out_count)
{
    *out_count = 0;

    GraphPropIndex *index = graph_prop_index_get(mgr, index_name);
    if (!index) return NULL;

    graph_vertex_id_t *results = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * 256);
    size_t capacity = 256;

    GraphPropValueEntry *entry, *tmp;
    HASH_ITER(hh, index->value_index, entry, tmp) {
        if (graph_prop_value_compare(&entry->value, min_value) >= 0) {
            if (*out_count + entry->num_vertices >= capacity) {
                capacity *= 2;
                results = (graph_vertex_id_t *)realloc(results, sizeof(graph_vertex_id_t) * capacity);
            }

            for (size_t i = 0; i < entry->num_vertices; i++) {
                results[*out_count] = entry->vertex_ids[i];
                (*out_count)++;
            }
        }
    }

    return results;
}

graph_vertex_id_t *graph_prop_index_range_le(GraphPropIndexMgr *mgr,
                                            const char *index_name,
                                            const GraphPropValue *max_value,
                                            size_t *out_count)
{
    *out_count = 0;

    GraphPropIndex *index = graph_prop_index_get(mgr, index_name);
    if (!index) return NULL;

    graph_vertex_id_t *results = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * 256);
    size_t capacity = 256;

    GraphPropValueEntry *entry, *tmp;
    HASH_ITER(hh, index->value_index, entry, tmp) {
        if (graph_prop_value_compare(&entry->value, max_value) <= 0) {
            if (*out_count + entry->num_vertices >= capacity) {
                capacity *= 2;
                results = (graph_vertex_id_t *)realloc(results, sizeof(graph_vertex_id_t) * capacity);
            }

            for (size_t i = 0; i < entry->num_vertices; i++) {
                results[*out_count] = entry->vertex_ids[i];
                (*out_count)++;
            }
        }
    }

    return results;
}

graph_vertex_id_t *graph_prop_index_range_between(GraphPropIndexMgr *mgr,
                                                 const char *index_name,
                                                 const GraphPropValue *min_value,
                                                 const GraphPropValue *max_value,
                                                 size_t *out_count)
{
    *out_count = 0;

    GraphPropIndex *index = graph_prop_index_get(mgr, index_name);
    if (!index) return NULL;

    graph_vertex_id_t *results = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * 256);
    size_t capacity = 256;

    GraphPropValueEntry *entry, *tmp;
    HASH_ITER(hh, index->value_index, entry, tmp) {
        int cmp_min = graph_prop_value_compare(&entry->value, min_value);
        int cmp_max = graph_prop_value_compare(&entry->value, max_value);

        if (cmp_min >= 0 && cmp_max <= 0) {
            if (*out_count + entry->num_vertices >= capacity) {
                capacity *= 2;
                results = (graph_vertex_id_t *)realloc(results, sizeof(graph_vertex_id_t) * capacity);
            }

            for (size_t i = 0; i < entry->num_vertices; i++) {
                results[*out_count] = entry->vertex_ids[i];
                (*out_count)++;
            }
        }
    }

    return results;
}

/* ========================================================================
 * 向量索引管理器实现
 * ======================================================================== */

GraphVectorIndexMgr *graph_vector_index_mgr_create(void *mem_pool)
{
    GraphVectorIndexMgr *mgr = (GraphVectorIndexMgr *)malloc(sizeof(GraphVectorIndexMgr));
    if (!mgr) return NULL;

    mgr->indexes = NULL;
    mgr->num_indexes = 0;
    mgr->mem_pool = mem_pool;
    return mgr;
}

void graph_vector_index_mgr_destroy(GraphVectorIndexMgr *mgr)
{
    if (!mgr) return;

    for (size_t i = 0; i < mgr->num_indexes; i++) {
        free(mgr->indexes[i]);
    }
    free(mgr->indexes);
    free(mgr);
}

int graph_vector_index_create(GraphVectorIndexMgr *mgr,
                             const char *index_name,
                             const char *prop_name,
                             bool is_vertex_index,
                             const GraphVectorIndexConfig *config)
{
    GraphVectorIndex *index = (GraphVectorIndex *)malloc(sizeof(GraphVectorIndex));
    if (!index) return -1;

    strncpy(index->index_name, index_name, sizeof(index->index_name) - 1);
    strncpy(index->prop_name, prop_name, sizeof(index->prop_name) - 1);
    index->is_vertex_index = is_vertex_index;
    index->config = config ? *config : (GraphVectorIndexConfig){0};
    index->index_impl = NULL;
    index->mem_pool = mgr->mem_pool;

    mgr->indexes = (GraphVectorIndex **)realloc(
        mgr->indexes, sizeof(GraphVectorIndex *) * (mgr->num_indexes + 1));
    mgr->indexes[mgr->num_indexes++] = index;

    return 0;
}

int graph_vector_index_drop(GraphVectorIndexMgr *mgr, const char *index_name)
{
    for (size_t i = 0; i < mgr->num_indexes; i++) {
        if (strcmp(mgr->indexes[i]->index_name, index_name) == 0) {
            free(mgr->indexes[i]);

            for (size_t j = i; j < mgr->num_indexes - 1; j++) {
                mgr->indexes[j] = mgr->indexes[j + 1];
            }
            mgr->num_indexes--;

            return 0;
        }
    }
    return -1;
}

GraphVectorIndex *graph_vector_index_get(GraphVectorIndexMgr *mgr, const char *index_name)
{
    for (size_t i = 0; i < mgr->num_indexes; i++) {
        if (strcmp(mgr->indexes[i]->index_name, index_name) == 0) {
            return mgr->indexes[i];
        }
    }
    return NULL;
}

int graph_vector_index_add(GraphVectorIndexMgr *mgr,
                         const char *index_name,
                         graph_vertex_id_t vid,
                         const float *vector,
                         int dim)
{
    (void)mgr; (void)index_name; (void)vid; (void)vector; (void)dim;
    /* 需要与 HNSW/IVF 等向量索引集成 */
    return 0;
}

int graph_vector_index_remove(GraphVectorIndexMgr *mgr,
                             const char *index_name,
                             graph_vertex_id_t vid)
{
    (void)mgr; (void)index_name; (void)vid;
    return 0;
}

static float euclidean_distance(const float *a, const float *b, int dim)
{
    float dist = 0;
    for (int i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

static float cosine_distance(const float *a, const float *b, int dim)
{
    float dot = 0, norm_a = 0, norm_b = 0;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return 1.0f - dot / (sqrtf(norm_a) * sqrtf(norm_b) + 1e-10f);
}

GraphVectorSearchResult *graph_vector_index_search(GraphVectorIndexMgr *mgr,
                                                   const char *index_name,
                                                   const float *query_vector,
                                                   int query_dim,
                                                   int top_k,
                                                   size_t *out_count)
{
    GraphVectorIndex *index = graph_vector_index_get(mgr, index_name);
    if (!index) {
        *out_count = 0;
        return NULL;
    }

    /* 简化实现：返回空结果（实际需要调用底层向量索引） */
    *out_count = 0;
    return NULL;
}

GraphVectorSearchResult *graph_vector_index_search_filter(GraphVectorIndexMgr *mgr,
                                                        const char *index_name,
                                                        const float *query_vector,
                                                        int query_dim,
                                                        int top_k,
                                                        const graph_label_id_t *labels,
                                                        size_t num_labels,
                                                        size_t *out_count)
{
    (void)mgr; (void)index_name; (void)query_vector; (void)query_dim;
    (void)top_k; (void)labels; (void)num_labels;
    *out_count = 0;
    return NULL;
}

void graph_vector_search_result_free(GraphVectorSearchResult *results, size_t count)
{
    (void)count;
    free(results);
}

/* ========================================================================
 * 复合索引实现
 * ======================================================================== */

GraphCompositeIndex *graph_composite_index_create(GraphCompositeIndexDef *def,
                                                 void *mem_pool)
{
    GraphCompositeIndex *index = (GraphCompositeIndex *)malloc(sizeof(GraphCompositeIndex));
    if (!index) return NULL;

    memcpy(&index->def, def, sizeof(GraphCompositeIndexDef));
    index->index_impl = NULL;
    index->mem_pool = mem_pool;

    return index;
}

void graph_composite_index_destroy(GraphCompositeIndex *index)
{
    free(index);
}

int graph_composite_index_insert(GraphCompositeIndex *index,
                                graph_vertex_id_t vid,
                                const GraphPropValue *values)
{
    (void)index; (void)vid; (void)values;
    return 0;
}

int graph_composite_index_delete(GraphCompositeIndex *index, graph_vertex_id_t vid)
{
    (void)index; (void)vid;
    return 0;
}

graph_vertex_id_t *graph_composite_index_lookup(GraphCompositeIndex *index,
                                                const GraphPropValue *values,
                                                size_t *out_count)
{
    (void)index; (void)values;
    *out_count = 0;
    return NULL;
}
