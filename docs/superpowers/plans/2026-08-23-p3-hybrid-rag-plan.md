# P3 混合检索 + Embedding 解耦 + 跨模型 + RAG 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 P1+P2 完成的 SDK 之上，构建 hybrid 检索、embedding 解耦、跨模型查询、RAG pipeline 四层能力，对标 Chroma hybrid search、LanceDB RAG、Weaviate hybrid query。

**Architecture:** 自底向上分四阶段：
1. hybrid search — HNSW + FTS5 + metadata filter → RRF 重排
2. embedding adapter — 抽象 `mmdb_embedding_t` 接口，本地 hash/平均池化/OpenAI HTTP 占位
3. 跨模型查询 — collection 之间 JOIN（向量 ↔ 文本 ↔ 时序 ↔ 图）
4. RAG pipeline — retrieve → rerank → context 注入辅助函数

**Tech Stack:** C11, C++17, SQLite (FTS5 + JSON1), HNSW (faiss_hnsw), GoogleTest, libcurl (Optional for OpenAI adapter), RRF (Reciprocal Rank Fusion)

## Global Constraints

- 语言：所有代码注释、文档、提交信息使用简体中文
- C ABI 是 FFI 锚点，Python/Go/C++ 都通过 C ABI 调用
- 编译产物输出到 `build/engineering/`，测试产物到 `test-results/engineering/`
- 测试文件用 `.cpp`，`extern "C"` 引用 C 头文件
- 公开 ABI 不破坏向后兼容（已发布的 `mmdb_*` 函数签名不变）
- 不可变（const）入参不变；新增接口使用 `mmdb_*_v2` 或全新前缀
- 所有新模块必须有 GoogleTest 测试覆盖（>=80% 行覆盖）
- 单个 PR/commit 只包含一个 Task 的相关变更
- 不在本任务中触及 Phase 1/2 已完成模块（除非明确指定）

---

# 阶段 1：Hybrid Search（HNSW + FTS5 + Filter + RRF）

## Task 1.1: RRF 重排算法骨架

**Files:**
- Create: `engineering/include/sdk/impl/hybrid_search.h`
- Create: `engineering/src/sdk/extra/rrf.c`
- Modify: `engineering/CMakeLists.txt`（注册 extra/rrf.c 到 mmsdk）
- Create: `engineering/test/sdk/hybrid/rrf_test.cpp`

**Interfaces:**
```c
/* Reciprocal Rank Fusion：将多路 ranked list 融合成单一排序
 * score(doc) = sum over channels of (1.0 / (k + rank_i))
 * 经典参数 k=60（来自 Cormack et al. 2009） */
typedef struct {
    int32_t k;  /* RRF 常数，默认 60 */
} mmdb_rrf_config_t;

void mmdb_rrf_config_init(mmdb_rrf_config_t* cfg);  /* 设置默认 k=60 */

typedef struct {
    const uint8_t* id;       /* 文档 ID（外部管理生命周期） */
    size_t id_len;
    double rrf_score;
    size_t source_ranks[8];  /* 各路排名（0 表示未命中） */
    size_t source_count;
} mmdb_rrf_doc_t;

/* 就地 RRF 融合：对每个 doc 累加 1/(k+rank_i) */
int mmdb_rrf_fuse(
    mmdb_rrf_doc_t* docs,
    size_t doc_count,
    const mmdb_rrf_config_t* cfg);
```

**Steps:**

- [ ] **Step 1: 写失败测试**

```cpp
// engineering/test/sdk/hybrid/rrf_test.cpp
extern "C" {
#include "sdk/impl/hybrid_search.h"
}

TEST(RRF, FuseTwoChannels) {
    mmdb_rrf_doc_t docs[3] = {};
    /* doc A: 通道 1 排名 1，通道 2 排名 3 */
    docs[0].id = (const uint8_t*)"A"; docs[0].id_len = 1;
    docs[0].source_ranks[0] = 1; docs[0].source_ranks[1] = 3;
    docs[0].source_count = 2;
    /* doc B: 通道 1 排名 2，通道 2 排名 1 */
    docs[1].id = (const uint8_t*)"B"; docs[1].id_len = 1;
    docs[1].source_ranks[0] = 2; docs[1].source_ranks[1] = 1;
    docs[1].source_count = 2;
    /* doc C: 通道 1 排名 3，通道 2 未命中 */
    docs[2].id = (const uint8_t*)"C"; docs[2].id_len = 1;
    docs[2].source_ranks[0] = 3; docs[2].source_count = 1;

    mmdb_rrf_config_t cfg;
    mmdb_rrf_config_init(&cfg);
    ASSERT_EQ(mmdb_rrf_fuse(docs, 3, &cfg), 0);

    /* A 得分 = 1/(60+1) + 1/(60+3) ≈ 0.0328
       B 得分 = 1/(60+2) + 1/(60+1) ≈ 0.0328
       C 得分 = 1/(60+3) ≈ 0.0159
       排序：B=A > C */
    EXPECT_GT(docs[0].rrf_score, docs[2].rrf_score);
    EXPECT_GT(docs[1].rrf_score, docs[2].rrf_score);
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `ninja -C build/engineering rrf_test && ./build/engineering/sdk_tests/rrf_test`
Expected: link error (undefined mmdb_rrf_fuse)

- [ ] **Step 3: 实现 RRF**

```c
// engineering/src/sdk/extra/rrf.c
#include "sdk/impl/hybrid_search.h"

void mmdb_rrf_config_init(mmdb_rrf_config_t* cfg) {
    cfg->k = 60;
}

int mmdb_rrf_fuse(mmdb_rrf_doc_t* docs, size_t doc_count,
                  const mmdb_rrf_config_t* cfg) {
    if (!docs || !cfg) return -1;
    double k = (double)cfg->k;
    for (size_t i = 0; i < doc_count; i++) {
        double s = 0.0;
        for (size_t j = 0; j < docs[i].source_count && j < 8; j++) {
            size_t rank = docs[i].source_ranks[j];
            if (rank > 0) s += 1.0 / (k + (double)rank);
        }
        docs[i].rrf_score = s;
    }
    return 0;
}
```

- [ ] **Step 4: 跑测试确认通过**

Run: 同 Step 2 命令
Expected: PASS

- [ ] **Step 5: 提交**

```bash
git add engineering/include/sdk/impl/hybrid_search.h \
        engineering/src/sdk/extra/rrf.c \
        engineering/CMakeLists.txt \
        engineering/test/sdk/hybrid/rrf_test.cpp
git commit -m "feat(sdk/hybrid): RRF 重排算法骨架"
```

---

## Task 1.2: hybrid search 公共 API

**Files:**
- Create: `engineering/include/sdk/mmdb_hybrid.h`
- Modify: `engineering/src/sdk/extra/rrf.c`（添加 mmdb_hybrid_search 实现）
- Create: `engineering/test/sdk/hybrid/hybrid_search_test.cpp`

**Interfaces:**
```c
typedef struct {
    const uint8_t* vector;       /* 可选：触发向量检索 */
    size_t vector_len;           /* 单位：字节，应等于 dim*sizeof(float) */
    size_t dim;
    const char* text_query;      /* 可选：触发 FTS5 检索 */
    const char* filter_json;     /* 可选：metadata 过滤 */
    size_t top_k;                /* 默认 10 */
    const mmdb_rrf_config_t* rrf;/* 默认 k=60 */
} mmdb_hybrid_query_t;

/* hybrid search：向量通道 + 文本通道 + filter 通道 → 融合排序 */
int mmdb_hybrid_search(
    mmdb_collection_t* c,
    const mmdb_hybrid_query_t* q,
    mmdb_result_t* out);
```

**Steps:**

- [ ] **Step 1: 写失败测试**

```cpp
TEST(HybridSearch, VectorAndText) {
    /* 创建 vector+text 双 collection（同一个 collection 支持多模型待评估，
       此处用两个 collection 模拟实际场景）*/
    /* ... 插入数据 ... */
    /* 同时调用向量搜索 + FTS5 搜索 → 验证 RRF 融合结果 */
}
```

- [ ] **Step 2: 实现 mmdb_hybrid_search**

实现要点：
- 若 `q->vector !=` NULL：调用内部向量搜索接口获取 top_k*2 候选
- 若 `q->text_query !=` NULL：调用 `mmdb_text_search` 获取 top_k*2 候选
- 合并 id 集合，每个 id 记录在各通道的排名
- 调用 `mmdb_rrf_fuse` 算分
- 按 `rrf_score` 降序排序，取前 `top_k`
- 回查 SQLite 取 metadata

- [ ] **Step 3-5: 测试 + 提交**

---

## Task 1.3: hybrid search 端到端基准

**Files:**
- Modify: `engineering/test/sdk/integration/cross_lang_consistency_test.cpp`

**Steps:**

- [ ] **Step 1: 添加 `HybridSearch.VectorAndTextRRF` 基准测试**

数据集：1000 个文档，每个文档同时存向量 + 文本 + metadata
- 向量通道：随机 128 维
- 文本通道：从预定义词表随机抽取 5-20 词
- metadata：JSON `{"category": "tech|news|sports", "year": 2020-2025}`

查询：随机 query 向量 + 文本关键词 "machine learning"
- 验证：top-10 至少 5 个出现在向量 top-20 ∩ 文本 top-20 集合

- [ ] **Step 2: 跑测试 + 提交**

---

# 阶段 2：Embedding Adapter

## Task 2.1: embedding 接口抽象

**Files:**
- Create: `engineering/include/sdk/mmdb_embedding.h`
- Create: `engineering/src/sdk/extra/embedding_local.c`
- Modify: `engineering/CMakeLists.txt`

**Interfaces:**
```c
typedef struct mmdb_embedding mmdb_embedding_t;
typedef struct mmdb_embedding_ctx mmdb_embedding_ctx_t;

typedef enum {
    MMDB_EMBED_HASH = 0,         /* 本地确定性 hash（零依赖，用于测试）*/
    MMDB_EMBED_AVERAGE_POOL = 1, /* 平均池化（要求输入已编码为定长 float）*/
    /* MMDB_EMBED_OPENAI = 2,  -- 占位，P3 后续阶段实现 */
} mmdb_embedding_kind_t;

typedef int (*mmdb_embed_fn_t)(
    mmdb_embedding_ctx_t* ctx,
    const char* text, size_t text_len,
    float* out_vector, size_t out_dim);

mmdb_embedding_t* mmdb_embedding_create(
    mmdb_embedding_kind_t kind, size_t dim);
void mmdb_embedding_drop(mMDB_embedding_t* emb);

int mmdb_embed_text(
    mmdb_embedding_t* emb, const char* text, size_t text_len,
    float* out_vec, size_t out_dim);
```

**实现要求：**
- `MMDB_EMBED_HASH`：用文本字节的 SHA-256-like mixing 生成确定性的伪随机向量（归一化）
- `MMDB_EMBED_AVERAGE_POOL`：仅做占位实现，返回错误码（不实际编码）

**Steps:**

- [ ] **Step 1: 写测试**

```cpp
TEST(Embedding, HashDeterministic) {
    auto* emb = mmdb_embedding_create(MMDB_EMBED_HASH, 128);
    float v1[128], v2[128];
    mmdb_embed_text(emb, "hello world", 11, v1, 128);
    mmdb_embed_text(emb, "hello world", 11, v2, 128);
    for (int i = 0; i < 128; i++) EXPECT_FLOAT_EQ(v1[i], v2[i]);
    mmdb_embedding_drop(emb);
}

TEST(Embedding, HashDifferentText) {
    /* 不同文本 → 不同向量 */
    /* 验证 L2 距离 > 0.1（确认非全零/全相同）*/
}
```

- [ ] **Step 2-5: 实现 + 测试 + 提交**

---

## Task 2.2: OpenAI embedding adapter（HTTP 占位）

**Files:**
- Create: `engineering/src/sdk/extra/embedding_openai.c`

**说明：** 本任务仅创建占位 stub（不实际调用 HTTP），定义协议结构。实际 HTTP 调用留给后续阶段（需要 libcurl 依赖）。P3 当前阶段只验证接口设计合理性。

**Steps:**

- [ ] **Step 1: 写接口 + 返回 `MMDB_ERR_NOT_IMPLEMENTED`**

- [ ] **Step 2: 写测试验证返回 NOT_IMPLEMENTED**

- [ ] **Step 3: 提交**

---

# 阶段 3：跨模型查询

## Task 3.1: collection 之间 join 接口

**Files:**
- Create: `engineering/include/sdk/mmdb_xquery.h`
- Create: `engineering/src/sdk/extra/xquery.c`

**说明：** 提供"跨 collection 联合查询"的最小 API。例如：从 text collection 取文档 id，再用这些 id 去 vector collection 取最近邻。

**Interfaces:**
```c
typedef struct {
    const char* source_collection;
    const char* source_query;       /* 文本搜索查询 */
    const char* target_collection;
    mmdb_query_t target_vector_query;  /* 向量查询模板 */
    size_t top_k;                   /* 最终返回数量 */
} mmdb_xquery_t;

int mmdb_xquery_text_to_vector(
    mmdb_t* db,
    const mmdb_xquery_t* xq,
    mmdb_result_t* out);
```

**Steps:**

- [ ] **Step 1: 写失败测试**

```cpp
TEST(XQuery, TextToVector) {
    /* text collection "docs": 1000 docs with various words */
    /* vector collection "embeddings": 1000 vectors indexed by same ids */
    /* 调用 mmdb_xquery_text_to_vector(db, "machine learning", "embeddings") */
    /* 验证：返回的 IDs 都来自 text 命中集合，且每个 ID 都有对应向量 */
}
```

- [ ] **Step 2: 实现**

实现要点：
1. `mmdb_text_search(source_collection, source_query, &tmp_result)` 拿到候选 ids
2. 对每个 id，构造向量查询 filter（WHERE id IN (...)）
3. 调用 `mmdb_vectors_search` 带 filter 的版本
4. 合并结果返回

- [ ] **Step 3-5: 测试 + 提交**

---

# 阶段 4：RAG Pipeline

## Task 4.1: RAG retrieve 接口

**Files:**
- Create: `engineering/include/sdk/mmdb_rag.h`
- Create: `engineering/src/sdk/extra/rag.c`

**Interfaces:**
```c
typedef struct {
    const char* query_text;
    const char* filter_json;       /* 可选 */
    size_t top_k;                  /* 默认 5 */
    size_t max_context_chars;      /* 拼装 prompt 时总字符上限，默认 8000 */
} mmdb_rag_query_t;

typedef struct {
    mmdb_result_t items;
    char* context;                 /* 拼装好的 prompt context 字符串 */
    size_t context_len;
} mmdb_rag_result_t;

int mmdb_rag_retrieve(
    mmdb_collection_t* c,         /* 必须是 text collection */
    const mmdb_rag_query_t* q,
    mmdb_rag_result_t* out);

void mmdb_rag_result_free(mmdb_rag_result_t* r);
```

**实现：**
1. 用 `mmdb_embedding_create(HASH)` 生成 query 向量
2. 用 hybrid search（向量 + FTS5）取 top_k
3. 按距离升序拼接 text 字段，用 `\n---\n` 分隔，截断到 `max_context_chars`

**Steps:**

- [ ] **Step 1: 写失败测试**

```cpp
TEST(RAG, RetrieveContext) {
    /* 创建 text collection，插入 100 段文档 */
    /* 检索 query："machine learning basics" */
    /* 验证 mmdb_rag_result.context 包含至少一个文档的 text 字段 */
    /* 验证 context_len <= max_context_chars */
}
```

- [ ] **Step 2-5: 实现 + 测试 + 提交**

---

## Task 4.2: RAG rerank（可选：交叉编码器占位）

**说明：** 实际 rerank 模型需要外部依赖。P3 提供接口 + 简单占位（按 BM25 score 重排）。

**Steps:**

- [ ] **Step 1: 添加 rerank 配置 + 占位实现**

- [ ] **Step 2: 测试 + 提交**

---

# 验收标准

## 阶段 1
- [ ] RRF 算法单测通过（含 k=60 默认值）
- [ ] hybrid_search 同时使用向量 + 文本通道时 RRF 分数正确
- [ ] 端到端基准：1000 docs 上 hybrid 检索 top-10 中至少 5 个命中双通道 top-20

## 阶段 2
- [ ] MMDB_EMBED_HASH 同输入 → 同输出（确定性）
- [ ] MMDB_EMBED_OPENAI 返回 NOT_IMPLEMENTED（占位）

## 阶段 3
- [ ] text → vector 跨 collection 查询返回的 ids 全部来自 text 命中集合

## 阶段 4
- [ ] mmdb_rag_retrieve 返回的 context 非空
- [ ] context_len 严格 <= max_context_chars
- [ ] 端到端：查询"机器学习基础"返回的 context 包含相关文档

## 整体非功能性验收
- 所有新模块测试通过
- 不破坏 P1+P2 已通过的测试
- C ABI 向后兼容（已有 mmdb_* 函数签名不变）
- 单文件变更 < 500 行（除非不可避免）
- 单 PR 只包含一个 Task 的代码

---

# 风险与缓解

| 风险 | 缓解 |
|------|------|
| OpenAI adapter 需要 libcurl，引入外部依赖 | P3 阶段 2 仅占位，不实际 HTTP 调用；后续阶段评估 |
| 跨模型查询性能 | 默认 limit 候选集 ≤ 100，超过则警告；后续阶段加索引 |
| RAG context 拼接可能引入 XSS | 仅拼接 SDK 返回的 text 字段，不做 HTML 转义（用户责任） |
| Hybrid search RRF 调参 | 默认 k=60，沿用学术默认；提供 mmdb_rrf_config_t 让用户覆盖 |

---

# 实施顺序

```
阶段 1（3-5 天）
  1.1 RRF 算法 → 1.2 hybrid API → 1.3 端到端基准

阶段 2（1-2 天）
  2.1 接口 + hash → 2.2 OpenAI 占位

阶段 3（2-3 天）
  3.1 跨 collection join

阶段 4（2-3 天）
  4.1 retrieve → 4.2 rerank 占位

总计：约 8-13 天
```

---

# 关键文件总览

| 文件 | 角色 |
|------|------|
| `engineering/include/sdk/mmdb_hybrid.h` | hybrid search 公共 API |
| `engineering/include/sdk/mmdb_embedding.h` | embedding adapter 接口 |
| `engineering/include/sdk/mmdb_xquery.h` | 跨模型查询接口 |
| `engineering/include/sdk/mmdb_rag.h` | RAG 接口 |
| `engineering/src/sdk/extra/rrf.c` | RRF 算法实现 |
| `engineering/src/sdk/extra/embedding_local.c` | hash/average-pool 实现 |
| `engineering/src/sdk/extra/embedding_openai.c` | OpenAI 占位 |
| `engineering/src/sdk/extra/xquery.c` | 跨 collection join |
| `engineering/src/sdk/extra/rag.c` | RAG pipeline |
| `engineering/test/sdk/hybrid/*_test.cpp` | hybrid 测试 |
| `engineering/test/sdk/embedding/*_test.cpp` | embedding 测试 |
| `engineering/test/sdk/xquery/*_test.cpp` | 跨模型测试 |
| `engineering/test/sdk/rag/*_test.cpp` | RAG 测试 |