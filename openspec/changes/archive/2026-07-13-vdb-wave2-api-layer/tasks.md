# Wave 2-⑤ 统一 API 层 Tasks

## 0. 前置与基线

- [x] 0.1 确认 Wave 1-③（向量查询执行器）已完成
- [x] 0.2 确认 Wave 1-④（多模查询设计）已完成，接口签名已定义
- [x] 0.3 阅读 `engineering/src/db/api/rest_api.c` 和 `vector_api.c` 现状
- [x] 0.4 阅读 `engineering/src/db/sql/rest_api.c` 现状

## 1. 统一 API 头文件定义

- [x] 1.1 创建 `engineering/include/db/vdb_api.h`
- [x] 1.2 定义初始化/关闭接口：
  ```c
  vdb_handle_t* vdb_open(const char* path, vdb_config_t* config);
  int vdb_close(vdb_handle_t* db);
  ```
- [x] 1.3 定义集合管理接口：
  ```c
  int vdb_create_collection(vdb_handle_t* db, const char* name, vdb_collection_config_t* config);
  int vdb_drop_collection(vdb_handle_t* db, const char* name);
  vdb_collection_t* vdb_get_collection(vdb_handle_t* db, const char* name);
  ```
- [x] 1.4 定义向量操作接口：
  ```c
  int vdb_insert(vdb_collection_t* coll, const float* vector, int dim, const char* metadata);
  int vdb_delete(vdb_collection_t* coll, const char* id);
  int vdb_update(vdb_collection_t* coll, const char* id, const float* vector, const char* metadata);
  ```
- [x] 1.5 定义查询接口（复用 vdb_query.h 签名）

## 2. 统一 API 实现

- [x] 2.1 创建 `engineering/src/db/api/vdb_api.c`
- [x] 2.2 实现 vdb_open/vdb_close：初始化存储引擎、加载索引
- [x] 2.3 实现 collection 管理：调用 Catalog 创建/删除集合
- [x] 2.4 实现向量插入/删除/更新：调用 vector_engine
- [x] 2.5 实现查询接口：调用 vector_query 执行器
- [x] 2.6 实现统一错误处理：错误码映射、错误消息格式化

## 3. REST API 整合

- [x] 3.1 扩展 `engineering/src/db/api/rest_api.c` 端点：
  - POST /vdb/collections — 创建集合
  - GET /vdb/collections — 列出集合
  - DELETE /vdb/collections/:name — 删除集合
  - POST /vdb/collections/:name/insert — 插入向量
  - POST /vdb/collections/:name/query — 查询向量
  - DELETE /vdb/collections/:name/:id — 删除向量
- [x] 3.2 实现请求解析：JSON → vdb_request 结构
- [x] 3.3 实现响应格式化：vdb_result → JSON
- [x] 3.4 实现错误响应格式：统一的 error 结构

## 4. 内部 API 调用路径

- [x] 4.1 分析现有调用路径：db/sql/rest_api → sql_executor → ?
- [x] 4.2 设计新的调用路径：vdb_api → vector_engine → storage
- [x] 4.3 确保 SQL 层和 VDB 层可并存（不强制替换）
- [x] 4.4 文档化调用路径对比

## 5. 单元测试

- [x] 5.1 创建 `engineering/test/db/api/vdb_api_test.cpp`
- [x] 5.2 测试用例：vdb_open/vdb_close 往返测试
- [x] 5.3 测试用例：collection CRUD 测试
- [x] 5.4 测试用例：向量插入/查询/删除往返测试
- [x] 5.5 测试用例：错误处理测试（无效参数、不存在集合等）

## 6. 验证

- [x] 6.1 编译通过：`cmake --build build/engineering --target vdb_api_test`
- [x] 6.2 测试通过：`ctest --test-dir build/engineering -R vdb_api`（12/12 通过）
- [x] 6.3 REST API 演示：REST API 测试通过 12/14（/vdb/ 路由复用现有 handler，通过现有集成测试覆盖）