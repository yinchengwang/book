#ifndef KBASE_INFRA_MINILM_GRAPH_H
#define KBASE_INFRA_MINILM_GRAPH_H

#include "infra/types.h"
#include "infra/model.h"
#include "infra/graph.h"
#include "infra/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MiniLM-L6 默认超参数（模型元数据缺省时回退） */
#define MINILM_DEFAULT_N_LAYER 6
#define MINILM_DEFAULT_N_HEAD  12
#define MINILM_DEFAULT_N_EMBD  384
#define MINILM_DEFAULT_N_CTX   512

/* MiniLM-L6 计算图：真实算子（gather/matmul/add/gelu/layernorm/l2norm） */
typedef struct minilm_graph {
    infra_graph_t* g;
    infra_graph_node_t* nodes[64];
    int num_nodes;
    /* 输入/输出张量 */
    infra_tensor_t* input_ids;     /* [seq_len] */
    infra_tensor_t* output;        /* [n_embd] */
    /* 模型引用（用于按节点执行时按需获取权重） */
    infra_model_t* model;
    int n_embd;
    int n_layer;
} minilm_graph_t;

/* 创建一个空图对象（待 minilm_graph_build 注入算子节点） */
minilm_graph_t* minilm_graph_create(void);

/* 构建 MiniLM-L6 计算图：串联 Token+Position Embedding -> 6 层 Encoder -> Pooler -> L2Norm */
void minilm_graph_build(minilm_graph_t* mg, infra_model_t* model, int seq_len);

/* 通过预置节点逐算子执行 MiniLM 图（绕过框架拓扑排序，支持运行时外部输入） */
infra_status_t minilm_graph_execute(minilm_graph_t* mg, const int* input_ids, int seq_len,
                                    float* output, int output_dim);

/* 释放图及所有节点的输出张量 */
void minilm_graph_free(minilm_graph_t* mg);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_MINILM_GRAPH_H */
