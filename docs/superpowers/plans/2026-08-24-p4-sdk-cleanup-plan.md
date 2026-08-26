# P4：SDK 收尾 + P3 遗留清理实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) 或 superpowers:executing-plans 来逐 Task 实施本计划。每个 Task 用 checkbox（`- [ ]`）追踪。
>
> **路线图位置**：本计划是商业化路线 meta-plan（`docs/superpowers/plans/2026-08-24-commercial-readiness-roadmap.md`）的第一个 sub-plan。
>
> **当前 BASE commit**：`7a8f2ca63`（P3 CI-1+CI-3 清理 commit）
> **执行决策**：main 分支直接做（用户决策，不创建 worktree）；P1 dirty 保留不动。

**Goal**：关闭 P3 whole-branch review 识别的 CI-1/2/3 三个设计妥协 + 清理 4 项延后 Minor findings，为后续 P5（性能规模化）铺路（Task 4.5 HNSW+filter）。

**Architecture**：
- **Task 4.1（RAG embedding 配置）**：`mmdb_rag_query_t` 末尾 append `embedding` 字段（NULL 时 fallback HASH），新增 `mmdb_rag_set_embedding()` 接口；OpenAI 适配器从 stub 升级到可选真实调用
- **Task 4.2（hybrid 双通道融合）**：扩展 `mmdb_hybrid_search` 路由，让 VECTOR 集合支持 text_query / TEXT 集合支持 vector；RRF 真正融合而非单通道
- **Task 4.3（xquery id 错误）**：CI-3 stderr warning 升级为返回 `MMDB_ERR_INVALID`；同时提高 id 缓冲到 256B（降低静默截断概率）
- **Task 4.4（Minor 清理）**：`= {}` 改 `= {0}` / 末尾换行符 / `MMDB_ERR_*` 宏替换字面量 / `tokenize_lower` 动态化
- **Task 4.5（HNSW + filter）**：HNSW 路径支持 filter，使 xquery 可走 HNSW（前置 P5 性能规模化）

**Tech Stack**：C11 / C++17 / CMake 3.20+ / GoogleTest / SQLite FTS5 / pthread_rwlock / faiss_hnsw

## Global Constraints

1. **C ABI**：不修改任何已有 `mmdb_*` 函数签名；`mmdb_rag_query_t` / `mmdb_hybrid_query_t` 等结构体**允许末尾 append 字段**（必须 append，保持 ABI 向后兼容）
2. **语言规范**：所有代码注释 / commit message / report 使用简体中文（CLAUDE.md）
3. **新增接口前缀**：`mmdb_*` 命名空间；新接口必须在 `engineering/include/sdk/mmdb_*.h` 公共头
4. **测试**：GoogleTest（vendored），`.`cpp` + `extern "C"` 引入 C 头；新增 `*_test.cpp` 与既有 `*_test.cpp` 同目录
5. **编译产物**：输出到 `build/engineering/`
6. **工作树约束**：不动 P1 dirty（M 文件）；不动 P3 已 commit 文件（除按 plan 修改）；每个 Task 单 commit
7. **错误处理**：使用 `MMDB_ERR_*` 宏（mmdb_error.h），禁止字面量 `-1/-2/-3`
8. **OpenSpec 流程**：P4 在 `openspec/changes/p4-sdk-cleanup/` 建独立目录（proposal.md / tasks.md / design.md / specs/）

---

## Task 4.1：RAG embedding 配置入口（CI-2 关闭）

**Files**：
- Modify: `engineering/include/sdk/mmdb_rag.h`
- Modify: `engineering/src/sdk/extra/rag.c`
- Modify: `engineering/src/sdk/extra/embedding_openai.c`
- Modify: `engineering/test/sdk/CMakeLists.txt`
- Create: `engineering/test/sdk/rag/rag_embedding_test.cpp`

**依赖**：无

**接口扩展**：

```c
/* mmdb_rag.h 末尾 append： */
struct mmdb_rag_query_t {
    const char* query_text;
    const char* filter_json;
    size_t top_k;
    size_t max_context_chars;
    mmdb_rag_rerank_config_t rerank;
    mmdb_embedding_t* embedding;  /* P4-4.1 新增；NULL 时 fallback HASH */
};

/* 新接口 */
int mmdb_rag_set_embedding(mmdb_collection_t* coll,
                           mmdb_embedding_t* embedding);
```

**实现要点**：
- `mmdb_rag_retrieve` 优先用 `q.embedding`；NULL 时 fallback `mmdb_embedding_create(MMDB_EMBED_HASH, dim)`
- 增 `mmdb_rag_set_embedding()` 将 embedding 注入 collection（持久化为 collection metadata，避免每次 retrieve 重建）
- `embedding_openai.c` 从 stub 升级：当 `MMDB_EMBED_OPENAI` 调用时尝试读 `MMDB_OPENAI_API_KEY` 环境变量；若未设置仍返回 `-1`（NOT_IMPLEMENTED 宏）+ stderr 提示

**Steps**：

- [ ] **Step 1：写失败测试** `engineering/test/sdk/rag/rag_embedding_test.cpp`

```cpp
TEST(RagEmbeddingTest, CustomEmbeddingTakesPrecedence) {
    // 构造 collection + 自定义 embedding + 验证 mmdb_rag_retrieve 用自定义
    mmdb_collection_t* coll = mmdb_open("test.db");
    mmdb_embedding_t* my_emb = mmdb_embedding_create(MMDB_EMBED_HASH, 128);
    ASSERT_EQ(mmdb_rag_set_embedding(coll, my_emb), MMDB_OK);

    mmdb_rag_query_t q = {};
    q.query_text = "test"; q.top_k = 5; q.max_context_chars = 1000;
    // q.embedding == NULL → 用 collection 注入的 my_emb
    mmdb_rag_result_t r = {};
    ASSERT_EQ(mmdb_rag_retrieve(coll, &q, &r), MMDB_OK);
    mmdb_rag_result_free(&r);
    mmdb_embedding_destroy(my_emb);
    mmdb_close(coll);
}
```

- [ ] **Step 2：跑测试确认失败** `cmake --build build/engineering --target rag_embedding_test && ./build/engineering/sdk_tests/rag_embedding_test` 期望 FAIL（未定义 `mmdb_rag_set_embedding`）

- [ ] **Step 3：扩展 mmdb_rag.h** 在 `mmdb_rag_query_t` 末尾 append `mmdb_embedding_t* embedding;` 字段；声明 `mmdb_rag_set_embedding()` 函数

- [ ] **Step 4：修改 rag.c** `mmdb_rag_retrieve` 优先用 `q.embedding`；NULL 时查 collection->embedding；都 NULL 时 fallback `MMDB_EMBED_HASH`

- [ ] **Step 5：升级 embedding_openai.c** 检测 `MMDB_OPENAI_API_KEY` 环境变量；未设置时 stderr 提示 + 返回 `MMDB_ERR_NOT_IMPLEMENTED`（**首次使用宏**）

- [ ] **Step 6：注册测试** `engineering/test/sdk/CMakeLists.txt` 添加 `rag_embedding_test`

- [ ] **Step 7：跑测试确认 PASS** 期望 1/1 PASS

- [ ] **Step 8：回归既有测试** `rag_test`（2/2）+ `rerank_test`（1/1）= 3/3 PASS

- [ ] **Step 9：Commit**
```bash
git add engineering/include/sdk/mmdb_rag.h \
        engineering/src/sdk/extra/rag.c \
        engineering/src/sdk/extra/embedding_openai.c \
        engineering/test/sdk/CMakeLists.txt \
        engineering/test/sdk/rag/rag_embedding_test.cpp
git commit -m "feat(sdk/rag): 暴露 embedding 配置入口与 OpenAI 环境变量检测"
```

**验收**：
- `mmdb_rag_query_t.embedding` 末尾 append，零破坏 ABI
- T4.1 既有 `rag_test` + `rerank_test` 零修改 PASS
- 新增 `rag_embedding_test` 1/1 PASS
- OpenAI 适配器检测环境变量（未设置给清晰错误）

---

## Task 4.2：hybrid_search 双通道真正融合（CI-1 关闭）

**Files**：
- Modify: `engineering/include/sdk/mmdb_hybrid.h`
- Modify: `engineering/src/sdk/extra/hybrid_search.c`
- Create: `engineering/test/sdk/hybrid/hybrid_dual_channel_test.cpp`

**依赖**：无

**接口扩展**（仅注释，无签名变化）：
- 更新 `mmdb_hybrid.h` 头部注释 + 函数注释，说明双通道真正融合策略

**实现要点**：
- 当前路由：`if (c->model == VECTOR && q->vector) ... else if (c->model == TEXT && q->text_query) ... else 静默忽略`
- 新路由：先按 c->model 决定主通道；若 q 同时提供次通道（如 VECTOR 集合上提供 text_query），**也启用次通道**走 mmdb_text_search，结果合并到主通道输出做 RRF 融合
- 通道使能判定：
  - `MMDB_MODEL_VECTOR`：主通道 vector；次通道 text_query（若提供）
  - `MMDB_MODEL_TEXT`：主通道 text_query；次通道 vector（若提供）—— 需 collection 已关联 embedding
  - 其他模型：仅主通道

**Steps**：

- [ ] **Step 1：写失败测试** `hybrid_dual_channel_test.cpp`

```cpp
TEST(HybridDualChannelTest, VectorCollectionWithTextQuery) {
    // 构造 vector collection + 同时填 q.vector 和 q.text_query
    // 验证输出包含双通道融合结果（不是单通道退化为 vector）
    mmdb_collection_t* coll = mmdb_open("test.db");
    /* 插入 vector+metadata 含 text 字段的 docs */
    mmdb_hybrid_query_t q = {};
    q.vector = some_vec; q.dim = 128;
    q.text_query = "machine learning";
    q.top_k = 10;
    mmdb_result_t r = {};
    ASSERT_EQ(mmdb_hybrid_search(coll, &q, &r), MMDB_OK);
    EXPECT_GT(r.count, 0);
    mmdb_result_free(&r);
    mmdb_close(coll);
}
```

- [ ] **Step 2：跑测试确认失败** 期望 FAIL（双通道未启用，r.count=0 或单通道结果）

- [ ] **Step 3：修改 hybrid_search.c** 重构 routing 逻辑支持双通道；新增 `hybrid_search_secondary_text()` 辅助函数

- [ ] **Step 4：更新 mmdb_hybrid.h 注释** 头部注释改为"双通道真正融合：主通道 + 次通道同时启用，RRF 重排"

- [ ] **Step 5：跑测试确认 PASS** 期望 1/1 PASS

- [ ] **Step 6：回归既有测试** `hybrid_search_test`（3/3）+ `cross_lang_consistency_test::Benchmark.HybridVectorAndTextRRF` PASS

- [ ] **Step 7：Commit**
```bash
git add engineering/include/sdk/mmdb_hybrid.h \
        engineering/src/sdk/extra/hybrid_search.c \
        engineering/test/sdk/hybrid/hybrid_dual_channel_test.cpp \
        engineering/test/sdk/CMakeLists.txt
git commit -m "feat(sdk/hybrid): 双通道真正融合（主+次通道 RRF 重排）"
```

**验收**：
- 双通道路由策略更新，T1.3 benchmark GT 不再退化
- CI-1 关闭（header 注释明示双通道支持）
- 既有 3/3 + 86 套件回归 PASS

---

## Task 4.3：xquery id 静默截断升级为错误（CI-3 关闭）

**Files**：
- Modify: `engineering/include/sdk/mmdb_xquery.h`
- Modify: `engineering/src/sdk/extra/xquery.c`
- Modify: `engineering/test/sdk/xquery/xquery_id_len_test.cpp`（新建）

**依赖**：无

**接口扩展**（仅注释）：
- 更新 `mmdb_xquery.h` 头部注释：id 超过 256B 返回 `MMDB_ERR_INVALID`，不再静默跳过

**实现要点**：
- `xq_cand_t` 的 `uint8_t id[64]` 改为 `uint8_t id[256]`
- `XQUERY_MAX_ID_LEN` 宏定义为 256
- 命中 id 超过 256B 时 stderr 警告 + `out->count = i`（返回 `MMDB_ERR_INVALID` 让调用方感知）

**Steps**：

- [ ] **Step 1：写失败测试** `xquery_id_len_test.cpp`

```cpp
TEST(XQueryIdLenTest, OversizedIdReturnsInvalid) {
    /* 构造 source 集合含 1 个 300B id 的 doc */
    mmdb_xquery_text_to_vector_t xq = {};
    xq.source = src_coll; xq.target = tgt_coll;
    xq.text_query = "test"; xq.query_vector = vec; xq.dim = 128;
    xq.top_k = 5;
    mmdb_result_t r = {};
    EXPECT_EQ(mmdb_xquery_text_to_vector(&xq, &r), MMDB_ERR_INVALID);
    mmdb_result_free(&r);
}
```

- [ ] **Step 2：跑测试确认失败** 期望 FAIL（当前静默跳过仍返回 MMDB_OK）

- [ ] **Step 3：修改 xquery.c** id 缓冲 64→256；增加宏定义；静默 continue 改为 stderr 警告 + 返回 MMDB_ERR_INVALID

- [ ] **Step 4：更新 mmdb_xquery.h 注释** header 注释明示"id 超过 256B 返回 MMDB_ERR_INVALID"

- [ ] **Step 5：跑测试确认 PASS** 期望 1/1 PASS

- [ ] **Step 6：回归既有测试** `xquery_test`（2/2）零修改 PASS

- [ ] **Step 7：Commit**
```bash
git add engineering/include/sdk/mmdb_xquery.h \
        engineering/src/sdk/extra/xquery.c \
        engineering/test/sdk/xquery/xquery_id_len_test.cpp \
        engineering/test/sdk/CMakeLists.txt
git commit -m "fix(sdk/xquery): id 静默截断升级为 MMDB_ERR_INVALID，缓冲 64→256B"
```

**验收**：
- CI-3 关闭（id 截断返回明确错误）
- 既有 `xquery_test` 2/2 PASS

---

## Task 4.4：清理 4 项延后 Minor findings

**Files**：
- Modify: `engineering/src/sdk/extra/hybrid_search.c`（`= {}` → `= {0}`）
- Modify: `engineering/src/sdk/extra/rag.c`（同上）
- Modify: `engineering/src/sdk/extra/rrf.c`（`return -1;` → `return MMDB_ERR_INVALID;`）
- Modify: `engineering/src/sdk/extra/embedding_local.c`（`return -1/-2/-3;` → MMDB_ERR_* 宏）
- Modify: 多个文件末尾换行符（见下）
- Modify: `engineering/src/sdk/extra/rag.c::tokenize_lower`（动态 max_tokens）

**依赖**：无

**实现要点**：
- **`= {}` 改 `= {0}`**：消除 `-Wpedantic` 警告（C11 ISO C forbids empty initializer braces before C23）
- **`-1/-2/-3` 字面量改宏**：`MMDB_ERR_INVALID` / `MMDB_ERR_NOT_IMPLEMENTED` / `MMDB_ERR_ALREADY`（需确认宏存在；若缺则先定义）
- **末尾换行符**：`mmdb_embedding.h` / `mmdb_hybrid.h` / `mmdb_xquery.h` / `rag.c` / `embedding_local.c` / `hybrid_search.c` 6 文件
- **`tokenize_lower` 动态化**：从 `max_tokens=64` 改为按 query 长度动态分配；至少 1024 tokens 上限

**Steps**：

- [ ] **Step 1：先确认 MMDB_ERR_NOT_IMPLEMENTED 宏存在** `grep -n "MMDB_ERR_NOT_IMPLEMENTED" engineering/include/sdk/mmdb_error.h`，若不存在则添加

- [ ] **Step 2：批量替换 `= {}` → `= {0}`** 用 sed 或 Edit 在 hybrid_search.c / rag.c 中替换

- [ ] **Step 3：批量替换错误码字面量** rrf.c / embedding_local.c 中 `-1/-2/-3` → 对应 `MMDB_ERR_*` 宏

- [ ] **Step 4：补末尾换行符** 6 个文件最后一行非空行补 `\n`

- [ ] **Step 5：tokenize_lower 动态化** rag.c 改为 `size_t max_tokens = strlen(query) / 2 + 1;` + 动态 `malloc`

- [ ] **Step 6：跑全量测试回归** 期望所有 SDK 测试 PASS：
  ```
  cmake --build build/engineering && ctest --test-dir build/engineering -R sdk --output-on-failure
  ```

- [ ] **Step 7：跑 -Werror 构建** `cmake -DCMAKE_C_FLAGS="-Werror -Wpedantic" --build build/engineering --target mmsdk` 期望 0 warning

- [ ] **Step 8：Commit**
```bash
git add engineering/include/sdk/mmdb_error.h \
        engineering/src/sdk/extra/hybrid_search.c \
        engineering/src/sdk/extra/rag.c \
        engineering/src/sdk/extra/rrf.c \
        engineering/src/sdk/extra/embedding_local.c
git commit -m "chore(sdk): 清理 4 项延后 Minor（= {}→= {0} / 错误码宏 / 换行符 / tokenize 动态化）"
```

**验收**：
- 16 项 Minor 中 4 项延后项全部清零
- -Wpedantic 警告归零
- 既有 SDK 测试全回归 PASS

---

## Task 4.5：HNSW 路径支持 filter（xquery 加速前置）

**Files**：
- Modify: `engineering/src/sdk/extra/hnsw/faiss_hnsw.h`（API 扩展）
- Modify: `engineering/src/sdk/extra/hnsw/faiss_hnsw.c`（filter 支持）
- Modify: `engineering/src/sdk/extra/xquery.c`（检测 HNSW 可用时走 HNSW 路径）
- Create: `engineering/test/sdk/hnsw/hnsw_filter_test.cpp`

**依赖**：Task 4.4（错误码宏定义）

**接口扩展**：
- `faiss_hnsw_search()` 末尾 append `const char* filter_json` 形参（NULL/空表示不过滤）
- 实现：在 HNSW 候选阶段应用 SQLite 元数据过滤（`SELECT id FROM ... WHERE filter_json`）

**实现要点**：
- HNSW 路径返回 top-K_cand 候选 ID（K_cand = top_k × 5 + 50，确保过滤后还有足够候选）
- 应用 filter 后精确读取向量 + 重算 L2 + 取 top_k
- 若 collection N < 10000 仍走 flat 路径（已有逻辑）

**Steps**：

- [ ] **Step 1：写失败测试** `hnsw_filter_test.cpp`

```cpp
TEST(HnswFilterTest, FilterAppliesInHnswPath) {
    /* 构造 10K+ vector collection，N ≥ 10000 触发 HNSW 路径 */
    /* 验证 filter_json 限定 metadata 范围后，结果都符合 filter */
    mmdb_collection_t* coll = mmdb_open("hnsw.db");
    /* bulk insert 10K+ vectors */
    faiss_hnsw_index_t* idx = faiss_hnsw_create(128, 16, 200);
    faiss_hnsw_search_params_t p = {};
    p.top_k = 10; p.filter_json = "{\"category\":\"A\"}";
    /* ... */
}
```

- [ ] **Step 2：跑测试确认失败** 期望 FAIL（HNSW filter 未实现）

- [ ] **Step 3：扩展 faiss_hnsw_search** 末尾 append `filter_json` 形参；新增 `faiss_hnsw_search_filtered()`

- [ ] **Step 4：实现 filter 逻辑** HNSW 候选 → SQLite 元数据过滤 → 重读向量 → 重排

- [ ] **Step 5：修改 xquery.c** 检测 collection HNSW 是否可用 + filter_json 是否提供，是则走 HNSW 路径（flat 仅 fallback）

- [ ] **Step 6：跑测试确认 PASS** 期望 1/1 PASS（10K+ 数据下走 HNSW 路径 + filter 正确）

- [ ] **Step 7：回归既有测试** `xquery_test` 2/2 + `hnsw_*` 测试全 PASS

- [ ] **Step 8：性能验证** 10K×128 + filter 下 search ≥ 5K qps（vs flat 路径 1.5K qps 提升）

- [ ] **Step 9：Commit**
```bash
git add engineering/src/sdk/extra/hnsw/ \
        engineering/src/sdk/extra/xquery.c \
        engineering/test/sdk/hnsw/hnsw_filter_test.cpp \
        engineering/test/sdk/CMakeLists.txt
git commit -m "feat(sdk/hnsw): filter 支持 + xquery 路由升级"
```

**验收**：
- HNSW filter API 实现
- xquery 在 N ≥ 10K + filter 提供时走 HNSW（性能 ≥ 5K qps）
- 既有测试全 PASS
- 为 P5 性能规模化铺路

---

## 整体验收（sub-plan 完成）

- [ ] CI-1 关闭（hybrid 双通道融合）
- [ ] CI-2 关闭（RAG embedding 配置入口）
- [ ] CI-3 关闭（xquery id 错误）
- [ ] 4 项延后 Minor 全部清零（Task 4.4）
- [ ] HNSW filter 支持（Task 4.5，P5 前置）
- [ ] 全部既有测试（P1/P2/P3）回归 PASS
- [ ] -Wpedantic 警告归零
- [ ] P4 plan ledger（`.superpowers/sdd/p4-progress.md`）收尾为 ARCHIVED
- [ ] OpenSpec `openspec/changes/p4-sdk-cleanup/{proposal,tasks,design}.md` 归档

---

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| Task 4.2 双通道融合破坏 T1.3 benchmark | Task 4.2 步骤 6 强制跑 `cross_lang_consistency_test::Benchmark.HybridVectorAndTextRRF` 验证 GT 不退化 |
| Task 4.5 HNSW filter 实现复杂度 | Task 4.5 步骤 4 拆为子步骤：filter 解析 → SQLite 查询 → 候选合并，每子步骤可独立测试 |
| Task 4.4 错误码宏不存在 | Task 4.4 步骤 1 先验证/补宏定义，避免后续 Task 链接失败 |
| MMDB_ERR_NOT_IMPLEMENTED 宏缺失 | 同上；如需新增宏则 Task 4.4 一并补 |

---

## 实施顺序

```
Task 4.4（错误码宏定义先行）
        ↓
Task 4.1（RAG embedding 配置）        Task 4.3（xquery id 错误）
        ↓                                 ↓
        └────────────┬────────────────────┘
                     ↓
              Task 4.2（hybrid 双通道）
                     ↓
              Task 4.5（HNSW filter）
                     ↓
              whole-branch review + 归档
```

- **第一阶段**：Task 4.4（基础）+ Task 4.1 / 4.3（可并行）
- **第二阶段**：Task 4.2（架构升级）
- **第三阶段**：Task 4.5（性能前置）
- **第四阶段**：whole-branch review + P4 归档

总工期估计：2-3 天（5 Task × 平均 0.5 天）。