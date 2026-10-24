# Phase 3: 性能优化实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将所有 O(n) 算法替换为 O(log n)/O(1)，修复线程安全，启用压缩

**Architecture:** 三组：3A 算法替换、3B 线程安全、3C 压缩启用

**Tech Stack:** C14, GCC/G++, GTest, pthread

---

## Task 27: kv_ordered skip list

**Files:**
- Modify: `src/db/storage/kv/kv_ordered.c`
- Modify: `tests/test_kv.c`

- [ ] **Step 1: 定义 skip list 数据结构**

```c
#define SKIP_LIST_MAX_LEVEL 16

typedef struct skip_list_node {
    struct skip_list_node *next[SKIP_LIST_MAX_LEVEL];
    char *key;
    size_t klen;
    char *val;
    size_t vlen;
    uint8_t level;
} skip_list_node_t;

typedef struct kv_ordered {
    skip_list_node_t *head;
    uint32_t size;
    uint8_t max_level;
} kv_ordered_t;
```

- [ ] **Step 2: 实现 skip list insert/get/delete**

- [ ] **Step 3: 替换 kv_ordered 当前实现**

- [ ] **Step 4: 写性能测试验证 O(log n)**

- [ ] **Step 5: Commit**

---

## Task 28: bm25 hash map 替换

**Files:**
- Modify: `src/db/storage/sparse/bm25_index.c`
- Modify: `tests/test_sparse.c`

- [ ] **Step 1: hash map 数据结构**

```c
#define BM25_HASH_SIZE 4096

typedef struct bm25_hash_entry {
    char *term;
    uint32_t doc_freq;
    uint32_t *doc_ids;
    uint32_t doc_count;
    struct bm25_hash_entry *next;
} bm25_hash_entry_t;

typedef struct bm25_hash {
    bm25_hash_entry_t *buckets[BM25_HASH_SIZE];
} bm25_hash_t;
```

- [ ] **Step 2: 替换 bm25_find_term 线性扫描**

- [ ] **Step 3: 写测试验证查找正确**

- [ ] **Step 4: Commit**

---

## Task 29: Sparse top-k heap

**Files:**
- Modify: `src/db/storage/sparse/hybrid_retrieval.c`
- Modify: `tests/test_sparse.c`

- [ ] **Step 1: min-heap 数据结构**

- [ ] **Step 2: 替换选择排序**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 30: ST kNN + R-Tree

**Files:**
- Modify: `src/db/storage/st/st_engine.c`
- Modify: `tests/test_st.c`

- [ ] **Step 1: R-Tree 空间索引查询替代全表扫描**

- [ ] **Step 2: 修复 kNN 取全局 top-k**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 31: CF stats + iter 优化

**Files:**
- Modify: `src/db/cf/cf_engine.c`
- Modify: `tests/test_cf.c`

- [ ] **Step 1: 增量统计缓存**

- [ ] **Step 2: cf_iter_next 指针偏移**

- [ ] **Step 3: 写测试**

- [ ] **Step 4: Commit**

---

## Task 32: Catalog hash table

**Files:**
- Modify: `src/db/storage/catalog/catalog.c`
- Modify: `tests/test_catalog.c`

- [ ] **Step 1: 链表 → hash table O(1)**

- [ ] **Step 2: 写测试验证查找性能**

- [ ] **Step 3: Commit**

---

## Task 33: BM25 线程安全

**Files:**
- Modify: `src/db/storage/sparse/bm25_index.c`

- [ ] **Step 1: 全局状态 → 结构体封装 + mutex**

- [ ] **Step 2: 所有访问加锁**

- [ ] **Step 3: 写并发测试**

- [ ] **Step 4: Commit**

---

## Task 34: Graph CSR rwlock

**Files:**
- Modify: `src/db/storage/graph/graph_csr.c`

- [ ] **Step 1: use_lock → 真实 rwlock**

- [ ] **Step 2: 读操作用 rdlock，写操作用 wrlock**

- [ ] **Step 3: 写并发测试**

- [ ] **Step 4: Commit**

---

## Task 35: Columnar 压缩启用

**Files:**
- Modify: `src/db/storage/columnar/columnar_engine.c`
- Modify: `tests/test_columnar.c`

- [ ] **Step 1: LZ4 压缩实现**

- [ ] **Step 2: Delta 编码（数值列）**

- [ ] **Step 3: 写测试验证压缩/解压往返**

- [ ] **Step 4: Commit**

---

## Task 36: Timeseries Gorilla 编码激活

**Files:**
- Modify: `src/db/storage/ts/ts_compress.c`
- Modify: `tests/test_timeseries.c`

- [ ] **Step 1: 激活 Gorilla 编码**

- [ ] **Step 2: 写测试验证压缩/解压**

- [ ] **Step 3: Commit**
