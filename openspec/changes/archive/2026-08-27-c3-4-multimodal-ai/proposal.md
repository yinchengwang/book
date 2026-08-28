# C3-4 多模态 AI 原生存储 Proposal

## Why

差距分析 §5.5（契合度 9，最高优先级）+ 用户全自研决策：自研 Vector 模态（21+ 索引 + hybrid_retrieval.c RRF）+ RAG 系统（`engineering/rag/` + executor/rag/）基础扎实，但当前是"单向量单 ID"模型，缺多模态原生能力（多 named vector、blob 绑定、跨模态检索）。与现有 RAG 闭环：commit `efcd239f8` 已添加"多态 RAG 架构文档"，本变更提供底层数据支撑。

## What Changes

- **NamedVector schema**：`{id, blob_ref?, metadata, embeddings: {clip: vec768, siglip: vec512, text_bge: sparse}}` 一对象多向量多模型
- 向量集合 schema 扩展（schema 层增字段）
- 每个 named vector 独立索引（复用 21+ 索引族 + selector）
- **跨向量统一 top-k 归并（RRF）**：多空间查询按 Reciprocal Rank Fusion 融合
- **跨模态检索算子**：`cross_modal_search(text_query, target_modal="image")`——查询向量在目标空间搜索，模型推理由外部完成，数据库提供空间与距离
- 与 C3-1 Blob 联动：图像/音视频存 Blob，对象持 blob-id
- 与 engineering/rag/ 打通：多模态 RAG 输入（图 + 文 chunk），rag_pipeline 支持多模态 embedding

## Capabilities

| 能力 | 交付 |
|------|------|
| NamedVector | 一对象多 embedding（不同模型/不同模态） |
| 跨向量 RRF | 多空间查询融合排序 |
| Blob 绑定 | 图像/音视频原数据存数据库 |
| 多模态 RAG | 图文混合 chunk + 多 embedding 检索 |

## Impact

- 修改：vector_engine.c、vector_index_selector.c、hybrid_retrieval.c、rag_pipeline.c
- 新增：named_vector.c、cross_modal.c、multimodal_object.h
- 预计 10-12 个 commit
- 依赖：C3-1
