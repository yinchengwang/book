# DiskANN SIFT 召回率测试实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建 DiskANN 召回率测试，使用 SIFT Small 数据集验证召回率 ≥ 0.90

**Architecture:** 复用现有 SIFT 数据加载逻辑，创建独立的 diskann_recall.c 测试文件，CMake 构建独立可执行文件

**Tech Stack:** C11、DiskANN 库、algo-prod 距离库、SIFT Small 数据集

## Global Constraints

- 向量维度：128（SIFT Small 固定）
- 数据集路径：`c:/code/book/dataset/siftsmall`
- Recall@10 目标：≥ 0.90
- 基本测试参数：R=32, L=100, search_list_size=[50, 100, 200], α=1.2

---

## Task 1: 创建 diskann_recall.c 测试文件

**Files:**
- Create: `engineering/test/db/index/sift_test/diskann_recall.c`

**Interfaces:**
- Consumes: `diskann_index_create()`, `diskann_index_add()`, `diskann_index_build()`, `diskann_index_search()`, `diskann_index_drop()` from `db/index/vector_index/diskann/diskann.h`
- Consumes: `distance_l2sqr()` from `algo-prod/distance/distance.h`
- Produces: 可执行文件 `diskann_recall`，输出召回率测试结果

- [ ] **Step 1: 创建 diskann_recall.c 文件框架**

```c
/*
 * diskann_recall.c
 *
 * Test DiskANN recall using SIFT dataset.
 * Supports both SIFT Small (10K vectors) and full SIFT 1M (1M vectors).
 *
 * Data format (binary):
 *   base.bin/query.bin: float32 array [count * dims]
 *   groundtruth.bin: int32 array [query_count * k]
 *   *_header.txt: count=xxx, dims=xxx, dtype=xxx
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* DiskANN 接口 */
#include "db/index/vector_index/diskann/diskann.h"
#include "algo-prod/distance/distance.h"

/* 后续步骤继续添加函数... */
```

- [ ] **Step 2: 添加数据加载函数（复用 sift_recall.c 逻辑）**

在文件框架后添加：

```c
/* 距离计算：L2 平方距离 */
static inline float l2sqr(const float *a, const float *b, int dims) {
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        dist += d * d;
    }
    return dist;
}

/* Read header file to get count and dims */
static int read_header(const char *bin_path, int *out_count, int *out_dims) {
    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s", bin_path);
    char *dot = strrchr(header_path, '.');
    if (dot) {
        strcpy(dot, "_header.txt");
    } else {
        strcat(header_path, "_header.txt");
    }

    FILE *f = fopen(header_path, "r");
    if (!f) {
        fprintf(stderr, "Warning: header file not found: %s\n", header_path);
        return -1;
    }

    char line[256];
    *out_count = 0;
    *out_dims = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "count=%d", out_count) == 1) continue;
        if (sscanf(line, "dims=%d", out_dims) == 1) continue;
    }

    fclose(f);

    if (*out_count == 0 || *out_dims == 0) {
        fprintf(stderr, "Error: invalid header file\n");
        return -1;
    }

    return 0;
}

/* Read pure binary format (float32 array) */
static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    /* Read header first */
    if (read_header(path, out_count, out_dims) != 0) {
        return NULL;
    }

    /* Read binary data */
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    size_t total_floats = (size_t)(*out_count) * (*out_dims);
    float *vectors = (float *)malloc(total_floats * sizeof(float));
    if (!vectors) {
        fclose(f);
        return NULL;
    }

    size_t read_count = fread(vectors, sizeof(float), total_floats, f);
    fclose(f);

    if (read_count != total_floats) {
        fprintf(stderr, "Error: read %zu floats, expected %zu\n", read_count, total_floats);
        free(vectors);
        return NULL;
    }

    printf("Read %s: %d vectors, dimension %d\n", path, *out_count, *out_dims);
    return vectors;
}

/* Read pure binary format (int32 array for groundtruth) */
static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    /* Read header first */
    int dims;
    if (read_header(path, out_count, &dims) != 0) {
        return NULL;
    }
    *out_k = dims;

    /* Read binary data */
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    size_t total_ints = (size_t)(*out_count) * (*out_k);
    int32_t *gt = (int32_t *)malloc(total_ints * sizeof(int32_t));
    if (!gt) {
        fclose(f);
        return NULL;
    }

    size_t read_count = fread(gt, sizeof(int32_t), total_ints, f);
    fclose(f);

    if (read_count != total_ints) {
        fprintf(stderr, "Error: read %zu ints, expected %zu\n", read_count, total_ints);
        free(gt);
        return NULL;
    }

    printf("Read %s: %d queries, K=%d\n", path, *out_count, *out_k);
    return gt;
}

/* 计算召回率 (Recall@K) */
static float compute_recall_at_k(const int32_t *result, const int32_t *gt, int k) {
    int hit = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (result[i] == gt[j]) { hit++; break; }
        }
    }
    return (float)hit / k;
}
```

- [ ] **Step 3: 添加 main() 函数主体**

在数据加载函数后添加：

```c
int main(int argc, char *argv[]) {
    /* Default: use siftsmall (10K vectors) */
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* Allow command-line override */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== DiskANN SIFT Recall Test ===\n\n");
    printf("Dataset: %s\n\n", data_dir);

    /* Construct file paths */
    char base_path[512], query_path[512], gt_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.bin", data_dir);
    snprintf(query_path, sizeof(query_path), "%s/query.bin", data_dir);
    snprintf(gt_path, sizeof(gt_path), "%s/groundtruth.bin", data_dir);

    /* Read data */
    int n_base, dims, n_query, gt_k;
    float *base = read_binary_fvecs(base_path, &n_base, &dims);
    float *query = read_binary_fvecs(query_path, &n_query, &dims);
    int32_t *gt = read_binary_groundtruth(gt_path, &n_query, &gt_k);

    if (!base || !query || !gt) {
        fprintf(stderr, "Failed to read data\n");
        return 1;
    }

    printf("\nDataset summary:\n");
    printf("  Base: %d vectors, dimension %d\n", n_base, dims);
    printf("  Query: %d vectors\n", n_query);
    printf("  Ground Truth: K=%d\n\n", gt_k);

    /* Build DiskANN index */
    int R = 32;              /* 目标邻居数 */
    int L = 100;             /* 构图候选数 */
    float alpha = 1.2f;      /* Vamana 剪枝参数 */

    printf("Building DiskANN index (R=%d, L=%d, α=%.1f)...\n", R, L, alpha);
    diskann_t *index = diskann_index_create(
        dims,               /* dims: 向量维度 */
        R,                  /* R: 目标邻居数 */
        L,                  /* L: 构图候选数 */
        DISTANCE_METRIC_L2_SQUARED
    );
    if (!index) {
        fprintf(stderr, "Failed to create DiskANN index\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 设置 alpha */
    diskann_index_set_alpha(index, alpha);

    /* 批量添加向量 */
    int ret = diskann_index_add(index, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "Failed to add vectors: %d\n", ret);
        diskann_index_drop(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 构建 Vamana 图 */
    ret = diskann_index_build(index);
    if (ret != 0) {
        fprintf(stderr, "Failed to build index: %d\n", ret);
        diskann_index_drop(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    printf("Index built, total vectors: %d\n\n", diskann_index_size(index));

    /* Test different search_list_size values */
    printf("Testing recall with different search_list_size values...\n");
    int search_list_sizes[] = {50, 100, 200};
    int n_sl = sizeof(search_list_sizes) / sizeof(search_list_sizes[0]);

    for (int s = 0; s < n_sl; s++) {
        int sl = search_list_sizes[s];
        printf("--- search_list_size = %d ---\n", sl);

        /* 测试 Recall@1, Recall@10, Recall@50 */
        for (int tk = 0; tk < 3; tk++) {
            int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
            if (k > gt_k) k = gt_k;

            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                float dists[100];
                int32_t ids[100];
                memset(ids, -1, sizeof(ids));
                memset(dists, 0, sizeof(dists));

                diskann_index_search(
                    index,
                    &query[q * dims],
                    k,
                    sl,            /* search_list_size */
                    10,            /* max_iterations */
                    dists,
                    ids
                );

                float recall = compute_recall_at_k(ids, &gt[q * gt_k], k);
                total_recall += recall;
            }
            float avg_recall = total_recall / n_query;
            printf("  Recall@%d: %.4f\n", k, avg_recall);
        }
    }

    /* Cleanup */
    diskann_index_drop(index);
    free(base);
    free(query);
    free(gt);

    printf("\nTest completed!\n");
    return 0;
}
```

---

## Task 2: 更新 CMakeLists.txt 添加构建目标

**Files:**
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt:57-76`

**Interfaces:**
- Consumes: diskann 库（已在现有 CMakeLists.txt 中链接）
- Produces: 可执行文件 `diskann_recall`

- [ ] **Step 1: 在 CMakeLists.txt 末尾添加 diskann_recall 目标**

```cmake
# DiskANN 召回率测试
add_executable(diskann_recall diskann_recall.c)

target_link_libraries(diskann_recall
    PRIVATE
        project_includes
        vector_index
        diskann
        ivf
        heap_vector_store
        storage_backend_common
        algo-prod
        db_utils
)

set_target_properties(diskann_recall PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

- [ ] **Step 2: 验证 CMakeLists.txt 语法正确**

确认添加内容位于文件末尾，与现有 `build_trace_ours` 目标格式一致。

---

## Task 3: 构建并运行测试

**Files:**
- Test: `build/engineering/diskann_recall`

- [ ] **Step 1: 重新生成 CMake 构建系统**

Run: `cmake --preset engineering`
Expected: 成功生成构建文件，无错误

- [ ] **Step 2: 编译 diskann_recall**

Run: `cmake --build build/engineering --target diskann_recall`
Expected: 编译成功，生成 `build/engineering/diskann_recall.exe`

- [ ] **Step 3: 运行召回率测试**

Run: `./build/engineering/diskann_recall`
Expected: 输出召回率结果，Recall@10 ≥ 0.90

- [ ] **Step 4: 检查测试输出**

确认输出包含：
- 数据集加载信息（10000 vectors, dimension 128）
- 索引构建信息
- 各 search_list_size 下的召回率（Recall@1, Recall@10, Recall@50）

---

## Task 4: 提交代码

- [ ] **Step 1: 查看变更**

Run: `git status`
Expected: 显示 `diskann_recall.c` 和 `CMakeLists.txt` 变更

- [ ] **Step 2: 提交变更**

```bash
git add engineering/test/db/index/sift_test/diskann_recall.c
git add engineering/test/db/index/sift_test/CMakeLists.txt
git commit -m "test(diskann): 添加 SIFT 数据集召回率测试"
```