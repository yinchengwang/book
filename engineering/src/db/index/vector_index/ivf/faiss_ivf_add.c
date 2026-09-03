/*
 * faiss_ivf_add.c
 *
 * 向量添加: 将新向量路由到对应倒排桶并记录元数据。
 *
 * === 添加流程 ===
 *
 * 1. 扩容存储区:
 *    Heap 模式: vector_refs 数组扩容，向量写入 heap_store
 *    兼容模式: vectors 数组用 realloc 扩展到 n_total + n
 *    若启用量化，codes 数组同步扩展，并分配残差计算临时缓冲。
 *
 * 2. 路由 (每个向量):
 *    a) 遍历一级中心，找到最近的一级簇
 *    b) 在该一级簇内遍历二级中心，找到最近的二级桶
 *    c) 将向量 id 追加到该桶末尾，记录 DirectMap 映射
 *
 * 3. PQ 编码 (每个向量，若启用量化):
 *    a) 计算向量与其桶中心的残差: residual = vector - centroid
 *    b) 对残差做 PQ 编码，存储到 codes 数组
 *    c) 搜索时用 ADC 距离表 + codes 代替完整距离计算
 *
 * === 关于 realloc 的安全性 ===
 * vectors/codes 的 realloc 失败时不会损坏原指针:
 *   new_vectors = realloc(index->vectors, ...);
 *   if (!new_vectors) return -1;   // index->vectors 仍有效
 *   index->vectors = new_vectors;  // 只在成功时更新
 */

#include "faiss_ivf_private.h"

#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>

/*
 * faiss_ivf_store_vector - 将向量写入存储区
 *
 * Heap 模式: 调用 heap_vector_insert 写入并记录引用
 * 兼容模式: 追加到 index->vectors 数组
 *
 * 返回 0 表示成功，非 0 表示失败。
 */
static int faiss_ivf_store_vector(faiss_ivf_t *index,
                                  const float *vector,
                                  int32_t new_id)
{
    if (index == NULL || vector == NULL || new_id < 0) {
        return -1;
    }

    if (index->heap_store != NULL) {
        /* 扩展 vector_refs 数组 */
        uint32_t new_size = (uint32_t)(new_id + 1);
        vector_ref_t *new_refs = (vector_ref_t *)realloc(
            index->vector_refs,
            (size_t)new_size * sizeof(vector_ref_t));
        if (new_refs == NULL) {
            return -1;
        }
        index->vector_refs = new_refs;
        index->vector_refs_size = new_size;

        /* 写入 Heap 并记录引用 */
        vector_ref_t ref = heap_vector_insert(index->heap_store, vector);
        if (!vector_ref_is_valid(&ref)) {
            return -1;
        }
        index->vector_refs[new_id] = ref;
        return 0;
    }

    /* 兼容路径: 直接存入 vectors 数组（已在调用方扩容） */
    memcpy(&index->vectors[new_id * index->dims],
           vector,
           (size_t)index->dims * sizeof(float));
    return 0;
}

int32_t faiss_ivf_index_add(faiss_ivf_t *index, int32_t n, const float *vectors)
{
    int32_t dims;
    float *new_vectors;
    uint8_t *new_codes = NULL;
    float *residual = NULL;
    int32_t i;

    if (!index || !index->trained || n <= 0 || !vectors) {
        return -1;
    }
    /* 量化器必须已训练（在 train 第 3 阶段完成） */
    if (index->quantizer && !quantizer_is_trained(index->quantizer)) {
        return -1;
    }

    /* ── 第一步: 扩展原始向量存储区 + 可选编码区 ── */
    dims = index->dims;

    /* Heap 模式: 扩展 vector_refs 数组（实际写入在 store_vector 中） */
    if (index->heap_store == NULL) {
        /* 兼容路径: 扩展 vectors 数组 */
        new_vectors = (float *)realloc(index->vectors, (size_t)(index->n_total + n) * dims * sizeof(float));
        if (!new_vectors) {
            return -1;
        }
        index->vectors = new_vectors;
    }

    if (index->quantizer) {
        new_codes = (uint8_t *)realloc(index->codes, (size_t)(index->n_total + n) * index->code_size * sizeof(uint8_t));
        if (!new_codes) {
            return -1;
        }
        index->codes = new_codes;
        /* 残差缓冲区复用，每个向量计算后即编码，不保留残差 */
        residual = (float *)malloc((size_t)dims * sizeof(float));
        if (!residual) {
            return -1;
        }
    }

    /* ── 第二步: 逐个路由向量到倒排桶 ── */
    for (i = 0; i < n; ++i) {
        const float *vector = &vectors[i * dims];
        int32_t new_id = index->n_total + i;

        /* 存储向量（Heap 或兼容模式） */
        if (faiss_ivf_store_vector(index, vector, new_id) != 0) {
            free(residual);
            return -1;
        }

        /* 两级路由: 先粗粒度（一级簇）再细粒度（二级桶） */
        int32_t best_primary = faiss_ivf_find_nearest_primary_centroid(index, vector);
        int32_t best_secondary = faiss_ivf_find_nearest_secondary_centroid(index, best_primary, vector);
        int32_t bucket_id = faiss_ivf_get_bucket_id(index, best_primary, best_secondary);

        /* 添加前的桶大小即为该向量在桶内的偏移 */
        size_t offset = faiss_inverted_lists_list_size(index->invlists, (size_t)bucket_id);

        /* 向倒排桶追加向量 id。code=NULL 因为编码稍后单独写入 codes 数组 */
        if (faiss_inverted_lists_add_entry(index->invlists, (size_t)bucket_id, new_id, NULL) != 0) {
            free(residual);
            return -1;
        }

        /* 写入 DirectMap: id → (bucket_id, offset)，支持 O(1) 删除和重建 */
        faiss_direct_map_add(&index->direct_map, new_id, bucket_id, (int64_t)offset);

        /* ── 第三步: PQ 编码残差（若启用）── */
        if (index->quantizer) {
            /* 残差 = 原始向量 - 桶中心。用残差而非原始向量做编码，
             * 因为残差分布更集中（接近零均值），PQ 量化误差更小。 */
            faiss_ivf_compute_residual(residual,
                                       vector,
                                       faiss_ivf_get_bucket_centroid_ptr(index, best_primary, best_secondary),
                                       dims);
            if (quantizer_encode(index->quantizer,
                                      residual,
                                      &index->codes[new_id * index->code_size]) != 0) {
                free(residual);
                return -1;
            }
        }
    }

    free(residual);
    index->n_total += n;
    return 0;
}
