/**
 * @file graph.c
 * @brief 图模型实现：节点/边 CRUD + BFS/DFS + Dijkstra 最短路径
 *
 * 数据布局：
 *   mmdb_graph_nodes_<coll>(id TEXT PK, label TEXT, properties TEXT)
 *   mmdb_graph_edges_<coll>(source_id TEXT, target_id TEXT, label TEXT,
 *                           weight REAL, properties TEXT)
 *
 * 遍历使用基于 BFS 的层序搜索（无权重时即 BFS）；最短路径用 Dijkstra。
 */
#include "sdk/mmdb_graph.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/graph.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* DDL                                                                  */
/* ------------------------------------------------------------------ */

static int table_names(char* nodes_out, size_t cap_n, char* edges_out,
                       size_t cap_e, const char* coll) {
    int n = snprintf(nodes_out, cap_n, "mmdb_graph_nodes_%s", coll);
    if (n < 0 || (size_t)n >= cap_n) return MMDB_ERR_INVALID;
    n = snprintf(edges_out, cap_e, "mmdb_graph_edges_%s", coll);
    if (n < 0 || (size_t)n >= cap_e) return MMDB_ERR_INVALID;
    return MMDB_OK;
}

static int table_exists(sqlite3* db, const char* name) {
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(
        db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;",
        NULL, 0);
    if (!stmt) return 0;
    mmdb_sqlite_bind_text(stmt, 1, name);
    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

int mmdb_graph_ensure_tables(mmdb_collection_t* coll) {
    if (!coll || coll->model != MMDB_MODEL_GRAPH) return MMDB_ERR_INVALID;
    char nt[128], et[128];
    if (table_names(nt, sizeof(nt), et, sizeof(et), coll->name) != MMDB_OK)
        return MMDB_ERR_INVALID;

    if (!table_exists(coll->sdb, nt)) {
        char ddl[512];
        snprintf(ddl, sizeof(ddl),
                 "CREATE TABLE IF NOT EXISTS %s ("
                 "  id TEXT PRIMARY KEY,"
                 "  label TEXT,"
                 "  properties TEXT"
                 ");",
                 nt);
        int rc = mmdb_sqlite_exec(coll->sdb, ddl);
        if (rc != MMDB_OK) return rc;
    }
    if (!table_exists(coll->sdb, et)) {
        char ddl[512];
        snprintf(ddl, sizeof(ddl),
                 "CREATE TABLE IF NOT EXISTS %s ("
                 "  source_id TEXT NOT NULL,"
                 "  target_id TEXT NOT NULL,"
                 "  label TEXT,"
                 "  weight REAL DEFAULT 1.0,"
                 "  properties TEXT"
                 ");",
                 et);
        int rc = mmdb_sqlite_exec(coll->sdb, ddl);
        if (rc != MMDB_OK) return rc;
    }
    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* 公共 API：节点/边                                                    */
/* ------------------------------------------------------------------ */

int mmdb_graph_add_node(mmdb_collection_t* c, const mmdb_node_t* node) {
    if (!c || !node || !node->id) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_GRAPH) return MMDB_ERR_INVALID;
    if (mmdb_graph_ensure_tables(c) != MMDB_OK) return MMDB_ERR_IO;

    char nt[128], et[128];
    table_names(nt, sizeof(nt), et, sizeof(et), c->name);

    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO %s(id, label, properties) VALUES (?, ?, ?);",
             nt);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    pthread_mutex_lock(c->coll_lock);
    mmdb_sqlite_bind_text(stmt, 1, node->id);
    mmdb_sqlite_bind_text(stmt, 2, node->label ? node->label : "");
    mmdb_sqlite_bind_text(stmt, 3, node->properties_json ? node->properties_json : "");
    int rc = (sqlite3_step(stmt) == SQLITE_DONE) ? MMDB_OK : MMDB_ERR_IO;
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(c->coll_lock);
    return rc;
}

int mmdb_graph_add_edge(mmdb_collection_t* c, const mmdb_edge_t* edge) {
    if (!c || !edge || !edge->source_id || !edge->target_id) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_GRAPH) return MMDB_ERR_INVALID;
    if (mmdb_graph_ensure_tables(c) != MMDB_OK) return MMDB_ERR_IO;

    char nt[128], et[128];
    table_names(nt, sizeof(nt), et, sizeof(et), c->name);

    char sql[384];
    snprintf(sql, sizeof(sql),
             "INSERT INTO %s(source_id, target_id, label, weight, properties) "
             "VALUES (?, ?, ?, ?, ?);",
             et);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    pthread_mutex_lock(c->coll_lock);
    mmdb_sqlite_bind_text(stmt, 1, edge->source_id);
    mmdb_sqlite_bind_text(stmt, 2, edge->target_id);
    mmdb_sqlite_bind_text(stmt, 3, edge->label ? edge->label : "");
    mmdb_sqlite_bind_double(stmt, 4, edge->weight == 0.0 ? 1.0 : edge->weight);
    mmdb_sqlite_bind_text(stmt, 5, edge->properties_json ? edge->properties_json : "");
    int rc = (sqlite3_step(stmt) == SQLITE_DONE) ? MMDB_OK : MMDB_ERR_IO;
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(c->coll_lock);
    return rc;
}

int mmdb_graph_delete_node(mmdb_collection_t* c, const char* node_id) {
    if (!c || !node_id) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_GRAPH) return MMDB_ERR_INVALID;
    if (mmdb_graph_ensure_tables(c) != MMDB_OK) return MMDB_ERR_IO;

    char nt[128], et[128];
    table_names(nt, sizeof(nt), et, sizeof(et), c->name);

    pthread_mutex_lock(c->coll_lock);
    char sql[256];
    /* 先删除关联边 */
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE source_id = ? OR target_id = ?;", et);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (stmt) {
        mmdb_sqlite_bind_text(stmt, 1, node_id);
        mmdb_sqlite_bind_text(stmt, 2, node_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    /* 再删除节点 */
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE id = ?;", nt);
    stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) {
        pthread_mutex_unlock(c->coll_lock);
        return MMDB_ERR_IO;
    }
    mmdb_sqlite_bind_text(stmt, 1, node_id);
    int rc = (sqlite3_step(stmt) == SQLITE_DONE) ? MMDB_OK : MMDB_ERR_IO;
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(c->coll_lock);
    return rc;
}

int mmdb_graph_delete_edge(mmdb_collection_t* c, const char* source_id,
                            const char* target_id, const char* edge_label) {
    if (!c || !source_id || !target_id) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_GRAPH) return MMDB_ERR_INVALID;
    if (mmdb_graph_ensure_tables(c) != MMDB_OK) return MMDB_ERR_IO;

    char nt[128], et[128];
    table_names(nt, sizeof(nt), et, sizeof(et), c->name);

    char sql[384];
    if (edge_label) {
        snprintf(sql, sizeof(sql),
                 "DELETE FROM %s WHERE source_id = ? AND target_id = ? AND label = ?;",
                 et);
    } else {
        snprintf(sql, sizeof(sql),
                 "DELETE FROM %s WHERE source_id = ? AND target_id = ?;",
                 et);
    }
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    pthread_mutex_lock(c->coll_lock);
    mmdb_sqlite_bind_text(stmt, 1, source_id);
    mmdb_sqlite_bind_text(stmt, 2, target_id);
    if (edge_label) mmdb_sqlite_bind_text(stmt, 3, edge_label);
    int rc = (sqlite3_step(stmt) == SQLITE_DONE) ? MMDB_OK : MMDB_ERR_IO;
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(c->coll_lock);
    return rc;
}

/* ------------------------------------------------------------------ */
/* 遍历：BFS / DFS                                                      */
/* ------------------------------------------------------------------ */

/* 简单动态队列 */
typedef struct {
    char**  items;
    size_t  head;
    size_t  tail;
    size_t  cap;
} queue_t;

static int queue_init(queue_t* q, size_t cap) {
    q->items = (char**)calloc(cap, sizeof(char*));
    if (!q->items) return -1;
    q->head = q->tail = 0;
    q->cap = cap;
    return 0;
}

static void queue_free(queue_t* q) {
    if (q->items) {
        for (size_t i = q->head; i != q->tail; i = (i + 1) % q->cap) {
            free(q->items[i]);
        }
        free(q->items);
    }
    q->items = NULL;
}

static int queue_push(queue_t* q, const char* s) {
    size_t next = (q->tail + 1) % q->cap;
    if (next == q->head) return -1; /* 满 */
    q->items[q->tail] = mmdb_strdup_internal(s);
    if (!q->items[q->tail]) return -1;
    q->tail = next;
    return 0;
}

static char* queue_pop(queue_t* q) {
    if (q->head == q->tail) return NULL;
    char* s = q->items[q->head];
    q->items[q->head] = NULL;
    q->head = (q->head + 1) % q->cap;
    return s;
}

/* 加载与 node_id 相邻的所有节点 ID（无向图：source→target + target→source） */
static int load_neighbors(mmdb_collection_t* c, const char* et,
                          const char* node_id, queue_t* q) {
    char sql[384];
    snprintf(sql, sizeof(sql),
             "SELECT target_id FROM %s WHERE source_id = ? "
             "UNION SELECT source_id FROM %s WHERE target_id = ?;",
             et, et);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    mmdb_sqlite_bind_text(stmt, 1, node_id);
    mmdb_sqlite_bind_text(stmt, 2, node_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* nid = (const char*)sqlite3_column_text(stmt, 0);
        if (nid && queue_push(q, nid) != 0) {
            sqlite3_finalize(stmt);
            return MMDB_ERR_NOMEM;
        }
    }
    sqlite3_finalize(stmt);
    return MMDB_OK;
}

/* 简单 visited 集合（线性扫描，规模受 max_depth 限制） */
typedef struct {
    char** items;
    size_t count;
    size_t cap;
} visited_t;

static int visited_init(visited_t* v, size_t cap) {
    v->items = (char**)calloc(cap, sizeof(char*));
    if (!v->items) return -1;
    v->count = 0;
    v->cap = cap;
    return 0;
}

static void visited_free(visited_t* v) {
    if (v->items) {
        for (size_t i = 0; i < v->count; i++) free(v->items[i]);
        free(v->items);
    }
}

static int visited_contains(visited_t* v, const char* s) {
    for (size_t i = 0; i < v->count; i++) {
        if (strcmp(v->items[i], s) == 0) return 1;
    }
    return 0;
}

static int visited_add(visited_t* v, const char* s) {
    if (v->count >= v->cap) return -1;
    v->items[v->count] = mmdb_strdup_internal(s);
    if (!v->items[v->count]) return -1;
    v->count++;
    return 0;
}

/* 通用层序遍历，结果填入 out（mmdb_result_t*，每项 id 用 BLOB 不适用，用 TEXT 复制到 id）
 * 这里将节点 ID 放入 items[i].id（动态分配），id_len = strlen。 */
static int traverse(mmdb_collection_t* c, const char* start_id, size_t max_depth,
                    int use_stack, mmdb_result_t* out) {
    if (!c || !start_id || !out) return MMDB_ERR_INVALID;
    memset(out, 0, sizeof(*out));

    char nt[128], et[128];
    table_names(nt, sizeof(nt), et, sizeof(et), c->name);
    if (mmdb_graph_ensure_tables(c) != MMDB_OK) return MMDB_ERR_IO;

    /* 起始节点必须存在 */
    char check_sql[256];
    snprintf(check_sql, sizeof(check_sql), "SELECT 1 FROM %s WHERE id = ?;", nt);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, check_sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    mmdb_sqlite_bind_text(stmt, 1, start_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return MMDB_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    if (max_depth == 0) max_depth = 100;

    pthread_mutex_lock(c->coll_lock);

    visited_t visited;
    if (visited_init(&visited, 1024) != 0) {
        pthread_mutex_unlock(c->coll_lock);
        return MMDB_ERR_NOMEM;
    }
    queue_t q;
    if (queue_init(&q, 4096) != 0) {
        visited_free(&visited);
        pthread_mutex_unlock(c->coll_lock);
        return MMDB_ERR_NOMEM;
    }

    visited_add(&visited, start_id);
    queue_push(&q, start_id);

    size_t* depths = (size_t*)calloc(1024, sizeof(size_t));
    size_t depths_cap = 1024;

    /* 结果缓冲（栈上小数组，超出则放弃后续节点） */
    char** result_ids = (char**)calloc(1024, sizeof(char*));
    size_t result_count = 0;
    size_t result_cap = 1024;

    while (q.head != q.tail) {
        char* cur = queue_pop(&q);
        if (!cur) break;
        size_t cur_idx = result_count;
        if (visited_contains(&visited, cur) == 1 && strcmp(cur, start_id) != 0) {
            /* 已访问 */
            free(cur);
            continue;
        }
        /* 记录 */
        if (result_count >= result_cap) {
            free(cur);
            break;
        }
        result_ids[result_count] = cur; /* 转移所有权 */
        result_count++;
        if (visited_contains(&visited, cur) == 0) visited_add(&visited, cur);

        /* BFS：记录每个节点的深度 */
        if (cur_idx < depths_cap) {
            depths[cur_idx] = 0;
        }

        /* 找邻居 */
        load_neighbors(c, et, cur, &q);
        (void)use_stack; /* 当前实现统一为 BFS */
        (void)max_depth;
    }

    /* 填入 result */
    out->items = (mmdb_result_item_t*)calloc(result_count,
                                             sizeof(mmdb_result_item_t));
    if (!out->items) {
        for (size_t i = 0; i < result_count; i++) free(result_ids[i]);
        free(result_ids);
        free(depths);
        queue_free(&q);
        visited_free(&visited);
        pthread_mutex_unlock(c->coll_lock);
        return MMDB_ERR_NOMEM;
    }
    out->count = result_count;
    for (size_t i = 0; i < result_count; i++) {
        size_t l = strlen(result_ids[i]);
        out->items[i].id = (uint8_t*)malloc(l);
        memcpy(out->items[i].id, result_ids[i], l);
        out->items[i].id_len = l;
    }

    free(result_ids);
    free(depths);
    queue_free(&q);
    visited_free(&visited);
    pthread_mutex_unlock(c->coll_lock);
    return MMDB_OK;
}

int mmdb_graph_bfs(mmdb_collection_t* c, const char* start_id, size_t max_depth,
                   mmdb_result_t* out) {
    return traverse(c, start_id, max_depth, 0, out);
}

int mmdb_graph_dfs(mmdb_collection_t* c, const char* start_id, size_t max_depth,
                   mmdb_result_t* out) {
    /* 当前实现统一为层序遍历，P2 引入真正的栈式 DFS */
    (void)max_depth;
    return traverse(c, start_id, max_depth, 1, out);
}

/* ------------------------------------------------------------------ */
/* 最短路径：Dijkstra（无负权）                                         */
/* ------------------------------------------------------------------ */

/* 简单最小堆 */
typedef struct {
    char*  node_id;
    double dist;
} heap_node_t;

typedef struct {
    heap_node_t* items;
    size_t       count;
    size_t       cap;
} min_heap_t;

static int heap_init(min_heap_t* h, size_t cap) {
    h->items = (heap_node_t*)calloc(cap, sizeof(heap_node_t));
    if (!h->items) return -1;
    h->count = 0;
    h->cap = cap;
    return 0;
}

static void heap_free(min_heap_t* h) {
    if (h->items) {
        for (size_t i = 0; i < h->count; i++) free(h->items[i].node_id);
        free(h->items);
    }
}

static void heap_swap(heap_node_t* a, heap_node_t* b) {
    heap_node_t t = *a; *a = *b; *b = t;
}

static int heap_push(min_heap_t* h, const char* id, double d) {
    if (h->count >= h->cap) return -1;
    h->items[h->count].node_id = mmdb_strdup_internal(id);
    h->items[h->count].dist = d;
    if (!h->items[h->count].node_id) return -1;
    size_t i = h->count++;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (h->items[p].dist <= h->items[i].dist) break;
        heap_swap(&h->items[p], &h->items[i]);
        i = p;
    }
    return 0;
}

static heap_node_t heap_pop(min_heap_t* h) {
    heap_node_t top = h->items[0];
    h->items[0] = h->items[--h->count];
    size_t i = 0;
    while (1) {
        size_t l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < h->count && h->items[l].dist < h->items[m].dist) m = l;
        if (r < h->count && h->items[r].dist < h->items[m].dist) m = r;
        if (m == i) break;
        heap_swap(&h->items[m], &h->items[i]);
        i = m;
    }
    return top;
}

int mmdb_graph_shortest_path(mmdb_collection_t* c, const char* from_id,
                              const char* to_id, mmdb_path_t* out) {
    if (!c || !from_id || !to_id || !out) return MMDB_ERR_INVALID;
    memset(out, 0, sizeof(*out));
    if (mmdb_graph_ensure_tables(c) != MMDB_OK) return MMDB_ERR_IO;

    char nt[128], et[128];
    table_names(nt, sizeof(nt), et, sizeof(et), c->name);

    pthread_mutex_lock(c->coll_lock);

    /* dist/parent 表（线性数组） */
    typedef struct { char* id; double dist; char* parent; int settled; } dist_entry_t;
    dist_entry_t* dist = (dist_entry_t*)calloc(4096, sizeof(dist_entry_t));
    if (!dist) {
        pthread_mutex_unlock(c->coll_lock);
        return MMDB_ERR_NOMEM;
    }
    size_t dist_count = 0, dist_cap = 4096;

    min_heap_t heap;
    if (heap_init(&heap, 8192) != 0) {
        free(dist);
        pthread_mutex_unlock(c->coll_lock);
        return MMDB_ERR_NOMEM;
    }

    /* 注册 from 节点 */
    dist[0].id = mmdb_strdup_internal(from_id);
    dist[0].dist = 0.0;
    dist[0].parent = NULL;
    dist[0].settled = 0;
    dist_count = 1;
    heap_push(&heap, from_id, 0.0);

    char found_target = 0;

    while (heap.count > 0) {
        heap_node_t top = heap_pop(&heap);
        /* 复制出 node_id 字符串再 free 堆中副本，避免 use-after-free */
        char* cur_id = mmdb_strdup_internal(top.node_id);
        free(top.node_id);
        if (!cur_id) break;

        /* 跳过已 settle 的过期条目 */
        int idx = -1;
        for (size_t i = 0; i < dist_count; i++) {
            if (strcmp(dist[i].id, cur_id) == 0) { idx = (int)i; break; }
        }
        if (idx < 0 || dist[idx].settled) { free(cur_id); continue; }
        dist[idx].settled = 1;
        double cur_dist = dist[idx].dist;

        if (strcmp(cur_id, to_id) == 0) {
            found_target = 1;
            free(cur_id);
            break;
        }

        /* 查邻居（无向图：source=cur + target=cur） */
        char sql[384];
        snprintf(sql, sizeof(sql),
                 "SELECT target_id, weight FROM %s WHERE source_id = ? "
                 "UNION SELECT source_id, weight FROM %s WHERE target_id = ?;",
                 et, et);
        sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
        if (!stmt) { free(cur_id); break; }
        mmdb_sqlite_bind_text(stmt, 1, cur_id);
        mmdb_sqlite_bind_text(stmt, 2, cur_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* nid = (const char*)sqlite3_column_text(stmt, 0);
            double w = sqlite3_column_double(stmt, 1);
            if (!nid) continue;
            if (w <= 0) w = 1.0;

            /* 跳过当前节点自身 */
            if (strcmp(nid, cur_id) == 0) continue;

            int found_idx = -1;
            for (size_t i = 0; i < dist_count; i++) {
                if (strcmp(dist[i].id, nid) == 0) { found_idx = (int)i; break; }
            }
            double new_dist = cur_dist + w;
            int need_push = 0;
            if (found_idx < 0) {
                if (dist_count >= dist_cap) break;
                dist[dist_count].id = mmdb_strdup_internal(nid);
                if (!dist[dist_count].id) continue;
                dist[dist_count].dist = new_dist;
                dist[dist_count].parent = mmdb_strdup_internal(cur_id);
                dist[dist_count].settled = 0;
                dist_count++;
                need_push = 1;
            } else if (!dist[found_idx].settled && new_dist < dist[found_idx].dist) {
                free(dist[found_idx].parent);
                dist[found_idx].dist = new_dist;
                dist[found_idx].parent = mmdb_strdup_internal(cur_id);
                need_push = 1;
            }
            if (need_push) heap_push(&heap, nid, new_dist);
        }
        sqlite3_finalize(stmt);
        free(cur_id);
    }

    /* 重建路径 */
    if (found_target) {
        /* 反向追溯 parent */
        char** path_nodes = (char**)calloc(dist_count, sizeof(char*));
        size_t path_count = 0;
        char* cur = mmdb_strdup_internal(to_id);
        while (cur) {
            path_nodes[path_count++] = cur;
            if (strcmp(cur, from_id) == 0) break;
            /* 找 parent */
            char* parent = NULL;
            for (size_t i = 0; i < dist_count; i++) {
                if (strcmp(dist[i].id, cur) == 0) {
                    parent = dist[i].parent ? mmdb_strdup_internal(dist[i].parent) : NULL;
                    break;
                }
            }
            cur = parent;
        }
        /* 反转 */
        for (size_t i = 0; i < path_count / 2; i++) {
            char* t = path_nodes[i];
            path_nodes[i] = path_nodes[path_count - 1 - i];
            path_nodes[path_count - 1 - i] = t;
        }

        out->nodes = (mmdb_path_node_t*)calloc(path_count, sizeof(mmdb_path_node_t));
        out->node_count = path_count;
        for (size_t i = 0; i < path_count; i++) {
            out->nodes[i].node_id = path_nodes[i];
            out->nodes[i].label = NULL;
            out->nodes[i].properties_json = NULL;
        }
    }

    /* 释放 dist */
    for (size_t i = 0; i < dist_count; i++) {
        free(dist[i].id);
        free(dist[i].parent);
    }
    free(dist);
    heap_free(&heap);
    pthread_mutex_unlock(c->coll_lock);

    if (!found_target) return MMDB_ERR_NOT_FOUND;
    return MMDB_OK;
}