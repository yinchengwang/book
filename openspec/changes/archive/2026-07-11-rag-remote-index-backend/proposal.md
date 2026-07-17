## Why

RAG 系统（`rag/`）目前使用自带的简化 HNSWIndex（实际是暴力线性扫描）和自写 BM25Index，而 Engineering 项目（`engineering/`）有生产级的 C 语言实现（真实 HNSW 图搜索、DiskANN、IVF、BM25 等）。两套代码互相独立，REST Server 的 handlers 全是桩代码（`// TODO: 调用 VectorAPI`），导致 Python SDK ↔ REST Server ↔ 真实索引 的链路断裂。Python demo `rag_demo.py` 无法端到端运行。

需要打通全链路：Python Demo → SDK → Engineering REST Server（真实索引）← HTTP ← RAG Server（仅做编排）。

## What Changes

- **Engineering REST Server handlers 接入真实 VectorAPI 后端**：`handle_create_collection`、`handle_insert_vectors`、`handle_search` 等从桩代码改为真实调用
- **Python SDK 增加 `IndexType` 枚举**：`create_collection` 支持传 `index_type` 参数
- **REST Server 新增 BM25 text-search 端点**：`POST /collections/{name}/text-search`
- **RAG Server 替换索引后端为远程 HTTP**：`rag::HNSWIndex` 和 `rag::BM25Index` 改为通过 HTTP Client 调 Engineering REST Server
- **Demo 端到端调通**：`rag_demo.py` → SDK → REST Server → 真实索引 ← RAG Server

## Capabilities

### New Capabilities
- `index-type-selection`: 允许用户在创建集合时通过 SDK 选择索引算法（HNSW / IVF / DiskANN / BruteForce），SDK 传入 `IndexType` 常量，REST Server 透传到 `VectorCreateParams.index_type`
- `rag-remote-index`: RAG Engine 通过 HTTP Client 使用远程 Engineering REST Server 作为索引后端，支持向量搜索和 BM25 文本检索，RAG Server 仅做分块、检索融合（RRF）、LLM 生成

### Modified Capabilities
- `rest-api`: REST Server 的 `/collections/*` 系列处理器从桩代码改为真实后端；请求体支持 `index_type` 参数；新增 `POST /collections/{name}/text-search` BM25 端点；路径规整统一（当前 `rest-api` spec 定义 `/api/v1/` 前缀但实现是 `/collections`，需对齐）
- `python-sdk`: `MiniVecDBClient.create_collection()` 增加 `index_type` 参数；新增 `IndexType` 枚举类（HNSW=0, DISKANN=1, IVF=2, BRUTE_FORCE=3, AUTO=99）；`collection.py` 的 insert/search 接口保持不变
- `rag-integration`: RAG 系统的向量索引和 BM25 索引从本地简化实现改为远程 HTTP 调用；RAG Engine 新增配置开关选择本地/远程模式

## Impact

- `rag/include/rag/bm25_index.h` — 抽取 `IBm25Index` 抽象基类，BM25Index 改为继承自它
- `rag/include/rag/vector_index.h` — init/save/load/clear 改为默认空实现
- `rag/include/rag/engine.h` — BM25Index 成员类型改为 `shared_ptr<IBm25Index>`
- `engineering/src/db/api/handlers.c` — 9 个桩处理器改为真实逻辑
- `engineering/src/db/api/rest_api.c` — 无大改，`vector_api` 成员已存在
- `engineering/include/db/core/vector_query.h` — 新增 `VECTOR_INDEX_BRUTE_FORCE = 3`
- `sdk/python/minivecdb/client.py` — `create_collection()` 加 `index_type` 参数
- `sdk/python/minivecdb/__init__.py` — 导出 `IndexType` 枚举
- `sdk/python/minivecdb/collection.py` — 无改
- `rag/src/rag/index/` — 新增 `remote_vector_index.cpp`、`remote_bm25_index.cpp`
- `rag/src/rag/index/bm25_index.cpp` — IBm25Index 实现适配
- `rag/src/rag/index/CMakeLists.txt` — 添加新文件
- `demo/rag/rag_demo.py` — 传入 `index_type=IndexType.HNSW`

不影响：`test/`、`apps/`、`third_part/`、现有的 C 索引算法本身（HNSW/DiskANN/IVF/BM25）。
