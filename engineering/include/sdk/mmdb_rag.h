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
#include "sdk/mmdb_embedding.h"  /* P4-T4.1：为 mmdb_rag_query_t.embedding 提供类型 */

#ifdef __cplusplus
extern "C" {
#endif

/* Rerank 配置：NONE = 不重排（hybrid 默认顺序）；BM25 = 用词频打
 * 分与 RRF score 加权混合（占位实现，真实交叉编码器 rerank 留待后续
 * plan）。注意：BM25 是简化的词频版本，非 FTS5 真实 BM25，便于零依
 * 赖单测验证。 */
typedef enum {
    MMDB_RAG_RERANK_NONE  = 0,
    MMDB_RAG_RERANK_BM25  = 1,
} mmdb_rag_rerank_kind_t;

/* rerank 混合权重配置：final_score = (1 - weight) * orig - weight * bm25
 * 其中 orig 为 hybrid RRF 得分（取负号），bm25 为 query_text 在 text
 * 字段上的简化词频分数。weight=0 等价于 NONE，weight=1 完全由 BM25
 * 主导，默认 0.5。 */
typedef struct {
    mmdb_rag_rerank_kind_t kind;
    double                 weight;
} mmdb_rag_rerank_config_t;

/* RAG 查询参数。
 * ABI 注意：rerank 字段于 T4.2 追加到结构体末尾，旧调用方
 * （T4.1 之前的代码）按字段名或 memset 后访问不受影响；C++ 聚合初始
 * 化（按顺序列出所有字段）仍兼容。P4-T4.1 又于末尾追加 embedding 字
 * 段；同样不影响既有调用方。 */
typedef struct {
    const char* query_text;        /* 必填：用户原始查询文本 */
    const char* filter_json;       /* 可选：metadata 过滤（NULL 跳过） */
    size_t      top_k;              /* 候选数（0 → 默认 5） */
    size_t      max_context_chars;  /* context 字符串最大字节数（0 → 默认 8000） */
    mmdb_rag_rerank_config_t rerank; /* rerank 配置（默认 NONE，等价于不重排） */
    mmdb_embedding_t* embedding;   /* P4-T4.1 新增：per-call embedding 覆盖；
                                    *   NULL 时 fallback 到 collection-level（通过
                                    *   mmdb_rag_set_embedding 注入），仍为 NULL 则
                                    *   使用 HASH 默认 embedding。 */
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

/**
 * @brief 为 collection 注入默认 embedding（P4-T4.1 新增）
 *
 * 用于在 collection 级别持久化 embedding 选择，避免每次 mmdb_rag_retrieve
 * 都显式传入 q.embedding。优先级链：q.embedding > coll->embedding >
 * fallback HASH。
 *
 * 所有权约定：coll 仅持有指针（不复制、不接管生命周期）；调用方需自行
 * 保证 embedding 在 collection 关闭之前有效。可重复调用以更换；旧指针
 * 不会被自动 drop（调用方负责前一次的释放）。
 *
 * @param coll      目标 collection
 * @param embedding embedding 句柄（可为 NULL：等价于清除当前注入）
 * @return MMDB_OK；MMDB_ERR_INVALID（coll == NULL）
 */
int mmdb_rag_set_embedding(mmdb_collection_t* coll,
                           mmdb_embedding_t* embedding);

#ifdef __cplusplus
}
#endif

#endif  /* MMDB_RAG_H */
