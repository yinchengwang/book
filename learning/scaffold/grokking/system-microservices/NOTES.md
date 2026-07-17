# 微服务架构 学习笔记

## 核心概念

- **服务拆分**: 按业务领域（DDD - Domain Driven Design）拆分
- **API 网关**: 统一入口，负责路由/认证/限流/聚合
- **服务发现**: 注册中心维护服务实例列表，支持健康检查
- **服务网格 (Service Mesh)**: 将通信逻辑下沉到 Sidecar 代理

## 微服务关键模式

| 模式 | 说明 | 工具/实现 |
|------|------|-----------|
| 服务发现 | 实例注册与健康检查 | Consul/Eureka |
| API 网关 | 统一入口与路由 | Kong/APISIX |
| 配置中心 | 运行时配置管理 | Apollo/Nacos |
| 熔断降级 | 防止级联故障 | Hystrix/Sentinel |
| 分布式追踪 | 全链路监控 | Jaeger/Zipkin |

## 工程对照

`engineering/` 轨中的模块划分本质上就是微服务架构的"单体库"形式——`db/storage`（存储引擎）、`db/catalog`（元数据）、`db/lock`（锁管理）、`db/buf`（缓存管理）各自是独立的库，通过清晰的头文件接口和 CMake 依赖管理进行协作。`engineering/CMakeLists.txt` 中的库依赖关系（`index → algo → project_includes`）对应微服务间的 API 依赖关系——每个库有明确的职责边界和接口契约。`engineering/apps/web/knowledge_hub/` 作为独立的前端应用（Taro 3.6 + React 18 + Vite 5），通过 API 调用后端服务，这是典型的前后端分离模式——前端通过网关（`/api/` 前缀）调用后端微服务。`engineering/rag/` 中的 RAG 系统作为一个独立的子系统，通过 SDK 接口与主数据库交互，体现了微服务架构中的"独立部署"原则。

## 面试要点

1. 微服务不是银弹——分布式复杂性显著增加
2. 前期建议：从单体起步，按需拆分
3. 服务间通信首选同步 REST/gRPC + 异步 MQ 混合
