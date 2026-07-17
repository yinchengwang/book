# HNSW neighbors 布局修复：对齐 FAISS 的预留布局

## 问题

`faiss_hnsw_add.c` 中 neighbors 数组采用**紧凑布局**（每个节点只占用实际邻居数），而搜索端 `get_neighbor` 使用 `cum_nneighbor` 做分层偏移定位。两者布局不兼容，导致搜索时 `level_offset` 指向错误位置，返回空结果，召回率 = 0。

## 根因

| 环节 | 布局模型 | 说明 |
|------|---------|------|
| **add.c**（步骤 7-8） | 紧凑布局 | `offsets` 按 `counts[i]` 累加，`neighbors` 只存实际邻居 |
| **search.c / search_layer.c**（`get_neighbor`） | 预留布局 | `neighbors[vec_offset + level_offset + i]` 用 `cum_nneighbor[level-1]` 定位层 |
| **create.c**（`cum_nneighbor`） | 预留布局 | `cum_nneighbor[0]=2*M, cum_nneighbor[l]=cum_nneighbor[l-1]+M` |

紧凑布局下，节点 i 的邻居数组是 `[nbr_0, nbr_1, ...]`（无分层间隙）。预留布局下，节点 i 的邻居数组是 `[level0_nbrs..., -1填充, level1_nbrs..., -1填充, ...]`，每层有其固定容量。

## 修复方案

参考 FAISS `HNSW::prepare_level_tab`，将 `faiss_hnsw_add.c` 的 neighbors 布局统一为**预留布局**。

### FAISS 参考

```cpp
// HNSW::set_default_probas: cum_nneighbor_per_level 是前缀和
//   cum_nneighbor_per_level[0] = 0
//   cum_nneighbor_per_level[1] = 2*M       // level 0 容量
//   cum_nneighbor_per_level[2] = 2*M + M    // level 0 + 1
//   cum_nneighbor_per_level[l] = 2*M + (l)*M

// HNSW::prepare_level_tab: 每个节点按 cum_nb_neighbors(level+1) 预留容量
//   offsets.push_back(offsets.back() + cum_nb_neighbors(pt_level + 1));
//   neighbors.resize(offsets.back(), -1);

// HNSW::neighbor_range: 搜索时用 cum_nneighbor 定位
//   *begin = o + cum_nb_neighbors(layer_no);
//   *end   = o + cum_nb_neighbors(layer_no + 1);
```

### 改动范围

仅修改 `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_add.c`，涉及步骤 7-9。

### 改动细节

#### 步骤 7（offsets 计算）— 改为按预留容量

```c
// 旧：紧凑布局
int32_t total_neighbors = 0;
for (int32_t i = 0; i < new_total; i++) {
    index->offsets[i] = total_neighbors;
    total_neighbors += counts[i];
}

// 新：预留布局（参考 FAISS prepare_level_tab）
int32_t total_reserved = 0;
for (int32_t i = 0; i < n; i++) {
    int32_t vec_id = old_total + i;
    int32_t level = index->levels[vec_id];
    // cum_nneighbor[level] = 该节点所有层邻居槽位总和
    // 例：level=0 → 2*M, level=1 → 2*M+M, level=2 → 2*M+2M
    int32_t reserved = (level >= 0) ? index->cum_nneighbor[level] : 0;
    index->offsets[vec_id] = total_reserved;
    total_reserved += reserved;
}
```

> **注意**：`cum_nneighbor[0] = 2*M` 即 level 0 容量，`cum_nneighbor[1] = 2*M + M` 即 level 0+1 总容量。所以 `cum_nneighbor[level]` 就是节点在 level 及以下所有层的邻居槽位总和，与 FAISS 的 `cum_nb_neighbors(pt_level+1)` 语义一致。

#### 步骤 8（neighbors 分配）— 全初始化为 -1

```c
// 旧：按实际邻居数分配
int32_t *new_neighbors = (int32_t *)realloc(
    index->neighbors, (size_t)total_neighbors * sizeof(int32_t));
memset(index->neighbors, 0, (size_t)total_neighbors * sizeof(int32_t));

// 新：按预留容量分配，初始化为 -1（与 FAISS 一致，-1 表示空槽位）
int32_t *new_neighbors = (int32_t *)realloc(
    index->neighbors, (size_t)total_reserved * sizeof(int32_t));
if (new_neighbors) {
    index->neighbors = new_neighbors;
    // 新扩展部分初始化为 -1
    int32_t old_size = (old_total > 0) ? index->offsets[old_total] : 0;
    memset(index->neighbors + old_size, -1, 
           (size_t)(total_reserved - old_size) * sizeof(int32_t));
}
```

#### 步骤 9（填充邻居）— 按层写入对应槽位

```c
// 旧：紧凑写入
index->neighbors[index->offsets[vec_id] + actual] = search_buf[best_idx];

// 新：按层写入对应槽位
for (int32_t l = 0; l <= level; l++) {
    int32_t capacity = (l == 0) ? (2 * index->M) : index->M;
    int32_t layer_offset = (l > 0) ? index->cum_nneighbor[l - 1] : 0;
    int32_t base = index->offsets[vec_id] + layer_offset;
    
    // 取该层需要的邻居数（最多 capacity 个）
    int32_t nbr_count = (l == 0) ? counts[vec_id] : 0;  // 简化：目前只有 level 0 有邻居
    // 实际需要从候选邻居中按层分配，当前实现只处理 level 0
    for (int32_t j = 0; j < nbr_count && j < capacity; j++) {
        index->neighbors[base + j] = candidate_ids[j];
    }
    // 剩余槽位保持 -1（已在步骤 8 初始化）
}
```

> **当前 add.c 的简化**：目前 `faiss_hnsw_add` 只处理 level 0 的邻居选择（`brute_search_ef` 找最近邻，取前 M0 个）。多层邻居分配逻辑后续按需扩展，但布局已经预留了各层空间。

### 不需要改动的文件

| 文件 | 原因 |
|------|------|
| `faiss_hnsw_create.c` | `cum_nneighbor` 初始化保持不动 |
| `faiss_hnsw_search.c` | `get_neighbor_internal` 中的 `level_offset` 逻辑不变 |
| `faiss_hnsw_search_layer.c` | `get_neighbor` / `get_neighbor_count` 逻辑不变 |
| `faiss_hnsw_internal.h` | 结构体定义不变 |

## 验证方式

1. 编译通过：`cmake --build build/engineering`
2. 现有 HNSW 搜索测试：`ctest --test-dir build/engineering -R hnsw`
3. 手动验证：搜索后检查返回结果中是否有有效 id（非 -1），确认召回率 > 0

## 未竟事宜

1. **多层邻居分配**：当前 `faiss_hnsw_add` 只处理 level 0 层邻居选择（`brute_search_ef` 取 top-M0）。`prepare_level_tab` 的多层预留布局已预备好，但多层邻居的搜索/选择逻辑（FAISS `add_links_starting_from` 中的逐层搜索）尚未实现。后续可参照 FAISS 的 `search_neighbors_to_add` 实现。

2. **老节点迁移**：如果已经在磁盘上存在紧凑布局的索引文件，加载后需要迁移到预留布局。当前持久化代码（`vector_index_persist.c`）在加载时重建 `neighbors` 数组，但布局格式与写入时一致。如果老索引是紧凑格式，需要一次迁移步骤。