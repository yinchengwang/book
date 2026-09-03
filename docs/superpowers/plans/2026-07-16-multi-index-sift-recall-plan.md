# 多索引 SIFT 召回率测试实现计划（详细版）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建 10 个独立索引的 SIFT 召回率测试文件，覆盖所有 I 类向量索引，每个文件独立编译运行

**Architecture:** 每个索引一个独立 C 测试文件，复用 SIFT 数据加载逻辑，输出单个索引的召回率结果

**Tech Stack:** C11、各索引原生 API、SIFT Small 数据集（10K 向量, 128 维）

## Global Constraints

- 向量维度：128（SIFT Small 固定）
- 数据集路径：`c:/code/book/dataset/siftsmall`
- 每个索引独立测试文件，独立 CMake target
- 数据加载函数在各文件中重复（纯 C 函数，static inline，避免跨文件依赖）
- 测试参数使用业界典型值（见参数总表）
- 所有文件放在 `engineering/test/db/index/sift_test/` 目录下
- CMake 链接库一致：`project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils`

---

## 测试参数总表

| 索引 | 创建参数 | 搜索参数 | 流程 |
|------|----------|----------|------|
| IVF-Flat | nlist=100, dims=128 | nprobe=[1,5,10], K=1,10,50 | create→train(10000,base)→add(10000,base,ids)→search (nprobe 循环) |
| NSW | M=16, ef_construction=100, dims=128 | ef_search=50, K=1,10,50 | create→insert_batch(10000,base)→search |
| LSH | LSH_PSTABLE, n_hash=32, n_tables=32, dims=128 | 固定参数, K=1,10,50 | create→train(10000,base)→add_one_by_one→search |
| IVF-PQ | nlist=100, pq_m=8, pq_bits=8, dims=128 | nprobe=[1,5,10], K=1,10,50 | create→train(10000,base)→add(10000,base,ids)→search |
| faiss_ivf_compat | nlist=100, dims=128 | nprobe=[1,5,10], K=1,10,50 | create→train(10000,base)→add(10000,base,ids)→search |
| milvus_ivf | nlist=100, dims=128 | nprobe=[1,5,10], K=1,10,50 | create→train(10000,base)→add(10000,base,ids)→search |
| ScaNN | n_neighbors=10, dims=128 | 固定, K=1,10,50 | create→train(10000,base)→add(10000,base,ids)→search |
| Annoy | dims=128, metric="euclidean" | n_trees=[10,30,50], K=1,10,50 | create→add_item循环→build(n_trees)→search |
| KD-Tree | dims=128 | 固定, K=1,10,50 | create→build(10000,base,ids)→search |
| Ball Tree | dims=128 | 固定, K=1,10,50 | create→build(10000,base,ids)→search |

---

## 数据加载模板（每个文件共用）

每个 .c 文件开头都包含以下数据加载函数（static，内容与 diskann_recall.c 完全一致）：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* 索引头文件 */
#include "db/index/vector_index/xxx/xxx.h"
#include "algo-prod/distance/distance.h"

static inline float l2sqr(const float *a, const float *b, int dims) {
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) { float d = a[i] - b[i]; dist += d * d; }
    return dist;
}

static int read_header(const char *bin_path, int *out_count, int *out_dims) {
    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s", bin_path);
    char *dot = strrchr(header_path, '.');
    if (dot) strcpy(dot, "_header.txt");
    else strcat(header_path, "_header.txt");
    FILE *f = fopen(header_path, "r");
    if (!f) { fprintf(stderr, "Warning: header not found: %s\n", header_path); return -1; }
    char line[256]; *out_count = 0; *out_dims = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "count=%d", out_count) == 1) continue;
        if (sscanf(line, "dims=%d", out_dims) == 1) continue;
    }
    fclose(f);
    if (*out_count == 0 || *out_dims == 0) { fprintf(stderr, "Error: invalid header\n"); return -1; }
    return 0;
}

static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    if (read_header(path, out_count, out_dims) != 0) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }
    size_t total = (size_t)(*out_count) * (*out_dims);
    float *v = (float *)malloc(total * sizeof(float));
    if (!v) { fclose(f); return NULL; }
    size_t r = fread(v, sizeof(float), total, f);
    fclose(f);
    if (r != total) { free(v); return NULL; }
    printf("Read %s: %d vectors, dimension %d\n", path, *out_count, *out_dims);
    return v;
}

static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    int dims;
    if (read_header(path, out_count, &dims) != 0) return NULL;
    *out_k = dims;
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }
    size_t total = (size_t)(*out_count) * (*out_k);
    int32_t *gt = (int32_t *)malloc(total * sizeof(int32_t));
    if (!gt) { fclose(f); return NULL; }
    size_t r = fread(gt, sizeof(int32_t), total, f);
    fclose(f);
    if (r != total) { free(gt); return NULL; }
    printf("Read %s: %d queries, K=%d\n", path, *out_count, *out_k);
    return gt;
}

static float compute_recall_at_k(const int32_t *result, const int32_t *gt, int k) {
    int hit = 0;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < k; j++)
            if (result[i] == gt[j]) { hit++; break; }
    return (float)hit / k;
}
```

---

## Task 1: IVF-Flat 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/ivf_flat_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
ivf_flat_index_t *ivf_flat_create(int nlist, int dims);
void ivf_flat_destroy(ivf_flat_index_t *idx);
int ivf_flat_train(ivf_flat_index_t *idx, int n, const float *vectors);
int ivf_flat_add(ivf_flat_index_t *idx, int n, const float *vectors, const int *ids);
int ivf_flat_search(const ivf_flat_index_t *idx, const float *query, int k,
                    int *result_ids, float *result_dists);
void ivf_flat_set_nprobe(ivf_flat_index_t *idx, int nprobe);
```

**参数：** nlist=100, nprobe=[1,5,10]

**main() 结构：**
```c
int main(int argc, char *argv[]) {
    const char *data_dir = "c:/code/book/dataset/siftsmall";
    // ... 数据加载同上 ...

    int nlist = 100;
    ivf_flat_index_t *idx = ivf_flat_create(nlist, dims);
    // train
    ivf_flat_train(idx, n_base, base);
    // add: 需要构造 ids 数组
    int *ids = (int *)malloc(n_base * sizeof(int));
    for (int i = 0; i < n_base; i++) ids[i] = i;
    ivf_flat_add(idx, n_base, base, ids);
    free(ids);

    // 测试不同 nprobe
    int nprobe_values[] = {1, 5, 10};
    for (int np = 0; np < 3; np++) {
        ivf_flat_set_nprobe(idx, nprobe_values[np]);
        printf("--- nprobe = %d ---\n", nprobe_values[np]);
        // 测试 K=1,10,50
        for (int tk = 0; tk < 3; tk++) {
            int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
            if (k > gt_k) k = gt_k;
            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                float dists[100]; int32_t ids_out[100];
                memset(ids_out, -1, sizeof(ids_out));
                memset(dists, 0, sizeof(dists));
                ivf_flat_search(idx, &query[q * dims], k, ids_out, dists);
                total_recall += compute_recall_at_k(ids_out, &gt[q * gt_k], k);
            }
            printf("  Recall@%d: %.4f\n", k, total_recall / n_query);
        }
    }
    ivf_flat_destroy(idx);
    // ... free ...
}
```

**头文件：** `#include "db/index/vector_index/ivf_flat/ivf_flat.h"`

---

## Task 2: NSW 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/nsw_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
nsw_index_t *nsw_index_create(const nsw_config_t *config);
void nsw_index_destroy(nsw_index_t *index);
int32_t nsw_index_insert_batch(nsw_index_t *index, int32_t n, const float *vectors);
int32_t nsw_index_search(const nsw_index_t *index, const float *query,
                         int32_t k, int32_t *result_ids, float *result_dists);
int32_t nsw_index_size(const nsw_index_t *index);
```

**参数：** M=16, ef_construction=100, ef_search=50

**注意：** `nsw_config_t` 是纯 struct 而非指针，需要填充字段后取地址传给 create。`nsw_config_t` 定义在 `nsw.h` 中：
```c
typedef struct nsw_config {
    int32_t M;              /* 每个节点的最大邻居数 */
    int32_t ef_construction;/* 构建时的候选队列大小 */
    int32_t ef_search;      /* 搜索时的候选队列大小 */
    int32_t dims;           /* 向量维度 */
    distance_metric_t metric; /* 距离度量 */
} nsw_config_t;
```

**main() 结构：**
```c
nsw_config_t config;
config.M = 16;
config.ef_construction = 100;
config.ef_search = 50;
config.dims = dims;
config.metric = DISTANCE_METRIC_L2_SQUARED;

nsw_index_t *idx = nsw_index_create(&config);
nsw_index_insert_batch(idx, n_base, base);
// ... 搜索（无 nprobe 参数，直接搜索）...
```

**注意：** NSW 的 `ef_search` 在 create 时通过 config 传入，搜索时不需要额外参数。只测试一组参数。

**头文件：** `#include "db/index/vector_index/nsw/nsw.h"`

---

## Task 3: LSH 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/lsh_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
lsh_index_t *lsh_create(lsh_type_t type, int n_hash, int n_tables, int dims);
int lsh_train(lsh_index_t *idx, int n, const float *vectors);
int lsh_add(lsh_index_t *idx, int n, const float *vectors, const int *ids);
int lsh_search(const lsh_index_t *idx, const float *query, int k, int *ids, float *distances);
void lsh_destroy(lsh_index_t *idx);
```

**参数：** LSH_PSTABLE, n_hash=32, n_tables=32

**注意：** `lsh_add` 虽然支持批量，但参考 `lsh_benchmark.cpp` 使用逐条添加模式更可靠：
```c
for (int i = 0; i < n_base; i++) {
    int32_t ret = lsh_add(idx, 1, &base[i * dims], &i);
    if (ret != 1) { fprintf(stderr, "lsh_add failed at %d\n", i); break; }
}
```

**头文件：** `#include "db/index/vector_index/lsh/lsh.h"`

---

## Task 4: IVF-PQ 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/ivf_pq_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
ivf_pq_index_t *ivf_pq_create(int nlist, int pq_m, int pq_bits, int dims);
void ivf_pq_set_nprobe(ivf_pq_index_t *idx, int nprobe);
int ivf_pq_train(ivf_pq_index_t *idx, int n, const float *vectors);
int ivf_pq_add(ivf_pq_index_t *idx, int n, const float *vectors, const int *ids);
int ivf_pq_search(const ivf_pq_index_t *idx, const float *query, int k, int *ids, float *distances);
void ivf_pq_destroy(ivf_pq_index_t *idx);
```

**参数：** nlist=100, pq_m=8, pq_bits=8, nprobe=[1,5,10]

**结构：** 与 IVF-Flat 类似，PQ 量化后搜索精度可能略低。构造 ids 数组。

**头文件：** `#include "db/index/vector_index/ivf_pq/ivf_pq.h"`

---

## Task 5: faiss_ivf_compat 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/faiss_ivf_compat_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
faiss_ivf_compat_t *faiss_ivf_compat_create(int dims, int nlist);
int faiss_ivf_compat_train(faiss_ivf_compat_t *idx, int n, const float *vectors);
int faiss_ivf_compat_add(faiss_ivf_compat_t *idx, int n, const float *vectors, const int *ids);
int faiss_ivf_compat_search(const faiss_ivf_compat_t *idx, const float *query,
                            int k, int *ids, float *distances, int nprobe);
void faiss_ivf_compat_destroy(faiss_ivf_compat_t *idx);
```

**参数：** nlist=100, nprobe=[1,5,10]

**头文件：** `#include "db/index/vector_index/faiss_ivf_compat/faiss_ivf_compat.h"`

---

## Task 6: milvus_ivf 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/milvus_ivf_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
milvus_ivf_t *milvus_ivf_create(int dims, int nlist);
int milvus_ivf_train(milvus_ivf_t *idx, int n, const float *vectors);
int milvus_ivf_add(milvus_ivf_t *idx, int n, const float *vectors, const int *ids);
int milvus_ivf_search(const milvus_ivf_t *idx, const float *query,
                      int k, int *ids, int nprobe);
void milvus_ivf_destroy(milvus_ivf_t *idx);
```

**参数：** nlist=100, nprobe=[1,5,10]

**注意：** `milvus_ivf_search` 无 distances 参数，只有 ids 输出。召回率计算只依赖 ids。

**头文件：** `#include "db/index/vector_index/milvus_ivf/milvus_ivf.h"`

---

## Task 7: ScaNN 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/scann_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
scann_t *scann_create(int dims, int n_neighbors);
int scann_train(scann_t *idx, int n, const float *vectors);
int scann_add(scann_t *idx, int n, const float *vectors, const int *ids);
int scann_search(const scann_t *idx, const float *query,
                 int k, int *ids, float *distances);
void scann_destroy(scann_t *idx);
```

**参数：** n_neighbors=10

**头文件：** `#include "db/index/vector_index/scann/scann.h"`

---

## Task 8: Annoy 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/annoy_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
annoy_index_t *annoy_create(int dims, const char *metric);
int annoy_add_item(annoy_index_t *idx, int id, const float *vector);
int annoy_build(annoy_index_t *idx, int n_trees);
int annoy_search(const annoy_index_t *idx, const float *query, int k,
                 int *result_ids, float *result_dists, int search_k);
void annoy_destroy(annoy_index_t *idx);
```

**参数：** n_trees=[10,30,50], search_k=-1

**注意：** `annoy_add_item` 逐条添加，需要循环 10000 次，耗时较长。`annoy_build` 在 add 全部完成后调用。

**头文件：** `#include "db/index/vector_index/annoy/annoy.h"`

---

## Task 9: KD-Tree 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/kd_tree_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
kd_tree_t *kd_tree_create(int dims);
int kd_tree_build(kd_tree_t *idx, int n, const float *vectors, const int *ids);
int kd_tree_search(const kd_tree_t *idx, const float *query, int k,
                   int *result_ids, float *result_dists);
void kd_tree_destroy(kd_tree_t *idx);
```

**参数：** dims=128（无额外参数，精确搜索，召回率应接近 1.0）

**头文件：** `#include "db/index/vector_index/kd_tree/kd_tree.h"`

---

## Task 10: Ball Tree 召回率测试

**Files:**
- Create: `engineering/test/db/index/sift_test/ball_tree_recall.c`
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

**API 签名：**
```c
ball_tree_t *ball_tree_create(int dims);
int ball_tree_build(ball_tree_t *idx, int n, const float *vectors, const int *ids);
int ball_tree_search(const ball_tree_t *idx, const float *query, int k,
                     int *result_ids, float *result_dists);
void ball_tree_destroy(ball_tree_t *idx);
```

**参数：** dims=128（精确搜索，召回率应接近 1.0）

**头文件：** `#include "db/index/vector_index/ball_tree/ball_tree.h"`

---

## Task 11: 更新 CMakeLists.txt 添加所有构建目标

**Files:**
- Modify: `engineering/test/db/index/sift_test/CMakeLists.txt`

在文件末尾（`build_trace_faiss` 目标之后）添加全部 10 个测试目标：

```cmake
# IVF-Flat 召回率测试
add_executable(ivf_flat_recall ivf_flat_recall.c)
target_link_libraries(ivf_flat_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(ivf_flat_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# NSW 召回率测试
add_executable(nsw_recall nsw_recall.c)
target_link_libraries(nsw_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(nsw_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# LSH 召回率测试
add_executable(lsh_recall lsh_recall.c)
target_link_libraries(lsh_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(lsh_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# IVF-PQ 召回率测试
add_executable(ivf_pq_recall ivf_pq_recall.c)
target_link_libraries(ivf_pq_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(ivf_pq_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# faiss_ivf_compat 召回率测试
add_executable(faiss_ivf_compat_recall faiss_ivf_compat_recall.c)
target_link_libraries(faiss_ivf_compat_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(faiss_ivf_compat_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# milvus_ivf 召回率测试
add_executable(milvus_ivf_recall milvus_ivf_recall.c)
target_link_libraries(milvus_ivf_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(milvus_ivf_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ScaNN 召回率测试
add_executable(scann_recall scann_recall.c)
target_link_libraries(scann_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(scann_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Annoy 召回率测试
add_executable(annoy_recall annoy_recall.c)
target_link_libraries(annoy_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(annoy_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# KD-Tree 召回率测试
add_executable(kd_tree_recall kd_tree_recall.c)
target_link_libraries(kd_tree_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(kd_tree_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Ball Tree 召回率测试
add_executable(ball_tree_recall ball_tree_recall.c)
target_link_libraries(ball_tree_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod db_utils)
set_target_properties(ball_tree_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
```

---

## Task 12: 批量构建并运行测试

**验证步骤：**

1. **重新生成 CMake 构建系统**
   Run: `cd c:/code/book/build/engineering && cmake ../../engineering 2>&1 | tail -10`
   Expected: 所有 10 个新目标被识别，无错误

2. **批量编译所有目标**
   Run: `cd c:/code/book/build/engineering && cmake --build . --target ivf_flat_recall && cmake --build . --target nsw_recall && cmake --build . --target lsh_recall && cmake --build . --target ivf_pq_recall && cmake --build . --target faiss_ivf_compat_recall && cmake --build . --target milvus_ivf_recall && cmake --build . --target scann_recall && cmake --build . --target annoy_recall && cmake --build . --target kd_tree_recall && cmake --build . --target ball_tree_recall`
   Expected: 全部编译成功，无错误

3. **逐个运行测试并记录结果**
   ```bash
   echo "=== IVF-Flat ===" && ./ivf_flat_recall
   echo "=== NSW ===" && ./nsw_recall
   echo "=== LSH ===" && ./lsh_recall
   echo "=== IVF-PQ ===" && ./ivf_pq_recall
   echo "=== faiss_ivf_compat ===" && ./faiss_ivf_compat_recall
   echo "=== milvus_ivf ===" && ./milvus_ivf_recall
   echo "=== ScaNN ===" && ./scann_recall
   echo "=== Annoy ===" && ./annoy_recall
   echo "=== KD-Tree ===" && ./kd_tree_recall
   echo "=== Ball Tree ===" && ./ball_tree_recall
   ```

4. **记录输出到文件**
   Run: `cd c:/code/book/build/engineering && ./ivf_flat_recall 2>&1 | tee ../../test-results/engineering/ivf_flat_recall.txt`
   为每个索引生成测试结果文件到 `test-results/engineering/` 目录。

---

## 文件变更清单

| 操作 | 文件 |
|------|------|
| 创建 | `engineering/test/db/index/sift_test/ivf_flat_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/nsw_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/lsh_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/ivf_pq_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/faiss_ivf_compat_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/milvus_ivf_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/scann_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/annoy_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/kd_tree_recall.c` |
| 创建 | `engineering/test/db/index/sift_test/ball_tree_recall.c` |
| 修改 | `engineering/test/db/index/sift_test/CMakeLists.txt` |