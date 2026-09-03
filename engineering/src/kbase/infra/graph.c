#include "infra/graph.h"
#include <stdlib.h>
#include <string.h>

infra_graph_t* infra_graph_create(int capacity) {
    infra_graph_t* g = (infra_graph_t*)calloc(1, sizeof(*g));
    if (!g) return NULL;
    if (capacity <= 0) capacity = 16;
    g->capacity = capacity;
    g->nodes = (infra_graph_node_t**)calloc(capacity, sizeof(infra_graph_node_t*));
    g->exec_order = (int*)calloc(capacity, sizeof(int));
    if (!g->nodes || !g->exec_order) {
        free(g->nodes); free(g->exec_order); free(g); return NULL;
    }
    return g;
}

void infra_graph_free(infra_graph_t* g) {
    if (!g) return;
    for (int i = 0; i < g->num_nodes; i++) {
        infra_graph_node_t* n = g->nodes[i];
        if (n) { free(n->inputs); free(n->params); free(n->name); free(n); }
    }
    free(g->nodes);
    free(g->exec_order);
    free(g);
}

infra_graph_node_t* infra_graph_add_node(infra_graph_t* g,
    const char* op_name, infra_tensor_t* output, const char* name)
{
    if (!g || !op_name) return NULL;
    const infra_op_t* op = infra_op_find(op_name);
    if (!op) return NULL;
    if (g->num_nodes >= g->capacity) {
        int newcap = g->capacity * 2;
        infra_graph_node_t** nn = (infra_graph_node_t**)realloc(g->nodes, newcap * sizeof(*nn));
        if (!nn) return NULL;
        g->nodes = nn;
        int* no = (int*)realloc(g->exec_order, newcap * sizeof(int));
        if (!no) return NULL;
        g->exec_order = no;
        g->capacity = newcap;
    }
    infra_graph_node_t* n = (infra_graph_node_t*)calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->op = op;
    n->output = output;
    if (name) {
        size_t len = strlen(name);
        n->name = (char*)malloc(len + 1);
        memcpy(n->name, name, len + 1);
    }
    g->nodes[g->num_nodes++] = n;
    return n;
}

infra_status_t infra_graph_add_edge(infra_graph_t* g,
    infra_graph_node_t* from, infra_graph_node_t* to, int input_idx)
{
    if (!g || !to) return INFRA_ERR_PARAM;
    (void)from;  /* from 在 build 时通过 nodes 数组查找 */
    int new_n = to->num_inputs + 1;
    if (input_idx >= new_n) new_n = input_idx + 1;
    infra_graph_node_t** ni = (infra_graph_node_t**)realloc(to->inputs, new_n * sizeof(*ni));
    if (!ni) return INFRA_ERR_MEMORY;
    to->inputs = ni;
    to->inputs[input_idx] = from;
    if (to->num_inputs < new_n) to->num_inputs = new_n;
    return INFRA_OK;
}

/* Kahn 拓扑排序 */
infra_status_t infra_graph_build(infra_graph_t* g) {
    if (!g) return INFRA_ERR_PARAM;
    int n = g->num_nodes;
    int* indeg = (int*)calloc(n, sizeof(int));
    int** out = (int**)calloc(n, sizeof(int*));
    int* out_n = (int*)calloc(n, sizeof(int));

    for (int i = 0; i < n; i++) {
        infra_graph_node_t* node = g->nodes[i];
        for (int k = 0; k < node->num_inputs; k++) {
            int from = -1;
            for (int j = 0; j < n; j++) {
                if (g->nodes[j] == node->inputs[k]) { from = j; break; }
            }
            if (from < 0) { free(indeg); free(out); free(out_n); return INFRA_ERR_GRAPH; }
            indeg[i]++;
            int* tmp = (int*)realloc(out[from], (out_n[from] + 1) * sizeof(int));
            if (!tmp) {
                free(indeg);
                for (int j = 0; j < n; j++) free(out[j]);
                free(out); free(out_n);
                return INFRA_ERR_MEMORY;
            }
            out[from] = tmp;
            out[from][out_n[from]++] = i;
        }
    }

    int* q = (int*)malloc(n * sizeof(int));
    int qh = 0, qt = 0;
    for (int i = 0; i < n; i++) if (indeg[i] == 0) q[qt++] = i;
    int cnt = 0;
    while (qh < qt) {
        int u = q[qh++];
        g->exec_order[cnt++] = u;
        for (int k = 0; k < out_n[u]; k++) {
            int v = out[u][k];
            if (--indeg[v] == 0) q[qt++] = v;
        }
    }
    free(indeg);
    for (int i = 0; i < n; i++) free(out[i]);
    free(out); free(out_n); free(q);

    if (cnt != n) return INFRA_ERR_GRAPH;
    g->num_exec_nodes = cnt;
    g->built = 1;
    return INFRA_OK;
}

infra_status_t infra_graph_execute(infra_graph_t* g) {
    if (!g || !g->built) return INFRA_ERR_PARAM;
    for (int i = 0; i < g->num_exec_nodes; i++) {
        infra_graph_node_t* n = g->nodes[g->exec_order[i]];
        if (!n->op || !n->op->func) return INFRA_ERR_OP;
        /* 构造 input tensor 数组 */
        infra_tensor_t** in_tensors = (infra_tensor_t**)malloc(n->num_inputs * sizeof(infra_tensor_t*));
        if (!in_tensors) return INFRA_ERR_MEMORY;
        for (int k = 0; k < n->num_inputs; k++) {
            in_tensors[k] = n->inputs[k] ? n->inputs[k]->output : NULL;
        }
        infra_status_t s = n->op->func(in_tensors, n->num_inputs, &n->output, 1, n->params);
        free(in_tensors);
        if (s != INFRA_OK) return s;
    }
    return INFRA_OK;
}