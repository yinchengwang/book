/**
 * @file mmdb_hybrid.h
 * @brief 混合检索公共 API（向量 + FTS5 + filter → RRF 重排）
 *
 * 三通道中任意一个为可选：未填的通道不参与融合。
 * 至少需要填一个通道（vector 或 text_query），否则返回 MMDB_ERR_INVALID。
 *
 * 集合路由策略：根据入参 collection c 的模型（c->model）自动选择可用通道：
 *   - MMDB_MODEL_VECTOR: 启用向量通道（需 q->vector != NULL && q->dim > 0）
 *   - MMDB_MODEL_TEXT  : 启用文本通道（需 q->text_query != NULL）
 *   - 其他模型         : 当前不支持 hybrid
 *
 * 重要限制（P3 当前实现）：
 *   hybrid_search 当前为单通道路由策略：根据 c->model 决定启用哪个通道，
 *   不匹配的通道会被静默忽略。即 VECTOR 集合上提供 text_query 或 TEXT
 *   集合上提供 vector 都不会参与融合。这是 P3 阶段的设计妥协，
 *   真正的双通道融合留待后续 plan。
 */
#ifndef SDK_MMDB_HYBRID_H
#define SDK_MMDB_HYBRID_H

#include <stddef.h>

#include "sdk/mmdb_types.h"
#include "sdk/impl/hybrid_search.h"  /* mmdb_rrf_config_t */

#ifdef __cplusplus
extern "C" {
#endif

/* hybrid search 查询参数
 * 三通道中任意一个为可选：未填的通道不参与融合 */
typedef struct {
    const float* vector;         /* 可选：触发向量检索通道；NULL 跳过 */
    size_t dim;                  /* 向量维度；vector != NULL 时必填 */
    const char* text_query;      /* 可选：触发 FTS5 检索通道；NULL 跳过 */
    const char* filter_json;     /* 可选：metadata 过滤；NULL 表示不过滤 */
    size_t top_k;                /* 默认 10（<=0 时取默认值） */
    const mmdb_rrf_config_t* rrf;  /* RRF 配置；NULL 走默认 k=60 */
} mmdb_hybrid_query_t;

/**
 * @brief hybrid search：向量通道 + 文本通道 + filter → RRF 重排 → top_k
 *
 * 路由策略：根据 c->model 选择可用通道：
 *   - VECTOR 模型 + q->vector  → 调 mmdb_vectors_search
 *   - TEXT   模型 + q->text_query → 调 mmdb_text_search
 *   - VECTOR 模型 + q->text_query / TEXT 模型 + q->vector → 静默忽略不匹配通道
 *
 * @param c   目标 collection（必须非空；模型决定启用哪个通道）
 * @param q   查询参数（至少填一个通道；rrf=NULL 走默认 k=60）
 * @param out 输出结果（调用方负责 mmdb_result_free）
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法；MMDB_ERR_NOMEM 内存不足
 */
int mmdb_hybrid_search(
    mmdb_collection_t* c,
    const mmdb_hybrid_query_t* q,
    mmdb_result_t* out);

#ifdef __cplusplus
}
#endif

#endif  /* SDK_MMDB_HYBRID_H */
