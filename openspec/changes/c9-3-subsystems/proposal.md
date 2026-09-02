# C9-3 Subsystems 提案

## Why

OPSX 已全部归档，但工程中仍有大量子系统处于骨架状态：
- `engineering/rag/`：仅 2 个头文件（266 行），无实现
- `engineering/sdk/python/minivecdb`：Python SDK 基础框架
- `engineering/apps/db_driver`：1975 行 Python DB-API 驱动，缺集成测试
- `learning/`：524 个 C/C++ 文件，但部分主题缺测试

本变更目标：实现 RAG 核心管道、推进 db_driver 到可用状态、完成 learning 子项目验证。

## What Changes

### RAG 核心管道实现

新建 4 个源文件实现端到端管道：
- `rag_pipeline.c`：主管道接口（ingest/query/eval）
- `rag_ingestion.c`：分块 + 嵌入 + 索引写入
- `rag_retrieval.c`：检索 + rerank + 组装
- `rag_eval.c`：评估指标计算

复用现有组件：faiss_hnsw、ollama_embedding、bge_reranker、sqlite database。

### db_driver 推进

- 补充集成测试（连接内存 SQLite VDB）
- 连接池泄漏检测
- README 示例补充

### Learning 推进

- ds-c 链表专题验证（64 文件）
- algo-c 排序算法测试（18 文件）
- code-solutions LeetCode 前 10 题验证（524 文件）

## Capabilities

| 能力 | 交付 |
|------|------|
| RAG 端到端 | ingest + query + eval 最小数据集 |
| db_driver 可用 | 集成测试通过 |
| Learning 验证 | 链表/排序/LeetCode 测试通过 |

## Impact

- 新增 ~1100 行 C 代码（RAG 核心）
- 新增 ~400 行 C 代码（learning 测试）
- 新增 ~300 行 Python 代码（db_driver 集成测试）
- 修改 rag/CMakeLists.txt

## 验收标准

- `cmake --build build/engineering --target db_core` 通过（不受影响）
- RAG pipeline 最小端到端测试通过
- db_driver 集成测试通过
- Learning 测试通过率 ≥ 90%