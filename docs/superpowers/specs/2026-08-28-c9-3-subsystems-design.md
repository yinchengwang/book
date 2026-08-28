# C9-3 Subsystems 设计

> 日期：2026-08-28
> 目标：实现 RAG 核心管道、推进 learning 子项目、完成 apps 集成

## 一、背景

OPSX 已全部归档（openspec/changes/ 仅含 archive/）。但工程中仍有大量子系统处于骨架状态：

| 子系统 | 状态 | 规模 |
|--------|------|------|
| engineering/rag/ | 仅2个头文件（266 行），无实现 | 小 |
| engineering/sdk/python/minivecdb | Python SDK，有基础框架 | 小 |
| engineering/apps/db_driver | Python DB-API 驱动，1975 行实现 | 中 |
| learning/algo-c | 64 个 C/C++ 文件，18 个算法主题 | 大 |
| learning/code-solutions | 524 个 C/C++ 文件，LeetCode/面试题 | 大 |

本变更目标：
1. 实现 RAG 核心管道（ingestion + retrieval + query）
2. 推进 db_driver 到可用状态（补充集成测试）
3. 完成 3 个 learning 主题的实现

## 二、RAG 实现设计

### 2.1 架构

```
Pipeline
  ├─ ingest(text, metadata)
  │    ├─ chunker: text → chunks (固定大小/语义边界)
  │    ├─ embedding: chunk → vector (Ollama/OpenAI API)
  │    └─ vector_index: insert vectors into faiss_hnsw
  │
  ├─ query(question, top_k)
  │    ├─ retriever: semantic search → candidate docs
  │    ├─ reranker: cross-encoder re-rank
  │    └─ response: LLM generation + context injection
  │
  └─ eval(dataset) → metrics
```

### 2.2 核心接口

```c
// rag_pipeline.h（新建）
typedef struct rag_pipeline_s rag_pipeline_t;

rag_pipeline_t *rag_pipeline_create(const char *data_dir);
void rag_pipeline_free(rag_pipeline_t *pipe);

// ingest
int rag_pipeline_ingest(rag_pipeline_t *pipe,
                        const char *text, size_t len,
                        const char *metadata, size_t meta_len);

// query
int rag_pipeline_query(rag_pipeline_t *pipe,
                       const char *question, size_t q_len,
                       int top_k,
                       rag_result_t *out);

// eval
int rag_pipeline_eval(rag_pipeline_t *pipe,
                      const char *eval_path,
                      rag_metrics_t *out);
```

### 2.3 复用现有组件

- **chunker**：实现固定大小分块（BROKEN_WINDOW）和语义分块（利用现有 bm25_index.c）
- **embedding**：封装 Ollama REST API（已有 ollama_embedding.h）
- **vector_index**：复用 faiss_hnsw 索引（已有 create/insert/search API）
- **reranker**：实现 BGE cross-encoder 简化版（已有 bge_reranker.h）
- **LLM**：封装 Ollama chat completions（已有 ollama_llm.h）
- **database**：SQLite 持久化（已有 database.h）

### 2.4 实现文件

新建：
- `engineering/src/rag/rag_pipeline.c` — 核心管道实现（~400 行）
- `engineering/src/rag/rag_ingestion.c` — 分块 + 嵌入 + 索引写入（~300 行）
- `engineering/src/rag/rag_retrieval.c` — 检索 + rerank + 组装（~250 行）
- `engineering/src/rag/rag_eval.c` — 评估指标计算（~150 行）

修改：
- `engineering/include/rag/rag.h` — 扩展公共接口
- `engineering/rag/CMakeLists.txt` — 注册新源文件

测试：
- `engineering/test/db/rag/rag_pipeline_test.cpp` — 端到端管道测试

## 三、db_driver 推进

### 3.1 现状

`engineering/apps/db_driver/` 已有完整 Python DB-API 2.0 实现：
- `connection.py`（260 行）— 连接管理
- `cursor.py`（294 行）— SQL 执行
- `pool.py`（308 行）— 连接池
- `cli.py`（232 行）— 命令行工具
- `exceptions.py`（126 行）— 异常类
- `test_driver.py`（304 行）— 测试

### 3.2 待做

1. 添加集成测试（连接真实 VDB 服务器）
2. 补充 `pool.py` 的连接泄漏检测
3. 写 README 使用示例

## 四、Learning 推进

选择 3 个最重要且可验证的子项目：

### 4.1 ds-c 链表专题（64 文件，已完成大部分）

验证所有 `ds-c/linked_list/` 实现的测试通过。

### 4.2 algo-c 排序算法（18 文件）

实现 quick sort、merge sort 的标准测试（排序正确性 + 稳定性验证）。

### 4.3 code-solutions LeetCode 前 10 题（C 版）

确保 `learning/code-solutions/c/src/c/` 中前 10 题都有解且测试通过。

## 五、验收标准

1. RAG：端到端管道可 ingestion + query + eval（最小数据集）
2. db_driver：集成测试通过（连接本地 SQLite VDB 实例）
3. Learning：ds-c 链表测试、algo-c 排序测试、code-solutions 前 10 题测试全部通过
4. 编译：`cmake --build build/engineering --target test_cf_engine` 通过（不受 RAG 影响）
5. 不破坏现有模块：blob_engine_test、cf_engine_test、yang_test 仍通过

## 六、风险与缓解

| 风险 | 缓解 |
|------|------|
| RAG embedding 依赖 Ollama | 默认使用 mock embedding（随机向量），Ollama 为可选 |
| db_driver 集成测试需要数据库实例 | 使用内存 SQLite，无需独立服务器 |
| Learning 测试与主工程隔离 | 单独 CMakeLists.txt，不污染主构建 |

## 七、文件清单

新建：
- `engineering/src/rag/rag_pipeline.c`
- `engineering/src/rag/rag_ingestion.c`
- `engineering/src/rag/rag_retrieval.c`
- `engineering/src/rag/rag_eval.c`
- `engineering/test/db/rag/rag_pipeline_test.cpp`

修改：
- `engineering/include/rag/rag.h`
- `engineering/rag/CMakeLists.txt`
- `engineering/apps/db_driver/test_driver.py`（集成测试）
- `learning/algo-c/sorting/`（添加测试）
- `learning/code-solutions/c/src/c/`（前 10 题）
