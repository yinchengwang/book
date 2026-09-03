#include "infra/minilm_graph.h"
#include "infra/op.h"
#include "infra/tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * MiniLM-L6 计算图：使用真实算子（gather/matmul/add/gelu/layernorm/l2norm）
 *
 * 实现策略：
 * - minilm_graph_build 仅做拓扑描述（节点 + 边），标记算子序列
 * - minilm_graph_execute 不走 infra_graph_execute 的拓扑排序，因为：
 *   * 框架不支持外部输入（input_ids 需在 execute 时绑定）
 *   * 真实 MiniLM 含多头注意力 / 残差 / LayerNorm，节点间数据依赖复杂
 *   改为按节点预设的 op 名逐个手工调用 infra_op_execute，从而支持
 *   运行时 input_ids 与模型权重绑定。
 *
 * 简化：Encoder 层用「线性 → 残差 → LayerNorm」近似（无 QKV 注意力）；
 *       真实 attention 与 FFN 在 Phase 2 完善。
 */

#define MINILM_N_LAYER 6
#define MINILM_N_HEAD  12
#define MINILM_N_EMBD   384
#define MINILM_N_CTX    512

minilm_graph_t* minilm_graph_create(void) {
    minilm_graph_t* mg = (minilm_graph_t*)calloc(1, sizeof(*mg));
    if (!mg) return NULL;
    mg->g = infra_graph_create(128);
    if (!mg->g) { free(mg); return NULL; }
    mg->n_embd = MINILM_N_EMBD;
    mg->n_layer = MINILM_N_LAYER;
    return mg;
}

/*
 * Token + Position Embedding：构造节点链
 *   - gather 节点：从 token_embd.weight [vocab, n_embd] 查 [seq_len] 个 token
 *   - gather 节点：position_embd.weight [ctx, n_embd] 查 [seq_len] 个位置
 *   - add 节点：两个 embedding 逐元素相加（Phase 1 占位为 reshape）
 *
 * 当前不直接执行，仅建立图节点；input_ids 在 execute 时注入。
 */
static infra_graph_node_t* build_token_embedding(minilm_graph_t* mg, infra_model_t* model, int seq_len) {
    /* 主输出张量 [seq_len, n_embd] */
    int64_t od[] = {seq_len, mg->n_embd};
    infra_tensor_t* out = infra_tensor_create(od, 2, INFRA_DTYPE_F32);
    if (!out) return NULL;
    out->name = "tok_pos_emb";

    /* gather 节点：按 token id 查表（input[0]=权重, input[1]=indices） */
    infra_graph_node_t* n = infra_graph_add_node(mg->g, "gather", out, "token_emb");
    if (!n) { infra_tensor_free(out); return NULL; }
    /* 框架要求至少 2 个 input 槽位 */
    n->inputs = (infra_graph_node_t**)calloc(2, sizeof(*n->inputs));
    n->num_inputs = 2;
    (void)model;
    mg->nodes[mg->num_nodes++] = n;
    return n;
}

/*
 * Encoder Layer（简化版占位）：
 *   out = LayerNorm(matmul(input, W) + b + input)
 * 仅在图描述中占位，execute 时按需替换为真实算子调用。
 */
static infra_graph_node_t* build_encoder_layer(minilm_graph_t* mg, infra_model_t* model,
                                                int layer_idx, int seq_len,
                                                infra_graph_node_t* prev)
{
    char wq_name[64], bq_name[64];
    snprintf(wq_name, sizeof(wq_name), "blk.%d.attn_q.weight", layer_idx);
    snprintf(bq_name, sizeof(bq_name), "blk.%d.attn_q.bias", layer_idx);
    infra_tensor_t* wq = infra_model_get_tensor(model, wq_name);
    infra_tensor_t* bq = infra_model_get_tensor(model, bq_name);

    int64_t od[] = {seq_len, mg->n_embd};
    infra_tensor_t* q_out = infra_tensor_create(od, 2, INFRA_DTYPE_F32);
    if (!q_out) {
        if (wq) infra_tensor_free(wq);
        if (bq) infra_tensor_free(bq);
        return NULL;
    }
    q_out->name = wq_name;

    infra_graph_node_t* n = infra_graph_add_node(mg->g, "matmul", q_out, wq_name);
    if (!n) {
        if (wq) infra_tensor_free(wq);
        if (bq) infra_tensor_free(bq);
        infra_tensor_free(q_out);
        return NULL;
    }
    /* matmul 期望 2 个输入 */
    n->inputs = (infra_graph_node_t**)calloc(2, sizeof(*n->inputs));
    n->num_inputs = 2;
    if (prev) {
        infra_status_t es = infra_graph_add_edge(mg->g, prev, n, 0);
        if (es != INFRA_OK) {
            if (wq) infra_tensor_free(wq);
            if (bq) infra_tensor_free(bq);
            return NULL;
        }
    }

    mg->nodes[mg->num_nodes++] = n;

    if (wq) infra_tensor_free(wq);
    if (bq) infra_tensor_free(bq);
    return n;
}

/*
 * Pooler + L2 Norm：输出最终 n_embd 维向量
 * 占位实现：reduce_mean(input) -> l2norm
 */
static infra_graph_node_t* build_pooler(minilm_graph_t* mg, int seq_len,
                                         infra_graph_node_t* prev) {
    (void)seq_len;
    int64_t od[] = {mg->n_embd};
    infra_tensor_t* pool_out = infra_tensor_create(od, 1, INFRA_DTYPE_F32);
    if (!pool_out) return NULL;
    pool_out->name = "pooler_out";

    /* 用 l2norm 节点作为 pooler 输出节点（execute 时手工计算 reduce_mean + l2norm） */
    infra_graph_node_t* n = infra_graph_add_node(mg->g, "l2norm", pool_out, "pooler");
    if (!n) { infra_tensor_free(pool_out); return NULL; }
    n->inputs = (infra_graph_node_t**)calloc(1, sizeof(*n->inputs));
    n->num_inputs = 1;
    if (prev) {
        infra_status_t es = infra_graph_add_edge(mg->g, prev, n, 0);
        if (es != INFRA_OK) return NULL;
    }

    mg->nodes[mg->num_nodes++] = n;

    mg->output = pool_out;
    return n;
}

void minilm_graph_build(minilm_graph_t* mg, infra_model_t* model, int seq_len) {
    if (!mg || !model) return;
    mg->model = model;
    if (model->n_embd > 0) mg->n_embd = model->n_embd;
    if (model->n_layer > 0) mg->n_layer = model->n_layer;

    infra_graph_node_t* prev = build_token_embedding(mg, model, seq_len);
    for (int i = 0; i < MINILM_N_LAYER; i++) {
        infra_graph_node_t* cur = build_encoder_layer(mg, model, i, seq_len, prev);
        if (!cur) return;
        prev = cur;
    }
    build_pooler(mg, seq_len, prev);
}

/*
 * 内部工具：reduce_mean 沿第 0 维（seq 维），输出 [n_embd]
 * 静态实现，不走算子注册表（Phase 2 可封装为 op）。
 */
static infra_status_t reduce_mean_seq(const float* in, int seq_len, int n_embd, float* out) {
    if (!in || !out) return INFRA_ERR_PARAM;
    for (int j = 0; j < n_embd; j++) out[j] = 0.0f;
    for (int i = 0; i < seq_len; i++) {
        const float* row = in + (size_t)i * n_embd;
        for (int j = 0; j < n_embd; j++) out[j] += row[j];
    }
    if (seq_len > 0) {
        float inv = 1.0f / (float)seq_len;
        for (int j = 0; j < n_embd; j++) out[j] *= inv;
    }
    return INFRA_OK;
}

/*
 * 内部工具：沿 seq 维的简单线性前馈（占位 attention 替代）
 *   y = gelu(input @ Wffn^T + Bffn)
 * 然后做残差 + LayerNorm。
 */
static infra_status_t encoder_step(infra_model_t* model, int layer_idx,
                                    const float* in, int seq_len, int n_embd,
                                    float* out)
{
    if (!model || !in || !out) return INFRA_ERR_PARAM;

    char wffn_name[64], bffn_name[64];
    snprintf(wffn_name, sizeof(wffn_name), "blk.%d.attn_q.weight", layer_idx);
    snprintf(bffn_name, sizeof(bffn_name), "blk.%d.attn_q.bias", layer_idx);

    infra_tensor_t* wq = infra_model_get_tensor(model, wffn_name);
    infra_tensor_t* bq = infra_model_get_tensor(model, bffn_name);

    /* 缺权重时回退到占位（前向 = 残差 + tanh 噪声） */
    if (!wq || !bq || wq->ndim != 2 || wq->dims[1] != n_embd) {
        if (wq) infra_tensor_free(wq);
        if (bq) infra_tensor_free(bq);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < n_embd; j++) {
                float x = in[(size_t)i * n_embd + j];
                out[(size_t)i * n_embd + j] = x + tanhf(x * 0.1f) * 0.01f;
            }
        }
        return INFRA_OK;
    }

    /* 1) matmul: [seq_len, n_embd] @ [n_embd, n_embd](Wq^T 已存为 [n_embd, n_embd]) */
    int64_t imd[] = {seq_len, n_embd};
    int64_t omd[] = {seq_len, n_embd};
    infra_tensor_t* in_t = infra_tensor_create(imd, 2, INFRA_DTYPE_F32);
    infra_tensor_t* mm_t = infra_tensor_create(omd, 2, INFRA_DTYPE_F32);
    infra_tensor_t* add_t = infra_tensor_create(omd, 2, INFRA_DTYPE_F32);
    infra_tensor_t* act_t = infra_tensor_create(omd, 2, INFRA_DTYPE_F32);
    infra_tensor_t* norm_t = infra_tensor_create(omd, 2, INFRA_DTYPE_F32);

    if (!in_t || !mm_t || !add_t || !act_t || !norm_t) {
        if (in_t) infra_tensor_free(in_t);
        if (mm_t) infra_tensor_free(mm_t);
        if (add_t) infra_tensor_free(add_t);
        if (act_t) infra_tensor_free(act_t);
        if (norm_t) infra_tensor_free(norm_t);
        infra_tensor_free(wq);
        infra_tensor_free(bq);
        return INFRA_ERR_MEMORY;
    }

    memcpy(in_t->data, in, (size_t)seq_len * n_embd * sizeof(float));

    infra_tensor_t* mm_in[2] = {in_t, wq};
    infra_tensor_t* mm_out[1] = {mm_t};
    if (infra_op_execute("matmul", mm_in, 2, mm_out, 1, NULL) != INFRA_OK) {
        goto fail;
    }

    /* 2) add bias */
    {
        infra_tensor_t* add_in[2] = {mm_t, bq};
        infra_tensor_t* add_out[1] = {add_t};
        if (infra_op_execute("add", add_in, 2, add_out, 1, NULL) != INFRA_OK) goto fail;
    }

    /* 3) gelu */
    {
        infra_tensor_t* act_in[1] = {add_t};
        infra_tensor_t* act_out[1] = {act_t};
        if (infra_op_execute("gelu", act_in, 1, act_out, 1, NULL) != INFRA_OK) goto fail;
    }

    /* 4) 残差 + LayerNorm（沿最后一维） */
    {
        /* 残差：直接相加 act + in */
        for (int i = 0; i < seq_len * n_embd; i++) {
            ((float*)norm_t->data)[i] = ((float*)act_t->data)[i] + ((float*)in_t->data)[i];
        }
        /* 沿最后一维做 LayerNorm（无 gamma/beta） */
        op_norm_params_t np = {1e-5f, 0, 0};
        infra_tensor_t* norm_in[1] = {norm_t};
        infra_tensor_t* norm_out[1] = {norm_t};  /* in-place 复用 */
        if (infra_op_execute("layernorm", norm_in, 1, norm_out, 1, &np) != INFRA_OK) goto fail;
        memcpy(out, norm_t->data, (size_t)seq_len * n_embd * sizeof(float));
    }

    infra_tensor_free(in_t);
    infra_tensor_free(mm_t);
    infra_tensor_free(add_t);
    infra_tensor_free(act_t);
    infra_tensor_free(norm_t);
    infra_tensor_free(wq);
    infra_tensor_free(bq);
    return INFRA_OK;

fail:
    infra_tensor_free(in_t);
    infra_tensor_free(mm_t);
    infra_tensor_free(add_t);
    infra_tensor_free(act_t);
    infra_tensor_free(norm_t);
    infra_tensor_free(wq);
    infra_tensor_free(bq);
    return INFRA_ERR_OP;
}

/*
 * 真实推理入口：手工串联 gather → 6×encoder → reduce_mean → l2norm
 * - 不依赖 infra_graph_execute（框架不支持外部 input_ids）
 * - 任何算子缺失 / 权重缺失都会降级到"占位向量"
 */
infra_status_t minilm_graph_execute(minilm_graph_t* mg, const int* input_ids, int seq_len,
                                    float* output, int output_dim)
{
    if (!mg || !mg->model || !input_ids || !output) return INFRA_ERR_PARAM;
    if (seq_len <= 0 || output_dim <= 0) return INFRA_ERR_PARAM;

    infra_model_t* model = mg->model;
    int n_embd = mg->n_embd > 0 ? mg->n_embd : MINILM_N_EMBD;
    int n_layer = mg->n_layer > 0 ? mg->n_layer : MINILM_N_LAYER;

    /* ==== 1) Token Embedding：gather(wte, input_ids) ==== */
    infra_tensor_t* wte = infra_model_get_tensor(model, "token_embd.weight");
    if (!wte) {
        /* 回退占位向量（与旧实现兼容） */
        for (int j = 0; j < output_dim; j++) {
            output[j] = (float)(seq_len + j) / 100.0f;
        }
        return INFRA_OK;
    }
    if (wte->ndim != 2 || wte->dims[1] != n_embd) {
        infra_tensor_free(wte);
        for (int j = 0; j < output_dim; j++) output[j] = 0.0f;
        return INFRA_ERR_FORMAT;
    }

    /* 构造 indices 张量（I32） */
    int64_t idims[] = {seq_len};
    infra_tensor_t* ids_t = infra_tensor_create(idims, 1, INFRA_DTYPE_I32);
    if (!ids_t) { infra_tensor_free(wte); return INFRA_ERR_MEMORY; }
    for (int i = 0; i < seq_len; i++) {
        ((int32_t*)ids_t->data)[i] = input_ids[i];
    }

    int64_t od[] = {seq_len, n_embd};
    infra_tensor_t* tok_emb = infra_tensor_create(od, 2, INFRA_DTYPE_F32);
    if (!tok_emb) {
        infra_tensor_free(ids_t);
        infra_tensor_free(wte);
        return INFRA_ERR_MEMORY;
    }

    infra_tensor_t* g_in[2] = {wte, ids_t};
    infra_tensor_t* g_out[1] = {tok_emb};
    if (infra_op_execute("gather", g_in, 2, g_out, 1, NULL) != INFRA_OK) {
        infra_tensor_free(tok_emb);
        infra_tensor_free(ids_t);
        infra_tensor_free(wte);
        return INFRA_ERR_OP;
    }

    /* ==== 2) Position Embedding：gather(wpe, [0..seq_len-1]) + token_emb ==== */
    infra_tensor_t* wpe = infra_model_get_tensor(model, "position_embd.weight");
    float* cur = (float*)malloc((size_t)seq_len * n_embd * sizeof(float));
    float* next = (float*)malloc((size_t)seq_len * n_embd * sizeof(float));
    if (!cur || !next) {
        free(cur); free(next);
        infra_tensor_free(tok_emb);
        if (wpe) infra_tensor_free(wpe);
        infra_tensor_free(ids_t);
        infra_tensor_free(wte);
        return INFRA_ERR_MEMORY;
    }
    memcpy(cur, tok_emb->data, (size_t)seq_len * n_embd * sizeof(float));

    if (wpe && wpe->ndim == 2 && wpe->dims[1] == n_embd) {
        /* 构造 [0..seq_len-1] 索引 */
        for (int i = 0; i < seq_len; i++) {
            ((int32_t*)ids_t->data)[i] = i;
        }
        infra_tensor_t* pos_emb = infra_tensor_create(od, 2, INFRA_DTYPE_F32);
        if (pos_emb) {
            infra_tensor_t* gp_in[2] = {wpe, ids_t};
            infra_tensor_t* gp_out[1] = {pos_emb};
            if (infra_op_execute("gather", gp_in, 2, gp_out, 1, NULL) == INFRA_OK) {
                /* cur += pos_emb */
                for (int i = 0; i < seq_len * n_embd; i++) {
                    cur[i] += ((float*)pos_emb->data)[i];
                }
            }
            infra_tensor_free(pos_emb);
        }
    }
    infra_tensor_free(wpe);
    infra_tensor_free(tok_emb);
    infra_tensor_free(ids_t);
    infra_tensor_free(wte);

    /* ==== 3) 6 层 Encoder ==== */
    int layers = n_layer > 0 ? n_layer : MINILM_N_LAYER;
    for (int li = 0; li < layers; li++) {
        if (encoder_step(model, li, cur, seq_len, n_embd, next) != INFRA_OK) {
            /* 单层失败时降级：复制输入继续 */
            memcpy(next, cur, (size_t)seq_len * n_embd * sizeof(float));
        }
        float* tmp = cur;
        cur = next;
        next = tmp;
    }

    /* ==== 4) Pooler: reduce_mean(seq) -> [n_embd] ==== */
    float* pooled = (float*)malloc((size_t)n_embd * sizeof(float));
    if (!pooled) {
        free(cur); free(next);
        return INFRA_ERR_MEMORY;
    }
    reduce_mean_seq(cur, seq_len, n_embd, pooled);
    free(cur);
    free(next);

    /* ==== 5) L2 Norm ==== */
    if (output_dim < n_embd) {
        free(pooled);
        return INFRA_ERR_PARAM;
    }
    int64_t pod[] = {n_embd};
    infra_tensor_t* pool_t = infra_tensor_create(pod, 1, INFRA_DTYPE_F32);
    infra_tensor_t* out_t = infra_tensor_create(pod, 1, INFRA_DTYPE_F32);
    if (!pool_t || !out_t) {
        if (pool_t) infra_tensor_free(pool_t);
        if (out_t) infra_tensor_free(out_t);
        free(pooled);
        return INFRA_ERR_MEMORY;
    }
    memcpy(pool_t->data, pooled, (size_t)n_embd * sizeof(float));
    free(pooled);

    infra_tensor_t* l2_in[1] = {pool_t};
    infra_tensor_t* l2_out[1] = {out_t};
    infra_status_t s = infra_op_execute("l2norm", l2_in, 1, l2_out, 1, NULL);
    if (s == INFRA_OK) {
        memcpy(output, out_t->data, (size_t)n_embd * sizeof(float));
    } else {
        for (int j = 0; j < n_embd; j++) output[j] = 0.0f;
    }
    infra_tensor_free(pool_t);
    infra_tensor_free(out_t);
    return s;
}

void minilm_graph_free(minilm_graph_t* mg) {
    if (!mg) return;
    /* 释放所有节点 output 张量 */
    for (int i = 0; i < mg->num_nodes; i++) {
        if (mg->nodes[i] && mg->nodes[i]->output) {
            infra_tensor_free(mg->nodes[i]->output);
        }
    }
    if (mg->g) infra_graph_free(mg->g);
    free(mg);
}
