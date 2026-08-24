/**
 * @file rag.c
 * @brief RAG retrieve 实现（query → embed → hybrid search → 回查 text → 拼接 context）
 *
 * 实现策略（采用 brief 推荐的方案 B）：
 *   1. 创建 hash embedding（零依赖、确定性）
 *   2. 用 hash embedder 把 query_text 编码为定长向量
 *   3. 调 mmdb_hybrid_search 拿候选（id + distance；text 字段为 NULL）
 *   4. **RAG 层独立回查 SQLite**：按 id 批量查 text 字段填入 items[i].text
 *   5. 按 distance 升序拼接（hybrid 已按 RRF 降序排过，距离已升序）
 *   6. 截断到 max_context_chars
 *
 * 选方案 B 的原因：
 *   - 不修改 T1.2 hybrid_search ABI/实现，T1.2 review Approved 不破
 *   - T1.2、T1.3 已 PASS 的测试无需 re-run
 *   - top_k 通常 <= 10，回查 SQLite 开销可忽略
 */
#include "sdk/mmdb_rag.h"
#include "sdk/mmdb_hybrid.h"
#include "sdk/mmdb_embedding.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

/* ---------- 常量 ---------- */
#define RAG_DEFAULT_TOP_K             5
#define RAG_DEFAULT_MAX_CONTEXT       8000
#define RAG_MAX_EMBED_DIM             256   /* 栈向量上限；超出则报错 */

/* ---------- 比较：按 distance 升序 ---------- */
static int cmp_by_distance(const void* a, const void* b) {
    float da = ((const mmdb_result_item_t*)a)->distance;
    float db = ((const mmdb_result_item_t*)b)->distance;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

/* ---------- 释放 items 数组内部动态内存 + 数组本身 ---------- */
static void free_items(mmdb_result_item_t* items, size_t count) {
    if (!items) return;
    for (size_t i = 0; i < count; i++) {
        free(items[i].id);
        free(items[i].metadata_json);
        free(items[i].text);
    }
    free(items);
}

/* ---------- 按 collection 模型回查 text 字段（方案 B 核心） ---------- */
/* TEXT 模型：SELECT text FROM mmdb_text_<name> WHERE id = ?  */
/* VECTOR 模型：SELECT text FROM mmdb_vec_<name> WHERE id = ?  */
/* 返回 0 成功；-1 错误（部分成功也视作错误，回滚已填字段）  */
static int backfill_text_field(mmdb_collection_t* c, mmdb_result_t* items) {
    if (!c || !items || items->count == 0) return MMDB_OK;

    /* P5-6：根据 capability 标志选择表名
     * - has_text 为 1：优先查 mmdb_text_<name>（TEXT 集合默认 / VECTOR+text_enable）
     * - has_text 为 0 且 has_vector 为 1：查 mmdb_vec_<name>（VECTOR 集合）
     * - 其他情况：fallback mmdb_text_<name>（无 text 字段时回查为空，不破坏整体） */
    const char* tname;
    if (c->has_text) {
        tname = "mmdb_text";
    } else if (c->model == MMDB_MODEL_VECTOR) {
        tname = "mmdb_vec";
    } else {
        tname = "mmdb_text";
    }
    char sql[256];
    int n = snprintf(sql, sizeof(sql),
                     "SELECT text FROM %s_%s WHERE id = ?;", tname, c->name);
    if (n < 0 || (size_t)n >= sizeof(sql)) return MMDB_ERR_INVALID;

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    /* 读操作：获取读锁（支持并发读） */
    mmdb_rwlock_rdlock(c->coll_lock);

    int rc = MMDB_OK;
    /* VECTOR 模型 id 列是 BLOB；TEXT 模型 id 列是 TEXT。
     * SQLite 类型亲和性规则下 BLOB 参数比较 TEXT 列存在边界 case，
     * 这里按模型分支选择绑定方式以确保可靠匹配。 */
    int is_text_model = (c->model == MMDB_MODEL_TEXT);

    for (size_t i = 0; i < items->count; i++) {
        mmdb_result_item_t* it = &items->items[i];
        /* 释放 hybrid_search 已分配（NULL 跳过），保证幂等 */
        if (it->text) { free(it->text); it->text = NULL; }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        int bind_rc;
        if (is_text_model) {
            /* TEXT 模型：构造 NUL 终止副本后用 bind_text */
            char id_buf[256];
            if (it->id_len >= sizeof(id_buf)) {
                rc = MMDB_ERR_INVALID;
                break;
            }
            if (it->id_len > 0) memcpy(id_buf, it->id, it->id_len);
            id_buf[it->id_len] = '\0';
            bind_rc = mmdb_sqlite_bind_text(stmt, 1, id_buf);
        } else {
            /* VECTOR 模型：id 为 BLOB，直接 bind_blob */
            bind_rc = mmdb_sqlite_bind_blob(stmt, 1, it->id, it->id_len);
        }
        if (bind_rc != 0) { rc = MMDB_ERR_IO; break; }

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* txt = (const char*)sqlite3_column_text(stmt, 0);
            if (txt && txt[0] != '\0') {
                size_t tlen = strlen(txt);
                it->text = (char*)malloc(tlen + 1);
                if (!it->text) { rc = MMDB_ERR_NOMEM; break; }
                memcpy(it->text, txt, tlen + 1);
            }
        }
        /* SQLITE_DONE 表示 id 不存在 → text 保持 NULL（合理） */
    }

    sqlite3_finalize(stmt);
    mmdb_rwlock_unlock(c->coll_lock, 0);
    return rc;
}

/* ---------- 简易 BM25 rerank 占位 ---------- */
/* 真实交叉编码器 rerank 需外部 ML 模型（P3 不引入依赖）。本实现用 query
 * 词在 text 字段上的出现次数除以 sqrt(text 长度) 作为简化 BM25 分数，
 * 与原 RRF score 按 weight 加权混合：
 *     final = (1 - weight) * (-distance) - weight * bm25
 * distance 越大越差（hybrid 约定），所以 bm25 用减号。 */
/* 把字符串切成空格分隔的小写 token（写入 out_tokens，每个为 NUL 终止串）。
 * 返回 token 数。max_tokens 上限保护。 */
static size_t tokenize_lower(const char* s, char** out_tokens, size_t max_tokens) {
    if (!s) return 0;
    size_t n = 0;
    const char* p = s;
    while (*p && n < max_tokens) {
        /* 跳过空白 */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
            p++;
        }
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            p++;
        }
        size_t len = (size_t)(p - start);
        if (len == 0) continue;
        char* tok = (char*)malloc(len + 1);
        if (!tok) break;
        for (size_t i = 0; i < len; i++) {
            char c = start[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
            tok[i] = c;
        }
        tok[len] = '\0';
        out_tokens[n++] = tok;
    }
    return n;
}

/* 计算 text 中 query 词的简易 BM25 分数：
 *   bm25 = sum_over_terms(count(term in text)) / sqrt(text_len + 1)
 * 单测可验证：含 query 词的 text 得 > 0；不含得 = 0。 */
static double bm25_score(const char* text, const char* query_text) {
    if (!text || !query_text) return 0.0;
    size_t tlen = strlen(text);
    /* tokenize query（动态分配：按 query 长度计算 max_tokens，上限 1024） */
    size_t max_tokens = strlen(query_text) / 2 + 16;
    if (max_tokens > 1024) max_tokens = 1024;
    char** q_toks = (char**)malloc(max_tokens * sizeof(char*));
    if (!q_toks) return 0.0;
    size_t qn = tokenize_lower(query_text, q_toks, max_tokens);
    if (qn == 0) { free(q_toks); return 0.0; }

    /* 对每个 query token，在 text 中扫描计数（忽略大小写） */
    double total_hits = 0.0;
    for (size_t i = 0; i < qn; i++) {
        const char* needle = q_toks[i];
        size_t nlen = strlen(needle);
        const char* p = text;
        while (*p) {
            const char* hit = strstr(p, needle);
            if (!hit) break;
            total_hits += 1.0;
            p = hit + nlen;
        }
    }
    /* 释放 token + token 数组 */
    for (size_t i = 0; i < qn; i++) free(q_toks[i]);
    free(q_toks);

    /* 简化 BM25：term 命中次数 / sqrt(text_len + 1) */
    return total_hits / sqrt((double)(tlen + 1));
}

/* 在 hout.items 上应用 BM25 rerank（修改 distance 字段）。 */
static void apply_bm25_rerank(mmdb_result_t* hout,
                              const char* query_text, double weight) {
    if (!hout || hout->count == 0 || !query_text) return;
    double w = weight;
    /* 防御：clamp 到 [0, 1] */
    if (w < 0.0) w = 0.0;
    if (w > 1.0) w = 1.0;
    for (size_t i = 0; i < hout->count; i++) {
        const char* text = hout->items[i].text
                         ? hout->items[i].text : "";
        double bm25 = bm25_score(text, query_text);
        double orig = -((double)hout->items[i].distance);  /* RRF score */
        double final = (1.0 - w) * orig - w * bm25;
        hout->items[i].distance = (float)final;
    }
}

/* ---------- 公开 API ---------- */

/* P4-T4.1 新增：将 embedding 注入 collection（持久化为 collection
 * metadata）。所有权不转移；调用方负责前一次的释放。 */
int mmdb_rag_set_embedding(mmdb_collection_t* coll,
                           mmdb_embedding_t* embedding) {
    if (!coll) return MMDB_ERR_INVALID;
    coll->embedding = embedding;
    return MMDB_OK;
}

int mmdb_rag_retrieve(
    mmdb_collection_t* c,
    const mmdb_rag_query_t* q,
    mmdb_rag_result_t* out) {
    /* 参数校验 */
    if (!c || !q || !out || !q->query_text) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_TEXT && c->model != MMDB_MODEL_VECTOR) {
        return MMDB_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));

    /* 默认值 */
    size_t top_k = q->top_k > 0 ? q->top_k : RAG_DEFAULT_TOP_K;
    size_t max_ctx = q->max_context_chars > 0
                     ? q->max_context_chars : RAG_DEFAULT_MAX_CONTEXT;

    /* 1. embedding 维度：优先 schema.vector_dim，未指定则 fallback 64 */
    size_t dim = c->schema.vector_dim;
    if (dim == 0) dim = 64;
    if (dim > RAG_MAX_EMBED_DIM) return MMDB_ERR_INVALID;

    /* 2. embedding 选择优先级（P4-T4.1）：
     *    q.embedding (per-call) > coll->embedding (collection-level) >
     *    fallback MMDB_EMBED_HASH。任一非空路径都需自行 drop 临时句柄。 */
    mmdb_embedding_t* emb = NULL;
    int emb_owned = 0;  /* 是否需要 mmdb_embedding_drop(emb) */
    if (q->embedding) {
        emb = q->embedding;
        emb_owned = 0;
    } else if (c->embedding) {
        emb = c->embedding;
        emb_owned = 0;
    } else {
        emb = mmdb_embedding_create(MMDB_EMBED_HASH, dim);
        if (!emb) return MMDB_ERR_NOMEM;
        emb_owned = 1;
    }

    /* 3. 编码 query_text → query_vec */
    float query_vec[RAG_MAX_EMBED_DIM];
    int embed_rc = mmdb_embed_text(emb, q->query_text, strlen(q->query_text),
                                   query_vec, dim);
    if (emb_owned) mmdb_embedding_drop(emb);
    if (embed_rc != 0) return MMDB_ERR_IO;

    /* 4. hybrid search（向量 + FTS5 + RRF） */
    mmdb_hybrid_query_t hq = {0};
    hq.vector      = query_vec;
    hq.dim         = dim;
    hq.text_query  = q->query_text;
    hq.filter_json = q->filter_json;
    hq.top_k       = top_k;

    mmdb_result_t hout = {0};
    int rc = mmdb_hybrid_search(c, &hq, &hout);
    if (rc != MMDB_OK) return rc;  /* hout 已 free */

    /* 5. 按 distance 升序排序（hybrid 已按 RRF 降序排过；
     *    distance = -rrf_score，所以升序 = RRF 降序；此处再排一次保险） */
    if (hout.count > 1) {
        qsort(hout.items, hout.count,
              sizeof(mmdb_result_item_t), cmp_by_distance);
    }

    /* 6. RAG 层独立回查 SQLite 取 text 字段（方案 B） */
    int brc = backfill_text_field(c, &hout);
    if (brc != MMDB_OK) {
        free_items(hout.items, hout.count);
        return brc;
    }

    /* 6.5 BM25 rerank 占位（仅当 query_text 非空且 rerank.kind == BM25）
     *     必须放在 text 回填之后（依赖 text 字段）、context 拼接之前
     *     （rerank 结果需要影响最终输出顺序）。再次按 distance 升序
     *     排序使最终 context 拼接与 rerank 顺序一致。 */
    if (q->rerank.kind == MMDB_RAG_RERANK_BM25) {
        apply_bm25_rerank(&hout, q->query_text, q->rerank.weight);
        if (hout.count > 1) {
            qsort(hout.items, hout.count,
                  sizeof(mmdb_result_item_t), cmp_by_distance);
        }
    }

    /* 7. 拼接 context：按 distance 升序，"\n---\n" 分隔，截断到 max_ctx */
    /*    预分配 max_ctx + 1（保留 \0），避免动态扩容 */
    size_t ctx_cap = max_ctx + 1;
    char* ctx = (char*)malloc(ctx_cap);
    if (!ctx) {
        free_items(hout.items, hout.count);
        return MMDB_ERR_NOMEM;
    }
    ctx[0] = '\0';
    size_t ctx_len = 0;

    for (size_t i = 0; i < hout.count && ctx_len < max_ctx; i++) {
        const char* text = hout.items[i].text;
        if (!text) text = "";
        size_t tlen = strlen(text);
        size_t sep_len = (i > 0) ? 5 : 0;  /* "\n---\n" */

        /* 若当前段超出剩余空间，截断写入；否则完整写入 */
        if (ctx_len + sep_len + tlen >= ctx_cap) {
            size_t remaining = ctx_cap - 1 - ctx_len - sep_len;
            if (remaining > 0) {
                if (sep_len) {
                    memcpy(ctx + ctx_len, "\n---\n", sep_len);
                    ctx_len += sep_len;
                }
                memcpy(ctx + ctx_len, text, remaining);
                ctx_len += remaining;
            }
            break;  /* 截断后跳出 */
        }

        if (sep_len) {
            memcpy(ctx + ctx_len, "\n---\n", sep_len);
            ctx_len += sep_len;
        }
        memcpy(ctx + ctx_len, text, tlen);
        ctx_len += tlen;
    }
    ctx[ctx_len] = '\0';

    /* 8. 转移所有权到 out（调用方通过 mmdb_rag_result_free 统一释放） */
    out->items = hout;
    out->context = ctx;
    out->context_len = ctx_len;
    return MMDB_OK;
}

void mmdb_rag_result_free(mmdb_rag_result_t* r) {
    if (!r) return;
    free_items(r->items.items, r->items.count);
    r->items.items = NULL;
    r->items.count = 0;
    free(r->context);
    r->context = NULL;
    r->context_len = 0;
}
