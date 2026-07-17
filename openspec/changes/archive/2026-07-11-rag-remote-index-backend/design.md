## Context

当前有两套独立代码库：`engineering/` 包含生产级 C 语言索引实现（HNSW、DiskANN、IVF、BM25）和 REST Server 框架，但 `handlers.c` 全是桩代码（`// TODO: 调用 VectorAPI`）；`rag/` 有完整的 RAG 编排引擎，但使用自带的简化实现（暴力扫描冒充 HNSW，自写 BM25）。Python SDK 的 `/collections/*` 端点协议也没有任何 C++ 服务实现。

目标：Engineering REST Server 作为统一的后端服务，RAG Server 和 Python Demo 都通过 HTTP 调用它。打通 Demo → SDK → REST Server → 真实索引 → RAG Server 的全链路。

## Goals / Non-Goals

**Goals:**
- REST Server handlers 调真实 VectorAPI 后端，结束桩代码状态
- Python SDK 支持 `index_type` 参数选择索引算法
- REST Server 支持 BM25 文本检索端点
- RAG Server 通过 HTTP Client 使用远程索引，替代本地简化实现
- Python demo 端到端跑通

**Non-Goals:**
- 不修改现有 C 索引算法本身的实现（HNSW 图搜索、DiskANN、IVF 等不动）
- 不修改 RAG Engine 的编排逻辑（分块、RRF 融合、LLM 生成流程不变）
- 不引入新的外部依赖（HTTP Client 复用原生 socket 风格）
- 不做性能优化或基准测试（先打通链路）

## Decisions

### 1. 索引类型枚举归一化

**问题**：代码库存在两套数值不兼容的索引类型枚举：
- `VectorIndexType`（`vector_query.h`）：HNSW=0, DISKANN=1, IVF=2, AUTO=99
- `vector_index_type_t`（`vector_index_selector.h`）：BRUTE_FORCE=0, HNSW=1, IVF_PQ=2, DISKANN=3

**决定**：统一使用 `VectorIndexType`（VectorAPI 使用的枚举），在 `vector_index_selector.h` 中增加映射函数做转换。
Python SDK 的 `IndexType` 常量值与 `VectorIndexType` 对齐。

### 2. Handler 访问 VectorAPI 的方式

**问题**：`handlers.c` 的 `RequestHandler` 签名是 `int (*)(RequestContext *ctx)`，而 `RequestContext_s` 没有指向 `RestServer` 的指针，Handler 拿不到 `server->vector_api`。

**决定**：在 `RequestContext_s` 中增加 `void *user_data` 字段，handler thread 调用前设置为 `server->vector_api`。Handlers 通过 `(VectorAPI *)ctx->user_data` 访问。

```c
// rest_api.c: RequestContext_s 新增
struct RequestContext_s {
    // ... 现有字段 ...
    void *user_data;  // 新增
};

// request_handler_thread 中调用前
ctx.user_data = server->vector_api;
```

### 3. BM25 在 REST Server 中的管理

**问题**：VectorAPI 不管理 BM25 索引，`db_bm25` 是独立的静态库。

**决定**：REST Server 的 `RestServer_s` 中新增 `BM25IndexMap`（`collection_name → bm25_index_t*` 哈希表），独立于 VectorAPI 管理。Handler 操作 BM25 时查此映射。

```
handlers.c
  handle_create_collection → vector_api_create_collection + bm25_map_put(空 BM25)
  handle_delete_collection → vector_api_destroy_collection + bm25_map_remove
  handle_insert_vectors    → vector_api_insert + bm25_map_get → bm25_add_text
  handle_text_search       → bm25_map_get → bm25_search → JSON 响应
```

REST Server CMakeLists 新增 `db_bm25` 链接依赖。

### 4. RAG Server HTTP Client

**问题**：RAG 项目没有 HTTP 客户端，需要调 REST Server 的 HTTP API。

**决定**：在 `rag/src/rag/index/` 中新增两个类：
- `RemoteVectorIndex`：实现 `VectorIndex` 接口，`search()` 发 `POST /collections/{name}/search`
- `RemoteBM25Index`：实现 `BM25Index` 接口，`add()` 发 `POST /collections/{name}/vectors` + `POST /collections/{name}/text-search`

HTTP Client 使用原生 winsock/posix socket（与 Engineering REST Server 风格一致），JSON 使用已引入的 nlohmann/json。

### 5. RAG Engine 索引模式切换

**决定**：`RAGEngine` 新增构造参数 `IndexBackendType { LOCAL, REMOTE }`，多态替换索引实现：

```cpp
// RAGEngine::init_components()
if (config_.index_backend == REMOTE) {
    vector_index_ = std::make_shared<RemoteVectorIndex>(rest_url_);
    bm25_index_ = std::make_shared<RemoteBM25Index>(rest_url_);
} else {
    vector_index_ = std::make_shared<HNSWIndex>();     // 本地简化版
    bm25_index_ = std::make_shared<BM25Index>();        // 本地简化版
}
```

默认 `REMOTE` 模式。RAG Engine 本地的 SQLite（DocumentRepository/ChunkRepository）保留不变，用于管理文档结构和元数据。

### 6. API 路径规整

**问题**：`rest-api` spec 定义 `/api/v1/collections` 但实际 REST Server 实现用 `/collections`（无前缀），Python SDK 也用 `/collections`。

**决定**：保持 REST Server 现有路径风格（`/collections`、`/collections/{name}/vectors`、`/collections/{name}/search`）不变，后续如有 `/api/v1/` 需求再做版本化。BM25 端点路径定为 `POST /collections/{name}/text-search`。路径参数符号统一使用代码侧已有的 `:id` 风格。

### 7. BM25Index 接口抽象化

**问题**：BM25Index 是具体类（无虚方法），无法多态替换。设计需要 LOCAL/REMOTE 两种模式切换，但以当前结构无法实现 RemoteBM25Index。

**决定**：抽取 `IBm25Index` 抽象基类，BM25Index（本地实现）和 RemoteBM25Index（远程实现）均继承自它。RAGEngine 成员类型改为 `std::shared_ptr<IBm25Index>`。

```cpp
// 新抽象接口
class IBm25Index {
public:
    virtual ~IBm25Index() = default;
    virtual void init(const BM25Config& config) = 0;
    virtual void add(const std::string& id, const std::string& content) = 0;
    virtual void add_batch(const std::vector<std::string>& ids,
                           const std::vector<std::string>& contents) = 0;
    virtual std::vector<std::pair<std::string, float>> search(
        const std::string& query, int top_k) = 0;
    virtual std::vector<std::pair<std::string, float>> search(
        const std::vector<std::string>& terms, int top_k) = 0;
    virtual std::optional<std::string> get(const std::string& id) = 0;
    virtual void remove(const std::string& id) = 0;
    virtual void clear() = 0;
    virtual void save(const std::filesystem::path& path) = 0;
    virtual void load(const std::filesystem::path& path) = 0;
    virtual size_t size() const = 0;
    virtual bool is_loaded() const = 0;
    virtual const BM25Config& config() const = 0;
};

// 现有实现改名（代码不增不减）
class BM25Index : public IBm25Index { /* 现有代码完整保留 */ };
class RemoteBM25Index : public IBm25Index { /* HTTP 远程实现 */ };
```

### 8. VectorIndex 接口提供远程不适用方法的默认空实现

**问题**：`VectorIndex` 接口定义了 `init/save/load/clear` 等方法，这些在远程模式下无意义。但 RAGEngine 中无条件调用了这些方法（engine.cpp 第 66、86、133、243、379、380、394、397、406 行），如果从接口剥离，engine.cpp 必须引入模式判断分支，破坏"引擎不关心索引实现"的抽象。

**决定**：为 `init/save/load/clear` 等方法提供默认空实现（no-op body），`RemoteVectorIndex` 继承默认行为，`HNSWIndex` 按需 override。engine.cpp 不需要感知 REMOTE/LOCAL 模式。

```cpp
class VectorIndex {
public:
    // 核心方法保持纯虚
    virtual void add(const std::string& id, const std::vector<float>& vector) = 0;
    virtual std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query, int top_k) = 0;
    // ...

    // 远程不适用方法提供默认空实现
    virtual void init(const HNSWConfig&) {}              // 远程无操作
    virtual void save(const std::filesystem::path&) {}   // 远程无操作
    virtual void load(const std::filesystem::path&) {}   // 远程无操作
    virtual void clear() {}                              // 远程无操作
};
```

### 9. ID 类型映射策略

**问题**：RAG 层使用 `std::string` 作为向量 ID，Engineering VectorAPI 使用 `int64_t`。两套 ID 系统在 REST API 交互时必须互相转换。

**决定**：RemoteVectorIndex 内部维护 `string→int64` 的本地映射表（`std::unordered_map<std::string, int64_t>`），每次 `add()` 时为新的 string ID 分配自增 int64 值。搜索返回结果时，查映射表将 int64 转回 string。该映射为纯 RAG 层行为，不修改 VectorAPI 和 REST API 的任何接口。

```cpp
// RemoteVectorIndex 内部
std::unordered_map<std::string, int64_t> id_map_;
int64_t next_id_ = 1;

std::string to_string_id(int64_t numeric_id) {
    for (auto& [sid, nid] : id_map_) {
        if (nid == numeric_id) return sid;
    }
    return std::to_string(numeric_id);  // fallback
}
```

### 10. VectorIndexType 补充 BRUTE_FORCE 值

**问题**：`VectorIndexType`（vector_query.h）和 Python SDK `IndexType` 都在枚举中包含 BRUTE_FORCE=3（或计划包含），但 `VectorIndexType` 没有此值。

**决定**：在 `vector_query.h` 的 `VectorIndexType` 枚举中新增 `VECTOR_INDEX_BRUTE_FORCE = 3`。`vector_index_type_convert()` 映射函数据此做查表转换。

```c
typedef enum VectorIndexType_e {
    VECTOR_INDEX_HNSW = 0,
    VECTOR_INDEX_DISKANN = 1,
    VECTOR_INDEX_IVF = 2,
    VECTOR_INDEX_BRUTE_FORCE = 3,   // 新增
    VECTOR_INDEX_AUTO = 99
} VectorIndexType;
```

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|----------|
| [Performance] RAG Server 每次搜索都要过 HTTP 延迟 | 后续可在 RAG Server 侧加本地缓存，或改用 Unix Domain Socket |
| [API不一致] BM25 参数（k1, b）无法通过 REST API 传入 | text-search 请求体增加可选 `k1`、`b` 参数，默认值对齐 BM25Config |
| [数据一致性] 向量插入和 BM25 插入是两个独立调用，可能部分失败 | 目前接受最终一致性；后续可加事务 ID 或回滚补偿 |
| [枚举兼容] `VectorIndexType` 和 `vector_index_type_t` 数值历史遗留 | 新增映射函数集中处理，不改动现有枚举值 |
| [并发] REST Server 当前单线程 accept + 每请求开线程 | 已有 pthread 模式，基本够用；后续可用线程池 |
| [ID 映射] RAG 层 string ID ↔ int64 转换丢失或冲突 | 映射表在 RemoteVectorIndex 生命周期内保持；重启后重新分配 |
| [接口兼容] RemoteVectorIndex 继承的默认空实现可能掩盖错误 | 各方法打 LOG 警告；确保 engine.cpp 调用链在 REMOTE 模式下不会使用 save/load 结果

## Migration Plan

1. **接口层重构**（前置，所有步骤的前提）
   - `rag/bm25_index.h` 抽取 `IBm25Index` 抽象基类
   - `rag/vector_index.h` 中 init/save/load/clear 改为默认空实现
   - `rag/engine.h` BM25Index 成员类型改为 `shared_ptr<IBm25Index>`
2. **基础设施**（枚举归一化 + user_data 扩展）
   - `vector_query.h` 新增 `VECTOR_INDEX_BRUTE_FORCE = 3`
   - `vector_index_selector.h` 新增 `vector_index_type_convert()` 映射函数
   - `RequestContext_s` 新增 `void *user_data` 字段
3. **handlers.c 接 VectorAPI**（改 9 个 handler）
4. **Python SDK 加 IndexType**（改 client.py，增常量类）
5. **REST Server 加 BM25 端点**（增 handler、映射、CMake 依赖）
6. **RAG Server 加 RemoteVectorIndex/B, RAG Engine 支持 REMOTE 模式**
7. **Demo 端到端验证**

无回滚风险：各步骤增量添加，不删除现有代码，LOCAL 模式保留作为 fallback。**

无回滚风险：各步骤增量添加，不删除现有代码，LOCAL 模式保留作为 fallback。

## Open Questions（已解决）

- ~~RAG Server 初始化时需要在 REST Server 自动创建集合，还是预先手动创建？~~ → **自动创建**（见 Design §6.5 RemoteVectorIndex 集合自动初始化 Spec）
- ~~BM25 text-search 响应的格式需要与 SDK/search 对齐还是独立设计？~~ → **独立设计**，含 `action`、`id`、`query`、`top_k` 等字段
- ~~IVF 在 VectorIndexType 中值为 2，在 Python IndexType 中是否应显式包含 IVF_PQ 变体？~~ → **不包含**，IVF 保持 2 不变；IVF_PQ 是 selector 内部实现细节

## Remaining Open Questions

- RemoteVectorIndex 的 ID 映射表需要持久化吗？当前重启后重建，string ID 和 int64 的映射会变
- BM25 text-search 请求体中的 `k1`/`b` 参数是否需要显式校验或使用 BM25Index 的默认值？
