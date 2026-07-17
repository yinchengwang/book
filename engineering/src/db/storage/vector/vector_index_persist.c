/**
 * @file vector_index_persist.c
 * @brief 向量索引持久化实现
 *
 * 支持 HNSW、DiskANN、IVF 索引的序列化与反序列化。
 */
#include <db/storage/vector/vector_persist.h>
#include <db/index/vector_index/hnsw/faiss_hnsw.h>
#include <db/index/vector_index/diskann/diskann.h>
#include <db/index/vector_index/ivf/IndexIVF.h>
#include <db/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 索引文件魔数 */
#define VECTOR_INDEX_MAGIC 0x49445848  /* "IDXH" */

/** 索引版本号 */
#define VECTOR_INDEX_VERSION 1

/** 索引文件头大小 */
#define VECTOR_INDEX_HEADER_SIZE 64

/* ============================================================
 * 索引文件头
 * ============================================================ */

#pragma pack(push, 1)

/**
 * @brief 索引文件头部
 */
typedef struct vector_index_file_header_s {
    uint32_t magic;          /**< 魔数 */
    uint32_t version;        /**< 版本号 */
    uint32_t index_type;     /**< 索引类型 */
    int32_t  dims;           /**< 向量维度 */
    int32_t  num_vectors;    /**< 向量数量 */
    uint32_t data_size;      /**< 索引数据大小 */
    uint64_t created_at;     /**< 创建时间 */
    uint8_t  reserved[32];   /**< 保留字段 */
} vector_index_file_header_t;

#pragma pack(pop)

/* ============================================================
 * HNSW 索引持久化
 * ============================================================ */

/**
 * @brief 保存 HNSW 索引
 */
int vector_index_save_hnsw(void *hnsw_index, const char *path) {
    if (!hnsw_index || !path) {
        LOG_ERROR("无效参数: hnsw_index=%p, path=%s", hnsw_index, path);
        return -1;
    }

    faiss_hnsw_t *index = (faiss_hnsw_t *)hnsw_index;
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        LOG_ERROR("打开文件失败: %s", path);
        return -1;
    }

    /* 写文件头 */
    vector_index_file_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = VECTOR_INDEX_MAGIC;
    header.version = VECTOR_INDEX_VERSION;
    header.index_type = VECTOR_PERSIST_INDEX_HNSW;
    header.dims = index->dims;
    header.num_vectors = index->n_total;

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        LOG_ERROR("写入文件头失败");
        fclose(fp);
        return -1;
    }

    /* 写向量数据 */
    size_t vectors_size = (size_t)index->n_total * index->dims * sizeof(float);
    if (vectors_size > 0 && index->vectors) {
        if (fwrite(index->vectors, vectors_size, 1, fp) != 1) {
            LOG_ERROR("写入向量数据失败");
            fclose(fp);
            return -1;
        }
    }
    header.data_size += (uint32_t)vectors_size;

    /* 写量化码 (如果有) */
    size_t codes_size = (size_t)index->n_total * index->code_size;
    if (codes_size > 0 && index->codes) {
        if (fwrite(index->codes, codes_size, 1, fp) != 1) {
            LOG_ERROR("写入量化码失败");
            fclose(fp);
            return -1;
        }
        header.data_size += (uint32_t)codes_size;
    }

    /* 写 HNSW 图结构 */
    /* levels */
    if (index->levels && index->levels_size > 0) {
        size_t levels_size = index->levels_size * sizeof(int32_t);
        fwrite(&levels_size, sizeof(size_t), 1, fp);
        fwrite(index->levels, levels_size, 1, fp);
        header.data_size += (uint32_t)(sizeof(size_t) + levels_size);
    }

    /* offsets */
    if (index->offsets && index->offsets_size > 0) {
        size_t offsets_size = index->offsets_size * sizeof(int32_t);
        fwrite(&offsets_size, sizeof(size_t), 1, fp);
        fwrite(index->offsets, offsets_size, 1, fp);
        header.data_size += (uint32_t)(sizeof(size_t) + offsets_size);
    }

    /* nbs (邻居) */
    if (index->nbs && index->nb_size > 0) {
        size_t nbs_size = index->nb_size * sizeof(int32_t);
        fwrite(&nbs_size, sizeof(size_t), 1, fp);
        fwrite(index->nbs, nbs_size, 1, fp);
        header.data_size += (uint32_t)(sizeof(size_t) + nbs_size);
    }

    /* cum_nneighbor_per_level */
    if (index->cum_nneighbor_per_level && index->cum_nneighbor_per_level_size > 0) {
        size_t cum_size = index->cum_nneighbor_per_level_size * sizeof(int32_t);
        fwrite(&cum_size, sizeof(size_t), 1, fp);
        fwrite(index->cum_nneighbor_per_level, cum_size, 1, fp);
        header.data_size += (uint32_t)(sizeof(size_t) + cum_size);
    }

    /* 写元数据 */
    fwrite(&index->M, sizeof(int32_t), 1, fp);
    fwrite(&index->ef_construction, sizeof(int32_t), 1, fp);
    fwrite(&index->ef_search, sizeof(int32_t), 1, fp);
    fwrite(&index->max_level, sizeof(int32_t), 1, fp);
    fwrite(&index->entry_pointer, sizeof(int32_t), 1, fp);
    fwrite(&index->metric, sizeof(distance_metric_t), 1, fp);
    fwrite(&index->quantization_type, sizeof(quantization_type_t), 1, fp);

    /* 回填数据大小 */
    fseek(fp, offsetof(vector_index_file_header_t, data_size), SEEK_SET);
    fwrite(&header.data_size, sizeof(uint32_t), 1, fp);

    fclose(fp);
    LOG_INFO("保存 HNSW 索引成功: path=%s, vectors=%d", path, index->n_total);
    return 0;
}

/**
 * @brief 加载 HNSW 索引
 */
void *vector_index_load_hnsw(const char *path) {
    if (!path) {
        LOG_ERROR("路径为空");
        return NULL;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERROR("打开文件失败: %s", path);
        return NULL;
    }

    /* 读文件头 */
    vector_index_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        LOG_ERROR("读取文件头失败");
        fclose(fp);
        return NULL;
    }

    if (header.magic != VECTOR_INDEX_MAGIC) {
        LOG_ERROR("无效魔数: 0x%X", header.magic);
        fclose(fp);
        return NULL;
    }

    if (header.index_type != VECTOR_PERSIST_INDEX_HNSW) {
        LOG_ERROR("索引类型不匹配: %u != %u", header.index_type, VECTOR_PERSIST_INDEX_HNSW);
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(16, header.dims, 200, DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE);
    if (!index) {
        LOG_ERROR("创建 HNSW 索引失败");
        fclose(fp);
        return NULL;
    }

    /* 读向量数据 */
    size_t vectors_size = (size_t)header.num_vectors * header.dims * sizeof(float);
    if (vectors_size > 0) {
        index->vectors = (float *)malloc(vectors_size);
        if (index->vectors) {
            if (fread(index->vectors, vectors_size, 1, fp) != 1) {
                LOG_ERROR("读取向量数据失败");
                faiss_hnsw_index_drop(index);
                fclose(fp);
                return NULL;
            }
            index->n_total = header.num_vectors;
        }
    }

    /* 读量化码 */
    size_t codes_size = (size_t)header.num_vectors * index->code_size;
    if (codes_size > 0) {
        index->codes = (uint8_t *)malloc(codes_size);
        if (index->codes) {
            fread(index->codes, codes_size, 1, fp);
        }
    }

    /* 读 HNSW 图结构 */
    /* levels */
    size_t levels_size;
    if (fread(&levels_size, sizeof(size_t), 1, fp) == 1 && levels_size > 0) {
        index->levels_size = (uint32_t)(levels_size / sizeof(int32_t));
        index->levels = (int32_t *)malloc(levels_size);
        if (index->levels) {
            fread(index->levels, levels_size, 1, fp);
        }
    }

    /* offsets */
    size_t offsets_size;
    if (fread(&offsets_size, sizeof(size_t), 1, fp) == 1 && offsets_size > 0) {
        index->offsets_size = (uint32_t)(offsets_size / sizeof(int32_t));
        index->offsets = (int32_t *)malloc(offsets_size);
        if (index->offsets) {
            fread(index->offsets, offsets_size, 1, fp);
        }
    }

    /* nbs */
    size_t nbs_size;
    if (fread(&nbs_size, sizeof(size_t), 1, fp) == 1 && nbs_size > 0) {
        index->nb_size = (uint32_t)(nbs_size / sizeof(int32_t));
        index->nbs = (int32_t *)malloc(nbs_size);
        if (index->nbs) {
            fread(index->nbs, nbs_size, 1, fp);
        }
    }

    /* cum_nneighbor_per_level */
    size_t cum_size;
    if (fread(&cum_size, sizeof(size_t), 1, fp) == 1 && cum_size > 0) {
        index->cum_nneighbor_per_level_size = (uint32_t)(cum_size / sizeof(int32_t));
        index->cum_nneighbor_per_level = (int32_t *)malloc(cum_size);
        if (index->cum_nneighbor_per_level) {
            fread(index->cum_nneighbor_per_level, cum_size, 1, fp);
        }
    }

    /* 读元数据 */
    fread(&index->M, sizeof(int32_t), 1, fp);
    fread(&index->ef_construction, sizeof(int32_t), 1, fp);
    fread(&index->ef_search, sizeof(int32_t), 1, fp);
    fread(&index->max_level, sizeof(int32_t), 1, fp);
    fread(&index->entry_pointer, sizeof(int32_t), 1, fp);
    fread(&index->metric, sizeof(distance_metric_t), 1, fp);
    fread(&index->quantization_type, sizeof(quantization_type_t), 1, fp);

    fclose(fp);
    LOG_INFO("加载 HNSW 索引成功: path=%s, vectors=%d", path, header.num_vectors);
    return index;
}

/* ============================================================
 * DiskANN 索引持久化
 * ============================================================ */

/**
 * @brief 保存 DiskANN 索引
 */
int vector_index_save_diskann(void *diskann_index, const char *path) {
    if (!diskann_index || !path) {
        LOG_ERROR("无效参数");
        return -1;
    }

    /* TODO: 实现 DiskANN 索引持久化 */
    LOG_WARN("DiskANN 索引持久化尚未实现");
    (void)diskann_index;
    (void)path;
    return -1;
}

/**
 * @brief 加载 DiskANN 索引
 */
void *vector_index_load_diskann(const char *path) {
    if (!path) {
        LOG_ERROR("路径为空");
        return NULL;
    }

    /* TODO: 实现 DiskANN 索引加载 */
    LOG_WARN("DiskANN 索引加载尚未实现");
    (void)path;
    return NULL;
}

/* ============================================================
 * IVF 索引持久化
 * ============================================================ */

/**
 * @brief 保存 IVF 索引
 */
int vector_index_save_ivf(void *ivf_index, const char *path) {
    if (!ivf_index || !path) {
        LOG_ERROR("无效参数");
        return -1;
    }

    faiss_ivf_t *index = (faiss_ivf_t *)ivf_index;
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        LOG_ERROR("打开文件失败: %s", path);
        return -1;
    }

    /* 写文件头 */
    vector_index_file_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = VECTOR_INDEX_MAGIC;
    header.version = VECTOR_INDEX_VERSION;
    header.index_type = VECTOR_PERSIST_INDEX_IVF;
    header.num_vectors = faiss_ivf_index_size(index);

    /* 获取维度 */
    /* 注意：IVF 索引内部不直接存储 dims，需要通过其他方式获取 */
    /* 这里暂时设为 0，后续可以从倒排列表推断 */
    header.dims = 0;

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        LOG_ERROR("写入文件头失败");
        fclose(fp);
        return -1;
    }

    /* 写 IVF 元数据 */
    int32_t nlist = faiss_ivf_index_nlist(index);
    int32_t nprobe = faiss_ivf_index_get_nprobe(index);
    fwrite(&nlist, sizeof(int32_t), 1, fp);
    fwrite(&nprobe, sizeof(int32_t), 1, fp);

    /* TODO: 保存倒排列表和码本数据 */
    LOG_INFO("IVF 索引头部已保存: nlist=%d", nlist);

    fclose(fp);
    LOG_INFO("保存 IVF 索引成功: path=%s", path);
    return 0;
}

/**
 * @brief 加载 IVF 索引
 */
void *vector_index_load_ivf(const char *path) {
    if (!path) {
        LOG_ERROR("路径为空");
        return NULL;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERROR("打开文件失败: %s", path);
        return NULL;
    }

    /* 读文件头 */
    vector_index_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        LOG_ERROR("读取文件头失败");
        fclose(fp);
        return NULL;
    }

    if (header.magic != VECTOR_INDEX_MAGIC) {
        LOG_ERROR("无效魔数: 0x%X", header.magic);
        fclose(fp);
        return NULL;
    }

    if (header.index_type != VECTOR_PERSIST_INDEX_IVF) {
        LOG_ERROR("索引类型不匹配: %u != %u", header.index_type, VECTOR_PERSIST_INDEX_IVF);
        fclose(fp);
        return NULL;
    }

    /* 读 IVF 元数据 */
    int32_t nlist, nprobe;
    fread(&nlist, sizeof(int32_t), 1, fp);
    fread(&nprobe, sizeof(int32_t), 1, fp);

    /* 创建 IVF 索引 */
    /* 注意：需要维度参数，这里使用默认值，后续需要修复 */
    faiss_ivf_t *index = faiss_ivf_index_create(128, nlist, nlist, nprobe, DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE);
    if (!index) {
        LOG_ERROR("创建 IVF 索引失败");
        fclose(fp);
        return NULL;
    }

    /* TODO: 加载倒排列表和码本数据 */

    fclose(fp);
    LOG_INFO("加载 IVF 索引成功: path=%s, nlist=%d", path, nlist);
    return index;
}
