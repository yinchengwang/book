# C5 Vector 与多模态核心生产化 Proposal

## Why

04 卷已识别 Vector 模态并发安全（COW/UAF）和多模态检索路径骨架级实现。
本变更是批次 A 启动项，目标：

1. Vector 完整 COW 段（读快照 + 写缓冲）
2. 多模态检索完整路径（多 embedding 搜索 + RRF 归并 + blob 绑定 + metadata filter）
3. 多向量融合路径 + cross_modal_search 实接 21+ 索引族

## What Changes

- **Vector**：faiss_hnsw COW immutable buffer + 原子指针切换；faiss_hnsw_search 已 SIMD 委托
- **多模态**：mm_multimodal_search_v2 实现 per-named-vector 并行搜索 + RRF 融合
- **基础设施**：HNSW vtable 已就位（C4-2），本变更接入完整删除 stub
- **测试**：cross_modal_e2e 升级到真实路径
- **文档**：multi_modal DESIGN.md 更新

## Capabilities

| 能力 | 交付 |
|------|------|
| Vector COW 段 | 读快照不变，写入走 buffer + 原子切换 |
| 多 embedding 搜索 | per-named-vector 并行 + RRF 融合 top-k |
| cross_modal 检索 | text query → image space top-k |
| 多模态 metadata filter | 集合级 JSON 子串过滤 |
| 多模态 RAG 路径 | chunk 检索结果直接返回 |

## Impact

- 修改文件：`engineering/src/db/index/vector_index/faiss_hnsw/`、`engineering/src/db/storage/multimodal/`、`engineering/test/db/storage/cross_modal_e2e_test.cpp`
- 预计 8-12 个 commit
- 依赖：C0-1 mmdb_lock（已）、C2-3 graph algo（已）、C3-4 multimodal_object（已）

## Commit 总览

| # | 阶段 | 简述 |
|---|------|------|
| 1 | C5.1 | faiss_hnsw COW 段基础结构（immutable_buffer + atomic ptr） |
| 2 | C5.2 | faiss_hnsw 写入路径 COW 改造 |
| 3 | C5.3 | faiss_hnsw 读取快照（无锁原子读 segment） |
| 4 | C5.4 | mm_multimodal_search_v2 实现（per-vector 并行） |
| 5 | C5.5 | RRF 集成（接入 faiss_hnsw search 返回） |
| 6 | C5.6 | cross_modal_search 接入 21+ 索引族（通过 vtable） |
| 7 | C5.7 | blob + metadata filter 完整绑定 |
| 8 | C5.8 | e2e 真实路径 + DESIGN 更新 |
| 9 | Review | Whole-batch review |
| 10 | Archive | Archive C5 |
