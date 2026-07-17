# 调用路径分析

## 现有调用路径

### REST API 路径（Winsock，db/api/rest_api.c）

```
HTTP 请求 → rest_api.c (Winsock 服务器)
         → handlers.c (请求处理器)
         → vector_api.c (VectorAPI 层)
         → 内存数据结构 (VectorCollectionImpl)
         → 磁盘持久化 (collections.meta)
```

### SQL REST API 路径（POSIX，db/sql/rest_api.c）

```
HTTP 请求 → sql/rest_api.c (POSIX 服务器)
         → sql_executor (SQL 执行器)
         → 存储引擎 (catalog / buffer pool / heapam)
```

### 特点

- 两套 REST API 独立运行，互不干扰
- 向量 API 使用内存存储 + 文件持久化，不经过 SQL 层
- SQL 层使用 PG 风格的存储引擎（catalog + buffer pool + heapam）
- 集合存/取通过 `vector_api_save/load` 读写 `collections.meta` 文件

## 新增调用路径（VDB 统一 API）

### C SDK 调用路径

```
用户代码 → vdb_api.h/vdb_api.c (统一 C SDK)
         → vector_api.c (VectorAPI 层)
         → 内存数据结构 (VectorCollectionImpl)
         → 磁盘持久化 (collections.meta)
```

### VDB REST API 路径（/vdb/ 前缀）

```
HTTP 请求 → rest_api.c (Winsock 服务器)
         → handlers.c (复用现有 handler)
         → vdb_api.c (VDB 统一 API 层)
         → vector_api.c (VectorAPI 层)
         → 内存数据结构
```

### 路径对比

| 调用方式 | 入口 | 中间层 | 后端 | 适用场景 |
|---------|------|--------|------|---------|
| SQL REST | `/sql/` | sql_executor → 存储引擎 | catalog/buf/heapam | SQL 查询 |
| 旧 REST | `/collections/` | handlers → vector_api | 内存+文件 | 向量操作 |
| VDB REST | `/vdb/` | handlers → vdb_api → vector_api | 内存+文件 | 统一向量操作 |
| C SDK | `vdb_open()` | vdb_api → vector_api | 内存+文件 | 嵌入式集成 |

## 可并存性保证

- `vdb_api` 层包装了 `VectorAPI`，不修改其内部结构
- 旧 REST 端点（`/collections/`）和新端点（`/vdb/`）共享同一个 `vector_api` 实例
- SQL 层与 VDB 层使用完全不同的存储后端，互不冲突
- 不强制替换任何现有接口，渐进式迁移

## 下一步建议

1. 将 `vdb_api` 作为主入口，逐步弃用 `vector_api` 的直接调用
2. 实现 `vdb_api` 的 WAL 集成（当前 WAL 在 vector_engine 层，vdb_api 层未直接暴露）
3. 考虑统一两套 REST API 的启动方式