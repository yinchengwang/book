/*
 * diskann_spann.h
 *
 * SPANN 扩展接口。
 * 暂时提供空存根，待完整实现。
 */

#ifndef DISKANN_SPANN_H
#define DISKANN_SPANN_H

#include <stdbool.h>
#include <stdint.h>

/* 向前声明 */
typedef struct diskann_spann_config diskann_spann_config_t;

/* SPANN 上下文（空存根） */
typedef struct diskann_spann_context {
    int32_t dummy;
} diskann_spann_context_t;

/**
 * @brief 创建 SPANN 上下文
 */
diskann_spann_context_t *diskann_spann_create(const diskann_spann_config_t *config);

/**
 * @brief 释放 SPANN 上下文
 */
void diskann_spann_free(diskann_spann_context_t *ctx);

#endif /* DISKANN_SPANN_H */
