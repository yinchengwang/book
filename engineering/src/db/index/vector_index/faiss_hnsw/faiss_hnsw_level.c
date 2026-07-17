// faiss_hnsw_level.c
// 实现 faiss_hnsw_random_level：为新插入向量分配随机层号
// 参考 FAISS reference/open-source/faiss/faiss/impl/HNSW.cpp 的 HNSW::random_level()
//
// FAISS 原版逻辑：
//   int HNSW::random_level() {
//       std::uniform_real_distribution<> distrib(0.0, 1.0);
//       int l = 0;
//       for (double p = assign_probas[0]; distrib(rng) < p; l++) {
//           p = assign_probas[l + 1];
//       }
//       return l;
//   }
//
// 关键点：
//   - 从 level 0 开始，累积概率 p = assign_probas[0] = 1 - ml
//   - 生成 [0,1) 均匀随机数，如果 < p 则升到 level 1，更新 p = assign_probas[1]
//   - 重复直到随机数 >= p
//   - ml = 1.0 / log(M)；assign_probas[l] 表示"能进入第 l 层"的概率
//
// C 版本实现差异：
//   - 用 MT19937 实现的 faiss_hnsw_mt_uniform_01 替代 std::uniform_real_distribution
//   - 注意：assign_probas_size 必须 >= 2 才能确保 l+1 索引合法

#include "faiss_hnsw_internal.h"

int32_t faiss_hnsw_random_level(faiss_hnsw_t *idx, int32_t vec_id) {
    // 参数校验
    if (!idx || idx->assign_probas == NULL || idx->assign_probas_size <= 0) {
        return 0;
    }

    // 首次取随机数：使用 MT19937 产生 [0,1) 均匀分布
    // 注意：与 FAISS 不同，C 版本每次循环都重新取一次随机数（与 brief 描述一致）
    float rnd = faiss_hnsw_mt_uniform_01(&idx->rng_state);

    // 从 level 0 开始，p = assign_probas[0] = 1 - ml
    int32_t l = 0;
    float p = idx->assign_probas[0];

    // 循环条件：随机数 < 当前层概率 AND 未超出概率表范围
    // 边界保护：assign_probas_size - 1 防止 l+1 越界
    while (rnd < p && l < idx->assign_probas_size - 1) {
        l++;
        p = idx->assign_probas[l];
        rnd = faiss_hnsw_mt_uniform_01(&idx->rng_state);
    }

    // 抑制未使用参数警告（vec_id 在 HNSW::random_level 中实际未使用，
    // 仅用于在并行场景下将每个向量的随机数消耗解耦；保留以匹配 FAISS 接口签名）
    (void)vec_id;

    return l;
}