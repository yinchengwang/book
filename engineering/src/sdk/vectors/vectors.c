/**
 * @file vectors.c
 * @brief 向量模型公共 API（add/search/get/delete）
 *
 * 距离度量：L2（欧氏距离平方）。后续可在 P2 引入余弦/内积。
 * 索引：Flat（暴力扫描），N >= 10000 自动启用 HNSW。
 */
#include "sdk/mmdb_vectors.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/vectors.h"
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/filter_parser.h"

/* Phase 2: HNSW 索引集成 */
#include <db/index/vector_index/faiss_hnsw/faiss_hnsw.h>
/* Phase 5: 向量索引选择器 */
#include <db/index/vector_index/vector_index_selector.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* SIMD 加速（AVX2） */
#if defined(__AVX2__) || defined(_MSC_VER)
#include <immintrin.h>

/* 跨平台 AVX2 运行时检测 */
static int cpu_has_avx2(void) {
#if defined(__AVX2__)
    return 1;  /* 编译时已启用 AVX2 */
#elif defined(_MSC_VER)
    int regs[4];
    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        return (ebx >> 5) & 1;  /* EBX bit 5 = AVX2 */
    }
    return 0;
#else
    return 0;
#endif
}

/* 水平求和 __m256 */
static float hsum256_ps(__m256 v) {
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 sum = _mm_add_ps(hi, lo);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

/* AVX2 加速的 L2 平方距离 */
static float l2_distance_simd(const float* a, const float* b, size_t dim) {
    __m256 acc = _mm256_setzero_ps();
    size_t i = 0;

    /* 每次处理 8 个 float */
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 diff = _mm256_sub_ps(va, vb);
        acc = _mm256_add_ps(acc, _mm256_mul_ps(diff, diff));
    }

    float result = hsum256_ps(acc);

    /* 处理剩余元素 */
    for (; i < dim; i++) {
        float d = a[i] - b[i];
        result += d * d;
    }

    return result;
}
#endif

/* 标量 L2 平方距离（fallback） */
static float l2_distance_scalar(const float* a, const float* b, size_t dim) {
    float dist = 0.0f;
    for (size_t k = 0; k < dim; k++) {
        float d = a[k] - b[k];
        dist += d * d;
    }
    return dist;
}

/* ================================================================== */
/* Phase 2: HNSW 集成                                                  */
/* ================================================================== */

/* HNSW 启用阈值：向量数量 >= 此值时自动构建 HNSW 索引 */
#define HNSW_BUILD_THRESHOLD  10000
#define HNSW_DEFAULT_M        16
#define HNSW_DEFAULT_EF_C     128
#define HNSW_DEFAULT_EF_S     128

/* HNSW ID 映射表：int32_t HNSW ID ↔ 可变长 SDK ID (BLOB) */
typedef struct {
    uint8_t**  sdk_ids;      /* sdk_ids[i] = 对应 HNSW ID i 的 SDK 字节数组 ID */
    size_t*    sdk_id_lens;  /* sdk_id_lens[i] = sdk_ids[i] 的字节长度 */
    size_t     count;        /* 当前条目数 */
    size_t     capacity;     /* 已分配容量 */
} hnsw_id_map_t;

static hnsw_id_map_t* hnsw_id_map_create(size_t initial_cap) {
    hnsw_id_map_t* m = (hnsw_id_map_t*)calloc(1, sizeof(hnsw_id_map_t));
    if (!m) return NULL;
    m->capacity = initial_cap > 0 ? initial_cap : 256;
    m->sdk_ids = (uint8_t**)calloc(m->capacity, sizeof(uint8_t*));
    m->sdk_id_lens = (size_t*)calloc(m->capacity, sizeof(size_t));
    if (!m->sdk_ids || !m->sdk_id_lens) {
        free(m->sdk_ids);
        free(m->sdk_id_lens);
        free(m);
        return NULL;
    }
    return m;
}

static void hnsw_id_map_free(hnsw_id_map_t* m) {
    if (!m) return;
    for (size_t i = 0; i < m->count; i++) {
        free(m->sdk_ids[i]);
    }
    free(m->sdk_ids);
    free(m->sdk_id_lens);
    free(m);
}

/* 追加一条映射，返回 HNSW ID（从 0 开始递增） */
static int32_t hnsw_id_map_add(hnsw_id_map_t* m, const uint8_t* id, size_t id_len) {
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity * 2;
        uint8_t** new_ids = (uint8_t**)realloc(m->sdk_ids, new_cap * sizeof(uint8_t*));
        size_t* new_lens = (size_t*)realloc(m->sdk_id_lens, new_cap * sizeof(size_t));
        if (!new_ids || !new_lens) return -1;
        m->sdk_ids = new_ids;
        m->sdk_id_lens = new_lens;
        memset(m->sdk_ids + m->capacity, 0, (new_cap - m->capacity) * sizeof(uint8_t*));
        memset(m->sdk_id_lens + m->capacity, 0, (new_cap - m->capacity) * sizeof(size_t));
        m->capacity = new_cap;
    }
    int32_t hnsw_id = (int32_t)m->count;
    m->sdk_ids[hnsw_id] = (uint8_t*)malloc(id_len);
    if (!m->sdk_ids[hnsw_id]) return -1;
    memcpy(m->sdk_ids[hnsw_id], id, id_len);
    m->sdk_id_lens[hnsw_id] = id_len;
    m->count++;
    return hnsw_id;
}

/* 根据 HNSW ID 查找 SDK ID（用于搜索结果映射） */
static const uint8_t* hnsw_id_map_lookup(const hnsw_id_map_t* m, int32_t hnsw_id, size_t* out_len) {
    if (hnsw_id < 0 || (size_t)hnsw_id >= m->count) return NULL;
    *out_len = m->sdk_id_lens[hnsw_id];
    return m->sdk_ids[hnsw_id];
}

/* 标记某 HNSW ID 为已删除（释放其 SDK ID 内存，设为 NULL） */
static void hnsw_id_map_mark_deleted(hnsw_id_map_t* m, int32_t hnsw_id) {
    if (hnsw_id < 0 || (size_t)hnsw_id >= m->count) return;
    free(m->sdk_ids[hnsw_id]);
    m->sdk_ids[hnsw_id] = NULL;
    m->sdk_id_lens[hnsw_id] = 0;
}

/* 将当前 ID 映射表和已删除计数存储在 collection 的 hnsw 字段中 */
typedef struct {
    faiss_hnsw_t*   index;          /* HNSW 索引实例 */
    hnsw_id_map_t*  id_map;         /* int32_t ↔ SDK ID 映射 */
    size_t          deleted_count;   /* 已标记删除的条目数 */
} hnsw_wrapper_t;

/* P4-T4.5：HNSW filter 谓词上下文。
 * bitmap[i] = 1 表示 HNSW vec_id i 通过 metadata filter；0 表示被过滤。
 * 通过一次性 SQLite 查询构建，避免每个候选单独查询 metadata。 */
typedef struct {
    int8_t* bitmap;
    size_t  bitmap_size;  /* bitmap 元素数（与 HNSW 当前向量数一致） */
} hnsw_filter_ctx_t;

/* P4-T4.5：HNSW filter 谓词回调（faiss_hnsw_filter_fn 类型） */
static int hnsw_filter_predicate(int32_t vec_id, void* user_data) {
    hnsw_filter_ctx_t* ctx = (hnsw_filter_ctx_t*)user_data;
    if (!ctx || !ctx->bitmap) return 1;  /* 无 filter ctx 时全部通过 */
    if (vec_id < 0 || (size_t)vec_id >= ctx->bitmap_size) return 0;
    return ctx->bitmap[vec_id];
}

static hnsw_wrapper_t* hnsw_wrapper_create(void) {
    hnsw_wrapper_t* w = (hnsw_wrapper_t*)calloc(1, sizeof(hnsw_wrapper_t));
    return w;
}

static void hnsw_wrapper_free(hnsw_wrapper_t* w) {
    if (!w) return;
    if (w->index) faiss_hnsw_index_drop(w->index);
    hnsw_id_map_free(w->id_map);
    free(w);
}

/* P4-T4.5：从 SQLite 查询符合 filter 的 SDK ID，写入 bitmap（按 HNSW vec_id 索引）。
 * 用于 HNSW 加速路径下的 metadata 过滤：bitmap[i] = 1 表示 HNSW vec_id i 通过 filter。
 *
 * 注意：此函数必须在已持有 coll_lock 读锁的情况下调用（SQLite 操作需要稳定快照）。
 *
 * @param c       collection
 * @param where   mmdb_filter_compile 生成的 WHERE 片段
 * @param fp      filter 绑定参数
 * @param ctx     输出上下文（调用方负责 free_filter_ctx）
 * @return        MMDB_OK 或错误码
 */
/* build_table_name 前向声明（定义在文件后半部分） */
static int build_table_name(char* out, size_t out_cap, const char* coll);

static int build_filter_ctx(mmdb_collection_t* c, const char* where,
                             mmdb_filter_params_t* fp,
                             hnsw_filter_ctx_t* ctx) {
    hnsw_wrapper_t* w = (hnsw_wrapper_t*)c->hnsw;
    memset(ctx, 0, sizeof(*ctx));
    ctx->bitmap_size = w->id_map->count;
    if (ctx->bitmap_size == 0) return MMDB_OK;

    ctx->bitmap = (int8_t*)calloc(ctx->bitmap_size, sizeof(int8_t));
    if (!ctx->bitmap) return MMDB_ERR_NOMEM;

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);

    char sql[512];
    int n = snprintf(sql, sizeof(sql), "SELECT id FROM %s WHERE %s;", tname, where);
    if (n < 0 || (size_t)n >= sizeof(sql)) {
        free(ctx->bitmap);
        ctx->bitmap = NULL;
        return MMDB_ERR_INVALID;
    }

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) {
        free(ctx->bitmap);
        ctx->bitmap = NULL;
        return MMDB_ERR_IO;
    }
    int bind_idx = mmdb_filter_bind(stmt, fp, 1);
    if (bind_idx < 0) {
        sqlite3_finalize(stmt);
        free(ctx->bitmap);
        ctx->bitmap = NULL;
        return MMDB_ERR_INVALID;
    }

    /* 对每个匹配的 SDK ID，查找对应 HNSW vec_id 并置位 bitmap */
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* id_blob = sqlite3_column_blob(stmt, 0);
        int id_bytes = sqlite3_column_bytes(stmt, 0);
        if (id_bytes <= 0) continue;
        /* id_map 是小数组（N <= 100K），线性查找足够快 */
        for (size_t i = 0; i < w->id_map->count; i++) {
            if (w->id_map->sdk_ids[i] != NULL &&
                w->id_map->sdk_id_lens[i] == (size_t)id_bytes &&
                memcmp(w->id_map->sdk_ids[i], id_blob, id_bytes) == 0) {
                ctx->bitmap[i] = 1;
                break;
            }
        }
    }

    sqlite3_finalize(stmt);
    return MMDB_OK;
}

/* P4-T4.5：释放 filter 上下文 */
static void free_filter_ctx(hnsw_filter_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->bitmap) {
        free(ctx->bitmap);
        ctx->bitmap = NULL;
    }
    ctx->bitmap_size = 0;
}

/* 释放 collection 的 HNSW 索引内存 */
void mmdb_vectors_hnsw_free(mmdb_collection_t* c) {
    if (!c || !c->hnsw) return;
    hnsw_wrapper_free((hnsw_wrapper_t*)c->hnsw);
    c->hnsw = NULL;
}

/* ------------------------------------------------------------------ */
/* HNSW 重建：从 SQLite 全表扫描 bulk-load                             */
/* ------------------------------------------------------------------ */

/* 获取表中向量数量 */
static size_t hnsw_count_vectors(mmdb_collection_t* c, const char* tname) {
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", tname);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return 0;
    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = (size_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

/* build_table_name 前向声明（定义在文件后半部分） */
static int build_table_name(char* out, size_t out_cap, const char* coll);

/* 从 SQLite 重建 HNSW 索引（在 coll_lock 已持有的情况下调用） */
int mmdb_vectors_hnsw_rebuild(mmdb_collection_t* c) {
    hnsw_wrapper_t* w = (hnsw_wrapper_t*)c->hnsw;
    if (!w) return MMDB_ERR_INVALID;

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);

    /* 查询所有向量 */
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, vector FROM %s;", tname);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    size_t dim = c->schema.vector_dim;
    size_t total = hnsw_count_vectors(c, tname);
    if (total == 0) {
        sqlite3_finalize(stmt);
        return MMDB_OK;
    }

    /* 一次性分配所有向量数据（连续内存，faiss_hnsw_index_add 要求） */
    float* all_vectors = (float*)calloc(total * dim, sizeof(float));
    if (!all_vectors) {
        sqlite3_finalize(stmt);
        return MMDB_ERR_NOMEM;
    }

    /* 重建 ID 映射表 */
    hnsw_id_map_free(w->id_map);
    w->id_map = hnsw_id_map_create(total);
    if (!w->id_map) {
        free(all_vectors);
        sqlite3_finalize(stmt);
        return MMDB_ERR_NOMEM;
    }

    /* 释放旧 HNSW 索引，准备创建新的 */
    if (w->index) {
        faiss_hnsw_index_drop(w->index);
        w->index = NULL;
    }

    /* 扫描 SQLite，填充向量数组和 ID 映射 */
    size_t row_idx = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && row_idx < total) {
        const void* id_blob = sqlite3_column_blob(stmt, 0);
        int id_bytes = sqlite3_column_bytes(stmt, 0);
        const void* vec_blob = sqlite3_column_blob(stmt, 1);
        int vec_bytes = sqlite3_column_bytes(stmt, 1);

        if (id_bytes <= 0 || (size_t)vec_bytes != dim * sizeof(float)) continue;

        /* 追加到 ID 映射表 */
        int32_t hnsw_id = hnsw_id_map_add(w->id_map, (const uint8_t*)id_blob, (size_t)id_bytes);
        if (hnsw_id < 0) continue;

        /* 拷贝向量数据到连续数组 */
        memcpy(all_vectors + row_idx * dim, vec_blob, dim * sizeof(float));
        row_idx++;
    }
    sqlite3_finalize(stmt);

    if (row_idx == 0) {
        free(all_vectors);
        return MMDB_OK;
    }

    /* 创建新的 HNSW 索引并批量插入 */
    w->index = faiss_hnsw_index_create(
        HNSW_DEFAULT_M, (int32_t)dim, HNSW_DEFAULT_EF_C,
        DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE);
    if (!w->index) {
        free(all_vectors);
        return MMDB_ERR_NOMEM;
    }

    /*
     * 性能关键：bulk-load 前预分配索引容量
     * faiss_hnsw 默认每 add 一个向量就 realloc 5 个数组，百万级场景总拷贝量
     * O(N²)≈256TB，会无限卡死。reserve() 把开销降到 O(N) 一次性分配。
     */
    if (faiss_hnsw_index_reserve(w->index, (int32_t)row_idx) != 0) {
        free(all_vectors);
        faiss_hnsw_index_drop(w->index);
        w->index = NULL;
        return MMDB_ERR_NOMEM;
    }

    int32_t add_rc = faiss_hnsw_index_add(w->index, (int32_t)row_idx, all_vectors);
    free(all_vectors);

    if (add_rc != 0) {
        /* HNSW 构建失败，降级为 flat 模式（保持 w->index = NULL） */
        faiss_hnsw_index_drop(w->index);
        w->index = NULL;
        return MMDB_OK;  /* 不报错，降级到 flat */
    }

    w->deleted_count = 0;
    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* HNSW 懒创建：N >= 阈值时自动构建                                     */
/* ------------------------------------------------------------------ */

int mmdb_vectors_hnsw_ensure(mmdb_collection_t* c) {
    if (!c || c->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;

    /* 已有 HNSW 索引，直接返回 */
    if (c->hnsw) return MMDB_OK;

    /* 确保表存在 */
    if (mmdb_vectors_ensure_table(c) != MMDB_OK) return MMDB_ERR_IO;

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);

    /* 检查当前行数 */
    size_t count = hnsw_count_vectors(c, tname);
    if (count < HNSW_BUILD_THRESHOLD) return MMDB_OK;  /* 未达阈值，保持 flat */

    /* 使用索引选择器自动选择最优索引类型 */
    vector_data_info_t info = {
        .num_vectors = count,
        .dimension = (int32_t)c->schema.vector_dim,
        .available_memory_mb = 4096,  /* 默认可用内存估算 */
        .target_qps = 0.0f,
        .target_recall = 0.95f,      /* 默认高召回率 */
        .is_static = false
    };

    vector_index_decision_t decision;
    if (vector_index_selector_choose(&info, &decision) != 0) {
        /* 选择器失败，回退到默认 HNSW */
        decision.index_type = VECTOR_INDEX_HNSW;
        decision.param1 = HNSW_DEFAULT_M;
        decision.param2 = HNSW_DEFAULT_EF_C;
    }

    /* 根据选择器决策创建索引 */
    if (decision.index_type == VECTOR_INDEX_HNSW) {
        /* 创建 wrapper 并使用选择器推荐的参数重建 */
        hnsw_wrapper_t* w = hnsw_wrapper_create();
        if (!w) return MMDB_ERR_NOMEM;
        c->hnsw = w;

        int rc = mmdb_vectors_hnsw_rebuild(c);
        if (rc != MMDB_OK) {
            /* HNSW 创建失败，降级到 flat 模式 */
            hnsw_wrapper_free(w);
            c->hnsw = NULL;
            return MMDB_OK;  /* 不报错，降级到 flat */
        }

        return MMDB_OK;
    } else if (decision.index_type == VECTOR_INDEX_BRUTE_FORCE) {
        /* 选择器推荐暴力搜索，保持 flat 模式 */
        return MMDB_OK;
    } else {
        /* 其他索引类型（IVF-PQ/DiskANN）暂不支持，保持 flat 模式 */
        return MMDB_OK;
    }
}

/* ------------------------------------------------------------------ */
/* DDL：创建/校验 collection 数据表                                    */
/* ------------------------------------------------------------------ */

static int build_table_name(char* out, size_t out_cap, const char* coll) {
    int n = snprintf(out, out_cap, "mmdb_vec_%s", coll);
    if (n < 0 || (size_t)n >= out_cap) return MMDB_ERR_INVALID;
    return MMDB_OK;
}

int mmdb_vectors_ensure_table(mmdb_collection_t* coll) {
    if (!coll || coll->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;

    char tname[128];
    if (build_table_name(tname, sizeof(tname), coll->name) != MMDB_OK)
        return MMDB_ERR_INVALID;

    /* 检查 sqlite_master 中是否已存在 */
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(
        coll->sdb,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;", NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    mmdb_sqlite_bind_text(stmt, 1, tname);
    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    if (exists) return MMDB_OK;

    char ddl[256];
    snprintf(ddl, sizeof(ddl),
             "CREATE TABLE IF NOT EXISTS %s ("
             "  id BLOB PRIMARY KEY,"
             "  vector BLOB NOT NULL,"
             "  metadata TEXT,"
             "  text TEXT,"
             "  created_at INTEGER NOT NULL"
             ");",
             tname);
    return mmdb_sqlite_exec(coll->sdb, ddl);
}

/* ------------------------------------------------------------------ */
/* 公共 API                                                            */
/* ------------------------------------------------------------------ */

int mmdb_vectors_add(mmdb_collection_t* c, const mmdb_vector_t* vecs, size_t n) {
    if (!c || !vecs || n == 0) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;
    if (mmdb_vectors_ensure_table(c) != MMDB_OK) return MMDB_ERR_IO;

    /* 写操作：获取写锁 */
    mmdb_rwlock_wrlock(c->coll_lock);

    /* 优化：批量写入前临时禁用自动 checkpoint，完成后恢复 */
    sqlite3_exec(c->sdb, "PRAGMA wal_autocheckpoint=0;", NULL, NULL, NULL);

    /* BEGIN IMMEDIATE 获取写锁，避免 WAL 锁竞争 */
    sqlite3_exec(c->sdb, "BEGIN IMMEDIATE;", NULL, NULL, NULL);

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO %s(id, vector, metadata, text, created_at) "
             "VALUES (?, ?, ?, ?, ?);",
             tname);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) {
        sqlite3_exec(c->sdb, "ROLLBACK;", NULL, NULL, NULL);
        mmdb_rwlock_unlock(c->coll_lock, 1);
        sqlite3_exec(c->sdb, "PRAGMA wal_autocheckpoint=1000;", NULL, NULL, NULL);
        return MMDB_ERR_IO;
    }

    int rc = MMDB_OK;
    int64_t now = (int64_t)time(NULL);  /* 只获取一次时间戳，避免 10K 次系统调用 */

    for (size_t i = 0; i < n; i++) {
        const mmdb_vector_t* v = &vecs[i];
        if (v->dim != c->schema.vector_dim) {
            rc = MMDB_ERR_INVALID;
            break;
        }
        sqlite3_reset(stmt);

        /*
         * 优化要点：
         * 1. 使用 SQLITE_STATIC 代替 SQLITE_TRANSIENT，避免每行 malloc+memcpy
         *    （向量数据 512 字节 × 10K = 5MB 的无谓拷贝）
         * 2. 直接调用 sqlite3_bind_* 跳过封装函数开销
         * 3. 省略 sqlite3_clear_bindings —— 后续 bind 会覆盖旧值，无需显式清空
         */
        sqlite3_bind_blob(stmt, 1, v->id, (int)v->id_len, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, v->vector, (int)(v->dim * sizeof(float)),
                          SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3,
                          v->metadata_json ? v->metadata_json : "",
                          -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4,
                          v->text ? v->text : "",
                          -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, now);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            rc = MMDB_ERR_IO;
            break;
        }
    }
    sqlite3_finalize(stmt);

    if (rc == MMDB_OK) {
        sqlite3_exec(c->sdb, "COMMIT;", NULL, NULL, NULL);
    } else {
        sqlite3_exec(c->sdb, "ROLLBACK;", NULL, NULL, NULL);
    }

    /* 恢复自动 checkpoint */
    sqlite3_exec(c->sdb, "PRAGMA wal_autocheckpoint=1000;", NULL, NULL, NULL);

    mmdb_rwlock_unlock(c->coll_lock, 1);
    return rc;
}

int mmdb_vectors_upsert(mmdb_collection_t* c, const mmdb_vector_t* vecs,
                        size_t n) {
    /* 使用 INSERT OR REPLACE 实现 upsert 语义 */
    if (!c || !vecs || n == 0) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;
    if (mmdb_vectors_ensure_table(c) != MMDB_OK) return MMDB_ERR_IO;

    /* 写操作：获取写锁 */
    mmdb_rwlock_wrlock(c->coll_lock);

    /* 批量写入前临时禁用自动 checkpoint，完成后恢复 */
    sqlite3_exec(c->sdb, "PRAGMA wal_autocheckpoint=0;", NULL, NULL, NULL);

    /* BEGIN IMMEDIATE 获取写锁 */
    sqlite3_exec(c->sdb, "BEGIN IMMEDIATE;", NULL, NULL, NULL);

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO %s(id, vector, metadata, text, created_at) "
             "VALUES (?, ?, ?, ?, ?);",
             tname);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) {
        sqlite3_exec(c->sdb, "ROLLBACK;", NULL, NULL, NULL);
        mmdb_rwlock_unlock(c->coll_lock, 1);
        sqlite3_exec(c->sdb, "PRAGMA wal_autocheckpoint=1000;", NULL, NULL, NULL);
        return MMDB_ERR_IO;
    }

    int rc = MMDB_OK;
    int64_t now = (int64_t)time(NULL);

    for (size_t i = 0; i < n; i++) {
        const mmdb_vector_t* v = &vecs[i];
        if (v->dim != c->schema.vector_dim) {
            rc = MMDB_ERR_INVALID;
            break;
        }
        sqlite3_reset(stmt);

        sqlite3_bind_blob(stmt, 1, v->id, (int)v->id_len, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, v->vector, (int)(v->dim * sizeof(float)),
                          SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3,
                          v->metadata_json ? v->metadata_json : "",
                          -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4,
                          v->text ? v->text : "",
                          -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, now);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            rc = MMDB_ERR_IO;
            break;
        }
    }
    sqlite3_finalize(stmt);

    if (rc == MMDB_OK) {
        sqlite3_exec(c->sdb, "COMMIT;", NULL, NULL, NULL);
    } else {
        sqlite3_exec(c->sdb, "ROLLBACK;", NULL, NULL, NULL);
    }

    sqlite3_exec(c->sdb, "PRAGMA wal_autocheckpoint=1000;", NULL, NULL, NULL);

    mmdb_rwlock_unlock(c->coll_lock, 1);
    return rc;
}

int mmdb_vectors_delete(mmdb_collection_t* c, const uint8_t* id, size_t id_len) {
    if (!c || !id || id_len == 0) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE id = ?;", tname);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    /* 写操作：获取写锁 */
    mmdb_rwlock_wrlock(c->coll_lock);
    mmdb_sqlite_bind_blob(stmt, 1, id, id_len);
    int rc = (sqlite3_step(stmt) == SQLITE_DONE) ? MMDB_OK : MMDB_ERR_IO;
    sqlite3_finalize(stmt);

    /* Phase 2: HNSW 同步 — 在 ID 映射表中标记删除，后续 rebuild 自然过滤 */
    if (rc == MMDB_OK && c->hnsw) {
        hnsw_wrapper_t* w = (hnsw_wrapper_t*)c->hnsw;
        /* 遍历映射表找到匹配的 HNSW ID 并标记删除 */
        for (size_t i = 0; i < w->id_map->count; i++) {
            if (w->id_map->sdk_ids[i] != NULL &&
                w->id_map->sdk_id_lens[i] == id_len &&
                memcmp(w->id_map->sdk_ids[i], id, id_len) == 0) {
                hnsw_id_map_mark_deleted(w->id_map, (int32_t)i);
                /* 标记 HNSW 内部 bitmap（如果 faiss_hnsw 支持） */
                if (w->index) {
                    /* faiss_hnsw 不直接暴露 delete API，通过 bitmap 字段标记 */
                    /* 搜索时会跳过已删除的向量（由 faiss_hnsw 内部处理） */
                }
                w->deleted_count++;
                break;
            }
        }
    }

    mmdb_rwlock_unlock(c->coll_lock, 1);
    return rc;
}

int mmdb_vectors_get(mmdb_collection_t* c, const uint8_t* id, size_t id_len,
                     mmdb_vector_t* out) {
    if (!c || !id || !out) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;

    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT vector, metadata, text FROM %s WHERE id = ?;", tname);
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_NOT_FOUND;

    /* 读操作：获取读锁（支持并发读） */
    mmdb_rwlock_rdlock(c->coll_lock);
    mmdb_sqlite_bind_blob(stmt, 1, id, id_len);
    int rc = MMDB_ERR_NOT_FOUND;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        /* 注：out 字段为 const，仅作返回值载体 */
        const void* vec_blob = sqlite3_column_blob(stmt, 0);
        int vec_bytes = sqlite3_column_bytes(stmt, 0);
        size_t dim = vec_bytes / sizeof(float);
        /* 由于 mmdb_vector_t 的字段都是 const，调用方需自行保证生命周期 */
        memcpy((void*)out->vector, vec_blob, vec_bytes);
        out->dim = dim;
        out->id = id;
        out->id_len = id_len;
        rc = MMDB_OK;
    }
    sqlite3_finalize(stmt);
    mmdb_rwlock_unlock(c->coll_lock, 0);
    return rc;
}

/* ------------------------------------------------------------------ */
/* KNN 搜索（Flat 索引：暴力扫描）                                     */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* 最大堆（push/pop top-K 最近邻）                                     */
/* ------------------------------------------------------------------ */

/* 堆元素 */
typedef struct {
    float   dist;
    uint8_t id[64];
    size_t  id_len;
} knn_cand_t;

/* 下沉：维持最大堆性质（堆顶最大） */
static void heap_siftdown(knn_cand_t* heap, size_t size, size_t i) {
    while (1) {
        size_t largest = i;
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        if (left < size && heap[left].dist > heap[largest].dist)
            largest = left;
        if (right < size && heap[right].dist > heap[largest].dist)
            largest = right;
        if (largest == i) break;
        /* 交换 */
        knn_cand_t tmp = heap[i];
        heap[i] = heap[largest];
        heap[largest] = tmp;
        i = largest;
    }
}

/* 上浮：新元素放入堆尾后上浮 */
static void heap_siftup(knn_cand_t* heap, size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (heap[parent].dist >= heap[i].dist) break;
        knn_cand_t tmp = heap[i];
        heap[i] = heap[parent];
        heap[parent] = tmp;
        i = parent;
    }
}

/* 比较函数（用于最终排序） */
static int cand_cmp(const void* a, const void* b) {
    float da = ((const knn_cand_t*)a)->dist;
    float db = ((const knn_cand_t*)b)->dist;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

int mmdb_vectors_search(mmdb_collection_t* c, const mmdb_query_t* q,
                        mmdb_result_t* out) {
    if (!c || !q || !out || !q->query_vector) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_VECTOR) return MMDB_ERR_INVALID;
    if (q->dim != c->schema.vector_dim) return MMDB_ERR_INVALID;
    memset(out, 0, sizeof(*out));

    if (mmdb_vectors_ensure_table(c) != MMDB_OK) return MMDB_ERR_IO;

    /* selector：自动决策是否启用 HNSW 索引 */
    mmdb_vectors_hnsw_ensure(c);

    /* 编译 filter */
    mmdb_filter_params_t fp;
    char* where = mmdb_filter_compile(q->filter_json, &fp);
    if (!where) {
        mmdb_filter_params_free(&fp);
        return MMDB_ERR_INVALID;
    }

    size_t top_k = q->top_k > 0 ? q->top_k : 10;

    /* Phase 2: HNSW 加速搜索路径（P4-T4.5：支持 metadata filter）
     * 当 HNSW 可用且 N >= 阈值时，filter 为空走原路径，filter 非空走 filter 路径。
     * 两者最终都使用 faiss_hnsw_search_filtered()：filter 为空时传 NULL 谓词。 */
    if (c->hnsw) {
        hnsw_wrapper_t* w = (hnsw_wrapper_t*)c->hnsw;
        if (w->index && faiss_hnsw_index_size(w->index) > 0) {
            /* P4-T4.5：filter 非空 → 构建 bitmap 谓词；filter 为空 → NULL 谓词 */
            hnsw_filter_ctx_t filter_ctx;
            int has_filter = (where[0] != '\0');
            int filter_rc = MMDB_OK;
            if (has_filter) {
                mmdb_rwlock_rdlock(c->coll_lock);
                filter_rc = build_filter_ctx(c, where, &fp, &filter_ctx);
                mmdb_rwlock_unlock(c->coll_lock, 0);
                if (filter_rc != MMDB_OK) {
                    free(where);
                    mmdb_filter_params_free(&fp);
                    return filter_rc;
                }
            }

            /* HNSW 搜索：获取候选 ID 和距离 */
            int32_t search_k = (int32_t)(top_k * 2);  /* 多取一些候选以补偿删除标记 */
            float* distances = (float*)calloc(search_k, sizeof(float));
            int32_t* hnsw_ids = (int32_t*)calloc(search_k, sizeof(int32_t));
            if (!distances || !hnsw_ids) {
                free(distances);
                free(hnsw_ids);
                free(where);
                free_filter_ctx(&filter_ctx);
                mmdb_filter_params_free(&fp);
                return MMDB_ERR_NOMEM;
            }

            /* 读操作：获取读锁（支持并发读） */
            mmdb_rwlock_rdlock(c->coll_lock);
            int32_t found = faiss_hnsw_search_filtered(
                w->index, (const float*)q->query_vector,
                search_k,
                has_filter ? hnsw_filter_predicate : NULL,
                has_filter ? (void*)&filter_ctx : NULL,
                hnsw_ids, distances);
            mmdb_rwlock_unlock(c->coll_lock, 0);

            if (found <= 0) {
                free(distances);
                free(hnsw_ids);
                free(where);
                free_filter_ctx(&filter_ctx);
                mmdb_filter_params_free(&fp);
                return MMDB_OK;  /* 无结果 */
            }

            /* 构建候选堆（过滤已删除的向量） */
            knn_cand_t* heap = (knn_cand_t*)calloc(top_k + 16, sizeof(knn_cand_t));
            if (!heap) {
                free(distances);
                free(hnsw_ids);
                free(where);
                free_filter_ctx(&filter_ctx);
                mmdb_filter_params_free(&fp);
                return MMDB_ERR_NOMEM;
            }
            size_t heap_size = 0;

            for (int32_t i = 0; i < found; i++) {
                int32_t h_id = hnsw_ids[i];
                /* 跳过已删除的向量 */
                if (h_id < 0 || (size_t)h_id >= w->id_map->count) continue;
                if (w->id_map->sdk_ids[h_id] == NULL) continue;  /* 已标记删除 */

                size_t sdk_id_len = w->id_map->sdk_id_lens[h_id];
                const uint8_t* sdk_id = w->id_map->sdk_ids[h_id];

                /* 最大堆维护 top-K */
                if (heap_size < top_k) {
                    knn_cand_t* cand = &heap[heap_size];
                    cand->dist = distances[i];
                    size_t cp = sdk_id_len < sizeof(cand->id) ? sdk_id_len : sizeof(cand->id);
                    memcpy(cand->id, sdk_id, cp);
                    cand->id_len = cp;
                    heap_siftup(heap, heap_size);
                    heap_size++;
                } else if (distances[i] < heap[0].dist) {
                    heap[0].dist = distances[i];
                    size_t cp = sdk_id_len < sizeof(heap[0].id) ? sdk_id_len : sizeof(heap[0].id);
                    memcpy(heap[0].id, sdk_id, cp);
                    heap[0].id_len = cp;
                    heap_siftdown(heap, heap_size, 0);
                }
            }

            free(distances);
            free(hnsw_ids);
            free(where);
            free_filter_ctx(&filter_ctx);

            /* 排序候选（按距离升序） */
            qsort(heap, heap_size, sizeof(knn_cand_t), cand_cmp);

            out->items = (mmdb_result_item_t*)calloc(heap_size, sizeof(mmdb_result_item_t));
            if (!out->items) {
                free(heap);
                mmdb_filter_params_free(&fp);
                return MMDB_ERR_NOMEM;
            }
            out->count = heap_size;

            /* 复制 ID + distance（不依赖 SQLite） */
            for (size_t i = 0; i < heap_size; i++) {
                out->items[i].id = (uint8_t*)malloc(heap[i].id_len);
                if (!out->items[i].id) {
                    for (size_t j = 0; j < i; j++) free(out->items[j].id);
                    free(out->items);
                    out->items = NULL;
                    free(heap);
                    mmdb_filter_params_free(&fp);
                    return MMDB_ERR_NOMEM;
                }
                memcpy((void*)out->items[i].id, heap[i].id, heap[i].id_len);
                out->items[i].id_len = heap[i].id_len;
                out->items[i].distance = heap[i].dist;
                out->items[i].metadata_json = NULL;
                out->items[i].text = NULL;
            }

            /* 批量回查 SQLite：一次查询取所有 metadata/text（O(1) round-trip） */
            if (heap_size > 0) {
                char tname[128];
                build_table_name(tname, sizeof(tname), c->name);

                /* 构造 WHERE id IN (?, ?, ?, ...) SQL */
                char back_sql[512];
                int pos = snprintf(back_sql, sizeof(back_sql),
                                   "SELECT id, metadata, text FROM %s WHERE id IN (",
                                   tname);
                for (size_t i = 0; i < heap_size && pos < (int)sizeof(back_sql) - 4; i++) {
                    int n = snprintf(back_sql + pos, sizeof(back_sql) - pos,
                                     "%s?", (i == 0) ? "" : ",");
                    if (n < 0 || n >= (int)sizeof(back_sql) - pos) break;
                    pos += n;
                }
                if (pos < (int)sizeof(back_sql) - 2) {
                    back_sql[pos++] = ')';
                    back_sql[pos++] = ';';
                    back_sql[pos] = '\0';
                }

                /* 读操作：单次获取读锁，整批回查 */
                mmdb_rwlock_rdlock(c->coll_lock);
                sqlite3_stmt* back_stmt = mmdb_sqlite_prepare(c->sdb, back_sql, NULL, 0);
                if (back_stmt) {
                    /* 绑定所有 ID */
                    for (size_t i = 0; i < heap_size; i++) {
                        sqlite3_bind_blob(back_stmt, (int)(i + 1),
                                          heap[i].id, (int)heap[i].id_len,
                                          SQLITE_TRANSIENT);
                    }
                    /* 单次扫描建立 id → (metadata, text) 映射 */
                    while (sqlite3_step(back_stmt) == SQLITE_ROW) {
                        const void* row_id = sqlite3_column_blob(back_stmt, 0);
                        int row_id_len = sqlite3_column_bytes(back_stmt, 0);
                        const char* meta = (const char*)sqlite3_column_text(back_stmt, 1);
                        const char* txt = (const char*)sqlite3_column_text(back_stmt, 2);

                        /* 在 out->items 中查找匹配的 ID（线性扫描，K 通常 <= 10） */
                        for (size_t i = 0; i < heap_size; i++) {
                            if (out->items[i].id_len == (size_t)row_id_len &&
                                memcmp(out->items[i].id, row_id, row_id_len) == 0) {
                                if (meta && meta[0] != '\0') {
                                    out->items[i].metadata_json = strdup(meta);
                                }
                                if (txt && txt[0] != '\0') {
                                    out->items[i].text = strdup(txt);
                                }
                                break;
                            }
                        }
                    }
                    sqlite3_finalize(back_stmt);
                }
                mmdb_rwlock_unlock(c->coll_lock, 0);
            }

            free(heap);
            mmdb_filter_params_free(&fp);
            return MMDB_OK;
        }
    }

    free(where);  /* 释放 filter（HNSW 路径已提前释放） */

    /* Flat 暴力扫描路径 */
    char tname[128];
    build_table_name(tname, sizeof(tname), c->name);
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, vector, metadata, text FROM %s;", tname);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) {
        mmdb_filter_params_free(&fp);
        return MMDB_ERR_IO;
    }

    int bind_idx = mmdb_filter_bind(stmt, &fp, 1);
    mmdb_filter_params_free(&fp);
    if (bind_idx < 0) {
        sqlite3_finalize(stmt);
        return MMDB_ERR_INVALID;
    }

    knn_cand_t* heap = (knn_cand_t*)calloc(top_k + 16, sizeof(knn_cand_t));
    if (!heap) {
        sqlite3_finalize(stmt);
        return MMDB_ERR_NOMEM;
    }
    size_t heap_size = 0;

    /* 读操作：获取读锁（支持并发读） */
    mmdb_rwlock_rdlock(c->coll_lock);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* id_blob = sqlite3_column_blob(stmt, 0);
        size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
        const void* vec_blob = sqlite3_column_blob(stmt, 1);
        int vec_bytes = sqlite3_column_bytes(stmt, 1);
        size_t dim = (size_t)vec_bytes / sizeof(float);

        if (dim != q->dim) continue;

        /* L2 平方距离（SIMD 加速） */
        const float* a = (const float*)q->query_vector;
        const float* b = (const float*)vec_blob;
        float dist = 0.0f;

#if defined(__AVX2__) || defined(_MSC_VER)
        static int avx2_checked = 0;
        static int avx2_available = 0;
        if (!avx2_checked) {
            avx2_available = cpu_has_avx2();
            avx2_checked = 1;
        }
        if (avx2_available) {
            dist = l2_distance_simd(a, b, dim);
        } else {
            dist = l2_distance_scalar(a, b, dim);
        }
#else
        dist = l2_distance_scalar(a, b, dim);
#endif

        /* 使用最大堆维护 top-K 候选（堆顶为最大距离，新距离更小则替换） */
        if (heap_size < top_k) {
            /* 堆未满，直接放入堆尾并上浮 */
            knn_cand_t* cand = &heap[heap_size];
            cand->dist = dist;
            size_t cp = id_len < sizeof(cand->id) ? id_len : sizeof(cand->id);
            memcpy(cand->id, id_blob, cp);
            cand->id_len = cp;
            heap_siftup(heap, heap_size);
            heap_size++;
        } else if (dist < heap[0].dist) {
            /* 新距离小于堆顶（最大值），替换堆顶并下沉 */
            heap[0].dist = dist;
            size_t cp = id_len < sizeof(heap[0].id) ? id_len : sizeof(heap[0].id);
            memcpy(heap[0].id, id_blob, cp);
            heap[0].id_len = cp;
            heap_siftdown(heap, heap_size, 0);
        }
    }
    sqlite3_finalize(stmt);
    mmdb_rwlock_unlock(c->coll_lock, 0);

    /* 排序并填充结果 */
    qsort(heap, heap_size, sizeof(knn_cand_t), cand_cmp);

    out->items = (mmdb_result_item_t*)calloc(heap_size, sizeof(mmdb_result_item_t));
    if (!out->items) {
        free(heap);
        return MMDB_ERR_NOMEM;
    }

    out->count = heap_size;
    for (size_t i = 0; i < heap_size; i++) {
        out->items[i].id = (uint8_t*)malloc(heap[i].id_len);
        if (!out->items[i].id) {
            /* 回滚已分配的 id */
            for (size_t j = 0; j < i; j++) free(out->items[j].id);
            free(out->items);
            out->items = NULL;
            free(heap);
            return MMDB_ERR_NOMEM;
        }
        memcpy((void*)out->items[i].id, heap[i].id, heap[i].id_len);
        out->items[i].id_len = heap[i].id_len;
        out->items[i].distance = heap[i].dist;
        out->items[i].metadata_json = NULL;
        out->items[i].text = NULL;
    }
    free(heap);
    return MMDB_OK;
}