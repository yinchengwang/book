# RAG 框架克隆清单

本目录收录 11 个主流开源 RAG（Retrieval-Augmented Generation，检索增强生成）框架源码，作为学习与对照参考。

## 项目清单

| 项目 | GitHub | 本地路径 | tier | 备注 |
|------|--------|----------|------|------|
| langchain | langchain-ai/langchain | rag/langchain/ | core | LLM 应用编排主流 |
| llama-index | run-llama/llama_index | rag/llama-index/ | core | RAG 数据框架 |
| haystack-ai | deepset-ai/haystack | rag/haystack-ai/ | core | NLP 管道框架 |
| txtai | neuml/txtai | rag/txtai/ | representative | 语义搜索 |
| embedchain | embedchain/embedchain | rag/embedchain/ | representative | 快速构建 RAG |
| verba | weaviate/verba | rag/verba/ | innovative | Weaviate 出品 |
| cognita | truefoundry/cognita | rag/cognita/ | representative | TrueFoundry 出品 |
| r2r | SciPhi-AI/R2R | rag/r2r/ | innovative | SciPhi 出品 |
| quivr | QuivrHQ/quivr | rag/quivr/ | representative | 个人知识库 |
| private-gpt | zylon-ai/private-gpt | rag/private-gpt/ | representative | 本地私有 GPT |
| qanything | netease-youdao/QAnything | rag/qanything/ | representative | 网易出品 |

## 层级说明

- **core**：学习必读、产业标准事实标杆。
- **representative**：具有代表性的官方/大厂产品。
- **innovative**：新锐思路，对前两类的补充。

## 更新方式

```bash
cd reference/ai/rag/<project>
git fetch --depth 1 origin
git reset --hard origin/main
```

## 来源

完整的元数据登记在仓库根的 [`reference/catalog.yml`](../catalog.yml) 中，以 `model: ai/rag` 字段标记。
