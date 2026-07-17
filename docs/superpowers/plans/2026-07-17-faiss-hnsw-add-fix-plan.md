# faiss_hnsw_add neighbors 布局修复 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 将 `faiss_hnsw_add.c` 的 neighbors 数组布局从紧凑布局改为 FAISS 标准的预留布局，使搜索端 `get_neighbor` 的 `cum_nneighbor` 偏移能正确定位每层邻居，修复召回率 = 0 的 bug。

**架构：** 仅修改 `faiss_hnsw_add.c` 中步骤 7-9（offsets 计算、neighbors 分配、邻居填充），对齐 FAISS `HNSW::prepare_level_tab` 的布局模型。

**涉及文件：**
- Modify: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_add.c`

---

## 任务分解

### 任务 1：修改 offsets 计算和 neighbors 分配

**文件：**
- Modify: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_add.c:247-273`

**接口：**
- 消费：`index->levels[vec_id]`（步骤 6 已分配），`index->cum_nneighbor[]`（create.c 初始化）
- 产出：`index->offsets[vec_id]` 按预留容量排列，`index->neighbors` 容量 = 所有节点预留容量之和

- [ ] **步骤 1：修改步骤 7 — 按预留容量计算 offsets**

将紧凑布局的 `total_neighbors += counts[i]` 改为按 `cum_nneighbor[level]` 计算：

```c
// 旧代码（第 248-254 行）
int32_t total_neighbors = 0;
for (int32_t i = 0; i < new_total; i++) {
    index->offsets[i] = total_neighbors;
    total_neighbors += counts[i];
}

// 新代码：按预留容量计算
int32_t total_reserved = 0;
for (int32_t i = 0; i < n; i++) {
    int32_t vec_id = old_total + i;
    int32_t level = index->levels[vec_id];
    // cum_nneighbor[level] = 该节点所有层邻居槽位总和
    // level=0 → 2*M, level=1 → 2*M+M, level=2 → 2*M+2M
    int32_t reserved = (level >= 0) ? index->cum_nneighbor[level] : 0;
    index->offsets[vec_id] = total_reserved;
    total_reserved += reserved;
}
```

- [ ] **步骤 2：修改步骤 8 — 按预留容量分配 neighbors，初始化为 -1**

```c
// 旧代码（第 259-273 行）
if (total_neighbors > 0) {
    int32_t *new_neighbors = (int32_t *)realloc(
        index->neighbors, (size_t)total_neighbors * sizeof(int32_t));
    ...
    memset(index->neighbors, 0, (size_t)total_neighbors * sizeof(int32_t));
}

// 新代码：按预留容量分配，-1 初始化
if (total_reserved > 0) {
    int32_t old_neighbors_size = (old_total > 0) ? index->offsets[old_total] : 0;
    int32_t *new_neighbors = (int32_t *)realloc(
        index->neighbors, (size_t)total_reserved * sizeof(int32_t));
    if (!new_neighbors) {
        free(counts); free(candidate_ids); free(candidate_dists);
        free(search_buf); free(search_dists);
        return -1;
    }
    index->neighbors = new_neighbors;
    // 只初始化新扩展部分为 -1（老部分保持不动）
    memset(index->neighbors + old_neighbors_size, -1,
           (size_t)(total_reserved - old_neighbors_size) * sizeof(int32_t));
}
```

> 注意：`total_neighbors` 变量不再需要，改为 `total_reserved`。所有后续使用 `total_neighbors` 的地方都要改为 `total_reserved`。检查步骤 9 之前是否有使用。

- [ ] **步骤 3：验证编译通过**

```bash
cmake --build build/engineering --target faiss_hnsw
```

预期：编译成功，无警告。

---

### 任务 2：修改邻居填充逻辑

**文件：**
- Modify: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_add.c:275-320`

**接口：**
- 消费：`index->offsets[vec_id]`（预留布局偏移），`index->cum_nneighbor[l-1]`（每层偏移）
- 产出：`index->neighbors` 中按层槽位正确填充

- [ ] **步骤 1：修改步骤 9 — 按层槽位写入邻居**

当前步骤 9 的填充代码（第 314 行）是紧凑写入：
```c
index->neighbors[index->offsets[vec_id] + actual] = search_buf[best_idx];
```

改为按层写入（目前只处理 level 0，但布局已预留各层空间）：

```c
// 替换第 275-320 行（步骤 9 全部）
// 9. 重新遍历，填充每个新向量的邻居（按层写入预留槽位）
int32_t fill_idx = 0;
for (int32_t i = 0; i < n; i++) {
    int32_t vec_id = old_total + i;
    const float *vec = vectors + (size_t)i * (size_t)dims;
    int32_t level = index->levels[vec_id];

    if (counts[vec_id] == 0) {
        continue;  // 无邻居，槽位保持 -1（已在步骤 8 初始化）
    }

    // 暴力搜索 ef 个候选
    int32_t n_candidates = 0;
    if (old_total + fill_idx > 0) {
        int32_t saved_total = index->n_total;
        index->n_total = old_total + fill_idx;
        n_candidates = brute_search_ef(index, vec, ef, search_buf, search_dists);
        index->n_total = saved_total;
    }

    if (n_candidates <= 0) {
        fill_idx++;
        continue;
    }

    // 从候选中取 counts[vec_id] 个最近邻
    int32_t take = n_candidates < counts[vec_id] ? n_candidates : counts[vec_id];
    int32_t *used = (int32_t *)calloc((size_t)n_candidates, sizeof(int32_t));
    if (!used) break;

    int32_t sorted_ids[64];  // 足够容纳 M0（最大 64）
    int32_t sorted_count = 0;
    for (int32_t k = 0; k < take; k++) {
        float best = 1e30f;
        int32_t best_idx = -1;
        for (int32_t j = 0; j < n_candidates; j++) {
            if (used[j]) continue;
            if (search_dists[j] < best) {
                best = search_dists[j];
                best_idx = j;
            }
        }
        if (best_idx < 0) break;
        used[best_idx] = 1;
        sorted_ids[sorted_count++] = search_buf[best_idx];
    }
    free(used);

    // 写入 level 0 的槽位
    int32_t capacity_l0 = 2 * index->M;
    int32_t base_l0 = index->offsets[vec_id];  // level 0 偏移 = 0
    for (int32_t j = 0; j < sorted_count && j < capacity_l0; j++) {
        index->neighbors[base_l0 + j] = sorted_ids[j];
    }
    // 更高层的槽位保持 -1（当前简化：只处理 level 0）

    fill_idx++;
}
```

- [ ] **步骤 2：验证编译通过**

```bash
cmake --build build/engineering --target faiss_hnsw
```

预期：编译成功，无警告。

- [ ] **步骤 3：运行搜索测试，验证召回率 > 0**

```bash
ctest --test-dir build/engineering -R hnsw --output-on-failure
```

预期：搜索能返回有效结果（id 非 -1），不再返回空。

---

## 全局约束

1. 只改 `faiss_hnsw_add.c` 一个文件，其他文件不动
2. `cum_nneighbor[level]` 语义：level=0 → 2*M，level=1 → 2*M+M，即所有层邻居槽位的前缀和
3. 未使用的邻居槽位必须填 -1（与 FAISS 一致）
4. 搜索端 `get_neighbor` / `get_neighbor_count` 不需要改动