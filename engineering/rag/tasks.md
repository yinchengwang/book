# RAG 系统实现任务

> 最后更新：2026-07-06

## 概述

为 book 项目实现本地 RAG（检索增强生成）系统，支持对项目文档进行智能问答。

## 任务列表

### 核心基础设施

- [x] **T1: 错误处理系统** - 错误码定义、异常类、重试策略
- [x] **T2: 日志系统** - spdlog 集成、结构化日志、上下文追踪
- [x] **T3: 配置管理** - YAML 配置解析、配置验证、配置热加载

### 数据模型

- [x] **T4: 核心数据结构** - Chunk、Document、Embedding、QueryResult
- [x] **T5: SQLite Schema** - documents/chunks/embeddings/index_status 表

### 分块与解析

- [x] **T6: 分块器实现** - FixedChunker、RecursiveChunker、CodeChunker
- [x] **T7: 解析器实现** - MarkdownParser、CodeParser、TextParser

### 索引与检索

- [x] **T8: HNSW 索引** - 向量索引封装
- [x] **T9: BM25 索引** - 全文检索索引
- [x] **T10: 检索器** - HNSWRetriever、BM25Retriever、HybridRetriever、RRF 融合

### 模型服务

- [x] **T11: Embedding 服务** - 本地模型推理、批量编码（简化版）
- [x] **T12: LLM 服务** - SimpleLLMService（简化版，生产环境集成 llama.cpp）

### 核心引擎

- [x] **T13: RAG 引擎** - 协调各组件、索引构建、查询处理
- [x] **T14: 指标收集** - Prometheus 指标、健康检查

### 接口层

- [x] **T15: CLI 工具** - index/query/doc/config 命令
- [x] **T16: REST API** - HTTP 服务器，/api/v1/query 等端点

### 构建与测试

- [x] **T17: CMake 配置** - 子目录 CMakeLists.txt
- [x] **T18: 单元测试** - GoogleTest 测试用例（6 个测试文件）

## 实现顺序

```
T1 → T2 → T3 → T4 → T5 → T6 → T7 → T8 → T9 → T10 → T11 → T12 → T13 → T14 → T15 → T16 → T17 → T18
```

## 设计文档

详见 `docs/` 下的 10 个设计文档。

## 项目结构

```
rag/
├── include/rag/           # 头文件
│   ├── rag.h              # 主头文件
│   ├── error.h            # 错误处理
│   ├── logger.h           # 日志系统
│   ├── config.h           # 配置管理
│   ├── types.h            # 数据结构
│   ├── database.h         # SQLite
│   ├── chunker.h          # 分块器
│   ├── parser.h           # 解析器
│   ├── vector_index.h     # HNSW
│   ├── bm25_index.h       # BM25
│   ├── retriever.h        # 检索器
│   ├── llm_service.h      # LLM 服务
│   ├── metrics.h          # 指标
│   ├── server.h           # REST API
│   ├── engine.h           # 核心引擎
│   └── cli.h              # CLI
├── src/rag/               # 实现
│   ├── error/
│   ├── logger/
│   ├── config/
│   ├── data/
│   ├── chunker/
│   ├── parser/
│   ├── index/
│   ├── retrieval/
│   ├── llm/
│   ├── metrics/
│   ├── engine/
│   ├── server/
│   └── cli/
├── test/rag/              # 单元测试
├── docs/                  # 设计文档
├── diagrams/              # UML 图
└── CMakeLists.txt
```

## 进度

| 任务 | 状态 | 说明 |
|------|------|------|
| T1 | ✅ | 错误码、异常类、重试策略 |
| T2 | ✅ | spdlog 日志系统 |
| T3 | ✅ | YAML 配置解析 |
| T4 | ✅ | 核心数据结构 |
| T5 | ✅ | SQLite 数据库和 Repository |
| T6 | ✅ | Fixed/Recursive/Code 分块器 |
| T7 | ✅ | Markdown/Code/Text 解析器 |
| T8-T9 | ✅ | HNSW/BM25 索引 |
| T10 | ✅ | 混合检索器 + RRF 融合 |
| T11 | ✅ | 简化版 Embedding 服务 |
| T12 | ✅ | 简化版 LLM 服务 |
| T13 | ✅ | RAG 核心引擎 |
| T14 | ✅ | Prometheus 指标 + 健康检查 |
| T15 | ✅ | CLI 命令行工具 |
| T16 | ✅ | REST API 服务器 |
| T17 | ✅ | CMake 构建配置 |
| T18 | ✅ | 6 个单元测试文件 |

## 提交记录

- `feat(rag): 完成 RAG 系统核心实现` - 初始实现
- `feat(rag): 完成剩余模块实现` - LLM/指标/API/测试
