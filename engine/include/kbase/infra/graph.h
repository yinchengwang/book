#ifndef KBASE_INFRA_GRAPH_H
#define KBASE_INFRA_GRAPH_H

#include "infra/types.h"
#include "infra/tensor.h"
#include "infra/op.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct infra_graph_node {
    const infra_op_t* op;
    infra_tensor_t* output;
    struct infra_graph_node** inputs;
    int num_inputs;
    void* params;
    size_t params_size;
    char* name;  /* owned */
} infra_graph_node_t;

typedef struct {
    infra_graph_node_t** nodes;
    int num_nodes;
    int capacity;
    int* exec_order;
    int num_exec_nodes;
    int built;
} infra_graph_t;

infra_graph_t* infra_graph_create(int capacity);
void infra_graph_free(infra_graph_t* g);
infra_graph_node_t* infra_graph_add_node(infra_graph_t* g,
    const char* op_name, infra_tensor_t* output, const char* name);
infra_status_t infra_graph_add_edge(infra_graph_t* g,
    infra_graph_node_t* from, infra_graph_node_t* to, int input_idx);
infra_status_t infra_graph_build(infra_graph_t* g);
infra_status_t infra_graph_execute(infra_graph_t* g);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_GRAPH_H */