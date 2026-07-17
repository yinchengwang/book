# 向量数据库 Wave 2-⑤：统一 API 层 — 变更提案

## Why

向量数据库需要统一的外部接口层，整合现有的 REST API、C SDK 接口、向量引擎接口，提供一致的调用体验。这是向量数据库作为"产品"而非"库"的关键交付物。

## What Changes

### 代码修改

| 文件 | 变更内容 |
|------|---------|
| `engineering/include/db/vdb_api.h` | 定义统一 API 头文件（整合 vdb_query.h） |
| `engineering/src/db/api/vdb_api.c` | 实现统一 API 层，路由到各引擎 |
| `engineering/src/db/api/rest_api.c` | 扩展 REST API 端点，适配多模查询 |
| `engineering/src/db/api/handlers.c` | 优化 API handlers，统一错误处理 |
| `engineering/test/db/api/vdb_api_test.cpp` | API 层单元测试 |

### 新增文件

| 文件 | 说明 |
|------|------|
| `docs/vdb-api-design.md` | 统一 API 设计文档 |

## Capabilities

- New: `vdb-api` — 统一 API 层能力（C SDK + REST + 内部调用）

## Impact

### 功能影响

- 统一的 C SDK 接口：初始化、插入、查询、删除、关闭
- 统一的 REST API：/vdb/collections、/vdb/query、/vdb/insert
- 统一的错误码和返回格式

### 测试覆盖

- 新增 `vdb_api_test.cpp`：API 调用往返测试、错误处理测试

## Tasks

见 `tasks.md`

## Risk

- 中风险：需要整合多个现有 API（db/sql/rest_api、db/api/vector_api），避免冲突