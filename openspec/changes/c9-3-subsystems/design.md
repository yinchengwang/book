# C9-3 Subsystems 设计

> 日期：2026-08-28
> 目标：实现 RAG 核心管道、推进 db_driver、完成 learning 子项目验证

## 一、RAG 实现

### 架构
```
Pipeline
  ├─ ingest(text, metadata)
  │    ├─ chunker: text → chunks
  │    ├─ embedding: chunk → vector (Ollama API)
  │    └─ vector_index: insert into faiss_hnsw
  │
  ├─ query(question, top_k)
  │    ├─ retriever: semantic search
  │    ├─ reranker: BGE cross-encoder
  │    └─ response: LLM generation
  │
  └─ eval(dataset) → metrics
```

### 新建文件
- `engineering/src/rag/rag_pipeline.c`（400 行）
- `engineering/src/rag/rag_ingestion.c`（300 行）
- `engineering/src/rag/rag_retrieval.c`（250 行）
- `engineering/src/rag/rag_eval.c`（150 行）
- `engineering/test/db/rag/rag_pipeline_test.cpp`（300 行）

### 复用现有
- faiss_hnsw（vector_index）
- ollama_embedding / ollama_llm
- bge_reranker
- sqlite database

## 二、db_driver 推进

- 补充集成测试（连接内存 SQLite）
- 连接池泄漏检测
- README 示例补充

## 三、Learning 推进

- ds-c 链表专题验证（64 文件）
- algo-c 排序算法测试（18 文件）
- code-solutions LeetCode 前 10 题验证

## 四、验收标准

1. RAG pipeline 端到端测试通过
2. db_driver 集成测试通过
3. Learning 测试通过率 ≥ 90%
4. 不破坏现有模块
