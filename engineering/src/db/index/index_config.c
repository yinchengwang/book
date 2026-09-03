/**
 * @file index_config.c
 * @brief 索引配置实现
 *
 * 提供 index_config_t 的默认值与合法性校验两个辅助函数。
 */

#include "index_config.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 默认配置
 * ============================================================ */

index_config_t index_config_default(void)
{
    index_config_t config;

    /* 存储配置：默认纯内存后端，不落盘 */
    config.storage_type = STORAGE_BACKEND_MEMORY;
    config.storage_path = NULL;
    config.page_size = 8192;

    /* 持久化开关：默认关闭 */
    config.persist_enabled = false;

    /* 向量维度：默认 128 维 */
    config.dims = 128;

    /* HNSW 系列参数：常见默认值 */
    config.M = 16;
    config.ef_construction = 200;
    config.ef_search = 100;

    /* 距离与量化：默认欧氏距离，无量化 */
    config.metric = DISTANCE_L2;
    config.quantization_type = QUANTIZATION_TYPE_NONE;

    return config;
}

/* ============================================================
 * 配置验证
 * ============================================================ */

int index_config_validate(const index_config_t *config)
{
    /* 配置指针必须非空 */
    if (config == NULL) {
        return -1;
    }

    /* 向量维度必须为正 */
    if (config->dims <= 0) {
        return -1;
    }

    /* HNSW 每层连接数必须为正 */
    if (config->M <= 0) {
        return -1;
    }

    /* 构建时搜索范围必须为正 */
    if (config->ef_construction <= 0) {
        return -1;
    }

    /* 搜索时搜索范围必须为正 */
    if (config->ef_search <= 0) {
        return -1;
    }

    return 0;
}