#include "infra/op.h"
#include <string.h>
#include <stdlib.h>

#define MAX_OPS 64

static infra_op_t g_op_table[MAX_OPS];
static int g_num_ops = 0;

/* 各算子的注册入口（由各 op_*.c 文件定义） */
extern void register_op_matmul(void);
extern void register_op_add(void);
extern void register_op_mul(void);
extern void register_op_activations(void);
extern void register_op_softmax(void);
extern void register_op_norm(void);
extern void register_op_reshape(void);
extern void register_op_transpose(void);
extern void register_op_gather(void);
extern void register_op_l2norm(void);
extern void register_op_attention(void);

void infra_op_register(const infra_op_t* op) {
    if (!op || !op->name || !op->func) return;
    if (g_num_ops >= MAX_OPS) return;
    /* 检查重复 */
    for (int i = 0; i < g_num_ops; i++) {
        if (strcmp(g_op_table[i].name, op->name) == 0) return;
    }
    g_op_table[g_num_ops++] = *op;
}

const infra_op_t* infra_op_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < g_num_ops; i++) {
        if (strcmp(g_op_table[i].name, name) == 0) {
            return &g_op_table[i];
        }
    }
    return NULL;
}

void infra_op_register_all(void) {
    register_op_matmul();
    register_op_add();
    register_op_mul();
    register_op_activations();
    register_op_softmax();
    register_op_norm();
    register_op_reshape();
    register_op_transpose();
    register_op_gather();
    register_op_l2norm();
    register_op_attention();
}

infra_status_t infra_op_execute(
    const char* op_name,
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params)
{
    const infra_op_t* op = infra_op_find(op_name);
    if (!op) return INFRA_ERR_NOT_FOUND;
    if (num_inputs < op->min_inputs || num_inputs > op->max_inputs) return INFRA_ERR_PARAM;
    if (num_outputs < op->min_outputs || num_outputs > op->max_outputs) return INFRA_ERR_PARAM;
    return op->func(inputs, num_inputs, outputs, num_outputs, params);
}