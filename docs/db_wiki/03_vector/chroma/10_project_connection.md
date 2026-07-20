# Chroma 项目关联

## 学习目标

- 分析 Chroma 设计对项目的启发

## 嵌入式设计

Chroma 的嵌入式设计（进程内运行，无独立服务）对项目的启发：

```c
// 项目中可以设计类似 Chroma 的 "嵌入式向量引擎"
// 无需启动独立进程，直接在应用进程内运行

// 嵌入式 API 设计
vdb_ctx_t *ctx = vdb_init("./vdb_data");  // 直接初始化
vdb_collection_t *col = ctx->create_collection("docs");
col->add(docs, embeddings, n);
vdb_results_t *res = col->query(query_emb, k, filter);
```

## Embedding Function 抽象

Chroma 的 Embedding Function 插件模式可借鉴到项目的向量引擎：

| Chroma | 项目借鉴 |
|--------|---------|
| EmbeddingFunction 接口 | vectorizer_t 抽象 |
| 内置模型 | 本地 ONNX Runtime 集成 |
| 外部 API 模型 | 远程模型调用 |
| 自定义函数 | 用户定义的向量化回调 |

## 要点总结

- 嵌入式设计适合项目的工具和应用场景
- Embedding Function 插件模式可复用
- Metadata 过滤的简单设计值得参考

## 思考题

1. 项目中是否需要一个"嵌入式向量引擎"？使用场景是什么？
2. Embedding Function 抽象是否应该成为向量引擎的一部分？