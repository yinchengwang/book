# C3-4 多模态 AI 原生存储设计文档

## 设计目标

把"单向量单 ID"升级为"多模态对象"：一个对象携带多 named embedding + Blob 引用 + 元数据，支撑跨模态检索与多模态 RAG。

## 方案

1. **NamedVector schema**：一对象多向量（不同模型/模态）—— 扩展 vector_collection_schema
2. **多 embedding 插入路径**：per-named-vector 独立索引（复用 21+ 索引族 + selector）
3. **跨向量 RRF**：多空间 top-k 归并（Reciprocal Rank Fusion）
4. **跨模态检索算子**：`cross_modal_search(text_query, target_modal="image")`
5. **与 RAG 联动**：多模态 chunk 的混合检索
6. **依赖 C3-1**：图像/视频 Blob 存 blob_engine

## 实现文件

- `engineering/include/db/multimodal_object.h`（新增）
- `engineering/src/db/storage/multimodal/named_vector.c`（新增）
- `engineering/src/db/storage/multimodal/cross_modal.c`（新增）
