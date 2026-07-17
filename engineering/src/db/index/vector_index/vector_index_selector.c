/**
 * @file vector_index_selector.c
 * @brief 向量索引选择器实现
 *
 * 根据数据规模自动选择最优索引类型。
 */

#include "db/index/vector_index/vector_index_selector.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 每个向量的基准内存开销（字节） */
#define MEMORY_PER_VECTOR_4D 16

/** HNSW M 参数范围 */
#define HNSW_M_MIN 8
#define HNSW_M_MAX 64
#define HNSW_M_DEFAULT 16

/** HNSW ef 参数范围 */
#define HNSW_EF_MIN 50
#define HNSW_EF_MAX 500
#define HNSW_EF_DEFAULT 100

/** IVF nlist 参数公式 */
#define IVF_NLIST_SCALE_FACTOR 4

/** IVF nprobe 默认比例 */
#define IVF_NPROBE_RATIO 0.05

/** 维度阈值 */
#define DIM_THRESHOLD_LOW 64
#define DIM_THRESHOLD_MEDIUM 256

/** 数据规模阈值 */
#define SCALE_SMALL 10000
#define SCALE_MEDIUM 100000
#define SCALE_LARGE 1000000

/** 内存阈值（MB） */
#define MEMORY_THRESHOLD_LOW 256
#define MEMORY_THRESHOLD_MEDIUM 2048

/* ========================================================================
 * 内存估算函数
 * ======================================================================== */

static float estimate_hnsw_memory(uint64_t num_vectors, int32_t dim, int m, int ef) {
    (void)ef;
    /* HNSW 内存估算：
     * - 向量存储: num_vectors * dim * 4 bytes
     * - 图结构: num_vectors * m * 2 * 4 bytes (每条边 2 个 int)
     * - 层信息: num_vectors * log(num_vectors) * 4 bytes
     */
    float vec_mem = num_vectors * dim * 4.0f / (1024 * 1024);
    float graph_mem = num_vectors * m * 2.0f * 4.0f / (1024 * 1024);
    float layer_mem = num_vectors * (log(num_vectors + 1) * 0.5) * 4.0f / (1024 * 1024);

    return vec_mem + graph_mem + layer_mem;
}

static float estimate_ivf_pq_memory(uint64_t num_vectors, int32_t dim, int nlist, int pq_m, int pq_bits) {
    /* IVF-PQ 内存估算：
     * - 向量存储（压缩）: num_vectors * pq_m * 1 byte
     * - 聚类中心: nlist * dim * 4 bytes
     * - PQ 码书: pq_m * (2^pq_bits) * dim / pq_m * 4 bytes
     */
    float code_mem = num_vectors * pq_m * 1.0f / (1024 * 1024);
    float centroid_mem = nlist * dim * 4.0f / (1024 * 1024);
    int pq_k = 1 << pq_bits;
    float codebook_mem = pq_m * pq_k * 4.0f / (1024 * 1024);

    return code_mem + centroid_mem + codebook_mem;
}

/* ========================================================================
 * QPS 估算函数
 * ======================================================================== */

static float estimate_hnsw_qps(uint64_t num_vectors, int m, int ef) {
    (void)m;
    /* 简化 QPS 估算：
     * - HNSW 搜索复杂度 O(log(num_vectors))
     * - ef 越大精度越高但 QPS 越低
     */
    float log_n = log(num_vectors + 1);
    float base_qps = 100000.0f / log_n;  /* 假设 100K QPS 基准 */
    return base_qps * (100.0f / ef);
}

static float estimate_ivf_pq_qps(uint64_t num_vectors, int nlist, int nprobe) {
    /* IVF-PQ QPS 估算：
     * - 搜索复杂度 O(nprobe + nprobe * avg_list_size)
     * - nprobe 越大精度越高但 QPS 越低
     */
    float avg_list_size = (float)num_vectors / nlist;
    float base_complexity = nprobe * (1 + avg_list_size * 0.01);
    float base_qps = 50000.0f / (base_complexity + 1);
    return base_qps;
}

static float estimate_brute_qps(uint64_t num_vectors) {
    /* 暴力搜索 QPS 估算 */
    return 10000.0f / (num_vectors / 10000.0f + 1);
}

/* ========================================================================
 * 召回率估算函数
 * ======================================================================== */

static float estimate_hnsw_recall(int ef, int m) {
    /* HNSW 召回率估算（简化） */
    float ef_factor = (ef > 0) ? (float)ef / 200.0f : 0.5f;
    float m_factor = (float)m / 16.0f;
    float recall = 0.85f + 0.1f * ef_factor + 0.05f * m_factor;
    return (recall > 0.99f) ? 0.99f : recall;
}

static float estimate_ivf_pq_recall(int nprobe, int nlist) {
    /* IVF-PQ 召回率估算（简化） */
    float probe_ratio = (float)nprobe / nlist;
    float recall = 0.6f + 0.35f * probe_ratio;
    return (recall > 0.95f) ? 0.95f : recall;
}

/* ========================================================================
 * 主选择函数
 * ======================================================================== */

int vector_index_selector_choose(const vector_data_info_t *info,
                               vector_index_decision_t *decision) {
    if (info == NULL || decision == NULL) {
        return -1;
    }

    /* 默认决策 */
    memset(decision, 0, sizeof(vector_index_decision_t));

    uint64_t n = info->num_vectors;
    int32_t dim = info->dimension;
    uint64_t mem_mb = info->available_memory_mb > 0 ? info->available_memory_mb : 4096;

    /* 小规模数据：直接使用暴力搜索或简单索引 */
    if (n < SCALE_SMALL) {
        decision->index_type = VECTOR_INDEX_BRUTE_FORCE;
        decision->param1 = 0;
        decision->param2 = 0;
        decision->estimated_memory_mb = n * dim * 4.0f / (1024 * 1024);
        decision->estimated_qps = estimate_brute_qps(n);
        decision->estimated_recall = 1.0f;
        return 0;
    }

    /* 中等规模数据：选择 HNSW 或 IVF-PQ */
    if (n < SCALE_LARGE) {
        /* 内存充足且目标召回率高：选择 HNSW */
        if (mem_mb > MEMORY_THRESHOLD_MEDIUM || info->target_recall > 0.95f) {
            decision->index_type = VECTOR_INDEX_HNSW;

            /* 根据维度选择 M */
            int m = HNSW_M_DEFAULT;
            if (dim > DIM_THRESHOLD_MEDIUM) {
                m = 8;  /* 高维降低 M */
            } else if (dim < DIM_THRESHOLD_LOW) {
                m = 32; /* 低维可以增加 M */
            }

            /* 根据召回率选择 ef */
            int ef = HNSW_EF_DEFAULT;
            if (info->target_recall > 0.95f) {
                ef = 200;
            } else if (info->target_recall < 0.80f) {
                ef = 50;
            }

            decision->param1 = m;
            decision->param2 = ef;
            decision->estimated_memory_mb = estimate_hnsw_memory(n, dim, m, ef);
            decision->estimated_qps = estimate_hnsw_qps(n, m, ef);
            decision->estimated_recall = estimate_hnsw_recall(ef, m);

            return 0;
        }

        /* 内存受限：选择 IVF-PQ */
        decision->index_type = VECTOR_INDEX_IVF_PQ;

        int nlist = (int)(sqrt(n) * IVF_NLIST_SCALE_FACTOR);
        if (nlist < 64) nlist = 64;
        if (nlist > 4096) nlist = 4096;

        int pq_m = dim / 4;
        if (pq_m < 4) pq_m = 4;
        if (pq_m > 64) pq_m = 64;

        int nprobe = (int)(nlist * IVF_NPROBE_RATIO);
        if (nprobe < 1) nprobe = 1;

        decision->param1 = nlist;
        decision->param2 = nprobe;
        decision->estimated_memory_mb = estimate_ivf_pq_memory(n, dim, nlist, pq_m, 8);
        decision->estimated_qps = estimate_ivf_pq_qps(n, nlist, nprobe);
        decision->estimated_recall = estimate_ivf_pq_recall(nprobe, nlist);

        return 0;
    }

    /* 大规模数据：IVF-PQ 或 DiskANN */
    if (mem_mb < MEMORY_THRESHOLD_MEDIUM) {
        /* 内存受限：IVF-PQ */
        decision->index_type = VECTOR_INDEX_IVF_PQ;

        int nlist = (int)(sqrt(n) * 2);
        if (nlist < 256) nlist = 256;
        if (nlist > 16384) nlist = 16384;

        int pq_m = 16;  /* 更大的压缩比 */
        int nprobe = (int)(nlist * 0.02);  /* 降低 nprobe 提高性能 */
        if (nprobe < 1) nprobe = 1;

        decision->param1 = nlist;
        decision->param2 = nprobe;
        decision->estimated_memory_mb = estimate_ivf_pq_memory(n, dim, nlist, pq_m, 8);
        decision->estimated_qps = estimate_ivf_pq_qps(n, nlist, nprobe);
        decision->estimated_recall = estimate_ivf_pq_recall(nprobe, nlist);

        return 0;
    }

    /* 内存充足：HNSW */
    decision->index_type = VECTOR_INDEX_HNSW;
    decision->param1 = HNSW_M_MAX;
    decision->param2 = 500;
    decision->estimated_memory_mb = estimate_hnsw_memory(n, dim, HNSW_M_MAX, 500);
    decision->estimated_qps = estimate_hnsw_qps(n, HNSW_M_MAX, 500);
    decision->estimated_recall = estimate_hnsw_recall(500, HNSW_M_MAX);

    return 0;
}

void vector_index_selector_generate_params(vector_index_type_t index_type,
                                        const vector_data_info_t *info,
                                        int *param1, int *param2) {
    if (param1) *param1 = 0;
    if (param2) *param2 = 0;
    if (info == NULL) return;

    uint64_t n = info->num_vectors;
    int32_t dim = info->dimension;

    switch (index_type) {
    case VECTOR_INDEX_BRUTE_FORCE:
        /* 无参数 */
        break;

    case VECTOR_INDEX_HNSW:
        if (param1) {
            *param1 = (dim > DIM_THRESHOLD_MEDIUM) ? 8 :
                      (dim < DIM_THRESHOLD_LOW) ? 32 : HNSW_M_DEFAULT;
        }
        if (param2) {
            *param2 = (info->target_recall > 0.95f) ? 200 :
                      (info->target_recall < 0.80f) ? 50 : HNSW_EF_DEFAULT;
        }
        break;

    case VECTOR_INDEX_IVF_PQ:
        if (param1) {
            *param1 = (int)(sqrt(n) * IVF_NLIST_SCALE_FACTOR);
            if (*param1 < 64) *param1 = 64;
            if (*param1 > 4096) *param1 = 4096;
        }
        if (param2) {
            int nlist = param1 ? *param1 : (int)(sqrt(n) * IVF_NLIST_SCALE_FACTOR);
            *param2 = (int)(nlist * IVF_NPROBE_RATIO);
            if (*param2 < 1) *param2 = 1;
        }
        break;

    case VECTOR_INDEX_DISKANN:
        /* DiskANN 参数 */
        if (param1) *param1 = 64;
        if (param2) *param2 = 100;
        break;

    default:
        break;
    }
}

float vector_index_selector_estimate_memory(vector_index_type_t index_type,
                                        uint64_t num_vectors,
                                        int32_t dimension,
                                        int param1, int param2) {
    switch (index_type) {
    case VECTOR_INDEX_BRUTE_FORCE:
        return num_vectors * dimension * 4.0f / (1024 * 1024);

    case VECTOR_INDEX_HNSW:
        return estimate_hnsw_memory(num_vectors, dimension,
                                 param1 > 0 ? param1 : HNSW_M_DEFAULT,
                                 param2 > 0 ? param2 : HNSW_EF_DEFAULT);

    case VECTOR_INDEX_IVF_PQ: {
        int pq_m = dimension / 4;
        if (pq_m < 4) pq_m = 4;
        return estimate_ivf_pq_memory(num_vectors, dimension,
                                    param1 > 0 ? param1 : 256,
                                    pq_m, 8);
    }

    case VECTOR_INDEX_DISKANN:
        /* DiskANN 通常内存占用较小（依赖磁盘） */
        return num_vectors * dimension * 0.5f / (1024 * 1024);

    default:
        return 0;
    }
}

const char *vector_index_selector_get_name(vector_index_type_t index_type) {
    switch (index_type) {
    case VECTOR_INDEX_BRUTE_FORCE: return "brute_force";
    case VECTOR_INDEX_HNSW: return "hnsw";
    case VECTOR_INDEX_IVF_PQ: return "ivf_pq";
    case VECTOR_INDEX_DISKANN: return "diskann";
    default: return "unknown";
    }
}

const char *vector_index_selector_get_desc(vector_index_type_t index_type) {
    switch (index_type) {
    case VECTOR_INDEX_BRUTE_FORCE: return "暴力搜索（精确但慢）";
    case VECTOR_INDEX_HNSW: return "HNSW 图索引（快速高召回）";
    case VECTOR_INDEX_IVF_PQ: return "IVF-PQ 聚类索引（内存优化）";
    case VECTOR_INDEX_DISKANN: return "DiskANN 磁盘索引（超大规模）";
    default: return "未知索引类型";
    }
}

/* ========================================================================
 * VectorIndexType → vector_index_type_t 映射
 *
 * 接受 VectorAPI 的枚举值（vector_query.h 中定义），
 * 返回 Selector 内部枚举（vector_index_selector.h 中定义）。
 * 无法映射时默认回退到 VECTOR_INDEX_HNSW。
 *
 * 映射表：
 *   VectorAPI                    → Selector
 *   VECTOR_INDEX_HNSW    (0)     → VECTOR_INDEX_HNSW       (1)
 *   VECTOR_INDEX_DISKANN (1)     → VECTOR_INDEX_DISKANN    (3)
 *   VECTOR_INDEX_IVF     (2)     → VECTOR_INDEX_IVF_PQ     (2)
 *   VECTOR_INDEX_BRUTE_FORCE (3) → VECTOR_INDEX_BRUTE_FORCE (0)
 *   VECTOR_INDEX_AUTO    (99)    → VECTOR_INDEX_HNSW       (1) 默认
 * ======================================================================== */
vector_index_type_t vector_index_type_convert(int type) {
    switch (type) {
    case 0: return VECTOR_INDEX_HNSW;
    case 1: return VECTOR_INDEX_DISKANN;
    case 2: return VECTOR_INDEX_IVF_PQ;
    case 3: return VECTOR_INDEX_BRUTE_FORCE;
    case 99: return VECTOR_INDEX_HNSW;  /* AUTO 默认 HNSW */
    default: return VECTOR_INDEX_HNSW;  /* 未知值回退 */
    }
}
