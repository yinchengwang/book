/*
 * faiss_ivf_core.c
 *
 * IVF 索引生命周期管理: 创建、销毁、重置、参数配置、删除、compact。
 *
 * === 生命周期 ===
 * create → [set_params] → train → add* → search* → reset → drop
 *                                   ↓
 *                              remove / compact
 *
 * === 删除策略 ===
 * remove_ids 采用"墓碑"机制: 将倒排桶中对应位置 id 标为 -1，
 * DirectMap 中对应槽置 -1，n_total 递减。
 * 墓碑会在 compact 时被物理移除（真空压缩），避免频繁 realloc。
 *
 * === compact 的必要性 ===
 * 多次删除后，桶内积累大量墓碑（id=-1），浪费扫描时间。
 * compact 遍历所有桶，将有效元素前移，截断尾部空洞，
 * 并同步更新 DirectMap 中的偏移映射。
 */

#include "faiss_ivf_private.h"

void faiss_ivf_index_reset(faiss_ivf_t *index)
{
    if (!index) {
        return;
    }

    /* reset 只清空"已训练结果和已添加数据"，保留索引配置（nlist/dims/metric 等） */
    free(index->vectors);
    free(index->codes);
    index->vectors = NULL;
    index->codes = NULL;
    index->n_total = 0;
    index->trained = false;
    index->code_size = 0;

    /* 倒排桶通过抽象层统一清空（保留已分配容量，避免下次 add 时频繁扩容） */
    faiss_inverted_lists_reset(index->invlists);
    faiss_direct_map_clear(&index->direct_map);

    memset(index->secondary_counts, 0, (size_t)index->nlist * sizeof(int32_t));
    if (index->quantizer) {
        quantizer_reset(index->quantizer);
    }
}

bool faiss_ivf_index_is_trained(const faiss_ivf_t *index)
{
    if (!index) {
        return false;
    }
    return index->trained;
}

int32_t faiss_ivf_index_size(const faiss_ivf_t *index)
{
    if (!index) {
        return -1;
    }
    return index->n_total;
}

int32_t faiss_ivf_index_nlist(const faiss_ivf_t *index)
{
    if (!index) {
        return -1;
    }
    return index->nlist;
}

int32_t faiss_ivf_index_nlist2(const faiss_ivf_t *index)
{
    if (!index) {
        return -1;
    }
    return index->nlist2;
}

quantization_type_t faiss_ivf_index_quantization_type(const faiss_ivf_t *index)
{
    return index ? index->quantization_type : QUANTIZATION_TYPE_NONE;
}

int32_t faiss_ivf_index_set_nprobe(faiss_ivf_t *index, int32_t nprobe)
{
    if (!index || nprobe <= 0) {
        return -1;
    }
    /* nprobe 越大扫描桶越多，精度越高但速度越慢。
     * 典型值: nlist 的 1%~10% */
    index->nprobe = nprobe;
    return 0;
}

int32_t faiss_ivf_index_get_nprobe(const faiss_ivf_t *index)
{
    if (!index) {
        return -1;
    }
    return index->nprobe;
}

/*
 * 量化参数只能在训练前修改。
 * 修改后会重建底层 quantizer 实例，之前训练的参数丢失。
 */
int32_t faiss_ivf_index_set_quantization_params(faiss_ivf_t *index,
                                                const faiss_ivf_quantization_params_t *params)
{
    if (!faiss_ivf_quantization_params_are_valid(index, params)) {
        return -1;
    }

    /* 已训练或已添加数据时禁止修改，避免 code_size 不一致 */
    if (index->trained || index->n_total > 0) {
        return -1;
    }

    index->quantization_params = *params;
    index->quantizer_config.pq_subquantizers = params->pq_m;
    index->quantizer_config.pq_bits = params->pq_bits;
    if (!quantizer_config_validate(&index->quantizer_config)) {
        return -1;
    }

    return faiss_ivf_rebuild_quantizer(index);
}

int32_t faiss_ivf_index_get_quantization_params(const faiss_ivf_t *index,
                                                faiss_ivf_quantization_params_t *params)
{
    if (!index || !params || index->quantization_type == QUANTIZATION_TYPE_NONE) {
        return -1;
    }

    *params = index->quantization_params;
    return 0;
}

int32_t faiss_ivf_index_set_training_params(faiss_ivf_t *index, const faiss_ivf_training_params_t *params)
{
    if (!index || !faiss_ivf_training_params_are_valid(params)) {
        return -1;
    }
    index->training_params = *params;
    return 0;
}

int32_t faiss_ivf_index_get_training_params(const faiss_ivf_t *index, faiss_ivf_training_params_t *params)
{
    if (!index || !params) {
        return -1;
    }
    *params = index->training_params;
    return 0;
}

/*
 * 创建 IVF 索引的三步分配:
 *   1. 分配主结构 + 基础参数赋值
 *   2. 分配训练结果数组（中心向量、计数）+ 倒排桶 + DirectMap
 *   3. 若启用量化，创建 quantizer 实例
 *
 * 任一步失败都会回滚已分配资源，保证不泄漏。
 */
faiss_ivf_t *faiss_ivf_index_create(int32_t dims,
                                    int32_t nlist,
                                    int32_t nlist2,
                                    int32_t nprobe,
                                    distance_metric_t metric,
                                    quantization_type_t quantization_type)
{
    faiss_ivf_t *index;

    if (dims <= 0 || nlist <= 0 || nlist2 <= 0 || nprobe <= 0 ||
        !distance_metric_is_valid(metric) || !quantization_type_is_valid(quantization_type)) {
        return NULL;
    }

    /* 第一步: 分配索引主结构并写入基础参数 */
    index = (faiss_ivf_t *)calloc(1, sizeof(faiss_ivf_t));
    if (!index) {
        return NULL;
    }

    index->dims = dims;
    index->nlist = nlist;
    index->nlist2 = nlist2;
    index->nprobe = nprobe;
    index->metric = metric;
    index->quantization_type = quantization_type;
    index->total_bucket_count = nlist * nlist2;
    faiss_ivf_set_default_training_params(&index->training_params);

    /* 第二步: 分配训练结果与倒排桶所需的基础内存 */
    index->primary_centroids = (float *)malloc((size_t)nlist * dims * sizeof(float));
    index->secondary_centroids = (float *)calloc((size_t)index->total_bucket_count * dims, sizeof(float));
    index->secondary_counts = (int32_t *)calloc((size_t)nlist, sizeof(int32_t));

    /* 倒排桶通过 InvertedLists 抽象层管理，初始 code_size=0（训练后才确定） */
    index->invlists = faiss_inverted_lists_create((size_t)index->total_bucket_count, 0);

    /* DirectMap 延迟初始化: type=1 启用数组映射，capacity=0 表示按需扩容 */
    faiss_direct_map_init(&index->direct_map, 1, 0);

    /* 第三步: 若启用量化，则同步创建 quantizer */
    if (quantization_type != QUANTIZATION_TYPE_NONE) {
        quantizer_config_init(&index->quantizer_config, dims, quantization_type);
        index->quantization_params.pq_m = index->quantizer_config.pq_subquantizers;
        index->quantization_params.pq_bits = index->quantizer_config.pq_bits;
        index->quantizer = quantizer_create(&index->quantizer_config);
    }

    /* 任一关键分配失败则回滚 */
    if (!index->primary_centroids || !index->secondary_centroids || !index->secondary_counts ||
        !index->invlists ||
        (quantization_type != QUANTIZATION_TYPE_NONE && !index->quantizer)) {
        free(index->primary_centroids);
        free(index->secondary_centroids);
        free(index->secondary_counts);
        faiss_inverted_lists_drop(index->invlists);
        quantizer_drop(index->quantizer);
        free(index);
        return NULL;
    }

    return index;
}

void faiss_ivf_index_drop(faiss_ivf_t *index)
{
    if (!index) {
        return;
    }

    /* drop 在 reset 的基础上继续释放训练配置和桶元数据。
     * 先 reset 清空数据区，再释放倒排桶、DirectMap、中心数组、索引本身。 */
    faiss_ivf_index_reset(index);
    quantizer_drop(index->quantizer);
    faiss_inverted_lists_drop(index->invlists);
    faiss_direct_map_drop(&index->direct_map);
    free(index->primary_centroids);
    free(index->secondary_centroids);
    free(index->secondary_counts);
    free(index);
}

/*
 * 批量删除向量。
 *
 * 采用墓碑策略而非物理删除:
 *   1. 通过 DirectMap 查询 id 对应的 (list_no, offset)
 *   2. 在倒排桶中将该位置 id 标为 -1
 *   3. 清除 DirectMap 映射
 *   4. n_total 递减
 *
 * 墓碑在 compact 时被物理移除。这种设计避免删除操作触发大量内存移动。
 */
int32_t faiss_ivf_index_remove_ids(faiss_ivf_t *index, const int32_t *ids_to_remove, int32_t n_remove)
{
    if (!index || !ids_to_remove || n_remove <= 0) {
        return -1;
    }

    int32_t removed = 0;
    for (int32_t i = 0; i < n_remove; i++) {
        int32_t id = ids_to_remove[i];
        if (id < 0 || id >= index->n_total) {
            continue;
        }

        /* DirectMap 查询: 返回值高 32 位 = list_no, 低 32 位 = offset */
        int64_t entry = faiss_direct_map_get(&index->direct_map, id);
        if (entry < 0) {
            continue; /* 已删除或不存在 */
        }

        int32_t list_no = (int32_t)(entry >> 32);
        int32_t offset = (int32_t)(entry & 0xFFFFFFFF);

        /* 在倒排桶中标记墓碑 */
        faiss_inverted_lists_remove_entry(index->invlists, (size_t)list_no, (size_t)offset);

        /* 清除 DirectMap 映射 */
        faiss_direct_map_remove(&index->direct_map, id);

        removed++;
    }

    index->n_total -= removed;
    return removed;
}

/*
 * 重建向量: 从索引中恢复指定 id 的原始向量。
 *
 * 非量化模式: 直接从 vectors 数组拷贝（O(1) 内存访问）。
 * 量化模式: 暂不支持，因为需要实现 quantizer_decode（PQ 解码是有损的）。
 */
int32_t faiss_ivf_index_reconstruct(const faiss_ivf_t *index, int32_t id, float *recons)
{
    if (!index || !recons || id < 0) {
        return -1;
    }

    /* 非量化模式：直接拷贝原始向量 */
    if (!index->quantizer || index->quantization_type == QUANTIZATION_TYPE_NONE) {
        if (id >= index->n_total) {
            return -1;
        }
        memcpy(recons, &index->vectors[id * index->dims], (size_t)index->dims * sizeof(float));
        return 0;
    }

    /* 量化模式暂不支持直接 decode（需 quantizer_decode API） */
    return -1;
}

/*
 * 真空压缩: 遍历所有桶，将墓碑后的有效元素前移，去除碎片。
 *
 * 压缩过程中同步更新 DirectMap 中的偏移——因为元素在桶内位置变化了。
 * 只更新位置确实改变的元素，已在正确位置的保持不变。
 */
void faiss_ivf_index_compact(faiss_ivf_t *index)
{
    if (!index) {
        return;
    }

    /* 遍历所有桶进行压缩，并更新 DirectMap 中的偏移 */
    for (int32_t bucket = 0; bucket < index->total_bucket_count; bucket++) {
        size_t old_size = faiss_inverted_lists_list_size(index->invlists, (size_t)bucket);
        const int32_t *ids = faiss_inverted_lists_get_ids(index->invlists, (size_t)bucket);

        /* 计算压缩后每个有效元素的新偏移，并同步 DirectMap */
        size_t new_offset = 0;
        for (size_t pos = 0; pos < old_size; pos++) {
            int32_t vid = ids[pos];
            if (vid >= 0) {
                /* 若元素前移了，更新 DirectMap 中的偏移 */
                if (pos != new_offset) {
                    faiss_direct_map_add(&index->direct_map, vid, bucket, (int64_t)new_offset);
                }
                new_offset++;
            }
        }

        /* 执行物理压缩（前移有效元素，截断尾部） */
        faiss_inverted_lists_compact_list(index->invlists, (size_t)bucket);
    }
}
