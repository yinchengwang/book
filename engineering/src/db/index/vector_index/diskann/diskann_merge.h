/*
 * diskann_merge.h
 *
 * Merge-Vamana 扩展接口。
 * 暂时提供空存根，待完整实现。
 */

#ifndef DISKANN_MERGE_H
#define DISKANN_MERGE_H

#include <stdbool.h>
#include <stdint.h>

/* Merge 上下文（空存根） */
typedef struct diskann_merge_context {
    int32_t dummy;
} diskann_merge_context_t;

/**
 * @brief 释放 Merge 上下文
 */
void diskann_merge_context_free(diskann_merge_context_t *ctx);

#endif /* DISKANN_MERGE_H */
