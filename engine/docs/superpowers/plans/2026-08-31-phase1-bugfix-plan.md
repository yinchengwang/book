# Phase 1: 数据破坏性 Bug 修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 修复 4 个数据破坏性/崩溃级 Bug，使核心模态达到安全使用标准

**Architecture:** 4 个修复互不依赖，可完全并行

**Tech Stack:** C14, GCC/G++, GTest

## Global Constraints

1. 统一 `storage_result_t` 错误码
2. WAL-first 模式
3. 编译标准: C14, C++14 (测试)
4. 每个修复必须有回归测试

---

## Task 8: Spatial 数据破坏修复

**Files:**
- Modify: `src/db/storage/spatial/spatial_engine.c`
- Modify: `src/db/storage/spatial/rtree.c`
- Modify: `tests/test_spatial.c`

**Interfaces:**
- 修复 `spatial_tuple_update` 和 `spatial_tuple_delete` 不破坏数据
- 激活 `rtree_split_root` 调用

- [ ] **Step 1: 读取现有代码**

Read: `src/db/storage/spatial/spatial_engine.c` — 找到 update/delete 函数

- [ ] **Step 2: 修复 tuple_update**

```c
// 修复前（破坏性）
int spatial_tuple_update(..., const void *new_val, size_t new_len) {
    memcpy(old_tuple->data, new_val, new_len);  // 破坏原值
}

// 修复后（安全）
int spatial_tuple_update(spatial_engine_t *eng, uint64_t tuple_id,
                         const void *new_val, size_t new_len) {
    // 1. 读取旧值
    spatial_tuple_t *old = spatial_get_tuple(eng, tuple_id);
    if (!old) return STORAGE_ERR_NOT_FOUND;

    // 2. WAL 先写旧值
    wal_record_t rec = {0};
    rec.op = WAL_OP_UPDATE;
    rec.modality = MODALITY_SPATIAL;
    // 序列化旧值到 rec.data
    wal_append(eng->wal, &rec);

    // 3. 更新 R-Tree 索引
    bbox_t old_bbox = spatial_compute_bbox(old);
    bbox_t new_bbox = spatial_compute_bbox_from_val(new_val, new_len);
    rtree_update(eng->rtree, old_bbox, new_bbox, tuple_id);

    // 4. 写入新值
    memcpy(old->data, new_val, new_len);
    return STORAGE_OK;
}
```

- [ ] **Step 3: 修复 tuple_delete**

同理：先 WAL 写旧值 → 从 R-Tree 移除 → 标记删除

- [ ] **Step 4: 激活 R-Tree split**

```c
// rtree.c — rtree_insert 中激活 split
int rtree_insert(rtree_t *rtree, bbox_t bbox, uint64_t id) {
    if (rtree->root->count >= RTREE_MAX_ENTRIES) {
        return rtree_split_root(rtree);  // 激活
    }
    return rtree_insert_into(rtree->root, bbox, id);
}
```

- [ ] **Step 5: 写回归测试**

```c
TEST_F(SpatialTest, UpdatePreservesData) {
    // 插入 → 更新 → 验证旧位置数据正确
    uint64_t id = spatial_insert(eng, point, sizeof(point));
    spatial_update(eng, id, new_point, sizeof(new_point));
    spatial_tuple_t *t = spatial_get_tuple(eng, id);
    EXPECTmemcmp(t->data, new_point, sizeof(new_point));
}

TEST_F(SpatialTest, DeleteRemovesFromIndex) {
    uint64_t id = spatial_insert(eng, point, sizeof(point));
    spatial_delete(eng, id);
    // 查询不应返回已删除的点
    bbox_t query = {-180, -90, 180, 90};
    uint64_t *results; uint32_t count;
    spatial_range_query(eng, query, &results, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(SpatialTest, RTreeSplitTriggers) {
    // 插入超过 RTREE_MAX_ENTRIES 个点，验证不崩溃
    for (int i = 0; i < RTREE_MAX_ENTRIES + 10; i++) {
        spatial_point_t p = {(double)i, (double)i};
        spatial_insert(eng, &p, sizeof(p));
    }
    SUCCEED();
}
```

- [ ] **Step 6: 编译运行测试**

- [ ] **Step 7: Commit**

```bash
git add src/db/storage/spatial/spatial_engine.c src/db/storage/spatial/rtree.c tests/test_spatial.c
git commit -m "fix(spatial): safe update/delete with WAL logging, activate R-Tree split"
```

---

## Task 9: Graph CSR 修复

**Files:**
- Modify: `src/db/storage/graph/graph_csr.c`
- Modify: `tests/test_graph.c`

**Interfaces:**
- 修复 compact 后反向索引构建
- 修复 compact 指针失效
- 实现 scan

- [ ] **Step 1: 读取现有代码**

Read: `src/db/storage/graph/graph_csr.c` — 找到 compact 函数

- [ ] **Step 2: 修复 compact**

```c
int graph_csr_compact(graph_csr_t *csr) {
    if (csr->coo_count == 0) return 0;

    // 1. 排序 COO
    qsort(csr->coo_edges, csr->coo_count, sizeof(graph_csr_edge_t), cmp_by_src);

    // 2. 分配新的连续内存（修复指针失效）
    csr->out_offset = realloc(csr->out_offset,
                              (csr->vertex_count + 1) * sizeof(uint32_t));
    csr->out_edges = malloc(csr->edge_count * sizeof(graph_csr_edge_t));

    // 3. 建立偏移
    uint32_t edge_idx = 0;
    for (uint64_t v = 0; v <= csr->vertex_count; v++) {
        csr->out_offset[v] = edge_idx;
        while (edge_idx < csr->edge_count &&
               csr->coo_edges[edge_idx].src == v) {
            csr->out_edges[edge_idx] = csr->coo_edges[edge_idx];
            edge_idx++;
        }
    }

    // 4. 构建反向索引（修复前遗漏）
    graph_csr_build_reverse_index(csr);

    // 5. 释放 COO
    free(csr->coo_edges);
    csr->coo_edges = NULL;
    csr->coo_count = 0;
    csr->compact_count = 0;

    return STORAGE_OK;
}
```

- [ ] **Step 3: 实现 reverse index**

```c
void graph_csr_build_reverse_index(graph_csr_t *csr) {
    // 按 dst 分组
    csr->in_offset = calloc(csr->vertex_count + 1, sizeof(uint32_t));
    // 统计每个顶点的入度
    for (uint32_t i = 0; i < csr->edge_count; i++) {
        csr->in_offset[csr->out_edges[i].dst + 1]++;
    }
    // 前缀和
    for (uint64_t v = 1; v <= csr->vertex_count; v++) {
        csr->in_offset[v] += csr->in_offset[v - 1];
    }
    // 填充
    csr->in_edges = malloc(csr->edge_count * sizeof(graph_csr_edge_t));
    uint32_t *cursor = calloc(csr->vertex_count, sizeof(uint32_t));
    for (uint32_t i = 0; i < csr->edge_count; i++) {
        uint64_t dst = csr->out_edges[i].dst;
        uint32_t pos = csr->in_offset[dst] + cursor[dst];
        csr->in_edges[pos] = csr->out_edges[i];
        csr->in_edges[pos].src = csr->out_edges[i].dst;  // 反转
        csr->in_edges[pos].dst = csr->out_edges[i].src;
        cursor[dst]++;
    }
    free(cursor);
}
```

- [ ] **Step 4: 写回归测试**

```c
TEST_F(GraphCsrTest, CompactReverseIndex) {
    for (uint64_t i = 0; i < 5; i++) graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    graph_csr_add_edge(csr, 1, 2, 0, NULL, 0);
    graph_csr_add_edge(csr, 2, 0, 0, NULL, 0);

    graph_csr_compact(csr);

    // 验证反向索引
    uint32_t in_count = 0;
    const graph_csr_edge_t *in_edges = graph_csr_get_in_edges(csr, 0, &in_count);
    EXPECT_EQ(in_count, 1u);  // 2→0
    EXPECT_EQ(in_edges[0].src, 2u);

    in_edges = graph_csr_get_in_edges(csr, 1, &in_count);
    EXPECT_EQ(in_count, 1u);  // 0→1
    EXPECT_EQ(in_edges[0].src, 0u);
}

TEST_F(GraphCsrTest, ScanAfterCompact) {
    for (uint64_t i = 0; i < 3; i++) graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    graph_csr_add_edge(csr, 0, 2, 0, NULL, 0);
    graph_csr_compact(csr);

    uint64_t *ids; uint32_t count;
    int ret = graph_csr_scan(csr, &ids, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 3u);  // 3 个顶点
    free(ids);
}
```

- [ ] **Step 5: 编译运行测试**

- [ ] **Step 6: Commit**

```bash
git add src/db/storage/graph/graph_csr.c tests/test_graph.c
git commit -m "fix(graph): build reverse index in compact, fix pointer invalidation, implement scan"
```

---

## Task 10: Yang NULL Handle 修复

**Files:**
- Modify: `src/db/storage/yang/yang_tree.c`
- Modify: `tests/test_tree_yang.c`

**Interfaces:**
- 修复 `yang_sql_ancestors/descendants` NULL crash

- [ ] **Step 1: 修复 NULL 检查**

```c
yang_result_t yang_sql_ancestors(yang_tree_t *tree, uint64_t node_id, ...) {
    if (!tree || !tree->root) {
        return YANG_ERR_INVALID_TREE;
    }
    // 正常逻辑
}

yang_result_t yang_sql_descendants(yang_tree_t *tree, uint64_t node_id, ...) {
    if (!tree || !tree->root) {
        return YANG_ERR_INVALID_TREE;
    }
    // 正常逻辑
}
```

- [ ] **Step 2: 写回归测试**

```c
TEST_F(YangTest, AncestorsNullTree) {
    uint64_t *ids; uint32_t count;
    EXPECT_NE(yang_sql_ancestors(NULL, 1, &ids, &count), 0);
}

TEST_F(YangTest, DescendantsNullTree) {
    uint64_t *ids; uint32_t count;
    EXPECT_NE(yang_sql_descendants(NULL, 1, &ids, &count), 0);
}
```

- [ ] **Step 3: 编译运行测试**

- [ ] **Step 4: Commit**

```bash
git add src/db/storage/yang/yang_tree.c tests/test_tree_yang.c
git commit -m "fix(yang): null-safe ancestors/descendants, prevent NULL deref crash"
```

---

## Task 11: Timeseries Partition + Tag Index

**Files:**
- Create: `src/db/storage/ts/ts_partition.c`
- Modify: `src/db/storage/ts/ts_tag_index.c`
- Modify: `tests/test_timeseries.c`

**Interfaces:**
- 新增 partition 创建/插入/查询
- 修复 tag_index 查询过滤

- [ ] **Step 1: 实现 ts_partition.c**

```c
// ts_partition.c
#include "db/storage/ts/ts_partition.h"

ts_partition_t *ts_partition_create(const char *dir, uint64_t start, uint64_t end) {
    ts_partition_t *part = calloc(1, sizeof(ts_partition_t));
    part->start_time = start;
    part->end_time = end;
    snprintf(part->filepath, sizeof(part->filepath), "%s/part_%lu.bin", dir, start);
    part->segment_count = 0;
    part->segments = NULL;
    return part;
}

int ts_partition_insert(ts_partition_t *part, const ts_point_t *point) {
    if (!part || !point) return STORAGE_ERR_INVALID_ARG;
    FILE *f = fopen(part->filepath, "ab");
    if (!f) return STORAGE_ERR_IO;
    fwrite(point, sizeof(ts_point_t), 1, f);
    fclose(f);
    part->segment_count++;
    return STORAGE_OK;
}

int ts_partition_query(ts_partition_t *part, uint64_t start, uint64_t end,
                       ts_point_t **results, uint32_t *count) {
    if (!part || !results || !count) return STORAGE_ERR_INVALID_ARG;
    FILE *f = fopen(part->filepath, "rb");
    if (!f) { *count = 0; return STORAGE_OK; }

    // 收集匹配的点
    uint32_t capacity = 64;
    *results = malloc(capacity * sizeof(ts_point_t));
    *count = 0;

    ts_point_t point;
    while (fread(&point, sizeof(ts_point_t), 1, f) == 1) {
        if (point.timestamp >= start && point.timestamp <= end) {
            if (*count >= capacity) {
                capacity *= 2;
                *results = realloc(*results, capacity * sizeof(ts_point_t));
            }
            (*results)[(*count)++] = point;
        }
    }
    fclose(f);
    return STORAGE_OK;
}
```

- [ ] **Step 2: 修复 ts_tag_index_query**

```c
int ts_tag_index_query(ts_tag_index_t *idx, const char *tag,
                       const char *value, uint64_t **series_ids, uint32_t *count) {
    uint32_t hash = fnv1a_hash(tag, strlen(tag));
    ts_tag_entry_t *entry = idx->buckets[hash % TAG_INDEX_BUCKETS];
    *count = 0;
    uint32_t capacity = 16;
    *series_ids = malloc(capacity * sizeof(uint64_t));

    while (entry) {
        if (strcmp(entry->tag, tag) == 0 && strcmp(entry->value, value) == 0) {
            // 收集此 entry 的 series_ids
            for (uint32_t i = 0; i < entry->series_count; i++) {
                if (*count >= capacity) {
                    capacity *= 2;
                    *series_ids = realloc(*series_ids, capacity * sizeof(uint64_t));
                }
                (*series_ids)[(*count)++] = entry->series_ids[i];
            }
        }
        entry = entry->next;
    }
    return STORAGE_OK;
}
```

- [ ] **Step 3: 写回归测试**

```c
TEST_F(TimeseriesTest, PartitionCreateInsertQuery) {
    ts_partition_t *part = ts_partition_create("./test_ts_part", 0, 1000);
    ASSERT_NE(part, nullptr);

    ts_point_t p1 = {100, 1.0};
    ts_point_t p2 = {200, 2.0};
    ts_point_t p3 = {500, 3.0};

    EXPECT_EQ(ts_partition_insert(part, &p1), 0);
    EXPECT_EQ(ts_partition_insert(part, &p2), 0);
    EXPECT_EQ(ts_partition_insert(part, &p3), 0);

    ts_point_t *results; uint32_t count;
    EXPECT_EQ(ts_partition_query(part, 150, 300, &results, &count), 0);
    EXPECT_EQ(count, 1u);  // 只有 p2
    EXPECT_DOUBLE_EQ(results[0].value, 2.0);
    free(results);

    // 清理
    remove(part->filepath);
    free(part);
}

TEST_F(TimeseriesTest, TagIndexFilter) {
    ts_tag_index_t idx;
    ts_tag_index_init(&idx);

    uint64_t s1 = 1, s2 = 2, s3 = 3;
    ts_tag_index_add(&idx, "host", "server1", s1);
    ts_tag_index_add(&idx, "host", "server2", s2);
    ts_tag_index_add(&idx, "region", "us-east", s3);

    uint64_t *ids; uint32_t count;
    ts_tag_index_query(&idx, "host", "server1", &ids, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], s1);
    free(ids);

    ts_tag_index_destroy(&idx);
}
```

- [ ] **Step 4: 编译运行测试**

- [ ] **Step 5: Commit**

```bash
git add src/db/storage/ts/ts_partition.c src/db/storage/ts/ts_tag_index.c tests/test_timeseries.c
git commit -m "feat(ts): implement partition CRUD, fix tag_index query filtering"
```
