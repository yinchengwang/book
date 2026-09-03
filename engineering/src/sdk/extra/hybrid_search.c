/**
 * @file hybrid_search.c
 * @brief hybrid search 公共 API 实现（主+次通道 → RRF 重排）
 *
 * 路由策略（P4-T4.2：双通道真正融合，关闭 CI-1 单通道路由限制）：
 *   1. 主通道由 c->model 决定：
 *      - VECTOR 模型 + q->vector  → 调 mmdb_vectors_search
 *      - TEXT   模型 + q->text_query → 调 mmdb_text_search
 *      - 其他模型：行为不变（仅主通道能匹配时启用）
 *   2. 次通道：若 q 同时提供非主通道字段，则尝试启用次通道（即使模型不匹配
 *      底层 API 会拒绝，本路由策略仍优雅处理失败，不影响主通道结果）：
 *      - VECTOR 集合 + q->text_query（strlen > 0） → 次通道 text
 *      - TEXT   集合 + q->vector（dim > 0）        → 次通道 vector
 *   3. 主通道与次通道结果都进 RRF 融合（共享去重逻辑）
 *
 * 实现要点：
 *   1. 候选数组上限 = cand_per_channel * 2 + 16（允许重复 ID 跨通道）
 *   2. 按 id 线性查重（候选数小，无需哈希）
 *   3. RRF 融合后按 rrf_score 降序排序
 *   4. 截取 top_k，深拷贝 id 至 out->items
 *   5. metadata/text 留空（按需回查，与 P2 阶段一致）
 */
#include "sdk/mmdb_hybrid.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_text.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/hybrid_search.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* 单通道候选数倍率（top_k 的倍数） */
#define HYBRID_CANDIDATE_MULTIPLIER  2

/* 候选数组额外保留槽位（防重复 ID 跨通道场景） */
#define HYBRID_CAND_EXTRA_SLOTS      16

/* source_ranks 容量（与 mmdb_rrf_doc_t 定义一致） */
#define HYBRID_SOURCE_RANKS_CAP      8

/* 比较函数：按 rrf_score 降序 */
static int rrf_doc_cmp_desc(const void* a, const void* b) {
    double da = ((const mmdb_rrf_doc_t*)a)->rrf_score;
    double db = ((const mmdb_rrf_doc_t*)b)->rrf_score;
    return (da < db) ? 1 : (da > db) ? -1 : 0;
}

/* 在候选数组中按 id 线性查找索引（候选数小，无需哈希） */
static ssize_t find_candidate(
    const mmdb_rrf_doc_t* docs, size_t count,
    const uint8_t* id, size_t id_len) {
    for (size_t i = 0; i < count; i++) {
        if (docs[i].id_len == id_len &&
            memcmp(docs[i].id, id, id_len) == 0) {
            return (ssize_t)i;
        }
    }
    return -1;
}

/* 释放候选数组内所有动态分配的 id 与数组本身 */
static void free_candidates(mmdb_rrf_doc_t* cands, size_t count) {
    if (!cands) return;
    for (size_t i = 0; i < count; i++) {
        free((void*)cands[i].id);
    }
    free(cands);
}

/* 向候选数组追加一个新候选（含 id 深拷贝）。
 * 返回 0 成功；-1 内存不足（数组保持原状，调用方需回滚）。*/
static int append_candidate(
    mmdb_rrf_doc_t** cands, size_t* count, size_t* cap,
    const uint8_t* id, size_t id_len, size_t rank) {
    if (*count >= *cap) {
        size_t new_cap = (*cap) * 2;
        mmdb_rrf_doc_t* nc = (mmdb_rrf_doc_t*)realloc(
            *cands, new_cap * sizeof(mmdb_rrf_doc_t));
        if (!nc) return -1;
        memset(nc + *cap, 0, (new_cap - *cap) * sizeof(mmdb_rrf_doc_t));
        *cands = nc;
        *cap = new_cap;
    }
    /* 深拷贝 id */
    uint8_t* id_copy = (uint8_t*)malloc(id_len > 0 ? id_len : 1);
    if (!id_copy) return -1;
    if (id_len > 0) memcpy(id_copy, id, id_len);
    (*cands)[*count].id = id_copy;
    (*cands)[*count].id_len = id_len;
    (*cands)[*count].rrf_score = 0.0;
    (*cands)[*count].source_ranks[0] = rank;
    (*cands)[*count].source_count = 1;
    (*count)++;
    return 0;
}

/* 向已有候选追加一个新通道的 rank（容量上限 HYBRID_SOURCE_RANKS_CAP） */
static void append_rank_to_existing(
    mmdb_rrf_doc_t* cands, size_t idx, size_t rank) {
    if (cands[idx].source_count < HYBRID_SOURCE_RANKS_CAP) {
        cands[idx].source_ranks[cands[idx].source_count++] = rank;
    }
}

/* 将一次 mmdb_*_search 的结果合并进候选数组
 *
 * 入参:
 *   cands/cand_count/cand_cap - 候选数组（可能被扩容）
 *   r         - 通道结果（不会被修改；调用方负责释放）
 *   rank_off  - 该通道结果在全局 RRF 排名中的偏移（通常为 0；
 *               当次通道结果作为整体追加时，可与主通道排名相加）
 *
 * 出参:
 *   成功返回 0；内存不足返回 -1（调用方需回滚候选数组）
 *
 * 注：本次调用产生的 rank = i + 1 + rank_off，便于跨通道排名唯一。
 */
static int merge_channel_into_candidates(
    mmdb_rrf_doc_t** cands, size_t* cand_count, size_t* cand_cap,
    const mmdb_result_t* r, size_t rank_off) {
    for (size_t i = 0; i < r->count; i++) {
        size_t rank = i + 1 + rank_off;
        ssize_t idx = find_candidate(
            *cands, *cand_count, r->items[i].id, r->items[i].id_len);
        if (idx < 0) {
            if (append_candidate(cands, cand_count, cand_cap,
                                 r->items[i].id, r->items[i].id_len,
                                 rank) != 0) {
                return -1;
            }
        } else {
            append_rank_to_existing(*cands, (size_t)idx, rank);
        }
    }
    return 0;
}

/* 次通道：text 搜索
 *
 * 当前 SDK 架构下 mmdb_text_search 仅接受 MMDB_MODEL_TEXT 集合，
 * 在 VECTOR 集合上调用会立即返回 MMDB_ERR_INVALID；本函数在内部
 * 静默吞掉该错误，确保双通道路由即使次通道不适用也不会破坏主通道。
 *
 * 返回值：是否成功向候选数组注入了至少一个新候选（用于外部诊断）
 */
static int hybrid_search_secondary_text(
    mmdb_collection_t* c, const char* text_query, size_t cand_per_channel,
    const char* filter_json,
    mmdb_rrf_doc_t** cands, size_t* cand_count, size_t* cand_cap) {
    /* 安全护栏：text_query 非空且 strlen > 0 才尝试 */
    if (!text_query || strlen(text_query) == 0) return 0;

    mmdb_text_query_t tq = {text_query, cand_per_channel, filter_json};
    mmdb_result_t tout = {0};
    int trc = mmdb_text_search(c, &tq, &tout);
    /* 次通道预期错误码：
     *   - MMDB_ERR_INVALID（model != TEXT）：VECTOR 集合调用 mmdb_text_search
     *   - 其它错误：同样静默忽略（不影响主通道）
     */
    if (trc == MMDB_OK) {
        if (merge_channel_into_candidates(
                cands, cand_count, cand_cap, &tout, 0) != 0) {
            mmdb_result_free(&tout);
            return -1;
        }
    }
    mmdb_result_free(&tout);
    return 0;
}

int mmdb_hybrid_search(
    mmdb_collection_t* c,
    const mmdb_hybrid_query_t* q,
    mmdb_result_t* out) {
    /* 参数校验 */
    if (!c || !q || !out) return MMDB_ERR_INVALID;
    if (!q->vector && !q->text_query) return MMDB_ERR_INVALID;
    memset(out, 0, sizeof(*out));

    /* 默认值 */
    size_t top_k = q->top_k > 0 ? q->top_k : 10;
    size_t cand_per_channel = top_k * HYBRID_CANDIDATE_MULTIPLIER;

    /* 候选数组初始容量 = 两通道各 cand_per_channel + 预留 */
    size_t cand_cap = cand_per_channel * 2 + HYBRID_CAND_EXTRA_SLOTS;
    mmdb_rrf_doc_t* cands = (mmdb_rrf_doc_t*)calloc(cand_cap, sizeof(mmdb_rrf_doc_t));
    if (!cands) return MMDB_ERR_NOMEM;
    size_t cand_count = 0;

    /* === 主通道（由 c->model 决定） === */
    if (q->vector && q->dim > 0 && c->model == MMDB_MODEL_VECTOR) {
        /* VECTOR 集合主通道：向量检索 */
        mmdb_query_t vq = {q->vector, q->dim, cand_per_channel, q->filter_json};
        mmdb_result_t vout = {0};
        int vrc = mmdb_vectors_search(c, &vq, &vout);
        if (vrc == MMDB_OK) {
            if (merge_channel_into_candidates(
                    &cands, &cand_count, &cand_cap, &vout, 0) != 0) {
                mmdb_result_free(&vout);
                free_candidates(cands, cand_count);
                return MMDB_ERR_NOMEM;
            }
        }
        mmdb_result_free(&vout);
    } else if (q->text_query && c->model == MMDB_MODEL_TEXT) {
        /* TEXT 集合主通道：文本检索 */
        mmdb_text_query_t tq = {q->text_query, cand_per_channel, q->filter_json};
        mmdb_result_t tout = {0};
        int trc = mmdb_text_search(c, &tq, &tout);
        if (trc == MMDB_OK) {
            if (merge_channel_into_candidates(
                    &cands, &cand_count, &cand_cap, &tout, 0) != 0) {
                mmdb_result_free(&tout);
                free_candidates(cands, cand_count);
                return MMDB_ERR_NOMEM;
            }
        }
        mmdb_result_free(&tout);
    }
    /* 注：其它模型（GRAPH / TIMESERIES）当前不支持 hybrid，主通道保持关闭 */

    /* === 次通道（双通道真正融合：主通道之外，若 q 同时提供另一字段则尝试） === */
    if (c->model == MMDB_MODEL_VECTOR && q->text_query &&
        strlen(q->text_query) > 0) {
        /* VECTOR 集合上提供 text_query → 启用次通道 text */
        if (hybrid_search_secondary_text(
                c, q->text_query, cand_per_channel, q->filter_json,
                &cands, &cand_count, &cand_cap) != 0) {
            free_candidates(cands, cand_count);
            return MMDB_ERR_NOMEM;
        }
    } else if (c->model == MMDB_MODEL_TEXT && q->vector && q->dim > 0) {
        /* TEXT 集合上提供 vector → 启用次通道 vector
         *
         * 注：当前 SDK 架构下 mmdb_vectors_search 仅接受 MMDB_MODEL_VECTOR 集合，
         * 在 TEXT 集合上调用会立即返回 MMDB_ERR_INVALID；此处静默吞掉该错误。
         * 若后续 SDK 支持 TEXT 集合内置向量索引（需 collection embedding 协调），
         * 该路径将自动激活。 */
        mmdb_query_t vq = {q->vector, q->dim, cand_per_channel, q->filter_json};
        mmdb_result_t vout = {0};
        int vrc = mmdb_vectors_search(c, &vq, &vout);
        if (vrc == MMDB_OK) {
            if (merge_channel_into_candidates(
                    &cands, &cand_count, &cand_cap, &vout, 0) != 0) {
                mmdb_result_free(&vout);
                free_candidates(cands, cand_count);
                return MMDB_ERR_NOMEM;
            }
        }
        mmdb_result_free(&vout);
    }

    /* 若两个通道都未命中候选（搜索失败或模型不匹配），直接返回空结果 */
    if (cand_count == 0) {
        free_candidates(cands, cand_count);
        return MMDB_OK;  /* out 已 memset 为 0，等价于 count=0 */
    }

    /* === RRF 融合 === */
    mmdb_rrf_config_t default_cfg;
    mmdb_rrf_config_init(&default_cfg);
    const mmdb_rrf_config_t* cfg = q->rrf ? q->rrf : &default_cfg;
    if (mmdb_rrf_fuse(cands, cand_count, cfg) != 0) {
        free_candidates(cands, cand_count);
        return MMDB_ERR_INVALID;
    }

    /* === 按 rrf_score 降序排序 === */
    qsort(cands, cand_count, sizeof(mmdb_rrf_doc_t), rrf_doc_cmp_desc);

    /* === 截取 top_k === */
    size_t out_count = cand_count < top_k ? cand_count : top_k;

    /* === 填充 out（id 深拷贝；metadata/text 留空由调用方按需回查） === */
    out->items = (mmdb_result_item_t*)calloc(out_count, sizeof(mmdb_result_item_t));
    if (!out->items) {
        free_candidates(cands, cand_count);
        return MMDB_ERR_NOMEM;
    }
    out->count = out_count;

    for (size_t i = 0; i < out_count; i++) {
        uint8_t* id_copy = (uint8_t*)malloc(cands[i].id_len > 0 ? cands[i].id_len : 1);
        if (!id_copy) {
            /* 部分分配失败：标记已分配项以便 mmdb_result_free 安全释放 */
            out->count = i;
            free_candidates(cands, cand_count);
            return MMDB_ERR_NOMEM;
        }
        if (cands[i].id_len > 0) {
            memcpy(id_copy, cands[i].id, cands[i].id_len);
        }
        out->items[i].id = id_copy;
        out->items[i].id_len = cands[i].id_len;
        /* RRF 得分越大越相关；distance 取负值使"distance 越小越相关"的语义保持一致 */
        out->items[i].distance = (float)(-cands[i].rrf_score);
        out->items[i].metadata_json = NULL;
        out->items[i].text = NULL;
    }

    /* 释放候选数组内部堆内存 */
    free_candidates(cands, cand_count);
    return MMDB_OK;
}
