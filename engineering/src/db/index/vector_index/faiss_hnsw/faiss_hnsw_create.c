// faiss_hnsw_create.c
// 实现 faiss_hnsw_index_create 和 faiss_hnsw_index_drop
// 参考 FAISS HNSW::HNSW(int M, ...) 构造函数以及 set_default_probas 逻辑
// 完整重写为纯 C 版本

#include "faiss_hnsw_internal.h"

#include <stdio.h>

// =============================================================================
// Mersenne Twister RNG 实现（与 FAISS RandomGenerator / std::mt19937 一致）
// =============================================================================
// 实现位于 faiss_hnsw_internal.h 中，便于其他 .c 文件复用
// （random_level 需要调用 mt_extract 提取随机数）

uint32_t faiss_hnsw_mt_extract(faiss_hnsw_mt_state_t *mt) {
    uint32_t y;
    static const uint32_t mag01[2] = {0x0UL, 0x9908b3dfUL};
    int kk;

    if (mt->index >= 624) {
        // 生成下一批状态
        for (kk = 0; kk < 227; kk++) {
            y = (mt->state[kk] & 0x80000000UL) | (mt->state[kk + 1] & 0x7fffffffUL);
            mt->state[kk] = mt->state[kk + 397] ^ (y >> 1) ^ mag01[y & 1UL];
        }
        for (; kk < 623; kk++) {
            y = (mt->state[kk] & 0x80000000UL) | (mt->state[kk + 1] & 0x7fffffffUL);
            mt->state[kk] = mt->state[kk + -227] ^ (y >> 1) ^ mag01[y & 1UL];
        }
        y = (mt->state[623] & 0x80000000UL) | (mt->state[0] & 0x7fffffffUL);
        mt->state[623] = mt->state[396] ^ (y >> 1) ^ mag01[y & 1UL];
        mt->index = 0;
    }

    y = mt->state[mt->index++];
    y ^= (y >> 18);
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);
    return y;
}

// 提取 [0, 1) 范围的 float
// 取 mt_extract() 结果的高 24 位作为尾数
// 即 uniform [0, 1) = extract() >> 8 * (1/16777216)
float faiss_hnsw_mt_uniform_01(faiss_hnsw_mt_state_t *mt) {
    return (faiss_hnsw_mt_extract(mt) >> 8) * (1.0f / 16777216.0f);
}

// =============================================================================
// faiss_hnsw_index_drop
// 释放 faiss_hnsw_t 持有的所有动态内存
// =============================================================================

void faiss_hnsw_index_drop(faiss_hnsw_t *index) {
    if (!index) {
        return;
    }

    free(index->vectors);
    free(index->levels);
    free(index->offsets);
    free(index->neighbors);
    free(index->assign_probas);
    free(index->cum_nneighbor);
    free(index->codes);
    free(index->delete_bitmap);
    free(index);
}

// =============================================================================
// faiss_hnsw_index_create
// 创建 HNSW 索引实例，初始化所有默认参数
// 与 FAISS HNSW(int M) + set_default_probas(M, 1.0/log(M)) 等价
// =============================================================================

faiss_hnsw_t *faiss_hnsw_index_create(int32_t M,
                                      int32_t dims,
                                      int32_t ef_construction,
                                      distance_metric_t metric,
                                      quantization_type_t quant_type) {
    // 参数校验
    if (M <= 0 || dims <= 0 || ef_construction <= 0) {
        return NULL;
    }

    faiss_hnsw_t *idx = (faiss_hnsw_t *)calloc(1, sizeof(faiss_hnsw_t));
    if (!idx) {
        return NULL;
    }

    // 基本参数
    idx->dims = dims;
    idx->M = M;
    idx->ef_construction = ef_construction;
    idx->ef_search = ef_construction;  // 默认 ef_search 等于 ef_construction
    idx->metric = metric;
    idx->quantization_type = quant_type;

    // 状态参数
    idx->n_total = 0;
    idx->max_level = -1;
    idx->entry_point = -1;

    // 数据指针（lazy 分配）
    idx->vectors = NULL;
    idx->codes = NULL;
    idx->code_size = 0;
    idx->levels = NULL;
    idx->offsets = NULL;
    idx->neighbors = NULL;
    idx->quantizer = NULL;
    idx->delete_bitmap = NULL;

    // ---------------------------------------------------------------------
    // 初始化 MT RNG（seed = 12345 与 FAISS 默认行为一致）
    // ---------------------------------------------------------------------
    idx->rng_state.index = 624;  // 首次调用 mt_extract 时会触发状态生成
    {
        uint32_t seed = 12345;
        for (int i = 0; i < 624; i++) {
            idx->rng_state.state[i] = seed;
            seed = (1812433253UL * seed) ^ ((seed >> 30) + 1);
        }
    }

    // ---------------------------------------------------------------------
    // 初始化层分配概率表（与 FAISS set_default_probas 一致）
    //   ml = 1.0 / log(M)
    //   level 0:    proba = 1 - ml
    //   level l>=1: proba = ml * (1 - ml)^(l - 1)
    // ---------------------------------------------------------------------
    {
        const int max_levels = 64;  // 足够覆盖绝大多数 HNSW 实际层数
        idx->assign_probas_size = max_levels + 1;
        idx->assign_probas = (float *)malloc((size_t)(max_levels + 1) * sizeof(float));
        if (!idx->assign_probas) {
            faiss_hnsw_index_drop(idx);
            return NULL;
        }

        double ml = 1.0 / log((double)M);
        idx->assign_probas[0] = (float)(1.0 - ml);
        for (int i = 1; i <= max_levels; i++) {
            idx->assign_probas[i] = (float)(ml * pow(1.0 - ml, (double)(i - 1)));
        }
    }

    // ---------------------------------------------------------------------
    // 初始化 cum_nneighbor（每层累计邻居数上限）
    //   cum_nneighbor[0] = 2 * M
    //   cum_nneighbor[l] = cum_nneighbor[l-1] + M
    // 与 FAISS HNSW.cpp 中 cum_nneighbor_per_level 含义一致
    // ---------------------------------------------------------------------
    {
        const int max_levels = 64;
        idx->cum_nneighbor_size = max_levels + 1;
        idx->cum_nneighbor = (int32_t *)malloc((size_t)(max_levels + 1) * sizeof(int32_t));
        if (!idx->cum_nneighbor) {
            faiss_hnsw_index_drop(idx);
            return NULL;
        }

        idx->cum_nneighbor[0] = 2 * M;
        for (int i = 1; i <= max_levels; i++) {
            idx->cum_nneighbor[i] = idx->cum_nneighbor[i - 1] + M;
        }
    }

    return idx;
}
