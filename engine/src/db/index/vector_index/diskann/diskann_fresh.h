/*
 * diskann_fresh.h
 *
 * FreshDiskANN 扩展接口。
 * 暂时提供空存根，待完整实现。
 */

#ifndef DISKANN_FRESH_H
#define DISKANN_FRESH_H

#include <stdbool.h>
#include <stdint.h>

/* FreshDiskANN 上下文（空存根） */
typedef struct diskann_fresh_context {
    int32_t dummy;
} diskann_fresh_context_t;

/* 向前声明 */
typedef struct diskann_fresh_config diskann_fresh_config_t;

/**
 * @brief 创建 FreshDiskANN 上下文
 */
diskann_fresh_context_t *diskann_fresh_create(const diskann_fresh_config_t *config, int32_t dims);

/**
 * @brief 初始化 FreshDiskANN 上下文
 */
int32_t diskann_fresh_init(diskann_fresh_context_t *ctx, int32_t dims);

/**
 * @brief 释放 FreshDiskANN 上下文
 */
void diskann_fresh_free(diskann_fresh_context_t *ctx);

#endif /* DISKANN_FRESH_H */
