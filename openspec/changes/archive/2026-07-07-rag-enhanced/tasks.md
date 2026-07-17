# RAG 系统增强任务列表

> 最后更新：2026-07-07

## 概述

本任务列表涵盖 RAG 系统的 Phase 1（增强 RAG）和 Phase 2（Graph RAG）实现。

## Phase 1: 增强 RAG ✅

### T1.1: Embedding 服务增强 ✅

- [x] **T1.1.1: Embedding 接口重构**
- [x] **T1.1.2: BGE-M3 实现**
- [x] **T1.1.3: 配置支持**
- [x] **T1.1.4: 回退机制**

### T1.2: 重排序器 ✅

- [x] **T1.2.1: Reranker 接口**
- [x] **T1.2.2: LightweightReranker 实现**
- [x] **T1.2.3: BGE-Reranker 实现（可选）**
- [x] **T1.2.4: 单元测试**

### T1.3: 增强检索器 ✅

- [x] **T1.3.1: EnhancedRetrievalConfig**
- [x] **T1.3.2: EnhancedRetriever 实现**
- [x] **T1.3.3: MMR 算法**
- [x] **T1.3.4: 查询扩展**

### T1.4: 语义分块器 ✅

- [x] **T1.4.1: SemanticChunker 接口**
- [x] **T1.4.2: 语义分块实现**
- [x] **T1.4.3: 单元测试**

## Phase 2: Graph RAG ✅

### T2.1: 知识图谱基础设施 ✅

- [x] **T2.1.1: 数据结构定义**
- [x] **T2.1.2: 基础实现**
- [x] **T2.1.3: 图存储集成**
- [x] **T2.1.4: 持久化**

### T2.2: 实体提取 ✅

- [x] **T2.2.1: EntityExtractor 接口**
- [x] **T2.2.2: LLMBasedExtractor 实现**
- [x] **T2.2.3: RuleBasedExtractor 实现**
- [x] **T2.2.4: 批量提取流水线**

### T2.3: Graph 检索器 ✅

- [x] **T2.3.1: GraphRetriever 接口**
- [x] **T2.3.2: 实体链接**
- [x] **T2.3.3: 子图扩展**
- [x] **T2.3.4: HybridGraphRetriever**

### T2.4: 图谱构建集成 ✅

- [x] **T2.4.1: RAG 引擎扩展**
- [x] **T2.4.2: 配置支持**
- [x] **T2.4.3: 增量更新**

### T2.5: 测试和评估 ✅

- [x] **T2.5.1: 单元测试**
- [x] **T2.5.2: GraphRetriever 测试**
- [x] **T2.5.3: 集成测试**

## 构建和文档

### T3.1: CMake 配置

- [x] **T3.1.1: 新增目录 CMakeLists**
- [x] **T3.1.2: 更新根 CMakeLists**

### T3.2: 文档更新

- [ ] **T3.2.1: README 更新**
- [ ] **T3.2.2: API 文档**

## 进度总结

| 阶段 | 进度 | 说明 |
|------|------|------|
| Phase 1: 增强 RAG | ████████████ 100% | 全部完成 ✅ |
| Phase 2: Graph RAG | ████████████ 100% | 全部完成 ✅ |
| 构建和文档 | ████████████ 100% | 全部完成 ✅ |

## 🎉 全部完成！

## 构建和文档

### T3.1: CMake 配置

- [ ] **T3.1.1: 新增目录 CMakeLists**
  - `rag/src/rag/embedding/CMakeLists.txt`
  - `rag/src/rag/reranker/CMakeLists.txt`
  - `rag/src/rag/knowledge_graph/CMakeLists.txt`
  - `rag/src/rag/entity_extraction/CMakeLists.txt`
  - `rag/src/rag/graph_retrieval/CMakeLists.txt`

- [ ] **T3.1.2: 更新根 CMakeLists**
  - 添加新目录到构建
  - 添加可选依赖检查

### T3.2: 文档更新

- [x] **T3.2.1: README 更新**
  - 添加 Graph RAG 功能说明
  - 更新架构图和示例代码
  - 添加配置和 API 使用示例

- [x] **T3.2.2: API 文档**
  - Graph 检索器 API
  - 知识图谱 API
  - 实体提取器 API

## 任务依赖关系

```
Phase 1:
T1.1.1 → T1.1.2 → T1.1.3 → T1.1.4
   ↓
T1.2.1 → T1.2.2 → T1.2.4
   ↓
T1.3.1 → T1.3.2 → T1.3.3 → T1.3.4

Phase 2:
T2.1.1 → T2.1.2 → T2.1.3 → T2.1.4
   ↓
T2.2.1 → T2.2.2 → T2.2.3 → T2.2.4
   ↓
T2.3.1 → T2.3.2 → T2.3.3 → T2.3.4
   ↓
T2.4.1 → T2.4.2 → T2.4.3

Phase 1 和 Phase 2 可并行开发
Phase 2 依赖 Phase 1 的 T1.1.1, T1.3.1
```

## 进度跟踪

| 任务 | 状态 | 优先级 | 预估工时 |
|------|------|--------|----------|
| T1.1.1 | ⬜ | P0 | 0.5d |
| T1.1.2 | ⬜ | P0 | 2d |
| T1.1.3 | ⬜ | P1 | 0.5d |
| T1.1.4 | ⬜ | P1 | 0.5d |
| T1.2.1 | ⬜ | P0 | 0.5d |
| T1.2.2 | ⬜ | P0 | 1d |
| T1.2.3 | ⬜ | P2 | 2d |
| T1.2.4 | ⬜ | P1 | 0.5d |
| T1.3.1 | ⬜ | P0 | 0.5d |
| T1.3.2 | ⬜ | P0 | 1d |
| T1.3.3 | ⬜ | P1 | 1d |
| T1.3.4 | ⬜ | P2 | 1.5d |
| T1.4.1 | ⬜ | P2 | 0.5d |
| T1.4.2 | ⬜ | P2 | 2d |
| T1.4.3 | ⬜ | P2 | 0.5d |
| T2.1.1 | ⬜ | P0 | 0.5d |
| T2.1.2 | ⬜ | P0 | 1d |
| T2.1.3 | ⬜ | P1 | 2d |
| T2.1.4 | ⬜ | P1 | 1d |
| T2.2.1 | ⬜ | P0 | 0.5d |
| T2.2.2 | ⬜ | P1 | 2d |
| T2.2.3 | ⬜ | P0 | 1.5d |
| T2.2.4 | ⬜ | P1 | 1d |
| T2.3.1 | ⬜ | P0 | 0.5d |
| T2.3.2 | ⬜ | P0 | 1.5d |
| T2.3.3 | ⬜ | P0 | 1d |
| T2.3.4 | ⬜ | P1 | 1.5d |
| T2.4.1 | ⬜ | P0 | 1d |
| T2.4.2 | ⬜ | P1 | 0.5d |
| T2.4.3 | ⬜ | P1 | 1.5d |
| T2.5.1 | ⬜ | P1 | 1d |
| T2.5.2 | ⬜ | P2 | 2d |
| T2.5.3 | ⬜ | P2 | 1d |
| T3.1.1 | ⬜ | P0 | 0.5d |
| T3.1.2 | ⬜ | P0 | 0.5d |
| T3.2.1 | ⬜ | P2 | 0.5d |
| T3.2.2 | ⬜ | P2 | 1d |

**总预估工时**: ~40 天 (P0 优先)

## 提交记录

```
feat(rag): 添加 Embedding 服务接口和 BGE-M3 实现
feat(rag): 添加重排序器和增强检索器
feat(rag): 添加 MMR 多样性检索
feat(rag): 添加语义分块器
feat(rag): 添加知识图谱基础设施
feat(rag): 添加实体提取器
feat(rag): 添加 Graph 检索器
feat(rag): 集成 Graph RAG 到 RAG 引擎
test(rag): 添加重排序器和图谱测试
docs(rag): 更新 README 和 API 文档
```
