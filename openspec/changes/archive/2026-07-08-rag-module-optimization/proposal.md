# Proposal: RAG 模块优化

## Why

当前 RAG 模块虽然架构完整，但存在两个致命缺陷：

1. **Embedding 服务仅使用随机向量**：`SimpleEmbeddingService` 使用 `mt19937` 生成伪随机向量，相似的文本不会产生相似的向量表示，导致向量检索退化为随机检索。

2. **LLM 服务仅使用关键词匹配**：`SimpleLLMService` 基于硬编码的关键词匹配生成固定回复，无法处理任意问题，也无法利用检索到的上下文。

这导致 RAG 系统**无法进行端到端的效果评估**——因为检索结果不可靠，生成结果也是预设的，无法评估真实的 RAG 质量。

## What Changes

### Phase 1: 核心能力补全

- [ ] **Ollama 集成**：集成 Ollama API，本地运行 Embedding 模型 (nomic-embed-text) 和 LLM (llama3.2/qwen2.5)
- [ ] **向量索引持久化**：实现 HNSW 索引的序列化/反序列化，支持增量插入和删除

### Phase 2: 评估框架引入

- [ ] **RAGAS 评估框架**：引入 RAGAS 指标计算，支持：
  - Faithfulness (忠实度) - 答案是否忠实于上下文
  - Answer Relevance (答案相关性) - 答案与问题的相关性
  - Context Relevance (上下文相关性) - 检索上下文与问题的相关性
  - Recall@K / MRR / NDCG - 检索质量指标

### Phase 3: 增强能力

- [ ] **文档解析扩展**：支持 PDF/Word/Excel 解析
- [ ] **查询结果缓存**：避免重复计算

## Capabilities

### New Capabilities

- `ollama-integration`: Ollama API 集成，支持本地 Embedding 模型和 LLM 推理
- `rag-evaluation`: RAG 系统评估框架，支持 RAGAS 指标计算和检索质量评估

### Modified Capabilities

- `rag-embedding`: 修改 Embedding 服务接口，新增 OllamaEmbeddingService 实现
- `rag-llm`: 修改 LLM 服务接口，新增 OllamaLLMService 实现

## Impact

### 受影响代码

| 模块 | 变更 |
|------|------|
| `rag/include/rag/embedding.h` | 新增 OllamaEmbeddingService 类 |
| `rag/include/rag/llm_service.h` | 新增 OllamaLLMService 类 |
| `rag/src/rag/embedding/` | 新增 `ollama_embedding.cpp` |
| `rag/src/rag/llm/` | 新增 `ollama_llm.cpp` |
| `rag/src/rag/` | 新增 `evaluator/` 目录 |

### 新增依赖

- `httplib` (httplib.h) - HTTP 客户端库，用于 Ollama API 调用
- 或使用已有的 libcurl

### 外部依赖

- Ollama 服务 (localhost:11434)
- Ollama 模型: `nomic-embed-text`, `llama3.2` (或其他中文模型)
