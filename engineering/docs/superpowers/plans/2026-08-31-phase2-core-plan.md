# Phase 2: 模态核心功能补完实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 补齐 15 个存储模态的 stub 函数、持久化、索引加载，使其达到功能完整

**Architecture:** 三组：2A 存储引擎补完、2B 索引/查询补完、2C 持久化修复

**Tech Stack:** C14, GCC/G++, GTest

## Global Constraints

1. 统一 `storage_result_t` 错误码
2. WAL-first 模式
3. 编译标准: C14, C++14 (测试)
4. 每个模态实现 `storage_ops_t` 接口

---

## Task 12: KV — LSM flush + skip list

**Files:**
- Modify: `src/db/storage/kv/lsm/lsm_tree.c`
- Modify: `src/db/storage/kv/kv_ordered.c`
- Modify: `tests/test_kv.c`

**Interfaces:**
- `lsm_flush`: memtable → SSTable flush
- `lsm_compact`: SSTable 合并
- `kv_ordered`: linked list → skip list O(log n)

- [ ] **Step 1: 实现 lsm_flush**

```c
int lsm_flush(lsm_tree_t *lsm) {
    if (!lsm || skip_list_size(lsm->memtable) == 0) return 0;

    sstable_t *sst = sstable_create(lsm->sst_dir, lsm->next_sst_id++);
    skip_list_iter_t *iter = skip_list_create_iter(lsm->memtable);
    while (skip_list_iter_next(iter)) {
        sstable_append(sst, iter->key, iter->klen, iter->val, iter->vlen);
    }
    skip_list_iter_free(iter);
    sstable_finish(sst);

    // WAL checkpoint
    wal_checkpoint(lsm->wal, lsm->memtable_min_lsn);

    // 清空 memtable
    skip_list_clear(lsm->memtable);
    lsm->memtable_min_lsn = 0;

    // 加入 SSTable 列表
    lsm->sstables[lsm->sst_count++] = sst;
    return STORAGE_OK;
}
```

- [ ] **Step 2: 实现 lsm_compact**

```c
int lsm_compact(lsm_tree_t *lsm) {
    if (lsm->sst_count < 2) return 0;

    // 合并所有 SSTable 为一个新的有序 SSTable
    sstable_t *merged = sstable_create(lsm->sst_dir, lsm->next_sst_id++);

    // 多路归并
    sstable_iter_t *iters[lsm->sst_count];
    for (int i = 0; i < lsm->sst_count; i++) {
        iters[i] = sstable_create_iter(lsm->sstables[i]);
        sstable_iter_next(iters[i]);
    }

    // 归并写入
    while (1) {
        int min_idx = -1;
        for (int i = 0; i < lsm->sst_count; i++) {
            if (!iters[i]->done) {
                if (min_idx < 0 || compare_keys(iters[i]->key, iters[min_idx]->key) < 0) {
                    min_idx = i;
                }
            }
        }
        if (min_idx < 0) break;
        sstable_append(merged, iters[min_idx]->key, iters[min_idx]->klen,
                       iters[min_idx]->val, iters[min_idx]->vlen);
        sstable_iter_next(iters[min_idx]);
    }

    sstable_finish(merged);

    // 清理旧 SSTable
    for (int i = 0; i < lsm->sst_count; i++) {
        sstable_destroy(lsm->sstables[i]);
        sstable_iter_free(iters[i]);
    }
    lsm->sstables[0] = merged;
    lsm->sst_count = 1;
    return STORAGE_OK;
}
```

- [ ] **Step 3: 写测试验证 flush/compaction**

```c
TEST_F(KvTest, LsmFlush) {
    // 插入数据超过 memtable 容量，触发 flush
    for (int i = 0; i < 1000; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        kv_put(db, key, strlen(key), val, strlen(val));
    }
    // 验证数据仍可读
    char buf[32]; size_t len;
    EXPECT_EQ(kv_get(db, "key_500", 7, buf, &len), 0);
}

TEST_F(KvTest, LsmCompact) {
    // 多次 flush 后 compaction
    for (int i = 0; i < 5000; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        kv_put(db, key, strlen(key), val, strlen(val));
    }
    // 验证数据正确
}
```

- [ ] **Step 4: Commit**

```bash
git add src/db/storage/kv/lsm/lsm_tree.c tests/test_kv.c
git commit -m "feat(kv): implement LSM flush and compaction"
```

---

## Task 13: Columnar — 持久化 + 压缩

**Files:**
- Modify: `src/db/storage/columnar/columnar_engine.c`
- Create: `tests/test_columnar.c`

- [ ] **Step 1: 修复序列化（指针 → 偏移量）**

- [ ] **Step 2: 实现 compress（LZ4/Delta）**

- [ ] **Step 3: 修复 destroy_chunk 双重 free**

- [ ] **Step 4: 实现 chunk 扩展（容量满时创建新 chunk）**

- [ ] **Step 5: 写测试验证创建/打开/追加/压缩往返**

- [ ] **Step 6: Commit**

---

## Task 14: Blob — 代码去重 + multipart 修复

**Files:**
- Modify: `src/db/storage/blob/blob_engine.c`
- Modify: `src/db/storage/blob/blob_gc.c`
- Modify: `src/db/storage/blob/blob_multipart.c`
- Create: `tests/test_blob.c`

- [ ] **Step 1: 提取公共 chunk 操作函数**

- [ ] **Step 2: 修复 multipart session 泄漏**

```c
int blob_multipart_abort(blob_multipart_t *mp) {
    if (!mp) return STORAGE_ERR_INVALID_ARG;
    for (int i = 0; i < mp->part_count; i++) {
        blob_engine_delete_chunk(mp->engine, mp->parts[i].chunk_id);
    }
    free(mp->upload_id);
    free(mp->parts);
    free(mp);  // 修复前遗漏
    return STORAGE_OK;
}
```

- [ ] **Step 3: hash 表扩容**

- [ ] **Step 4: 写测试验证无泄漏**

- [ ] **Step 5: Commit**

---

## Task 15: Document — doc_engine_get + inverted index load

**Files:**
- Modify: `src/db/storage/doc/doc_engine.c`
- Modify: `src/db/storage/doc/doc_inverted.c`
- Modify: `tests/test_document.c`

- [ ] **Step 1: 实现 doc_engine_get**

```c
int doc_engine_get(doc_engine_t *engine, const char *doc_id,
                   size_t id_len, doc_t **doc) {
    uint64_t offset = doc_inverted_find(engine->inverted_idx, doc_id, id_len);
    if (offset == 0) return STORAGE_ERR_NOT_FOUND;
    *doc = doc_read_from_disk(engine->data_path, offset);
    return STORAGE_OK;
}
```

- [ ] **Step 2: 实现 inverted index load**

- [ ] **Step 3: BM25 真实 TF-IDF 计算**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 16: Vector — top-k heap + persist

**Files:**
- Modify: `src/db/storage/vector/vector_engine.c`
- Modify: `src/db/storage/vector/vector_index_persist.c`
- Modify: `tests/test_vector.c`

- [ ] **Step 1: 选择排序 → min-heap**

```c
vector_search_result_t *vector_search(vector_engine_t *engine,
                                       const float *query, int k) {
    min_heap_t heap;
    min_heap_init(&heap, k);

    for (uint64_t i = 0; i < engine->vector_count; i++) {
        float dist = vector_distance(query, engine->vectors[i], engine->dimension);
        if (min_heap_size(&heap) < k) {
            min_heap_push(&heap, dist, i);
        } else if (dist < min_heap_peek(&heap)) {
            min_heap_pop(&heap);
            min_heap_push(&heap, dist, i);
        }
    }

    vector_search_result_t *results = malloc(k * sizeof(vector_search_result_t));
    for (int i = k - 1; i >= 0; i--) {
        results[i] = min_heap_pop(&heap);
    }
    return results;
}
```

- [ ] **Step 2: DiskANN/IVF 持久化**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 17: RDF — 索引激活 + 安全操作

**Files:**
- Modify: `src/db/storage/rdf/rdf_engine.c`
- Create: `tests/test_rdf.c`

- [ ] **Step 1: insert 路径激活索引**

```c
int rdf_engine_insert(rdf_db_t *db, rdf_triple_t *triple) {
    fwrite(triple, sizeof(rdf_triple_t), 1, db->triples_file);
    rdf_triple_list_append(&db->triples, triple);
    // 激活索引（修复前从未调用）
    rdf_index_add_triple(db->subject_index, triple, TRIPLE_POS_SUBJECT);
    rdf_index_add_triple(db->predicate_index, triple, TRIPLE_POS_PREDICATE);
    rdf_index_add_triple(db->object_index, triple, TRIPLE_POS_OBJECT);
    return STORAGE_OK;
}
```

- [ ] **Step 2: system("rm") → 平台无关删除**

- [ ] **Step 3: hash_term snprintf %d 修复**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 18: Relational — relation_create + crash recovery

**Files:**
- Modify: `src/db/storage/rel/rel_engine.c`
- Modify: `tests/test_relational.c`

- [ ] **Step 1: relation_create stub → 真实实现**

```c
int relation_create(uint32_t relid, TupleDesc desc, char relkind, int am) {
    if (relid == 0 || !desc) return -1;
    wal_record_t rec = { .modality = MODALITY_REL, .op = WAL_OP_CREATE };
    wal_append(g_wal, &rec);
    catalog_create_table(g_catalog, relid, desc, relkind, am);
    char path[256];
    snprintf(path, sizeof(path), "data/rel/%u.dat", relid);
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return STORAGE_ERR_IO;
    close(fd);
    return STORAGE_OK;
}
```

- [ ] **Step 2: heap_getnext 递归 → 迭代**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 19: Log — aggregate 真实计算

**Files:**
- Modify: `src/db/storage/log/log_aggregate.c`
- Modify: `tests/test_log.c`

- [ ] **Step 1: log_rate 真实实现**

- [ ] **Step 2: aggregate_sum/max/min 解析数值**

- [ ] **Step 3: log_query 使用 selector**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 20: Stream — 持久化修复

**Files:**
- Modify: `src/db/storage/stream/stream_engine.c`
- Create: `tests/test_stream.c`

- [ ] **Step 1: stream_open 重建指针**

- [ ] **Step 2: stream_consume offset 索引**

- [ ] **Step 3: consumer 绑定正确 partition**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 21: CF — SSTable 或移除

**Files:**
- Modify: `src/db/cf/cf_engine.c`
- Create: `tests/test_cf.c`

- [ ] **Step 1: cf_family_stats 增量统计**

- [ ] **Step 2: cf_iter_next 指针偏移**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 22: ST — R-Tree 索引集成

**Files:**
- Modify: `src/db/storage/st/st_engine.c`
- Create: `tests/test_st.c`

- [ ] **Step 1: 使用 rtree.h 构建空间索引**

- [ ] **Step 2: kNN 修复（全局 top-k）**

- [ ] **Step 3: system("rm") → 平台无关删除**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 23: Sparse — 线程安全 + 效率

**Files:**
- Modify: `src/db/storage/sparse/bm25_index.c`
- Create: `tests/test_sparse.c`

- [ ] **Step 1: 全局状态 → 结构体 + mutex**

- [ ] **Step 2: bm25_find_term 线性扫描 → hash map**

- [ ] **Step 3: token 数组动态分配**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 24: MMView — refresh 实现

**Files:**
- Modify: `src/db/storage/mmview/mview.c`
- Create: `tests/test_mmview.c`

- [ ] **Step 1: mview_refresh_complete 执行查询**

- [ ] **Step 2: mview_has_cycle DFS 拓扑排序**

- [ ] **Step 3: 修复编译错误 stats.refreshing_mviews**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**

---

## Task 25: Multimodal — 联合搜索

**Files:**
- Modify: `src/db/storage/multimodal/cross_modal.c`
- Create: `tests/test_multimodal.c`

- [ ] **Step 1: cross_modal_search 调用实际引擎**

- [ ] **Step 2: RRF score 真实计算**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 26: Integrity — 真实校验

**Files:**
- Modify: `src/db/storage/integrity/data_integrity.c`
- Create: `tests/test_integrity.c`

- [ ] **Step 1: 所有引擎检查 stub → 真实校验**

- [ ] **Step 2: page_verify_and_repair 真实实现**

- [ ] **Step 3: WAL 校验真实实现**

- [ ] **Step 4: 写测试**

- [ ] **Step 5: Commit**
