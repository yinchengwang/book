# Phase 4: 新增特性实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现跨模态事务、物化视图完整功能、RDF SPARQL 增强、分布式基础

**Architecture:** 4 个独立特性，可并行

**Tech Stack:** C14, GCC/G++, GTest

---

## Task 37: 跨模态事务

**Files:**
- Modify: `src/db/storage/txn/txn.c`
- Modify: `src/db/storage/multimodal/cross_modal.c`
- Create: `tests/test_cross_txn.c`

- [ ] **Step 1: 两阶段提交协议**

```c
typedef struct cross_txn {
    uint32_t txn_id;
    storage_ops_t *参与模态[16];
    int modality_count;
    wal_record_t *prepare_records;
    int prepare_count;
    cross_txn_status_t status;  // PREPARING/COMMITTED/ABORTED
} cross_txn_t;

int cross_txn_prepare(cross_txn_t *txn) {
    // Phase 1: Prepare - 每个模态写 prepare WAL
    for (int i = 0; i < txn->modality_count; i++) {
        wal_record_t rec = { .op = WAL_OP_PREPARE, .txn_id = txn->txn_id };
        wal_append(g_wal, &rec);
    }
    txn->status = PREPARING;
    return STORAGE_OK;
}

int cross_txn_commit(cross_txn_t *txn) {
    // Phase 2: Commit - 每个模态写 commit WAL
    for (int i = 0; i < txn->modality_count; i++) {
        wal_record_t rec = { .op = WAL_OP_COMMIT, .txn_id = txn->txn_id };
        wal_append(g_wal, &rec);
    }
    txn->status = COMMITTED;
    return STORAGE_OK;
}
```

- [ ] **Step 2: 跨模态 WAL 记录关联**

- [ ] **Step 3: 死锁检测扩展**

- [ ] **Step 4: 写测试验证原子性**

- [ ] **Step 5: Commit**

---

## Task 38: 物化视图完整实现

**Files:**
- Modify: `src/db/storage/mmview/mview.c`
- Modify: `tests/test_mmview.c`

- [ ] **Step 1: mview_refresh_complete 执行查询并写入数据**

```c
int mview_refresh_complete(mview_t *mview) {
    if (!mview) return STORAGE_ERR_INVALID_ARG;

    // 1. 执行源查询
    query_result_t *result = query_execute(mview->source_query);
    if (!result) return STORAGE_ERR_IO;

    // 2. 清空物化数据
    mview_clear_data(mview);

    // 3. 写入新数据
    for (uint32_t i = 0; i < result->row_count; i++) {
        mview_append_row(mview, result->rows[i]);
    }

    // 4. 更新统计
    mview->stats.last_refresh = time(NULL);
    mview->stats.row_count = result->row_count;
    mview->status = MVIEW_STATUS_READY;

    query_result_free(result);
    return STORAGE_OK;
}
```

- [ ] **Step 2: mview_has_cycle DFS 拓扑排序**

```c
bool mview_has_cycle(mview_registry_t *registry) {
    enum { WHITE, GRAY, BLACK } color[registry->count];
    memset(color, WHITE, sizeof(color));

    for (int i = 0; i < registry->count; i++) {
        if (color[i] == WHITE) {
            if (mview_dfs_visit(registry, i, color)) {
                return true;  // 发现环
            }
        }
    }
    return false;
}

bool mview_dfs_visit(mview_registry_t *registry, int idx, enum color *color) {
    color[idx] = GRAY;
    for (int j = 0; j < registry->views[idx]->dep_count; j++) {
        int dep = registry->views[idx]->deps[j];
        if (color[dep] == GRAY) return true;  // 反向边 = 环
        if (color[dep] == WHITE && mview_dfs_visit(registry, dep, color)) {
            return true;
        }
    }
    color[idx] = BLACK;
    return false;
}
```

- [ ] **Step 3: mview_refresh_fast 增量刷新**

- [ ] **Step 4: 修复编译错误 stats.refreshing_mviews**

- [ ] **Step 5: 写测试**

- [ ] **Step 6: Commit**

---

## Task 39: RDF SPARQL 增强

**Files:**
- Modify: `src/db/storage/rdf/sparql_parser.c`
- Create: `tests/test_rdf_sparql.c`

- [ ] **Step 1: FILTER 表达式支持**

```c
// 在 sparql_parser.c 中增加 FILTER 解析
int parse_filter(sparql_parser_t *parser, sparql_filter_t *filter) {
    // 解析 FILTER(?x > 10) 语法
    expect_token(parser, TOKEN_FILTER);
    expect_token(parser, TOKEN_LPAREN);
    parse_expression(parser, &filter->expr);
    expect_token(parser, TOKEN_RPAREN);
    return 0;
}
```

- [ ] **Step 2: OPTIONAL 模式匹配**

- [ ] **Step 3: GROUP BY + 聚合**

- [ ] **Step 4: PREFIX 应用到 URI**

- [ ] **Step 5: 写测试**

- [ ] **Step 6: Commit**
