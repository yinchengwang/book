# HNSW 索引重构执行计划

> **目标**：删除现有有 bug 的 HNSW 实现，参考 FAISS 重写 faiss_hnsw（内存版），基于 pgVector 页管理重写 hnsw（持久化版），两者算法逻辑一致，都支持 SQ/PQ/LVQ/RaBitQ 量化。

**架构**：内存版（faiss_hnsw/）和持久化版（hnsw/）共享相同的 HNSW 图算法逻辑，区别仅在存储层。

**依赖**：参考 `reference/open-source/faiss/faiss/impl/HNSW.cpp`、`reference/open-source/pgvector/src/hnsw.c`。

---

## 全局约束

- 编码风格：UTF-8，中文注释
- 编译：CMake 3.20+，C11
- 外部依赖：无（复用项目已有的 `quantizer_t` 和 `distance_metric_t`）
- 接口兼容：删除旧文件后，所有 `#include "db/index/vector_index/hnsw/faiss_hnsw.h"` 改为 `#include "db/index/vector_index/faiss_hnsw/faiss_hnsw.h"`，接口函数名不变
- 量化参数通过 `quantization_type_t`（已有枚举：`QUANTIZATION_TYPE_NONE/SQ/PQ/LVQ/RQ`）控制

---

## 文件变更总览

| 操作 | 文件 |
|------|------|
| 删除 | `src/db/index/vector_index/hnsw/*.c`（6个）|
| 删除 | `include/db/index/vector_index/hnsw/*.h`（2个）|
| 删除 | `include/db/utils/faiss/*.h`（4个）|
| 删除 | `src/db/utils/faiss_*.c`（4个）|
| 删除 | `test/db/index/sift_test/hnsw_graph_dump.c`, `hnsw_trace_100.c`, `debug_*.c`, `build_trace_*.c`, `compare_ours_vs_faiss.c`, `faiss_level_check.cpp`（10个）|
| 创建 | `src/db/index/vector_index/faiss_hnsw/*.c`（10个）|
| 创建 | `include/db/index/vector_index/faiss_hnsw/*.h`（4个）|
| 创建 | `src/db/index/vector_index/hnsw/*.c`（10个）|
| 创建 | `include/db/index/vector_index/hnsw/*.h`（4个）|
| 创建 | `src/db/index/vector_index/faiss_hnsw/DESIGN.md` |
| 创建 | `src/db/index/vector_index/hnsw/DESIGN.md` |
| 修改 | `src/db/index/vector_index/CMakeLists.txt`（删除 glob 排除 hnsw）|
| 修改 | `src/db/utils/CMakeLists.txt`（删除 faiss_utils）|
| 修改 | `test/db/index/sift_test/CMakeLists.txt`（删除旧测试目标）|
| 修改 | 13 个引用旧路径的 `.c/.h` 文件（include 路径更新）|

---

## 接口定义（新 faiss_hnsw/）

```c
// faiss_hnsw.h - 主接口（保持向后兼容）
typedef enum {
    QUANTIZATION_TYPE_NONE = 0,
    QUANTIZATION_TYPE_PQ   = 1,
    QUANTIZATION_TYPE_LVQ  = 2,
    QUANTIZATION_TYPE_SQ   = 3,
    QUANTIZATION_TYPE_RQ   = 4,  // RaBitQ
} quantization_type_t;

typedef struct faiss_hnsw {
    int32_t dims;
    int32_t M;
    int32_t ef_construction;
    int32_t ef_search;
    distance_metric_t metric;
    quantization_type_t quantization_type;
    int32_t n_total;
    int32_t max_level;
    int32_t entry_point;

    // 向量存储（内存连续数组）
    float *vectors;

    // 量化编码（可选）
    uint8_t *codes;
    int32_t code_size;

    // HNSW 图结构
    int32_t *levels;          // 每个向量所在层（0-indexed, level 0 即底层）
    int32_t *offsets;          // 每个向量在 neighbors 数组中的偏移
    int32_t *neighbors;        // 邻居数组（flattened）
    float *assign_probas;      // 层分配概率表
    int32_t assign_probas_size;
    int32_t *cum_nneighbor;    // 每层累计邻居数
    int32_t cum_nneighbor_size;

    // 随机数生成器（Mersenne Twister，与 FAISS 一致 seed=12345）
    void *rng_state;

    // 量化器
    void *quantizer;

    // 删除位图
    void *delete_bitmap;
} faiss_hnsw_t;

// 核心 API
faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims, int32_t ef_construction,
                                       distance_metric_t metric, quantization_type_t quant_type);
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vectors);
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query, int32_t k,
                                 int32_t ef_search, float *distances, int32_t *ids);
void faiss_hnsw_index_drop(faiss_hnsw_t *index);
```

---

## Task 1：清理旧文件

**Files:**
- 删除: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_create.c`
- 删除: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_create_with_config.c`
- 删除: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_delete.c`
- 删除: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_insert.c`
- 删除: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_search.c`
- 删除: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_utils.c`
- 删除: `engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h`
- 删除: `engineering/include/db/index/vector_index/hnsw/faiss_hnsw_utils.h`
- 删除: `engineering/include/db/utils/faiss/faiss_heap.h`
- 删除: `engineering/include/db/utils/faiss/faiss_minimax_heap.h`
- 删除: `engineering/include/db/utils/faiss/faiss_visited_table.h`
- 删除: `engineering/include/db/utils/faiss/single_result_handler.h`
- 删除: `engineering/src/db/utils/faiss_heap.c`
- 删除: `engineering/src/db/utils/faiss_minimax_heap.c`
- 删除: `engineering/src/db/utils/faiss_single_result_handler.c`
- 删除: `engineering/src/db/utils/faiss_visited_table.c`
- 删除: `engineering/test/db/index/sift_test/hnsw_graph_dump.c`
- 删除: `engineering/test/db/index/sift_test/hnsw_trace_100.c`
- 删除: `engineering/test/db/index/sift_test/debug_build.c`
- 删除: `engineering/test/db/index/sift_test/debug_build2.c`
- 删除: `engineering/test/db/index/sift_test/debug_entry_point.c`
- 删除: `engineering/test/db/index/sift_test/debug_search.c`
- 删除: `engineering/test/db/index/sift_test/debug_search_single.c`
- 删除: `engineering/test/db/index/sift_test/build_trace_ours.c`
- 删除: `engineering/test/db/index/sift_test/build_trace_faiss.cpp`
- 删除: `engineering/test/db/index/sift_test/compare_ours_vs_faiss.c`
- 删除: `engineering/test/db/index/sift_test/faiss_level_check.cpp`
- 修改: `engineering/src/db/utils/CMakeLists.txt` — 删除 faiss_heap/minimax_heap/single_result_handler/visited_table 源文件
- 修改: `engineering/src/db/index/vector_index/CMakeLists.txt` — 删除 hnsw 排除规则（`list(FILTER ... EXCLUDE ".*hnsw/.*")` 如果存在）

```bash
# 删除旧文件
rm -f engineering/src/db/index/vector_index/hnsw/*.c
rm -f engineering/include/db/index/vector_index/hnsw/*.h
rm -f engineering/include/db/utils/faiss/*.h
rm -f engineering/src/db/utils/faiss_*.c
rm -f engineering/test/db/index/sift_test/hnsw_*.c
rm -f engineering/test/db/index/sift_test/debug_*.c
rm -f engineering/test/db/index/sift_test/build_trace_*.c
rm -f engineering/test/db/index/sift_test/build_trace_faiss.cpp
rm -f engineering/test/db/index/sift_test/compare_ours_vs_faiss.c
rm -f engineering/test/db/index/sift_test/faiss_level_check.cpp
```

- [ ] **Step 1: 删除所有旧文件**

```bash
cd c:/code/book
rm -f engineering/src/db/index/vector_index/hnsw/*.c
rm -f engineering/include/db/index/vector_index/hnsw/*.h
rm -f engineering/include/db/utils/faiss/*.h
rm -f engineering/src/db/utils/faiss_*.c
rm -f engineering/test/db/index/sift_test/hnsw_*.c
rm -f engineering/test/db/index/sift_test/debug_*.c
rm -f engineering/test/db/index/sift_test/build_trace_ours.c
rm -f engineering/test/db/index/sift_test/build_trace_faiss.cpp
rm -f engineering/test/db/index/sift_test/compare_ours_vs_faiss.c
rm -f engineering/test/db/index/sift_test/faiss_level_check.cpp
```

- [ ] **Step 2: 修改 db/utils/CMakeLists.txt**

从 `src/db/utils/CMakeLists.txt` 中删除 faiss_heap/minimax_heap/single_result_handler/visited_table 的源文件引用。

- [ ] **Step 3: 修改 vector_index/CMakeLists.txt**

删除 `list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*hnsw/.*")` 行（如果有的话）。

- [ ] **Step 4: 修改 sift_test/CMakeLists.txt**

删除 `add_executable(hnsw_graph_dump ...)`、`add_executable(hnsw_trace_100 ...)`、`add_executable(build_trace_ours ...)`、`add_executable(build_trace_faiss ...)`、`add_executable(faiss_level_check ...)` 这 5 个 target 定义。

- [ ] **Step 5: 编译验证无编译错误**

```bash
cd build/engineering
cmake --build . --target vector_index 2>&1 | head -50
```

预期：成功编译 vector_index 静态库（hnsw 旧文件已删除，新文件尚未创建，此阶段预期有"缺少符号"链接错误，但不应有 include 路径错误）。

---

## Task 2：创建 faiss_hnsw 目录和 CMakeLists

**Files:**
- 创建: `engineering/include/db/index/vector_index/faiss_hnsw/faiss_hnsw.h`
- 创建: `engineering/include/db/index/vector_index/faiss_hnsw/faiss_hnsw_heap.h`
- 创建: `engineering/include/db/index/vector_index/faiss_hnsw/faiss_hnsw_visited_table.h`
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/CMakeLists.txt`
- Test: 编译验证

**Interfaces:**
- Produces: `faiss_hnsw_t` 结构体 + 核心 API 声明

- [ ] **Step 1: 创建目录**

```bash
mkdir -p engineering/include/db/index/vector_index/faiss_hnsw
mkdir -p engineering/src/db/index/vector_index/faiss_hnsw
```

- [ ] **Step 2: 创建 faiss_hnsw.h**

实现 `faiss_hnsw_t` 结构体（成员见上方接口定义）、`faiss_hnsw_index_create`、`faiss_hnsw_index_drop` 声明。

```c
// engineering/include/db/index/vector_index/faiss_hnsw/faiss_hnsw.h
#ifndef FAISS_HNSW_H
#define FAISS_HNSW_H

#include <stdint.h>
#include <stdbool.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faiss_hnsw faiss_hnsw_t;

faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims, int32_t ef_construction,
                                       distance_metric_t metric, quantization_type_t quant_type);
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vectors);
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query, int32_t k,
                                 int32_t ef_search, float *distances, int32_t *ids);
void faiss_hnsw_index_drop(faiss_hnsw_t *index);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 3: 创建 faiss_hnsw_heap.h（内聚到 faiss_hnsw 模块内部）**

将 `db/utils/faiss/faiss_minimax_heap.h` 的接口直接复制到这里，作为内部堆接口。

```c
// engineering/include/db/index/vector_index/faiss_hnsw/faiss_hnsw_heap.h
// MinimaxHeap：faiss_hnsw_search 专用的大顶堆
// 与 FAISS HNSW::MinimaxHeap 语义完全一致
typedef struct {
    int32_t n;      // 最大容量
    int32_t k;      // 当前元素数
    int32_t nvalid; // 有效元素数
    int32_t *ids;
    float *dis;
} faiss_hnsw_minimax_heap_t;

int32_t faiss_hnsw_minimax_heap_create(faiss_hnsw_minimax_heap_t **heap, int32_t n);
void faiss_hnsw_minimax_heap_push(faiss_hnsw_minimax_heap_t *heap, int32_t id, float dist);
int32_t faiss_hnsw_minimax_heap_pop_min(faiss_hnsw_minimax_heap_t *heap, float *dist_out);
float faiss_hnsw_minimax_heap_max(const faiss_hnsw_minimax_heap_t *heap);
int32_t faiss_hnsw_minimax_heap_size(const faiss_hnsw_minimax_heap_t *heap);
void faiss_hnsw_minimax_heap_clear(faiss_hnsw_minimax_heap_t *heap);
void faiss_hnsw_minimax_heap_drop(faiss_hnsw_minimax_heap_t *heap);
```

- [ ] **Step 4: 创建 faiss_hnsw_visited_table.h**

将 `db/utils/faiss/faiss_visited_table.h` 的接口直接复制到这里，作为内部访问表接口。

- [ ] **Step 5: 创建 src/CMakeLists.txt**

```cmake
# engineering/src/db/index/vector_index/faiss_hnsw/CMakeLists.txt
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS *.c)
add_library(faiss_hnsw STATIC ${SRC})
target_include_directories(faiss_hnsw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
)
target_link_libraries(faiss_hnsw PRIVATE algo-prod)
```

- [ ] **Step 6: 编译验证**

```bash
cd build/engineering
cmake --build . --target faiss_hnsw 2>&1 | head -30
```

预期：编译成功（空的库）。

---

## Task 3：实现 faiss_hnsw 创建/销毁

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_create.c`
- Test: 编译 + 运行测试用例验证结构体分配正确

**Interfaces:**
- Consumes: `faiss_hnsw_t` 结构体定义
- Produces: `faiss_hnsw_index_create`, `faiss_hnsw_index_drop` 实现

- [ ] **Step 1: 写测试**

```c
// 测试用例：创建 M=8, dim=128 的索引，验证各成员初始化正确
void test_faiss_hnsw_create(void) {
    faiss_hnsw_t *idx = faiss_hnsw_index_create(8, 128, 40, DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE);
    assert(idx != NULL);
    assert(idx->M == 8);
    assert(idx->dims == 128);
    assert(idx->ef_construction == 40);
    assert(idx->n_total == 0);
    assert(idx->max_level == -1);
    assert(idx->entry_point == -1);
    assert(idx->vectors == NULL);
    assert(idx->levels == NULL);
    faiss_hnsw_index_drop(idx);
}
```

- [ ] **Step 2: 运行测试验证**

```bash
cd build/engineering && cmake --build . --target faiss_hnsw
```

- [ ] **Step 3: 实现 faiss_hnsw_index_create**

参考 `reference/open-source/faiss/faiss/impl/HNSW.cpp` 中 `HNSW::HNSW(int M)` 构造函数：
- 调用 `set_default_probas(M, 1.0/log(M))` 初始化层分配概率
- 初始化 RNG 状态（seed=12345，Mersenne Twister）
- 分配 levels/offsets/neighbors/cum_nneighbor 数组
- 设置 efConstruction

```c
// 参考 FAISS set_default_probas:
// level 0: proba = 1 - ml = 1 - 1/log(M)
// level 1+: proba = ml * (1-ml)^(l-1) = ml * exp((l-1)*log(1-ml))
// cum_nneighbor[0] = 2*M, cum_nneighbor[l] = cum_nneighbor[l-1] + M
```

- [ ] **Step 4: 实现 faiss_hnsw_index_drop**

释放所有动态分配的内存：vectors、levels、offsets、neighbors、codes、quantizer、delete_bitmap。

- [ ] **Step 5: 提交**

```bash
git add -A
git commit -m "feat(faiss_hnsw): 创建 faiss_hnsw 目录框架和 create/drop 实现"
```

---

## Task 4：实现 faiss_hnsw 层级分配逻辑

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_level.c`
- Test: 用已知随机种子验证层级分配结果

**Interfaces:**
- Consumes: `faiss_hnsw_index_create` 创建的索引
- Produces: `faiss_hnsw_random_level(faiss_hnsw_t*, int32_t vec_id)` 返回随机层号

- [ ] **Step 1: 写测试**

```c
// 测试：固定 seed=12345，连续调用 20 次，验证层级分配
// 预期：与 FAISS HNSW hnsw.random_level() 结果一致
void test_random_level(void) {
    faiss_hnsw_t *idx = faiss_hnsw_index_create(8, 128, 40, DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE);
    // rng 内部已 seed=12345，直接调用
    int levels[20];
    for (int i = 0; i < 20; i++) {
        levels[i] = faiss_hnsw_random_level(idx, i);
    }
    // 预期：FAISS 给出的基准结果（通过独立 FAISS 程序生成）
    // 记录到代码注释中
    faiss_hnsw_index_drop(idx);
}
```

- [ ] **Step 2: 实现 faiss_hnsw_random_level**

参考 `reference/open-source/faiss/faiss/impl/HNSW.cpp` 的 `HNSW::random_level()`：
```cpp
int HNSW::random_level() {
    std::uniform_real_distribution<> distrib(0.0, 1.0);
    int l = 0;
    for (double p = assign_probas[0]; distrib(rng) < p; l++) {
        p = assign_probas[l + 1];
    }
    return l;
}
```

用 C 版本的 MT19937 实现相同逻辑。

- [ ] **Step 3: 编译 + 运行**

```bash
cd build/engineering && cmake --build . --target faiss_hnsw
```

---

## Task 5：实现 faiss_hnsw 搜索层（核心）

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_search_layer.c`
- Test: ef=5, ef=50 搜索结果与 FAISS 对比

**Interfaces:**
- Consumes: `faiss_hnsw_minimax_heap_t`, `faiss_hnsw_visited_table_t`
- Produces: `faiss_hnsw_search_layer(faiss_hnsw_t*, int32_t level, float* query, int32_t ef, int32_t* result_ids, float* result_dist, int32_t result_capacity)`

- [ ] **Step 1: 写测试**

```c
// 构建 100 向量索引，随机查询 10 次，ef=5/ef=50
// 验证：ef 越大，召回率越高（单调性）
void test_search_layer_monotonicity(void) {
    // 构建索引...
    float dist_ef5[5], dist_ef50[50];
    int ids_ef5[5], ids_ef50[50];
    faiss_hnsw_search(idx, query, 5, 5, dist_ef5, ids_ef5);  // ef=5
    faiss_hnsw_search(idx, query, 5, 50, dist_ef50, ids_ef50); // ef=50
    // ef=50 的前 5 个结果距离应 <= ef=5 的对应结果
    for (int i = 0; i < 5; i++) {
        assert(dist_ef50[i] <= dist_ef5[i]);
    }
}
```

- [ ] **Step 2: 实现 search_layer**

参考 `reference/open-source/faiss/faiss/impl/HNSW.cpp` 的 `search_layer_to_add` 和 `search_neighbors_to_add`：
1. 初始化 entry_point 为搜索起点
2. 创建 MinimaxHeap（容量 ef）
3. 创建 VisitedTable（大小 n_total）
4. 贪婪向下搜索：pop_min heap → 扩展邻居 → push 候选
5. 停止条件：`heap.top().dist > results.top().dist`（候选堆最小 > 结果堆最大）
6. 返回结果数组

关键：与 FAISS 行为完全一致，包括：
- 搜索方向：L2 时找距离最小的（平方距离越小越好）
- 扩展邻居顺序：无特殊顺序
- 停止条件必须严格实现

- [ ] **Step 3: 编译 + 运行**

```bash
cd build/engineering && cmake --build . --target faiss_hnsw
```

---

## Task 6：实现 faiss_hnsw 插入和建链

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_insert.c`
- Test: 20 向量批量插入，验证图结构与 FAISS 一致

**Interfaces:**
- Consumes: `faiss_hnsw_index_add`, `faiss_hnsw_random_level`
- Produces: `faiss_hnsw_insert_impl(faiss_hnsw_t*, int32_t vec_id, const float* vector)`

- [ ] **Step 1: 写测试**

```c
// 批量插入 20 向量，打印层级分组和图结构
// 与 FAISS IndexHNSW::add 对比（层级分组必须一致）
void test_insert_20_vectors(void) {
    // 与 FAISS 相同的随机种子（seed=12345）
    // 相同的插入顺序
    // 验证 entry_point/max_level/levels/邻居数一致
}
```

- [ ] **Step 2: 实现 faiss_hnsw_index_add**

参考 `reference/open-source/faiss/faiss/impl/HNSW.cpp` 的 `add_with_locks` 和 `add_links_starting_from`：

**批量插入流程（与 FAISS hnsw_add_vertices 一致）：**
1. 预先为所有向量分配层级（调用 `prepare_level_tab`）
2. bucket-sort 按层分组（高层先插入）
3. 对每层从高到低遍历：
   - 获取当前向量的层 level
   - 如果 level >= 当前层，调用 `add_links_starting_from`
   - `add_links_starting_from`：搜索候选 → shrink → 建链 → 更新双向连接

**add_links_starting_from 关键逻辑（与 FAISS 完全一致）：**
```c
// 从 ep 开始贪婪搜索 efConstruction 个候选
// 用 MinimaxHeap 收集
// 用 shrink_neighbor_list 剪枝到 M（或 2M for level 0）
// 添加双向连接（new→old 和 old→new）
```

- [ ] **Step 3: 编译 + 运行**

```bash
cd build/engineering && cmake --build . --target faiss_hnsw
```

---

## Task 7：实现 faiss_hnsw 量化支持（SQ/PQ/LVQ/RaBitQ）

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_quantize.c`
- Test: 使用 SQ/PQ 编码/解码验证正确性

**Interfaces:**
- Consumes: `quantizer_t`（来自 `algo-prod/quantization/quantization.h`）
- Produces: `faiss_hnsw_quantize_train`, `faiss_hnsw_quantize_encode`, `faiss_hnsw_quantize_decode`

- [ ] **Step 1: 写测试**

```c
// 测试量化误差
void test_quantize_sq8(void) {
    // 创建 SQ8 量化器
    // 编码向量
    // 解码向量
    // 计算 L2 误差（应在合理范围内）
}
```

- [ ] **Step 2: 实现量化训练和编码解码**

通过已有的 `quantizer_t` 接口：
- `quantizer_create(config)` 创建量化器
- `quantizer_train(q, n, vectors)` 训练
- `quantizer_encode(q, vector, code)` 编码
- `quantizer_decode(q, code, vector)` 解码

当 quantization_type != NONE 时：
- 构建阶段：插入向量时同步编码存储到 `codes` 数组
- 搜索阶段：搜索层用量化距离（ADC/PQ table lookup），底层精确重排序用解码向量

- [ ] **Step 3: 编译 + 运行**

```bash
cd build/engineering && cmake --build . --target faiss_hnsw
```

---

## Task 8：实现 faiss_hnsw 搜索接口

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_search.c`
- Test: SIFT Small 召回率测试 ef=5/ef=50

**Interfaces:**
- Consumes: `faiss_hnsw_search_layer`
- Produces: `faiss_hnsw_index_search` 实现

- [ ] **Step 1: 写测试**

```c
// SIFT Small 召回率测试
// 数据：10000 base, 100 query, groundtruth 已存在
// ef=5: R@10 > 0.70
// ef=50: R@10 > 0.95
void test_recall_sift_small(void) {
    // 构建索引...
    float recall = compute_recall(idx, query, gt, nq, k=10);
    assert(recall > 0.70);
}
```

- [ ] **Step 2: 实现 faiss_hnsw_index_search**

参考 `reference/open-source/faiss/faiss/impl/HNSW.cpp` 的 `search` 方法：

**搜索流程：**
1. 从 entry_point 开始，逐层向下搜索
2. 每层调用 `faiss_hnsw_search_layer`
3. 底层（level 0）搜索完成后，返回 ef_search 个候选
4. 若启用量化：先用量化距离排序 top-k，再精确重排序
5. 返回 k 个最近邻

- [ ] **Step 3: 编译 + 运行**

```bash
cd build/engineering && cmake --build . --target faiss_hnsw
```

---

## Task 9：更新全项目引用路径

**Files:**
- 修改: `engineering/include/db/vector_engine.h`（include 路径）
- 修改: `engineering/src/db/core/vector_query.c`
- 修改: `engineering/src/db/storage/vector/vector_engine.c`
- 修改: `engineering/src/db/storage/vector/vector_index_persist.c`
- 修改: `engineering/src/db/storage/vector/faiss_hnsw_stub.c`
- 修改: `engineering/src/db/index/vector_index/delete/graph_repair.c`
- 修改: `engineering/src/db/index/vector_index/delete/vector_index_delete_api.c`
- 修改: `engineering/src/kbase/kbase_index.c`
- 修改: `engineering/src/kbase/kbase_search.c`
- 修改: `engineering/tools/benchmark/c_api/vector_index_c_api.c`
- Test: 全项目编译成功

**Interfaces:**
- Consumes: `faiss_hnsw_index_create/add/search/drop` 函数声明
- Produces: 所有调用方使用新路径

- [ ] **Step 1: 批量替换 include 路径**

```bash
cd c:/code/book/engineering
# 替换所有引用旧 faiss_hnsw.h 路径的文件
sed -i 's|db/index/vector_index/hnsw/faiss_hnsw\.h|db/index/vector_index/faiss_hnsw/faiss_hnsw.h|g' \
    src/db/core/vector_query.c \
    src/db/storage/vector/vector_engine.c \
    src/db/storage/vector/vector_index_persist.c \
    src/db/storage/vector/faiss_hnsw_stub.c \
    src/db/index/vector_index/delete/graph_repair.c \
    src/db/index/vector_index/delete/vector_index_delete_api.c \
    src/kbase/kbase_index.c \
    src/kbase/kbase_search.c \
    tools/benchmark/c_api/vector_index_c_api.c \
    include/db/vector_engine.h
```

- [ ] **Step 2: 更新 sift_test/CMakeLists.txt**

保留 sift_recall 和 sift_recall_bruteforce_gt，修复 include 路径。

- [ ] **Step 3: 全项目编译**

```bash
cd build/engineering
cmake --build . 2>&1 | head -100
```

预期：成功编译，无链接错误。

---

## Task 10：实现 hnsw 目录（pgVector 风格持久化版）

**Files:**
- 创建: `engineering/include/db/index/vector_index/hnsw/hnsw.h`
- 创建: `engineering/include/db/index/vector_index/hnsw/hnsw_page.h`
- 创建: `engineering/include/db/index/vector_index/hnsw/hnsw_distance.h`
- 创建: `engineering/src/db/index/vector_index/hnsw/CMakeLists.txt`
- 创建: `engineering/src/db/index/vector_index/hnsw/hnsw_create.c`
- 创建: `engineering/src/db/index/vector_index/hnsw/hnsw_insert.c`
- 创建: `engineering/src/db/index/vector_index/hnsw/hnsw_search.c`
- 创建: `engineering/src/db/index/vector_index/hnsw/hnsw_page.c`
- 创建: `engineering/src/db/index/vector_index/hnsw/hnsw_delete.c`
- 创建: `engineering/src/db/index/vector_index/hnsw/hnsw_utils.c`
- Test: 与 faiss_hnsw 召回率一致

**Interfaces:**
- Consumes: `faiss_hnsw_search_layer`（复用算法逻辑）
- Produces: `hnsw_index_create/open/close`、`hnsw_index_add`、`hnsw_index_search`

**关键区别：**
- 向量存储在页面文件中（参考 pgVector `hnsw.c` 的页面管理）
- HNSW 图结构通过 `vector_ref_t` 引用向量（而非直接指针）
- 提供 `hnsw_index_create`（新建）和 `hnsw_index_open`（从页面恢复）

- [ ] **Step 1: 创建 hnsw_page.h**

参考 `reference/open-source/pgvector/src/hnsw.h` 的页面格式：
- 元页面：存储 M/efConstruction/efSearch/维度/entry_point/max_level
- 数据页面：存储向量数据和邻居信息
- 页面大小：8KB（可配置）

```c
// engineering/include/db/index/vector_index/hnsw/hnsw_page.h
#define HNSW_PAGE_SIZE 8192
#define HNSW_MAGIC_NUMBER 0xA953A953
#define HNSW_PAGE_ID 0xFF90

typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t dims;
    int32_t M;
    int32_t ef_construction;
    int32_t ef_search;
    int32_t max_level;
    int32_t entry_point;
    int32_t n_total;
} hnsw_meta_page_t;

typedef struct {
    uint16_t page_id;
    uint16_t unused;
    uint32_t next_blkno;
} hnsw_page_header_t;
```

- [ ] **Step 2: 实现 hnsw_create.c**

- [ ] **Step 3: 实现 hnsw_page.c**

- [ ] **Step 4: 实现 hnsw_insert.c**

**复用 faiss_hnsw_search_layer 算法**，区别：
- 用 `vector_ref_t` 存储向量引用（而非直接 float 指针）
- 搜索时通过 `vector_ref_t` 从页面加载向量

- [ ] **Step 5: 实现 hnsw_search.c**

- [ ] **Step 6: 编译 + 召回率测试**

```bash
cd build/engineering && cmake --build . --target hnsw
```

---

## Task 11：编写 DESIGN.md 设计文档

**Files:**
- 创建: `engineering/src/db/index/vector_index/faiss_hnsw/DESIGN.md`
- 创建: `engineering/src/db/index/vector_index/hnsw/DESIGN.md`

- [ ] **Step 1: 编写 faiss_hnsw/DESIGN.md**

使用 Mermaid 图描述：
1. 整体架构（向量存储 + 图结构 + 量化层）
2. 搜索流程（分层搜索 + MinimaxHeap + VisitedTable）
3. 插入流程（层级分配 + bucket-sort + 建链）
4. 量化集成（SQ/PQ/LVQ/RaBitQ 编码路径）
5. 与 FAISS 的差异对比（如有）

- [ ] **Step 2: 编写 hnsw/DESIGN.md**

使用 Mermaid 图描述：
1. 页面布局（元页面 + 数据页面）
2. 持久化流程（向量写入页面 + 图结构序列化）
3. 恢复流程（从页面加载图结构 + 动态加载向量）
4. 与 faiss_hnsw 的算法一致性保证

---

## Task 12：SIFT Small 召回率验证

**Files:**
- 创建: `engineering/test/db/index/sift_test/hnsw_recall_test.c`
- Test: R@10 > 0.80（ef=40）

- [ ] **Step 1: 编写召回率测试**

使用已有的 sift_recall_bruteforce_gt.c 生成 groundtruth，测试 faiss_hnsw 和 hnsw 两个实现的召回率。

- [ ] **Step 2: 运行测试**

```bash
cd build/engineering
cmake --build .
ctest -R hnsw_recall_test --output-on-failure
```

预期：
- faiss_hnsw: R@10 ≥ 0.80（ef=40）
- hnsw: R@10 ≥ 0.80（ef=40）
- 两者召回率差异 < 1%

---

## Task 13：全项目编译和清理

**Files:**
- Test: 全项目编译通过
- Test: 所有现有测试通过

- [ ] **Step 1: 全项目编译**

```bash
cd build/engineering
cmake --build . 2>&1 | grep -E "error:|warning:" | head -30
```

- [ ] **Step 2: 运行现有测试**

```bash
ctest --output-on-failure 2>&1 | tail -20
```

- [ ] **Step 3: 提交**

```bash
git add -A
git commit -m "feat: 完成 HNSW 索引重构（faiss_hnsw + hnsw），支持 SQ/PQ/LVQ/RaBitQ 量化"
```
