/**
 * @file graph_property.c
 * @brief 属性图存储实现
 */

#include "db/graph/graph_property.h"
#include "db/mm_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ========================================================================
 * 顶点属性存储实现
 * ======================================================================== */

GraphVertexStore *graph_vertex_store_create(void *mem_pool)
{
    GraphVertexStore *store = (GraphVertexStore *)malloc(sizeof(GraphVertexStore));
    if (!store) return NULL;

    store->vertices = NULL;
    store->next_vid = 1;
    store->vid_by_label = NULL;
    store->mem_pool = mem_pool;
    store->version = 1;
    return store;
}

void graph_vertex_store_destroy(GraphVertexStore *store)
{
    if (!store) return;

    GraphVertexProp *v, *tmp;
    HASH_ITER(hh, store->vertices, v, tmp) {
        for (size_t i = 0; i < v->num_props; i++) {
            free(v->props[i].name);
            graph_prop_value_free(&v->props[i].value);
        }
        free(v->props);
        free(v->labels);
        HASH_DEL(store->vertices, v);
        free(v);
    }
    free(store);
}

int graph_vertex_store_add(GraphVertexStore *store,
                          graph_vertex_id_t vid,
                          const GraphProperty *props,
                          size_t num_props,
                          const graph_label_id_t *labels,
                          size_t num_labels)
{
    GraphVertexProp *v = (GraphVertexProp *)malloc(sizeof(GraphVertexProp));
    if (!v) return -1;

    v->vid = vid;
    v->num_props = num_props;
    v->num_labels = num_labels;
    v->version = store->version;

    if (num_props > 0) {
        v->props = (GraphProperty *)calloc(num_props, sizeof(GraphProperty));
        if (!v->props) {
            free(v);
            return -1;
        }
        for (size_t i = 0; i < num_props; i++) {
            v->props[i].name = strdup(props[i].name);
            if (!v->props[i].name) {
                /* 回滚已分配的资源 */
                for (size_t j = 0; j < i; j++) {
                    free(v->props[j].name);
                    graph_prop_value_free(&v->props[j].value);
                }
                free(v->props);
                free(v);
                return -1;
            }
            graph_prop_value_copy_to(&props[i].value, &v->props[i].value);
        }
    } else {
        v->props = NULL;
    }

    if (num_labels > 0) {
        v->labels = (graph_label_id_t *)malloc(sizeof(graph_label_id_t) * num_labels);
        if (!v->labels) {
            /* 回滚 props */
            if (v->props) {
                for (size_t i = 0; i < num_props; i++) {
                    free(v->props[i].name);
                    graph_prop_value_free(&v->props[i].value);
                }
                free(v->props);
            }
            free(v);
            return -1;
        }
        memcpy(v->labels, labels, sizeof(graph_label_id_t) * num_labels);
    } else {
        v->labels = NULL;
    }

    HASH_ADD(hh, store->vertices, vid, sizeof(graph_vertex_id_t), v);

    if (vid >= store->next_vid) {
        store->next_vid = vid + 1;
    }

    return 0;
}

GraphVertexProp *graph_vertex_store_get(GraphVertexStore *store,
                                        graph_vertex_id_t vid)
{
    GraphVertexProp *v;
    HASH_FIND(hh, store->vertices, &vid, sizeof(graph_vertex_id_t), v);
    return v;
}

int graph_vertex_store_update(GraphVertexStore *store,
                             graph_vertex_id_t vid,
                             const GraphProperty *props,
                             size_t num_props)
{
    GraphVertexProp *v = graph_vertex_store_get(store, vid);
    if (!v) return -1;

    for (size_t i = 0; i < v->num_props; i++) {
        free(v->props[i].name);
        graph_prop_value_free(&v->props[i].value);
    }
    free(v->props);

    v->num_props = num_props;
    v->version++;

    if (num_props > 0) {
        v->props = (GraphProperty *)malloc(sizeof(GraphProperty) * num_props);
        for (size_t i = 0; i < num_props; i++) {
            v->props[i].name = strdup(props[i].name);
            graph_prop_value_copy_to(&props[i].value, &v->props[i].value);
        }
    } else {
        v->props = NULL;
    }

    return 0;
}

int graph_vertex_store_delete(GraphVertexStore *store, graph_vertex_id_t vid)
{
    GraphVertexProp *v = graph_vertex_store_get(store, vid);
    if (!v) return -1;

    for (size_t i = 0; i < v->num_props; i++) {
        free(v->props[i].name);
        graph_prop_value_free(&v->props[i].value);
    }
    free(v->props);
    free(v->labels);
    HASH_DEL(store->vertices, v);
    free(v);

    return 0;
}

graph_vertex_id_t *graph_vertex_store_get_by_label(GraphVertexStore *store,
                                                   graph_label_id_t label,
                                                   size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 64;
    graph_vertex_id_t *result = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * capacity);

    GraphVertexProp *v, *tmp;
    HASH_ITER(hh, store->vertices, v, tmp) {
        for (size_t i = 0; i < v->num_labels; i++) {
            if (v->labels[i] == label) {
                if (*out_count >= capacity) {
                    capacity *= 2;
                    result = (graph_vertex_id_t *)realloc(result, sizeof(graph_vertex_id_t) * capacity);
                }
                result[*out_count] = v->vid;
                (*out_count)++;
                break;
            }
        }
    }

    return result;
}

graph_vertex_id_t *graph_vertex_store_query(GraphVertexStore *store,
                                            const char *prop_name,
                                            const GraphPropValue *value,
                                            size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 64;
    graph_vertex_id_t *result = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * capacity);

    GraphVertexProp *v, *tmp;
    HASH_ITER(hh, store->vertices, v, tmp) {
        for (size_t i = 0; i < v->num_props; i++) {
            if (strcmp(v->props[i].name, prop_name) == 0) {
                if (graph_prop_value_compare(&v->props[i].value, value) == 0) {
                    if (*out_count >= capacity) {
                        capacity *= 2;
                        result = (graph_vertex_id_t *)realloc(result, sizeof(graph_vertex_id_t) * capacity);
                    }
                    result[*out_count] = v->vid;
                    (*out_count)++;
                }
                break;
            }
        }
    }

    return result;
}

uint64_t graph_vertex_store_count(const GraphVertexStore *store)
{
    return HASH_COUNT(store->vertices);
}

/* ========================================================================
 * 边属性存储实现
 * ======================================================================== */

GraphEdgeStore *graph_edge_store_create(void *mem_pool)
{
    GraphEdgeStore *store = (GraphEdgeStore *)malloc(sizeof(GraphEdgeStore));
    if (!store) return NULL;

    store->edges = NULL;
    store->next_eid = 1;
    store->edges_by_reltype = NULL;
    store->edges_by_src = NULL;
    store->edges_by_dst = NULL;
    store->mem_pool = mem_pool;
    store->version = 1;
    return store;
}

void graph_edge_store_destroy(GraphEdgeStore *store)
{
    if (!store) return;

    GraphEdgeProp *e, *tmp;
    HASH_ITER(hh, store->edges, e, tmp) {
        for (size_t i = 0; i < e->num_props; i++) {
            free(e->props[i].name);
            graph_prop_value_free(&e->props[i].value);
        }
        free(e->props);
        HASH_DEL(store->edges, e);
        free(e);
    }

    free(store);
}

static int add_edge_to_index(GraphEdgeStore *store, graph_edge_id_t eid,
                            graph_label_id_t rel_type,
                            graph_vertex_id_t src,
                            graph_vertex_id_t dst)
{
    (void)store; (void)eid; (void)rel_type; (void)src; (void)dst;
    return 0;
}

static void remove_edge_from_index(GraphEdgeStore *store, graph_edge_id_t eid,
                                   graph_label_id_t rel_type,
                                   graph_vertex_id_t src,
                                   graph_vertex_id_t dst)
{
    (void)store; (void)eid; (void)rel_type; (void)src; (void)dst;
}

int graph_edge_store_add(GraphEdgeStore *store,
                        graph_edge_id_t eid,
                        graph_vertex_id_t src,
                        graph_vertex_id_t dst,
                        graph_label_id_t rel_type,
                        const GraphProperty *props,
                        size_t num_props)
{
    GraphEdgeProp *e = (GraphEdgeProp *)malloc(sizeof(GraphEdgeProp));
    if (!e) return -1;

    e->eid = eid;
    e->src = src;
    e->dst = dst;
    e->rel_type = rel_type;
    e->num_props = num_props;
    e->version = store->version;

    if (num_props > 0) {
        e->props = (GraphProperty *)malloc(sizeof(GraphProperty) * num_props);
        for (size_t i = 0; i < num_props; i++) {
            e->props[i].name = strdup(props[i].name);
            graph_prop_value_copy_to(&props[i].value, &e->props[i].value);
        }
    } else {
        e->props = NULL;
    }

    HASH_ADD(hh, store->edges, eid, sizeof(graph_edge_id_t), e);
    add_edge_to_index(store, eid, rel_type, src, dst);

    if (eid >= store->next_eid) {
        store->next_eid = eid + 1;
    }

    return 0;
}

GraphEdgeProp *graph_edge_store_get(GraphEdgeStore *store, graph_edge_id_t eid)
{
    GraphEdgeProp *e;
    HASH_FIND(hh, store->edges, &eid, sizeof(graph_edge_id_t), e);
    return e;
}

int graph_edge_store_update(GraphEdgeStore *store,
                            graph_edge_id_t eid,
                            const GraphProperty *props,
                            size_t num_props)
{
    GraphEdgeProp *e = graph_edge_store_get(store, eid);
    if (!e) return -1;

    for (size_t i = 0; i < e->num_props; i++) {
        free(e->props[i].name);
        graph_prop_value_free(&e->props[i].value);
    }
    free(e->props);

    e->num_props = num_props;
    e->version++;

    if (num_props > 0) {
        e->props = (GraphProperty *)malloc(sizeof(GraphProperty) * num_props);
        for (size_t i = 0; i < num_props; i++) {
            e->props[i].name = strdup(props[i].name);
            graph_prop_value_copy_to(&props[i].value, &e->props[i].value);
        }
    } else {
        e->props = NULL;
    }

    return 0;
}

int graph_edge_store_delete(GraphEdgeStore *store, graph_edge_id_t eid)
{
    GraphEdgeProp *e = graph_edge_store_get(store, eid);
    if (!e) return -1;

    remove_edge_from_index(store, eid, e->rel_type, e->src, e->dst);

    for (size_t i = 0; i < e->num_props; i++) {
        free(e->props[i].name);
        graph_prop_value_free(&e->props[i].value);
    }
    free(e->props);
    HASH_DEL(store->edges, e);
    free(e);

    return 0;
}

graph_edge_id_t *graph_edge_store_get_out_edges(GraphEdgeStore *store,
                                                graph_vertex_id_t src,
                                                graph_label_id_t rel_type,
                                                size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 64;
    graph_edge_id_t *result = (graph_edge_id_t *)malloc(sizeof(graph_edge_id_t) * capacity);

    GraphEdgeProp *e, *tmp;
    HASH_ITER(hh, store->edges, e, tmp) {
        if (e->src == src) {
            if (rel_type == 0 || e->rel_type == rel_type) {
                if (*out_count >= capacity) {
                    capacity *= 2;
                    result = (graph_edge_id_t *)realloc(result, sizeof(graph_edge_id_t) * capacity);
                }
                result[*out_count] = e->eid;
                (*out_count)++;
            }
        }
    }

    return result;
}

graph_edge_id_t *graph_edge_store_get_in_edges(GraphEdgeStore *store,
                                               graph_vertex_id_t dst,
                                               graph_label_id_t rel_type,
                                               size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 64;
    graph_edge_id_t *result = (graph_edge_id_t *)malloc(sizeof(graph_edge_id_t) * capacity);

    GraphEdgeProp *e, *tmp;
    HASH_ITER(hh, store->edges, e, tmp) {
        if (e->dst == dst) {
            if (rel_type == 0 || e->rel_type == rel_type) {
                if (*out_count >= capacity) {
                    capacity *= 2;
                    result = (graph_edge_id_t *)realloc(result, sizeof(graph_edge_id_t) * capacity);
                }
                result[*out_count] = e->eid;
                (*out_count)++;
            }
        }
    }

    return result;
}

uint64_t graph_edge_store_count(const GraphEdgeStore *store)
{
    return HASH_COUNT(store->edges);
}

/* ========================================================================
 * 标签系统实现
 * ======================================================================== */

GraphLabelMgr *graph_label_mgr_create(void *mem_pool)
{
    GraphLabelMgr *mgr = (GraphLabelMgr *)malloc(sizeof(GraphLabelMgr));
    if (!mgr) return NULL;

    mgr->labels_by_name = NULL;
    mgr->labels_by_id = NULL;
    mgr->num_labels = 0;
    mgr->next_label_id = 1;
    mgr->mem_pool = mem_pool;
    return mgr;
}

void graph_label_mgr_destroy(GraphLabelMgr *mgr)
{
    if (!mgr) return;

    GraphLabelInfo *l, *tmp;
    HASH_ITER(hh, mgr->labels_by_name, l, tmp) {
        HASH_DEL(mgr->labels_by_name, l);
        free(l);
    }
    free(mgr->labels_by_id);
    free(mgr);
}

graph_label_id_t graph_label_mgr_create_vertex_label(GraphLabelMgr *mgr, const char *name)
{
    GraphLabelInfo *existing;
    HASH_FIND(hh, mgr->labels_by_name, name, strlen(name), existing);
    if (existing) return (graph_label_id_t)-1;

    GraphLabelInfo *label = (GraphLabelInfo *)malloc(sizeof(GraphLabelInfo));
    label->id = mgr->next_label_id++;
    strncpy(label->name, name, sizeof(label->name) - 1);
    label->name[sizeof(label->name) - 1] = '\0';
    label->is_vertex_label = true;
    label->vertex_count = 0;
    label->edge_count = 0;

    HASH_ADD(hh, mgr->labels_by_name, name, strlen(name), label);

    if (mgr->num_labels == 0) {
        mgr->labels_by_id = (GraphLabelInfo **)malloc(sizeof(GraphLabelInfo *) * 16);
    }
    mgr->labels_by_id = (GraphLabelInfo **)realloc(mgr->labels_by_id,
        sizeof(GraphLabelInfo *) * (mgr->next_label_id + 1));
    mgr->labels_by_id[label->id] = label;
    mgr->num_labels++;

    return label->id;
}

graph_label_id_t graph_label_mgr_create_edge_label(GraphLabelMgr *mgr, const char *name)
{
    GraphLabelInfo *existing;
    HASH_FIND(hh, mgr->labels_by_name, name, strlen(name), existing);
    if (existing) return (graph_label_id_t)-1;

    GraphLabelInfo *label = (GraphLabelInfo *)malloc(sizeof(GraphLabelInfo));
    label->id = mgr->next_label_id++;
    strncpy(label->name, name, sizeof(label->name) - 1);
    label->name[sizeof(label->name) - 1] = '\0';
    label->is_vertex_label = false;
    label->vertex_count = 0;
    label->edge_count = 0;

    HASH_ADD(hh, mgr->labels_by_name, name, strlen(name), label);

    mgr->labels_by_id = (GraphLabelInfo **)realloc(mgr->labels_by_id,
        sizeof(GraphLabelInfo *) * (mgr->next_label_id + 1));
    mgr->labels_by_id[label->id] = label;
    mgr->num_labels++;

    return label->id;
}

graph_label_id_t graph_label_mgr_get_or_create_vertex_label(GraphLabelMgr *mgr, const char *name)
{
    GraphLabelInfo *existing;
    HASH_FIND(hh, mgr->labels_by_name, name, strlen(name), existing);
    if (existing) {
        if (!existing->is_vertex_label) return (graph_label_id_t)-1;
        return existing->id;
    }
    return graph_label_mgr_create_vertex_label(mgr, name);
}

graph_label_id_t graph_label_mgr_get_or_create_edge_label(GraphLabelMgr *mgr, const char *name)
{
    GraphLabelInfo *existing;
    HASH_FIND(hh, mgr->labels_by_name, name, strlen(name), existing);
    if (existing) {
        if (existing->is_vertex_label) return (graph_label_id_t)-1;
        return existing->id;
    }
    return graph_label_mgr_create_edge_label(mgr, name);
}

GraphLabelInfo *graph_label_mgr_get_by_name(GraphLabelMgr *mgr, const char *name)
{
    GraphLabelInfo *label;
    HASH_FIND(hh, mgr->labels_by_name, name, strlen(name), label);
    return label;
}

GraphLabelInfo *graph_label_mgr_get_by_id(GraphLabelMgr *mgr, graph_label_id_t id)
{
    if (id < mgr->next_label_id) {
        return mgr->labels_by_id[id];
    }
    return NULL;
}

graph_label_id_t *graph_label_mgr_list_vertex_labels(GraphLabelMgr *mgr, size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 16;
    graph_label_id_t *result = (graph_label_id_t *)malloc(sizeof(graph_label_id_t) * capacity);

    GraphLabelInfo *l, *tmp;
    HASH_ITER(hh, mgr->labels_by_name, l, tmp) {
        if (l->is_vertex_label) {
            if (*out_count >= capacity) {
                capacity *= 2;
                result = (graph_label_id_t *)realloc(result, sizeof(graph_label_id_t) * capacity);
            }
            result[*out_count] = l->id;
            (*out_count)++;
        }
    }

    return result;
}

graph_label_id_t *graph_label_mgr_list_edge_labels(GraphLabelMgr *mgr, size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 16;
    graph_label_id_t *result = (graph_label_id_t *)malloc(sizeof(graph_label_id_t) * capacity);

    GraphLabelInfo *l, *tmp;
    HASH_ITER(hh, mgr->labels_by_name, l, tmp) {
        if (!l->is_vertex_label) {
            if (*out_count >= capacity) {
                capacity *= 2;
                result = (graph_label_id_t *)realloc(result, sizeof(graph_label_id_t) * capacity);
            }
            result[*out_count] = l->id;
            (*out_count)++;
        }
    }

    return result;
}

void graph_label_mgr_inc_vertex_count(GraphLabelMgr *mgr, graph_label_id_t label)
{
    GraphLabelInfo *l = graph_label_mgr_get_by_id(mgr, label);
    if (l) l->vertex_count++;
}

void graph_label_mgr_dec_vertex_count(GraphLabelMgr *mgr, graph_label_id_t label)
{
    GraphLabelInfo *l = graph_label_mgr_get_by_id(mgr, label);
    if (l && l->vertex_count > 0) l->vertex_count--;
}

void graph_label_mgr_inc_edge_count(GraphLabelMgr *mgr, graph_label_id_t label)
{
    GraphLabelInfo *l = graph_label_mgr_get_by_id(mgr, label);
    if (l) l->edge_count++;
}

void graph_label_mgr_dec_edge_count(GraphLabelMgr *mgr, graph_label_id_t label)
{
    GraphLabelInfo *l = graph_label_mgr_get_by_id(mgr, label);
    if (l && l->edge_count > 0) l->edge_count--;
}

/* ========================================================================
 * Schema 管理实现
 * ======================================================================== */

GraphSchemaMgr *graph_schema_mgr_create(void *mem_pool)
{
    GraphSchemaMgr *mgr = (GraphSchemaMgr *)malloc(sizeof(GraphSchemaMgr));
    if (!mgr) return NULL;

    mgr->vertex_schemas = NULL;
    mgr->num_vertex_schemas = 0;
    mgr->edge_schemas = NULL;
    mgr->num_edge_schemas = 0;
    mgr->mem_pool = mem_pool;
    return mgr;
}

void graph_schema_mgr_destroy(GraphSchemaMgr *mgr)
{
    if (!mgr) return;

    for (size_t i = 0; i < mgr->num_vertex_schemas; i++) {
        for (size_t j = 0; j < mgr->vertex_schemas[i].num_props; j++) {
            free(mgr->vertex_schemas[i].props[j].name);
        }
        free(mgr->vertex_schemas[i].props);
    }
    free(mgr->vertex_schemas);

    for (size_t i = 0; i < mgr->num_edge_schemas; i++) {
        for (size_t j = 0; j < mgr->edge_schemas[i].num_props; j++) {
            free(mgr->edge_schemas[i].props[j].name);
        }
        free(mgr->edge_schemas[i].props);
    }
    free(mgr->edge_schemas);

    free(mgr);
}

int graph_schema_mgr_add_vertex_schema(GraphSchemaMgr *mgr, const GraphSchema *schema)
{
    mgr->vertex_schemas = (GraphSchema *)realloc(mgr->vertex_schemas,
        sizeof(GraphSchema) * (mgr->num_vertex_schemas + 1));
    GraphSchema *s = &mgr->vertex_schemas[mgr->num_vertex_schemas];

    strncpy(s->label_name, schema->label_name, sizeof(s->label_name) - 1);
    s->is_vertex_schema = true;
    s->num_props = schema->num_props;

    if (schema->num_props > 0) {
        s->props = (GraphSchemaProp *)malloc(sizeof(GraphSchemaProp) * schema->num_props);
        for (size_t i = 0; i < schema->num_props; i++) {
            s->props[i].name = strdup(schema->props[i].name);
            s->props[i].type = schema->props[i].type;
            s->props[i].constraints = schema->props[i].constraints;
        }
    } else {
        s->props = NULL;
    }

    mgr->num_vertex_schemas++;
    return 0;
}

int graph_schema_mgr_add_edge_schema(GraphSchemaMgr *mgr, const GraphSchema *schema)
{
    mgr->edge_schemas = (GraphSchema *)realloc(mgr->edge_schemas,
        sizeof(GraphSchema) * (mgr->num_edge_schemas + 1));
    GraphSchema *s = &mgr->edge_schemas[mgr->num_edge_schemas];

    strncpy(s->label_name, schema->label_name, sizeof(s->label_name) - 1);
    s->is_vertex_schema = false;
    s->num_props = schema->num_props;

    if (schema->num_props > 0) {
        s->props = (GraphSchemaProp *)malloc(sizeof(GraphSchemaProp) * schema->num_props);
        for (size_t i = 0; i < schema->num_props; i++) {
            s->props[i].name = strdup(schema->props[i].name);
            s->props[i].type = schema->props[i].type;
            s->props[i].constraints = schema->props[i].constraints;
        }
    } else {
        s->props = NULL;
    }

    mgr->num_edge_schemas++;
    return 0;
}

GraphSchema *graph_schema_mgr_get_vertex_schema(GraphSchemaMgr *mgr, const char *label_name)
{
    for (size_t i = 0; i < mgr->num_vertex_schemas; i++) {
        if (strcmp(mgr->vertex_schemas[i].label_name, label_name) == 0) {
            return &mgr->vertex_schemas[i];
        }
    }
    return NULL;
}

GraphSchema *graph_schema_mgr_get_edge_schema(GraphSchemaMgr *mgr, const char *rel_type_name)
{
    for (size_t i = 0; i < mgr->num_edge_schemas; i++) {
        if (strcmp(mgr->edge_schemas[i].label_name, rel_type_name) == 0) {
            return &mgr->edge_schemas[i];
        }
    }
    return NULL;
}

static int validate_prop_type(const GraphPropValue *val, GraphPropType expected)
{
    if (val->type != expected) return -1;
    return 0;
}

int graph_schema_validate_vertex(GraphSchemaMgr *mgr,
                                const char *label_name,
                                const GraphProperty *props,
                                size_t num_props,
                                char *error_buf,
                                size_t error_buf_size)
{
    GraphSchema *schema = graph_schema_mgr_get_vertex_schema(mgr, label_name);
    if (!schema) return 0;

    for (size_t si = 0; si < schema->num_props; si++) {
        const GraphSchemaProp *sp = &schema->props[si];

        if (sp->constraints & GRAPH_CONSTRAINT_REQUIRED) {
            bool found = false;
            for (size_t pi = 0; pi < num_props; pi++) {
                if (strcmp(props[pi].name, sp->name) == 0) {
                    found = true;
                    if (validate_prop_type(&props[pi].value, sp->type) != 0) {
                        snprintf(error_buf, error_buf_size,
                            "属性 '%s' 类型错误，期望 %s",
                            sp->name, graph_prop_type_name(sp->type));
                        return -1;
                    }
                    break;
                }
            }
            if (!found) {
                snprintf(error_buf, error_buf_size,
                    "缺少必需属性 '%s'", sp->name);
                return -1;
            }
        }
    }

    return 0;
}

int graph_schema_validate_edge(GraphSchemaMgr *mgr,
                              const char *rel_type_name,
                              const GraphProperty *props,
                              size_t num_props,
                              char *error_buf,
                              size_t error_buf_size)
{
    GraphSchema *schema = graph_schema_mgr_get_edge_schema(mgr, rel_type_name);
    if (!schema) return 0;

    for (size_t si = 0; si < schema->num_props; si++) {
        const GraphSchemaProp *sp = &schema->props[si];

        if (sp->constraints & GRAPH_CONSTRAINT_REQUIRED) {
            bool found = false;
            for (size_t pi = 0; pi < num_props; pi++) {
                if (strcmp(props[pi].name, sp->name) == 0) {
                    found = true;
                    if (validate_prop_type(&props[pi].value, sp->type) != 0) {
                        snprintf(error_buf, error_buf_size,
                            "属性 '%s' 类型错误，期望 %s",
                            sp->name, graph_prop_type_name(sp->type));
                        return -1;
                    }
                    break;
                }
            }
            if (!found) {
                snprintf(error_buf, error_buf_size,
                    "缺少必需属性 '%s'", sp->name);
                return -1;
            }
        }
    }

    return 0;
}

/* ========================================================================
 * 属性值操作工具实现
 * ======================================================================== */

int graph_prop_value_copy_to(const GraphPropValue *src, GraphPropValue *dst)
{
    dst->type = src->type;
    switch (src->type) {
        case GRAPH_PROP_BOOL:
            dst->val.bool_val = src->val.bool_val;
            break;
        case GRAPH_PROP_INT:
        case GRAPH_PROP_INT64:
            dst->val.int_val = src->val.int_val;
            break;
        case GRAPH_PROP_DOUBLE:
            dst->val.double_val = src->val.double_val;
            break;
        case GRAPH_PROP_STRING:
            dst->val.string_val = src->val.string_val ? strdup(src->val.string_val) : NULL;
            break;
        case GRAPH_PROP_TIMESTAMP:
            dst->val.timestamp_val = src->val.timestamp_val;
            break;
        case GRAPH_PROP_NULL:
        default:
            break;
    }
    return 0;
}

GraphPropValue *graph_prop_value_copy(const GraphPropValue *val, void *mem_pool)
{
    (void)mem_pool;
    GraphPropValue *copy = (GraphPropValue *)malloc(sizeof(GraphPropValue));
    if (copy) {
        graph_prop_value_copy_to(val, copy);
    }
    return copy;
}

void graph_prop_value_free(GraphPropValue *val)
{
    if (!val) return;

    switch (val->type) {
        case GRAPH_PROP_STRING:
            free(val->val.string_val);
            break;
        case GRAPH_PROP_BYTES:
            free(val->val.bytes_val.data);
            break;
        case GRAPH_PROP_LIST:
            if (val->val.list_val) {
                for (size_t i = 0; i < val->val.list_val->count; i++) {
                    graph_prop_value_free(&val->val.list_val->values[i]);
                }
                free(val->val.list_val->values);
                free(val->val.list_val);
            }
            break;
        case GRAPH_PROP_MAP:
            if (val->val.map_val) {
                for (size_t i = 0; i < val->val.map_val->count; i++) {
                    free(val->val.map_val->keys[i]);
                    graph_prop_value_free(&val->val.map_val->values[i]);
                }
                free(val->val.map_val->keys);
                free(val->val.map_val->values);
                free(val->val.map_val);
            }
            break;
        default:
            break;
    }
    val->type = GRAPH_PROP_NULL;
}

int graph_prop_value_compare(const GraphPropValue *a, const GraphPropValue *b)
{
    if (a->type != b->type) {
        return (a->type > b->type) ? 1 : -1;
    }

    switch (a->type) {
        case GRAPH_PROP_NULL:
            return 0;
        case GRAPH_PROP_BOOL:
            return (a->val.bool_val == b->val.bool_val) ? 0 :
                   (a->val.bool_val > b->val.bool_val) ? 1 : -1;
        case GRAPH_PROP_INT:
        case GRAPH_PROP_INT64:
        case GRAPH_PROP_TIMESTAMP:
            return (a->val.int_val == b->val.int_val) ? 0 :
                   (a->val.int_val > b->val.int_val) ? 1 : -1;
        case GRAPH_PROP_DOUBLE:
            return (a->val.double_val == b->val.double_val) ? 0 :
                   (a->val.double_val > b->val.double_val) ? 1 : -1;
        case GRAPH_PROP_STRING:
            if (!a->val.string_val && !b->val.string_val) return 0;
            if (!a->val.string_val) return -1;
            if (!b->val.string_val) return 1;
            return strcmp(a->val.string_val, b->val.string_val);
        default:
            return 0;
    }
}

char *graph_prop_value_to_string(const GraphPropValue *val)
{
    char buf[256];

    switch (val->type) {
        case GRAPH_PROP_NULL:
            return strdup("null");
        case GRAPH_PROP_BOOL:
            snprintf(buf, sizeof(buf), "%s", val->val.bool_val ? "true" : "false");
            break;
        case GRAPH_PROP_INT:
            snprintf(buf, sizeof(buf), "%d", (int)val->val.int_val);
            break;
        case GRAPH_PROP_INT64:
            snprintf(buf, sizeof(buf), "%lld", (long long)val->val.int_val);
            break;
        case GRAPH_PROP_DOUBLE:
            snprintf(buf, sizeof(buf), "%.6f", val->val.double_val);
            break;
        case GRAPH_PROP_STRING:
            return strdup(val->val.string_val);
        case GRAPH_PROP_TIMESTAMP:
            {
                time_t t = val->val.timestamp_val;
                struct tm *tm_info = localtime(&t);
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
            }
            break;
        default:
            snprintf(buf, sizeof(buf), "<unknown>");
            break;
    }

    return strdup(buf);
}

const char *graph_prop_type_name(GraphPropType type)
{
    switch (type) {
        case GRAPH_PROP_NULL: return "null";
        case GRAPH_PROP_BOOL: return "bool";
        case GRAPH_PROP_INT: return "int";
        case GRAPH_PROP_INT64: return "int64";
        case GRAPH_PROP_DOUBLE: return "double";
        case GRAPH_PROP_STRING: return "string";
        case GRAPH_PROP_BYTES: return "bytes";
        case GRAPH_PROP_TIMESTAMP: return "timestamp";
        case GRAPH_PROP_VECTOR: return "vector";
        case GRAPH_PROP_LIST: return "list";
        case GRAPH_PROP_MAP: return "map";
        default: return "unknown";
    }
}

GraphPropType graph_parse_prop_type(const char *name)
{
    if (strcmp(name, "null") == 0) return GRAPH_PROP_NULL;
    if (strcmp(name, "bool") == 0) return GRAPH_PROP_BOOL;
    if (strcmp(name, "int") == 0) return GRAPH_PROP_INT;
    if (strcmp(name, "int64") == 0) return GRAPH_PROP_INT64;
    if (strcmp(name, "double") == 0) return GRAPH_PROP_DOUBLE;
    if (strcmp(name, "string") == 0) return GRAPH_PROP_STRING;
    if (strcmp(name, "bytes") == 0) return GRAPH_PROP_BYTES;
    if (strcmp(name, "timestamp") == 0) return GRAPH_PROP_TIMESTAMP;
    if (strcmp(name, "vector") == 0) return GRAPH_PROP_VECTOR;
    if (strcmp(name, "list") == 0) return GRAPH_PROP_LIST;
    if (strcmp(name, "map") == 0) return GRAPH_PROP_MAP;
    return GRAPH_PROP_NULL;
}
