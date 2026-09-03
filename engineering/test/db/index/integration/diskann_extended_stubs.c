/* diskann_extended_stubs.c
 *
 * 持久化测试侧的兼容支撑文件：仅供 engineering/test/db/index/integration
 * 下的端到端持久化测试链接使用。本文件不为生产代码路径提供任何能力。
 *
 * 背景：
 *   engineering/src/db/index/vector_index/diskann/diskann_persist.c 在
 *   `diskann_index_load_extended` / `diskann_save_*` 路径中调用了
 *   SPANN、FreshDiskANN、Merge 三个模块导出的 API（diskann_spann_create、
 *   diskann_fresh_create、diskann_merge_context_create 等），而上述模块
 *   当前未通过编译验证（见 engineering/src/db/index/vector_index/diskann/
 *   CMakeLists.txt 注释说明），因此对应的 .c 文件未被加入 diskann 静态库。
 *
 *   当 persistence_test 链接整个 diskann 静态库以使用 diskann_index_save
 *   / diskann_index_load 时，linker 需要解析这些未编译模块的外部引用，
 *   从而报错。运行时只要配置中 spann/fresh/merge 三者 enabled 均为 false
 *   （默认情况），对应代码分支不会被执行。
 *
 * 处理策略：
 *   本文件提供同名符号的最小兼容 stub，返回 NULL 或 -1，使 link 阶段可解析。
 *   测试用例仅调用 diskann_index_save / diskann_index_load（基础路径），不
 *   会触达扩展 API 内部；任何 stub 函数被运行时调用均代表配置异常，应让
 *   行为表现为失败以便尽早暴露，而不是静默通过。
 */

#include <stdio.h>
#include <stdint.h>

#include "db/index/vector_index/diskann/diskann_spann.h"
#include "db/index/vector_index/diskann/diskann_fresh.h"
#include "db/index/vector_index/diskann/diskann_merge.h"

/* SPANN：分区层次索引扩展 */
diskann_spann_context_t *diskann_spann_create(const diskann_spann_config_t *config) {
    (void)config;
    return NULL;
}

int32_t diskann_spann_persist(diskann_spann_context_t *ctx, FILE *file) {
    (void)ctx;
    (void)file;
    return -1;
}

diskann_spann_context_t *diskann_spann_load(const diskann_spann_config_t *config, FILE *file) {
    (void)config;
    (void)file;
    return NULL;
}

/* FreshDiskANN：增量更新扩展 */
diskann_fresh_context_t *diskann_fresh_create(const diskann_fresh_config_t *config, int32_t dims) {
    (void)config;
    (void)dims;
    return NULL;
}

int32_t diskann_fresh_init(diskann_fresh_context_t *ctx, int32_t dims) {
    (void)ctx;
    (void)dims;
    return -1;
}

int32_t diskann_fresh_persist(diskann_fresh_context_t *ctx, FILE *file) {
    (void)ctx;
    (void)file;
    return -1;
}

diskann_fresh_context_t *diskann_fresh_load(const diskann_fresh_config_t *config,
                                            int32_t dims,
                                            FILE *file) {
    (void)config;
    (void)dims;
    (void)file;
    return NULL;
}

/* Merge-Vamana：分图合并扩展 */
diskann_merge_context_t *diskann_merge_context_create(const diskann_merge_config_t *config) {
    (void)config;
    return NULL;
}
