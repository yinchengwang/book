/**
 * @file mmdb_hybrid.h
 * @brief 混合检索公共 API（主+次通道 → RRF 重排）
 *
 * 三通道中任意一个为可选：未填的通道不参与融合。
 * 至少需要填一个通道（vector 或 text_query），否则返回 MMDB_ERR_INVALID。
 *
 * 双通道路由策略（P4-T4.2，关闭 CI-1 单通道路由限制）：
 *   1. 主通道由 c->model 决定：
 *      - MMDB_MODEL_VECTOR + q->vector           → mmdb_vectors_search
 *      - MMDB_MODEL_TEXT   + q->text_query       → mmdb_text_search
 *      - 其它模型                                → 主通道关闭
 *   2. 次通道：若 q 同时提供非主通道字段（且满足基础条件）则尝试启用：
 *      - MMDB_MODEL_VECTOR + q->text_query（strlen > 0）→ 次通道 text
 *      - MMDB_MODEL_TEXT   + q->vector（dim > 0）       → 次通道 vector
 *      - 其它模型                                → 不启用次通道
 *   3. 主+次通道结果共享去重逻辑后统一 RRF 融合（既有 mmdb_rrf_fuse / rrf.c）
 *
 * 注：当前 SDK 架构下，mmdb_text_search 仅接受 TEXT 集合，mmdb_vectors_search
 *     仅接受 VECTOR 集合；次通道在底层 API 拒绝时会静默返回 0 候选，主通道
 *     结果不受影响。若后续 SDK 支持 VECTOR 集合内置 FTS5 或 TEXT 集合内置
 *     向量索引，本路由策略将自动激活完整双通道融合（无需修改 hybrid 层）。
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
 * @brief hybrid search：主通道 + 次通道 → RRF 重排 → top_k
 *
 * 路由策略（P4-T4.2 双通道真正融合）：
 *   主通道（由 c->model 决定）：
 *     - VECTOR 模型 + q->vector           → mmdb_vectors_search
 *     - TEXT   模型 + q->text_query       → mmdb_text_search
 *   次通道（若 q 同时提供非主通道字段则尝试，底层失败时静默忽略）：
 *     - VECTOR 模型 + q->text_query（>0） → mmdb_text_search（一般 model 不匹配）
 *     - TEXT   模型 + q->vector（dim>0）  → mmdb_vectors_search（一般 model 不匹配）
 *
 * @param c   目标 collection（必须非空；模型决定主通道）
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
