#ifndef KBASE_INFRA_OP_H
#define KBASE_INFRA_OP_H

#include "infra/types.h"
#include "infra/tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 算子函数签名 */
typedef infra_status_t (*infra_op_func_t)(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params);

/* 算子描述符 */
typedef struct {
    const char* name;              /* 算子名 */
    infra_op_func_t func;          /* 实现函数 */
    int min_inputs;
    int max_inputs;
    int min_outputs;
    int max_outputs;
    const char* description;
} infra_op_t;

/* 注册/查找 */
void infra_op_register(const infra_op_t* op);
const infra_op_t* infra_op_find(const char* name);
void infra_op_register_all(void);  /* 注册所有内置算子 */

/* 算子执行便捷接口 */
infra_status_t infra_op_execute(
    const char* op_name,
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params);

/* 通用算子参数 */
typedef struct {
    int transpose_a;
    int transpose_b;
} op_matmul_params_t;

typedef struct {
    int axis1;
    int axis2;
} op_transpose_params_t;

typedef struct {
    float eps;
    int has_gamma;
    int has_beta;
} op_norm_params_t;

typedef struct {
    int num_heads;   /* Head 数量（MiniLM-L6: 12） */
    float scale;     /* 缩放因子（默认 1/sqrt(head_dim)） */
} op_attention_params_t;

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_OP_H */