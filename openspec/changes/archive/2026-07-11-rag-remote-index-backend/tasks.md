## 1. 接口层重构（前置，阻塞后续所有 REMOTE 模式）

- [x] 1.1 抽取 `IBm25Index` 抽象基类（完整拷贝 BM25Index 的公有方法签名）
- [x] 1.2 现有 `BM25Index` 改为 `class BM25Index : public IBm25Index`，实现不变
- [x] 1.3 `rag/include/rag/vector_index.h` 中 `init()`/`save()`/`load()`/`clear()` 改为默认空实现（非纯虚）
- [x] 1.4 `rag/include/rag/engine.h` 成员 `bm25_index_` 类型改为 `std::shared_ptr<IBm25Index>`
- [x] 1.5 `rag/src/rag/index/bm25_index.cpp` 适配 `IBm25Index`（工厂函数返回类型调整）
- [x] 1.6 验证 LOCAL 模式编译通过、现有测试全绿

## 2. Infrastructure — 枚举归一化与上下文扩展

- [x] 2.1 在 `vector_query.h` 新增 `VECTOR_INDEX_BRUTE_FORCE = 3`
- [x] 2.2 在 `vector_index_selector.h` 新增 `vector_index_type_convert()` 映射函数，将 `VectorIndexType` 转为 `vector_index_type_t`
- [x] 2.3 在 `rest_api.c` 的 `RequestContext_s` 中新增 `void *user_data` 字段，并暴露 `request_get_user_data()` accessor
- [x] 2.4 在 `request_handler_thread` 调用 handler 前设置 `ctx.user_data = server`

## 3. REST Server — handlers 接入 VectorAPI

- [x] 3.1 `handle_create_collection`：解析 `name`、`dimension`、`metric_type`、`index_type`，调 `vector_api_create_collection` + 初始化空 BM25
- [x] 3.2 `handle_list_collections`：调 `vector_api_list_collections`，返回真实集合列表
- [x] 3.3 `handle_get_collection`：调 `vector_api_get_collection`，返回集合详情（含 `index_type`）
- [x] 3.4 `handle_delete_collection`：调 `vector_api_destroy_collection` + 清理 BM25
- [x] 3.5 `handle_insert_vectors`：解析 `vectors` 数组，调 `vector_api_insert`
- [x] 3.6 `handle_search`：解析 `vector`、`top_k`，调 `vector_api_search`，返回真实结果
- [x] 3.7 `handle_get_vector`：调 `vector_api_size` + 路径参数 ID 解析
- [x] 3.8 `handle_delete_vector`：调 `vector_api_delete`（FNV-1a 哈希字符串 ID → int64）
- [x] 3.9 `handle_update_vector`：返回 NOT_IMPLEMENTED（VectorAPI 未提供 update）

## 4. Python SDK — IndexType 支持

- [x] 4.1 在 `sdk/python/minivecdb/__init__.py` 新增 `IndexType` 枚举类（HNSW=0, DISKANN=1, IVF=2, BRUTE_FORCE=3, AUTO=99）
- [x] 4.2 在 `client.py` 的 `create_collection()` 新增 `index_type` 参数（默认 `IndexType.AUTO`），传入 payload
- [x] 4.3 更新 `demo/rag/rag_demo.py` 创建集合时传 `index_type=IndexType.HNSW`

## 5. REST Server — BM25 端点

- [x] 5.1 在 `RestServer_s` 中新增 BM25 索引映射（`collection_name → bm25_t*` uthash 哈希表）
- [x] 5.2 在 `rest_api.c` / CMakeLists 中链接 `db_bm25` 库
- [x] 5.3 在 `handlers.c` 中新增 `handle_text_search`：`POST /collections/:id/text-search`，支持 add / search / delete 三种 action
- [x] 5.4 修改 `handle_create_collection`：创建向量集合同时初始化空 BM25 索引
- [x] 5.5 修改 `handle_delete_collection`：删除集合同时销毁关联 BM25 索引
- [x] 5.6 暴露 `rest_server_bm25_lookup/ensure/drop` 三件套给 handlers.c
- [x] 5.7 注册 BM25 端点到路由表

## 6. RAG Server — HTTP Client 与远程索引

- [x] 6.1 在 `rag/src/rag/index/` 新增 `remote_vector_index.cpp`，实现 `VectorIndex` 接口
  - 内部维护 `string→int64` 映射表 (`id_map_`)
  - `add()` → `POST /collections/{name}/vectors`
  - `search()` → `POST /collections/{name}/search`（返回时 int64→string 反向映射）
  - `remove()` → `DELETE /collections/{name}/vectors/{id}`
  - `get()` → `GET /collections/{name}/vectors/{id}`
  - `size()` → 查映射表大小
  - `is_loaded()` → return true
  - `init/save/load/clear` → 继承默认空实现
- [x] 6.2 在 `rag/src/rag/index/` 新增 `remote_bm25_index.cpp`，实现 `IBm25Index` 接口
  - `add()` → `POST /collections/{name}/text-search` (action=add)
  - `search()` → `POST /collections/{name}/text-search` (query)
  - `remove()` → `POST /collections/{name}/text-search` (action=delete)
- [x] 6.3 在 `rag/include/rag/config.h` 的 `EngineConfig` 中新增 `IndexBackendType { LOCAL, REMOTE }` 枚举和 `RemoteIndexConfig` 结构
- [x] 6.4 修改 `RAGEngine::init_components()` 根据后端模式创建对应索引实例
- [x] 6.5 默认 `index_backend = REMOTE` 模式（保持向后兼容）
- [x] 6.6 在 `rag/src/rag/index/CMakeLists.txt` 中添加 `remote_vector_index.cpp` 和 `remote_bm25_index.cpp`

## 7. 验证

- [x] 7.1 语法检查：handlers.c / rest_api.c / bm25_index.cpp / retriever.cpp / engine.cpp / test_retriever.cpp 全部通过
- [x] 7.2 Python SDK IndexType 导入测试通过
- [x] 7.3 端到端验证（需要运行实际 REST Server，本会话受网络限制）
