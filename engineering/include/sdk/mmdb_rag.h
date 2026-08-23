/**
 * @file mmdb_rag.h
 * @brief RAG（Retrieval-Augmented Generation）retrieve 公共 API
 *
 * 流程：query_text → hash embedding → hybrid search（向量+FTS5+RRF）
 *      → 按候选 id 回查 SQLite 取 text 字段 → 拼接为 context 字符串
 *      → 截断到 max_context_chars
 *
 * 设计要点：
 *   - 不修改 T1.2 hybrid_search 签名/行为；text 字段由 RAG 层独立回查
 *     （hybrid layer 透传 id/distance，metadata/text 留 NULL，按需回查）
 *   - top_k=0 取默认 5；max_context_chars=0 取默认 8000
 *   - context 按 distance 升序拼接，分隔符 "\n---\n"
 *   - items.count 不超过 top_k；context_len 严格 <= max_context_chars
 */
#ifndef MMDB_RAG_H
#define MMDB_RAG_H

#include <stddef.h>

#include "sdk/mmdb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RAG 查询参数 */
typedef struct {
    const char* query_text;        /* 必填：用户原始查询文本 */
    const char* filter_json;       /* 可选：metadata 过滤（NULL 跳过） */
    size_t      top_k;              /* 候选数（0 → 默认 5） */
    size_t      max_context_chars;  /* context 字符串最大字节数（0 → 默认 8000） */
} mmdb_rag_query_t;

/* RAG 结果 */
typedef struct {
    mmdb_result_t items;   /* 候选结果（id/distance/text/metadata_json） */
    char*         context; /* 拼接好的 prompt context（含 \0 终止符） */
    size_t        context_len;  /* context 实际字节数（不含 \0） */
} mmdb_rag_result_t;

/**
 * @brief RAG 检索：embedding → hybrid search → 回查 text → 拼接 context
 *
 * @param c   目标 collection（当前支持 MMDB_MODEL_TEXT / MMDB_MODEL_VECTOR；
 *            schema.vector_dim 用于 hash embedding 维度；0 时 fallback 64）
 * @param q   查询参数（query_text 必填；其他字段可缺省走默认值）
 * @param out 输出结果（调用方负责 mmdb_rag_result_free 释放 items 与 context）
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法；MMDB_ERR_NOMEM 内存不足；
 *         MMDB_ERR_IO SQLite 错误
 */
int mmdb_rag_retrieve(
    mmdb_collection_t* c,
    const mmdb_rag_query_t* q,
    mmdb_rag_result_t* out);

/**
 * @brief 释放 mmdb_rag_result_t 内部所有堆内存
 *
 * 调用后 items.count=0 / items.items=NULL / context=NULL / context_len=0
 */
void mmdb_rag_result_free(mmdb_rag_result_t* r);

#ifdef __cplusplus
}
#endif

#endif  /* MMDB_RAG_H */
