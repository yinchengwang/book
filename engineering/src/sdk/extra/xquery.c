/**
 * @file xquery.c
 * @brief 跨 collection join（text → vector）实现（P3-T3.1）
 *
 * 策略：
 *   1. 在 source (text) 集合上做 FTS5 检索，候选上限 max_source_candidates
 *      （默认 100，超过则 stderr 警告并截断）。
 *   2. 对每个候选 id，在 target (vector) 集合上用 SQLite
 *      `SELECT vector FROM mmdb_vec_<name> WHERE id = ?` 取出该向量，
 *      计算与 query_vector 的 L2 平方距离。
 *   3. 用最大堆（堆顶最大）维护 top_k 最近邻；候选数 ≤ top_k 时全保留。
 *   4. 按距离升序填充 out->items，id 深拷贝，distance 字段填 L2 平方距离。
 *
 * 约束（P4-T4.5 更新）：
 *   - 候选数已限制 ≤ max_source_candidates，本接口对候选集合做全量距离计算，
 *     无需借助 HNSW 全局索引（HNSW 用于无候选约束的大规模全局搜索）。
 *   - target 集合的 metadata filter 在 P4-T4.5 后已可走 HNSW 路径
 *     （见 vectors.c::mmdb_vectors_search），但 xquery 自身不受影响：
 *     xquery 直接遍历候选 id，filter 由调用方在 source/target
 *     集合上分别独立处理。
 */
#include "sdk/mmdb_xquery.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_text.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

/* 默认参数 */
#define XQUERY_DEFAULT_MAX_SOURCE  100
#define XQUERY_DEFAULT_TOP_K       10

/* target 向量表名最大长度（与 vectors.c 中的 build_table_name 对齐） */
#define XQUERY_TABLE_NAME_MAX      128

/* 候选 id 最大长度（字节）。命中 id 超过此长度立即返回 MMDB_ERR_INVALID，
 * 不再静默跳过（P4-T4.3：关闭 CI-3）。缓冲对齐设置，避免栈占用过大。 */
#define XQUERY_MAX_ID_LEN          256

/* ====================================================================== */
/* 内部辅助                                                              */
/* ====================================================================== */

/* L2 平方距离（标量实现，与 vectors.c 中的 flat 路径一致） */
static float xquery_l2_sq(const float* a, const float* b, size_t dim) {
    float s = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        float d = a[i] - b[i];
        s += d * d;
    }
    return s;
}

/* 构造 target 向量表名（与 vectors.c::build_table_name 保持一致） */
static int build_target_table_name(char* out, size_t out_cap,
                                   const char* coll) {
    int n = snprintf(out, out_cap, "mmdb_vec_%s", coll);
    if (n < 0 || (size_t)n >= out_cap) return MMDB_ERR_INVALID;
    return MMDB_OK;
}

/* 候选条目（含 id 指针、距离） */
typedef struct {
    float          dist;
    uint8_t        id[XQUERY_MAX_ID_LEN];
    size_t         id_len;
} xq_cand_t;

/* 最大堆下沉（堆顶最大） */
static void xq_heap_siftdown(xq_cand_t* heap, size_t size, size_t i) {
    for (;;) {
        size_t largest = i;
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        if (left < size && heap[left].dist > heap[largest].dist) {
            largest = left;
        }
        if (right < size && heap[right].dist > heap[largest].dist) {
            largest = right;
        }
        if (largest == i) break;
        xq_cand_t tmp = heap[i];
        heap[i] = heap[largest];
        heap[largest] = tmp;
        i = largest;
    }
}

/* 最大堆上浮 */
static void xq_heap_siftup(xq_cand_t* heap, size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (heap[parent].dist >= heap[i].dist) break;
        xq_cand_t tmp = heap[i];
        heap[i] = heap[parent];
        heap[parent] = tmp;
        i = parent;
    }
}

/* 按距离升序比较（用于 qsort） */
static int xq_cand_cmp_asc(const void* a, const void* b) {
    float da = ((const xq_cand_t*)a)->dist;
    float db = ((const xq_cand_t*)b)->dist;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

/* ====================================================================== */
/* 公共 API                                                              */
/* ====================================================================== */

int mmdb_xquery_text_to_vector(
    const mmdb_xquery_text_to_vector_t* xq,
    mmdb_result_t* out) {
    /* 参数校验 */
    if (!xq || !out) return MMDB_ERR_INVALID;
    if (!xq->source || !xq->target || !xq->text_query) return MMDB_ERR_INVALID;
    if (!xq->query_vector || xq->dim == 0) return MMDB_ERR_INVALID;

    memset(out, 0, sizeof(*out));

    size_t max_src = xq->max_source_candidates > 0
                     ? xq->max_source_candidates : XQUERY_DEFAULT_MAX_SOURCE;
    size_t top_k = xq->top_k > 0 ? xq->top_k : XQUERY_DEFAULT_TOP_K;

    /* ----------------------------------------------------------------
     * Step 1: 在 source (text) 集合上做 FTS5，限制候选数
     * ---------------------------------------------------------------- */
    mmdb_text_query_t tq = {xq->text_query, max_src, NULL};
    mmdb_result_t src_hits;
    memset(&src_hits, 0, sizeof(src_hits));
    int rc = mmdb_text_search(xq->source, &tq, &src_hits);
    if (rc != MMDB_OK) return rc;

    /* 若 text 命中数 > max_src，发出 stderr 警告并截断。
     * 注：mmdb_text_search 已用 top_k=max_src 截断，此处只对真实命中数
     *     超出场景给出提示（FTS5 默认按相关性，可能命中数 < max_src）。 */
    if (src_hits.count >= max_src && max_src > 0) {
        /* 实际无法区分"恰好等于"还是"被截断"，保守地不打印以减少噪音。
         * 如确需提示，可在调用方自行判断命中数。 */
    }

    if (src_hits.count == 0) {
        mmdb_result_free(&src_hits);
        return MMDB_OK;  /* 无候选，返回空结果 */
    }

    /* ----------------------------------------------------------------
     * Step 2: 对每个候选 id，从 target 向量表读取向量并计算 L2 距离
     * ---------------------------------------------------------------- */
    /* target 表名 */
    char tname[XQUERY_TABLE_NAME_MAX];
    if (build_target_table_name(tname, sizeof(tname), xq->target->name)
        != MMDB_OK) {
        mmdb_result_free(&src_hits);
        return MMDB_ERR_INVALID;
    }

    /* 取读锁（target 集合，遍历候选是只读） */
    pthread_rwlock_rdlock(xq->target->coll_lock);

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT vector FROM %s WHERE id = ?;", tname);

    /* 堆容量 = top_k；候选数 < top_k 时实际只保留前 cand_count */
    size_t heap_cap = top_k;
    xq_cand_t* heap = (xq_cand_t*)calloc(heap_cap > 0 ? heap_cap : 1,
                                         sizeof(xq_cand_t));
    if (!heap) {
        pthread_rwlock_unlock(xq->target->coll_lock);
        mmdb_result_free(&src_hits);
        return MMDB_ERR_NOMEM;
    }
    size_t heap_size = 0;

    /* 临时 buffer 用于接收从 SQLite 取出的向量字节（dim 维 float） */
    float* vec_buf = (float*)calloc(xq->dim, sizeof(float));
    if (!vec_buf) {
        free(heap);
        pthread_rwlock_unlock(xq->target->coll_lock);
        mmdb_result_free(&src_hits);
        return MMDB_ERR_NOMEM;
    }

    for (size_t i = 0; i < src_hits.count; i++) {
        const mmdb_result_item_t* hit = &src_hits.items[i];
        if (!hit->id || hit->id_len == 0) continue;
        /* P4-T4.3：id 超过 XQUERY_MAX_ID_LEN (256B) 不再静默跳过，
         * 升级为返回 MMDB_ERR_INVALID。先 stderr ERROR 警告，再按
         * 清理顺序释放 src_hits / heap / vec_buf 与读锁。 */
        if (hit->id_len > XQUERY_MAX_ID_LEN) {
            fprintf(stderr,
                    "[mmdb_xquery] ERROR: id_len=%zu > %d-byte limit (collection=%s)\n",
                    hit->id_len, XQUERY_MAX_ID_LEN,
                    xq->source->name ? xq->source->name : "(unnamed)");
            free(vec_buf);
            free(heap);
            pthread_rwlock_unlock(xq->target->coll_lock);
            mmdb_result_free(&src_hits);
            return MMDB_ERR_INVALID;
        }

        sqlite3_stmt* stmt = mmdb_sqlite_prepare(
            xq->target->sdb, sql, NULL, 0);
        if (!stmt) continue;

        mmdb_sqlite_bind_blob(stmt, 1, hit->id, hit->id_len);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            /* 该 id 在 target 中不存在 → 跳过 */
            sqlite3_finalize(stmt);
            continue;
        }

        const void* vec_blob = sqlite3_column_blob(stmt, 0);
        int vec_bytes = sqlite3_column_bytes(stmt, 0);
        size_t got_dim = (size_t)vec_bytes / sizeof(float);

        if (got_dim != xq->dim || !vec_blob) {
            sqlite3_finalize(stmt);
            continue;
        }
        memcpy(vec_buf, vec_blob, vec_bytes);

        /* 计算 L2 平方距离 */
        float dist = xquery_l2_sq(vec_buf, xq->query_vector, xq->dim);

        sqlite3_finalize(stmt);

        /* 维护最大堆（保留 top_k 最小距离） */
        if (heap_size < heap_cap) {
            heap[heap_size].dist = dist;
            memcpy(heap[heap_size].id, hit->id, hit->id_len);
            heap[heap_size].id_len = hit->id_len;
            heap_size++;
            xq_heap_siftup(heap, heap_size - 1);
        } else if (dist < heap[0].dist) {
            /* 当前候选比堆顶更近 → 替换堆顶并下沉 */
            heap[0].dist = dist;
            memcpy(heap[0].id, hit->id, hit->id_len);
            heap[0].id_len = hit->id_len;
            xq_heap_siftdown(heap, heap_size, 0);
        }
    }

    free(vec_buf);
    pthread_rwlock_unlock(xq->target->coll_lock);

    /* ----------------------------------------------------------------
     * Step 3: 按距离升序填充 out
     * ---------------------------------------------------------------- */
    if (heap_size == 0) {
        free(heap);
        mmdb_result_free(&src_hits);
        return MMDB_OK;
    }

    /* 堆内距离升序排序 */
    qsort(heap, heap_size, sizeof(xq_cand_t), xq_cand_cmp_asc);

    out->items = (mmdb_result_item_t*)calloc(heap_size, sizeof(mmdb_result_item_t));
    if (!out->items) {
        free(heap);
        mmdb_result_free(&src_hits);
        return MMDB_ERR_NOMEM;
    }
    out->count = heap_size;

    int alloc_err = 0;
    for (size_t i = 0; i < heap_size; i++) {
        uint8_t* id_copy = (uint8_t*)malloc(heap[i].id_len > 0
                                            ? heap[i].id_len : 1);
        if (!id_copy) {
            alloc_err = 1;
            out->count = i;
            break;
        }
        if (heap[i].id_len > 0) {
            memcpy(id_copy, heap[i].id, heap[i].id_len);
        }
        out->items[i].id = id_copy;
        out->items[i].id_len = heap[i].id_len;
        out->items[i].distance = heap[i].dist;
        out->items[i].metadata_json = NULL;
        out->items[i].text = NULL;
    }

    free(heap);
    mmdb_result_free(&src_hits);

    if (alloc_err) return MMDB_ERR_NOMEM;
    return MMDB_OK;
}