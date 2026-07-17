# RAG 模块优化任务清单

> 最后更新：2026-07-08

## 1. Ollama 集成

- [x] 1.1 创建 `rag/include/rag/ollama_embedding.h` 头文件
- [x] 1.2 创建 `rag/src/rag/llm/ollama_embedding.cpp` 实现文件
- [x] 1.3 创建 `rag/include/rag/ollama_llm.h` 头文件
- [x] 1.4 创建 `rag/src/rag/llm/ollama_llm.cpp` 实现文件
- [x] 1.5 更新 `rag/CMakeLists.txt` 添加新文件
- [x] 1.6 添加 Ollama Embedding 单元测试
- [x] 1.7 添加 Ollama LLM 单元测试
- [x] 1.8 测试 Ollama 服务降级（服务不可用时回退到 Simple）

## 2. RAGAS 评估框架

- [x] 2.1 创建 `rag/include/rag/evaluator.h` 头文件
- [x] 2.2 创建 `rag/src/rag/evaluator/evaluator.cpp` 实现文件
- [x] 2.3 实现 RetrievalMetrics 计算（Recall@K, MRR, NDCG@K）
- [x] 2.4 实现 Faithfulness 计算（基于 LLM）
- [x] 2.5 实现 Answer Relevance 计算（基于 LLM）
- [x] 2.6 实现 Context Relevance 计算（基于 LLM）
- [x] 2.7 实现 RAGAS 综合得分计算
- [x] 2.8 实现评估报告生成（JSON 格式）
- [x] 2.9 添加评估框架单元测试
- [x] 2.10 创建基准测试数据集（JSON 格式）

## 3. 向量索引持久化

- [x] 3.1 创建 `rag/include/rag/hnsw_persist.h` 头文件
- [x] 3.2 实现 HNSW 索引序列化（save）
- [x] 3.3 实现 HNSW 索引反序列化（load）
- [x] 3.4 实现增量插入 API
- [x] 3.5 实现增量删除 API
- [x] 3.6 添加持久化单元测试

## 4. 集成与测试

- [x] 4.1 集成 Ollama Embedding 到 RAGEngine
- [x] 4.2 集成 Ollama LLM 到 RAGEngine
- [x] 4.3 集成评估框架到 RAGEngine
- [x] 4.4 创建端到端测试脚本
- [x] 4.5 更新 CLI 添加 `evaluate` 命令
- [x] 4.6 更新 REST API 添加 `/api/v1/evaluate` 端点

## 5. 文档与示例

- [x] 5.1 更新 `rag/README.md` 集成说明
- [x] 5.2 创建 `rag/docs/ollama-integration.md` 使用指南
- [x] 5.3 创建 `rag/docs/rag-evaluation.md` 评估指南
- [x] 5.4 添加性能基准测试结果

---

## 完成度统计

| Section | 任务数 | 已完成 | 完成率 |
|---------|--------|--------|--------|
| 1. Ollama 集成 | 8 | 8 | 100% |
| 2. RAGAS 评估框架 | 10 | 10 | 100% |
| 3. 向量索引持久化 | 6 | 6 | 100% |
| 4. 集成与测试 | 6 | 6 | 100% |
| 5. 文档与示例 | 4 | 4 | 100% |
| **总计** | **34** | **34** | **100%** |

---

## 变更日志

### 2026-07-08
- 完成所有 34 个任务
- 新增 Ollama Embedding/LLM 服务
- 新增 RAGAS 评估框架
- 新增 HNSW 索引持久化
- 新增单元测试和集成测试
- 新增使用文档
