/*
 * diskann_stubs.c
 *
 * DiskANN 模块临时桩：补齐 diskann_search.c 引用的 Fresh 相关符号。
 *
 * 背景：
 * - diskann_private.h 对 diskann_fresh_context_t / diskann_spann_context_t /
 *   diskann_merge_context_t 仅做前置声明（opaque 指针），而 diskann_fresh.c /
 *   diskann_spann.c / diskann_merge.c / diskann_partition.c 中包含 diskann_private.h
 *   之后再包含对应的公有头，会把同一类型当作不透明类型重新声明，导致编译失败。
 * - 为使 Task 1.3 基准测试聚焦于基础 Vamana 图构建/搜索路径，此处为被
 *   diskann_search.c 引用的 Fresh 相关符号提供最小桩，使链接通过。
 *
 * 注意：以下桩均假定 Fresh 模式未启用（FreshDiskANN 相关功能在另一个
 * 独立任务中修复），因此实现为恒返回"无新鲜数据"的占位语义：
 *   - diskann_fresh_count        -> 0
 *   - diskann_fresh_search       -> 0（未命中）
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "diskann_private.h"
#include "diskann_spann.h"
#include "diskann_fresh.h"
#include "diskann_merge.h"

int32_t diskann_fresh_count(const diskann_fresh_context_t *ctx) {
    (void)ctx;
    return 0;
}

int32_t diskann_fresh_search(diskann_fresh_context_t *ctx,
                             const float *query,
                             int32_t k,
                             int32_t search_list_size,
                             float *distances,
                             int32_t *labels) {
    (void)ctx;
    (void)query;
    (void)k;
    (void)search_list_size;
    (void)distances;
    (void)labels;
    return 0;
}

/* 扩展持久化桩：SPANN/Merge/Fresh 相关函数 */
diskann_spann_context_t *diskann_spann_create(const diskann_spann_config_t *config) {
    (void)config;
    return NULL;
}

void diskann_spann_free(diskann_spann_context_t *ctx) {
    (void)ctx;
}

int32_t diskann_spann_persist(diskann_spann_context_t *ctx, FILE *file) {
    (void)ctx;
    (void)file;
    return 0;
}

diskann_spann_context_t *diskann_spann_load(const diskann_spann_config_t *config, FILE *file) {
    (void)config;
    (void)file;
    return NULL;
}

diskann_fresh_context_t *diskann_fresh_create(const diskann_fresh_config_t *config, int32_t dims) {
    (void)config;
    (void)dims;
    return NULL;
}

int32_t diskann_fresh_init(diskann_fresh_context_t *ctx, int32_t dims) {
    (void)ctx;
    (void)dims;
    return 0;
}

void diskann_fresh_free(diskann_fresh_context_t *ctx) {
    (void)ctx;
}

int32_t diskann_fresh_persist(diskann_fresh_context_t *ctx, FILE *file) {
    (void)ctx;
    (void)file;
    return 0;
}

diskann_fresh_context_t *diskann_fresh_load(const diskann_fresh_config_t *config, int32_t dims, FILE *file) {
    (void)config;
    (void)dims;
    (void)file;
    return NULL;
}

diskann_merge_context_t *diskann_merge_context_create(const diskann_merge_config_t *config) {
    (void)config;
    return NULL;
}

void diskann_merge_context_free(diskann_merge_context_t *ctx) {
    (void)ctx;
}