# A3: API 层 + CLI 工具

## 概述

实现统一的 API 网关和 CLI 工具，让用户可以访问 MiniVecDB 系统。

## 问题背景

当前系统缺乏用户访问入口：
- 没有 gRPC 服务定义
- CLI 缺乏向量操作命令
- 缺乏客户端 SDK

## 目标

- gRPC API 服务端
- CLI 向量命令
- 客户端 SDK 基础
- 向量操作完整覆盖

## 任务清单

### Proto 定义

- [ ] A3.1 设计 `vector_service.proto` 接口
- [ ] A3.2 定义 Collection 服务
- [ ] A3.3 定义 Vector 服务
- [ ] A3.4 定义 Search 服务
- [ ] A3.5 定义 Admin 服务

### gRPC 服务端

- [ ] A3.6 实现 Collection 服务
- [ ] A3.7 实现 Vector 服务
- [ ] A3.8 实现 Search 服务
- [ ] A3.9 实现 Admin 服务
- [ ] A3.10 实现服务端拦截器 (日志/错误处理)

### CLI 扩展

- [ ] A3.11 扩展 CLI 向量命令
- [ ] A3.12 实现 create_collection 命令
- [ ] A3.13 实现 drop_collection 命令
- [ ] A3.14 实现 insert_vectors 命令
- [ ] A3.15 实现 search_vectors 命令
- [ ] A3.16 实现 delete_vectors 命令
- [ ] A3.17 实现 show_collection 命令

### 客户端 SDK 基础

- [ ] A3.18 实现 C 客户端基础
- [ ] A3.19 实现连接管理
- [ ] A3.20 实现请求封装
- [ ] A3.21 实现响应解析

### 对接

- [ ] A3.22 对接 vector_exec (A2)
- [ ] A3.23 对接 mm_storage
- [ ] A3.24 对接 catalog

### 测试

- [ ] A3.25 gRPC 服务单元测试
- [ ] A3.26 CLI 功能测试
- [ ] A3.27 端到端 API 测试

## 关键文件

### 新建

```
proto/vector_service.proto       # gRPC 接口定义
src/db/api/grpc_server.c        # gRPC 服务端
src/db/api/collection_service.c  # Collection 服务实现
src/db/api/vector_service.c      # Vector 服务实现
src/db/api/search_service.c      # Search 服务实现
include/db/api/client.h          # C 客户端头文件
src/db/api/client.c              # C 客户端实现
test/db/api/                     # API 测试
```

### Proto 定义示例

```protobuf
service VectorService {
  // Collection 操作
  rpc CreateCollection(CreateCollectionRequest) returns (CreateCollectionResponse);
  rpc DropCollection(DropCollectionRequest) returns (DropCollectionResponse);
  rpc ListCollections(ListCollectionsRequest) returns (ListCollectionsResponse);
  rpc DescribeCollection(DescribeCollectionRequest) returns (DescribeCollectionResponse);

  // 向量操作
  rpc InsertVectors(InsertVectorsRequest) returns (InsertVectorsResponse);
  rpc DeleteVectors(DeleteVectorsRequest) returns (DeleteVectorsResponse);

  // 搜索
  rpc SearchVectors(SearchVectorsRequest) returns (SearchVectorsResponse);
  rpc QueryVectors(QueryVectorsRequest) returns (QueryVectorsResponse);
}
```

### CLI 命令示例

```
# Collection 操作
minivecdb> create_collection my_vectors --dimension 128 --metric l2
minivecdb> drop_collection my_vectors
minivecdb> show_collections

# 向量操作
minivecdb> insert my_vectors --file vectors.bin
minivecdb> search my_vectors --query [0.1,0.2,...] --top_k 10
minivecdb> delete my_vectors --ids 1,2,3
```

## 依赖关系

- 前置: A2 (向量执行器)
- 后置: A4 (集成测试)

## 评估标准

- [ ] gRPC 服务可启动并响应请求
- [ ] CLI 可执行所有向量操作
- [ ] Collection CRUD 功能完整
- [ ] 搜索延迟 < 100ms (端到端)
