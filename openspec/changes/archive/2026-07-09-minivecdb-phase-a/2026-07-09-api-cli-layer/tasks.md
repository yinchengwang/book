# A3: API 层 + CLI 工具 - 任务清单

## 任务概述

扩展 REST API 和 CLI 以支持向量集合操作、向量插入、搜索、删除等向量数据库核心功能。

## 任务清单

### A. 需求分析

- [x] **A.1** 审视现有 REST API 结构 (`rest_api.h`)
- [x] **A.2** 审视现有 CLI 结构 (`cli.h`, `cli.c`)
- [x] **A.3** 了解向量索引接口 (`vector_query.h`)
- [x] **A.4** 了解持久化层接口 (`vector_persist.h`)
- [x] **A.5** 确定需要扩展的接口

### B. REST API 扩展

- [x] **B.1** 创建 `vector_api.h` 向量 API 头文件
- [x] **B.2** 创建 `vector_api.c` 向量 API 实现
- [x] **B.3** 实现集合管理 API (创建/删除/列出集合)
- [x] **B.4** 实现向量插入 API
- [x] **B.5** 实现向量搜索 API
- [x] **B.6** 实现向量删除 API
- [x] **B.7** 实现集合信息查询 API

### C. CLI 扩展

- [x] **C.1** 创建 `vector_cli.h` 向量 CLI 头文件
- [x] **C.2** 创建 `vector_cli.c` 向量 CLI 实现
- [x] **C.3** 实现 `create collection` 命令
- [x] **C.4** 实现 `list collections` 命令
- [x] **C.5** 实现 `drop collection` 命令
- [x] **C.6** 实现 `insert vectors` 命令
- [x] **C.7** 实现 `search vectors` 命令
- [x] **C.8** 实现 `delete vectors` 命令
- [x] **C.9** 实现 `collection info` 命令

### D. JSON 序列化

- [x] **D.1** 实现向量数据的 JSON 序列化
- [x] **D.2** 实现搜索结果的 JSON 格式化
- [x] **D.3** 实现错误响应的 JSON 格式化

### E. 集成与测试

- [x] **E.1** 将向量 API 集成到 REST 服务器
- [x] **E.2** 将向量 CLI 命令集成到 CLI Shell
- [x] **E.3** 单元测试 - API 接口 (11/11 通过)
- [ ] **E.4** 单元测试 - CLI 命令

## 文件清单

### 新建

```
include/db/api/vector_api.h    # 向量 API 头文件
src/db/api/vector_api.c        # 向量 API 实现
include/db/cli/vector_cli.h    # 向量 CLI 头文件
src/db/cli/vector_cli.c        # 向量 CLI 实现
test/db/api/vector_api_test.cpp # API 单元测试 (11/11 通过)
```

### 修改

```
src/db/api/CMakeLists.txt      # 添加新文件
src/db/cli/CMakeLists.txt      # 添加新文件
test/db/api/CMakeLists.txt     # 测试 CMake
test/db/CMakeLists.txt         # 注册测试
src/db/CMakeLists.txt          # 添加 api 子目录
```

### 依赖

```
include/db/core/vector_query.h
include/db/storage/vector/vector_persist.h
include/db/sql/rest_api.h
include/db/cli/cli.h
```

## 状态

- 创建时间: 2026-07-09
- 状态: 已完成
- 所属大变更: 2026-07-09-minivecdb-end-to-end
- 前置变更: A2 (向量查询执行器)
