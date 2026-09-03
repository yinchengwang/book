/**
 * @file mmdb_xquery.h
 * @brief 跨 collection 关联查询 API（P3-T3.1）
 *
 * 当前仅实现 text → vector 方向：先用 FTS5 在源（text）集合中检索
 * 出候选 ids（上限 max_source_candidates），再在目标（vector）集合中
 * 对每个候选 id 读取向量并计算与 query_vector 的 L2 距离，按距离升序
 * 截取 top_k。
 *
 * 实现约束：
 *   - 当前 SDK 的 HNSW 路径要求 filter == ""，因此本接口采用 flat 路径
 *     遍历候选（候选数已限制 ≤ max_source_candidates，默认 100）。
 *   - HNSW 支持 filter 是更大工程，留作后续独立 Task。
 *
 * 重要限制：
 *   当前实现内部 id 缓冲为 256 字节（见 xquery.c::xq_cand_t.id 与
 *   XQUERY_MAX_ID_LEN）。若 source (text) 集合返回的候选 id 超过
 *   256 字节，本接口立即返回 MMDB_ERR_INVALID，不再静默跳过：
 *     - 通过 stderr 输出 ERROR 级诊断（含 collection 名 + 越界 id 长度）
 *     - 调用方收到 MMDB_ERR_INVALID 后应修正源数据（缩短 id）或拆分集合
 *
 * 设计取舍：将静默跳过升级为硬错误，是因为静默跳过会丢失数据，调用方
 * 难以察觉；改为错误后，行为是显式的、可测试的、可被运维告警捕获的。
 */
#ifndef SDK_MMDB_XQUERY_H
#define SDK_MMDB_XQUERY_H

#include <stddef.h>
#include "sdk/mmdb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mmdb_collection_t* source;            /* text 集合（被查询侧） */
    const char*        text_query;        /* FTS5 检索关键词 */
    mmdb_collection_t* target;            /* vector 集合（被计算侧） */
    const float*       query_vector;      /* 用于在 target 上做 KNN */
    size_t             dim;               /* query_vector 维度 */
    size_t             top_k;             /* 返回结果数，<=0 视为 10 */
    size_t             max_source_candidates; /* 源集合候选上限，<=0 视为 100 */
} mmdb_xquery_text_to_vector_t;

/**
 * @brief 跨集合查询：从 source (text) FTS5 检索 ids，再在 target (vector)
 *        上对每个候选 id 计算 L2 距离，按距离升序取 top_k
 *
 * @param xq  查询参数（不可为 NULL；source/target/text_query/query_vector
 *            任意为 NULL/空 → MMDB_ERR_INVALID）
 * @param out 结果输出（不可为 NULL；调用方负责 mmdb_result_free）
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法；MMDB_ERR_NOMEM 内存不足；
 *         MMDB_ERR_IO SQLite 错误
 */
int mmdb_xquery_text_to_vector(
    const mmdb_xquery_text_to_vector_t* xq,
    mmdb_result_t* out);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_XQUERY_H */
