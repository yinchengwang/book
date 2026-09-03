/**
 * @file streaming_index.c
 * @brief 流式向量索引实现
 */

#include "streaming_index.h"
#include "vector_write_buffer.h"
#include "incremental_quant_train.h"
#include "merge_scheduler.h"
#include "concurrent_search.h"

#include <db/index/vector_index/hnsw/faiss_hnsw.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/**
 * @brief 流式索引内部结构
 */
struct streaming_index {
    /* 底层索引 */
    void *main_index;                    /* faiss_hnsw_t* 或 diskann_t* */
    streaming_index_type_t index_type;

    /* 流式插入组件 */
    vector_write_buffer_t *buffer;       /* 写入缓冲区 */
    merge_scheduler_t *scheduler;        /* 合并调度器 */
    concurrent_search_adapter_t *search_adapter;  /* 并发查询适配器 */
    incremental_quant_trainer_t *quant_trainer;   /* 增量量化器（可选） */

    /* 配置 */
    streaming_index_config_t config;

    /* 统计 */
    int32_t main_index_vectors;          /* 主索引中的向量数 */
    int64_t total_vectors_merged;        /* 累计合并向量数 */
    int32_t completed_merges;            /* 已完成的合并次数 */
};

/* ========================================================================
 * 配置
 * ======================================================================== */

streaming_index_config_t streaming_index_config_default(
    streaming_index_type_t type,
    int32_t dims) {

    streaming_index_config_t config = {
        .index_type = type,
        .dims = dims,
        .M = 16,
        .ef_construction = 200,
        .ef_search = 100,
        .buffer_capacity = STREAMING_INDEX_DEFAULT_BUFFER_CAPACITY,
        .buffer_flush_threshold = STREAMING_INDEX_DEFAULT_FLUSH_THRESHOLD,
        .merge_interval_ms = STREAMING_INDEX_DEFAULT_MERGE_INTERVAL_MS,
        .enable_incremental_quant = true,
        .max_incremental_updates = STREAMING_INDEX_DEFAULT_MAX_INCREMENTAL_UPDATES
    };

    return config;
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

streaming_index_t *streaming_index_create(const streaming_index_config_t *config) {
    if (config == NULL || config->dims <= 0) {
        return NULL;
    }

    streaming_index_t *index = (streaming_index_t *)calloc(1, sizeof(streaming_index_t));
    if (index == NULL) {
        return NULL;
    }

    index->config = *config;
    index->main_index_vectors = 0;
    index->total_vectors_merged = 0;
    index->completed_merges = 0;

    /* 创建底层索引 */
    if (config->index_type == STREAMING_INDEX_HNSW) {
        /* 创建 HNSW 索引 */
        index->main_index = faiss_hnsw_index_create(
            config->M,
            config->dims,
            config->ef_construction,
            DISTANCE_METRIC_L2_SQUARED,
            QUANTIZATION_TYPE_NONE  /* 默认不启用量化 */
        );
        if (index->main_index == NULL) {
            free(index);
            return NULL;
        }
    } else if (config->index_type == STREAMING_INDEX_DISKANN) {
        /* TODO: 创建 DiskANN 索引 */
        /* index->main_index = diskann_index_create(...); */
        index->main_index = NULL;
    } else {
        free(index);
        return NULL;
    }

    /* 创建缓冲区 */
    vector_buffer_config_t buf_config = {
        .capacity = config->buffer_capacity,
        .flush_threshold = config->buffer_flush_threshold,
        .dims = config->dims,
        .batch_size = 100
    };
    index->buffer = vector_buffer_create(&buf_config);
    if (index->buffer == NULL) {
        /* 清理并返回 */
        if (index->main_index != NULL) {
            faiss_hnsw_index_drop((faiss_hnsw_t *)index->main_index);
        }
        free(index);
        return NULL;
    }

    /* 创建合并调度器 */
    merge_scheduler_config_t sched_config = {
        .flush_interval_ms = config->merge_interval_ms,
        .max_pending_tasks = 10,
        .merge_batch_size = config->buffer_flush_threshold,
        .auto_flush = true
    };
    index->scheduler = merge_scheduler_create(
        (vector_index_t *)index->main_index, &sched_config);
    if (index->scheduler == NULL) {
        vector_buffer_destroy(index->buffer);
        if (index->main_index != NULL) {
            faiss_hnsw_index_drop((faiss_hnsw_t *)index->main_index);
        }
        free(index);
        return NULL;
    }

    /* 启动后台线程 */
    merge_scheduler_start(index->scheduler);

    /* 创建并发查询适配器 */
    concurrent_search_config_t search_config = {
        .enable_buffer_search = true,
        .buffer_search_k = 50,
        .buffer_weight = 1.0f,
        .deduplicate = true
    };
    index->search_adapter = concurrent_search_create(NULL, index->buffer, &search_config);

    return index;
}

streaming_index_t *streaming_index_wrap(
    void *base_index,
    streaming_index_type_t type,
    const streaming_index_config_t *config) {

    if (base_index == NULL) {
        return NULL;
    }

    /* 使用默认配置或用户配置 */
    streaming_index_config_t default_cfg = streaming_index_config_default(type, 0);
    const streaming_index_config_t *cfg = config ? config : &default_cfg;

    streaming_index_t *index = (streaming_index_t *)calloc(1, sizeof(streaming_index_t));
    if (index == NULL) {
        return NULL;
    }

    index->main_index = base_index;
    index->index_type = type;
    index->config = *cfg;
    index->main_index_vectors = 0;
    index->total_vectors_merged = 0;
    index->completed_merges = 0;

    /* 复用 base_index 的维度信息 */
    /* TODO: 从 base_index 获取维度信息 */

    /* 创建缓冲区 */
    vector_buffer_config_t buf_config = {
        .capacity = cfg->buffer_capacity,
        .flush_threshold = cfg->buffer_flush_threshold,
        .dims = cfg->dims,
        .batch_size = 100
    };
    index->buffer = vector_buffer_create(&buf_config);
    if (index->buffer == NULL) {
        free(index);
        return NULL;
    }

    /* 创建合并调度器 */
    merge_scheduler_config_t sched_config = {
        .flush_interval_ms = cfg->merge_interval_ms,
        .max_pending_tasks = 10,
        .merge_batch_size = cfg->buffer_flush_threshold,
        .auto_flush = true
    };
    index->scheduler = merge_scheduler_create(NULL, &sched_config);
    if (index->scheduler == NULL) {
        vector_buffer_destroy(index->buffer);
        free(index);
        return NULL;
    }

    merge_scheduler_start(index->scheduler);

    /* 创建并发查询适配器 */
    concurrent_search_config_t search_config = {
        .enable_buffer_search = true,
        .buffer_search_k = 50,
        .buffer_weight = 1.0f,
        .deduplicate = true
    };
    index->search_adapter = concurrent_search_create(NULL, index->buffer, &search_config);

    return index;
}

void streaming_index_destroy(streaming_index_t *index) {
    if (index == NULL) {
        return;
    }

    /* 停止并销毁合并调度器 */
    if (index->scheduler != NULL) {
        merge_scheduler_stop(index->scheduler);
        merge_scheduler_destroy(index->scheduler);
    }

    /* 销毁并发查询适配器 */
    if (index->search_adapter != NULL) {
        concurrent_search_destroy(index->search_adapter);
    }

    /* 销毁增量量化器 */
    if (index->quant_trainer != NULL) {
        incremental_quant_trainer_destroy(index->quant_trainer);
    }

    /* 销毁缓冲区 */
    if (index->buffer != NULL) {
        vector_buffer_destroy(index->buffer);
    }

    /* 销毁主索引（HNSW） */
    if (index->main_index != NULL && index->index_type == STREAMING_INDEX_HNSW) {
        faiss_hnsw_index_drop((faiss_hnsw_t *)index->main_index);
    }

    free(index);
}

/* ========================================================================
 * 插入操作
 * ======================================================================== */

/**
 * @brief 执行合并（内部使用）
 */
static int32_t do_merge(streaming_index_t *index, const float *vectors, int32_t n) {
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    /* 调用底层索引的插入接口 */
    if (index->index_type == STREAMING_INDEX_HNSW) {
        if (index->main_index != NULL) {
            int32_t ret = faiss_hnsw_index_add(
                (faiss_hnsw_t *)index->main_index,
                n,
                vectors
            );
            if (ret != 0) {
                return ret;
            }
        }
    } else if (index->index_type == STREAMING_INDEX_DISKANN) {
        /* TODO: diskann_index_add(index->main_index, n, vectors); */
    }

    index->main_index_vectors += n;
    index->total_vectors_merged += n;
    index->completed_merges++;

    return 0;
}

int32_t streaming_index_add(streaming_index_t *index, const float *vectors, int32_t n) {
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    return do_merge(index, vectors, n);
}

int32_t streaming_index_streaming_add(streaming_index_t *index, const float *vectors, int32_t n) {
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    /* 添加到缓冲区（非阻塞） */
    int32_t pushed = vector_buffer_push_batch(index->buffer, vectors, n);
    if (pushed <= 0) {
        return -1;
    }

    /* 注意：真正的合并由后台调度器处理，这里只缓冲数据 */
    /* 如果需要立即可搜索，可以调用 streaming_index_flush() */

    (void)index;  /* 避免未使用警告 */
    return pushed;
}

int32_t streaming_index_flush(streaming_index_t *index) {
    if (index == NULL) {
        return -1;
    }

    /* 获取所有缓冲区数据 */
    int32_t buffer_size = vector_buffer_size(index->buffer);
    if (buffer_size == 0) {
        return 0;
    }

    float *vectors = (float *)malloc((size_t)buffer_size * index->config.dims * sizeof(float));
    if (vectors == NULL) {
        return -1;
    }

    int32_t flushed = vector_buffer_flush(index->buffer, vectors, buffer_size);
    if (flushed > 0) {
        do_merge(index, vectors, flushed);
    }

    free(vectors);
    return flushed;
}

int32_t streaming_index_wait(streaming_index_t *index) {
    if (index == NULL) {
        return -1;
    }

    /* 刷新缓冲区 */
    streaming_index_flush(index);

    /* 等待合并完成 */
    if (index->scheduler != NULL) {
        merge_scheduler_wait(index->scheduler);
    }

    return 0;
}

/* ========================================================================
 * 查询操作
 * ======================================================================== */

int32_t streaming_index_search(
    streaming_index_t *index,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids) {

    if (index == NULL || query == NULL || k <= 0) {
        return -1;
    }

    if (index->search_adapter != NULL) {
        return concurrent_search(index->search_adapter, query, k, distances, ids);
    }

    /* 降级：仅搜索主索引 */
    return streaming_index_search_main(index, query, k, distances, ids);
}

int32_t streaming_index_search_main(
    streaming_index_t *index,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids) {

    if (index == NULL || query == NULL || k <= 0) {
        return -1;
    }

    if (index->main_index == NULL) {
        return 0;
    }

    /* 调用底层索引的搜索接口 */
    if (index->index_type == STREAMING_INDEX_HNSW) {
        return faiss_hnsw_index_search(
            (faiss_hnsw_t *)index->main_index,
            query,
            k,
            index->config.ef_search > 0 ? index->config.ef_search : 100,
            distances,
            ids
        );
    } else if (index->index_type == STREAMING_INDEX_DISKANN) {
        /* TODO: return diskann_index_search(index->main_index, query, k, distances, ids); */
    }

    return 0;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

void streaming_index_get_stats(
    const streaming_index_t *index,
    streaming_index_stats_t *stats) {

    if (index == NULL || stats == NULL) {
        return;
    }

    stats->total_vectors = index->main_index_vectors +
        vector_buffer_size(index->buffer);
    stats->main_index_vectors = index->main_index_vectors;
    stats->buffer_vectors = vector_buffer_size(index->buffer);
    stats->pending_tasks = 0;  /* TODO: 从调度器获取 */
    stats->completed_merges = index->completed_merges;
    stats->total_vectors_merged = index->total_vectors_merged;
    stats->is_merging = false;  /* TODO: 从调度器获取 */
}

void *streaming_index_get_main_index(streaming_index_t *index) {
    return index ? index->main_index : NULL;
}

int32_t streaming_index_size(const streaming_index_t *index) {
    if (index == NULL) {
        return 0;
    }

    return index->main_index_vectors + vector_buffer_size(index->buffer);
}
