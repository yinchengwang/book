# Appwrite 架构设计

## 学习目标

- 理解 Appwrite 的整体架构分层
- 掌握各服务之间的通信机制
- 了解权限系统的设计思路

## 整体架构

```mermaid
graph TD
    subgraph "客户端层"
        WEB["Web SDK"]
        MOBILE["Mobile SDK<br/>Flutter/React Native"]
        SERVER["Server SDK"]
        CLI["CLI 工具"]
    end

    subgraph "API 网关层"
        GW["API Gateway"]
        AUTH_API["Auth API"]
        DB_API["Database API"]
        STORAGE_API["Storage API"]
        FUNC_API["Functions API"]
    end

    subgraph "核心服务层"
        AUTH_S["Auth Service"]
        DB_S["Database Service"]
        STORAGE_S["Storage Service"]
        FUNC_S["Functions Service"]
        TEAMS_S["Teams Service"]
        REALTIME_S["Realtime Service"]
    end

    subgraph "消息与事件"
        QUEUE["Task Queue"]
        EVENTS["Event Bus"]
        WEBHOOK["Webhook Handler"]
    end

    subgraph "存储层"
        PG["PostgreSQL"]
        REDIS["Redis<br/>Sessions/Cache"]
        S3["S3 Compatible<br/>File Storage"]
    end

    WEB --> GW
    MOBILE --> GW
    SERVER --> GW
    CLI --> GW
    GW --> AUTH_API
    GW --> DB_API
    GW --> STORAGE_API
    GW --> FUNC_API
    AUTH_API --> AUTH_S
    DB_API --> DB_S
    STORAGE_API --> STORAGE_S
    FUNC_API --> FUNC_S
    AUTH_S --> PG
    AUTH_S --> REDIS
    DB_S --> PG
    STORAGE_S --> S3
    FUNC_S --> EVENTS
    EVENTS --> WEBHOOK
    EVENTS --> QUEUE
```

## 各层职责

### 客户端层

- **Web SDK**：JavaScript/TypeScript 前端 SDK
- **Mobile SDK**：Flutter、React Native、iOS、Android SDK
- **Server SDK**：Node.js、Python、Ruby 等服务端 SDK
- **CLI 工具**：命令行管理工具

### API 网关层

- **API Gateway**：统一入口，请求路由，限流
- **各领域 API**：Auth、Database、Storage、Functions 独立 API

### 核心服务层

- **Auth Service**：用户认证与授权
- **Database Service**：文档数据库 CRUD
- **Storage Service**：文件存储与管理
- **Functions Service**：Serverless 函数执行
- **Teams Service**：团队与权限管理
- **Realtime Service**：WebSocket 实时订阅

### 消息与事件

- **Task Queue**：异步任务队列
- **Event Bus**：服务间事件通信
- **Webhook Handler**：外部事件通知

## 服务间通信流程

```mermaid
sequenceDiagram
    participant C as 客户端
    participant GW as API Gateway
    participant AUTH as Auth Service
    participant DB as Database Service
    participant PG as PostgreSQL
    participant REDIS as Redis

    C->>GW: 请求（带 JWT Token）
    GW->>AUTH: 验证 Token
    AUTH->>REDIS: 查询 Session
    REDIS-->>AUTH: Session 信息
    AUTH-->>GW: 验证通过
    GW->>DB: 业务请求
    DB->>PG: SQL 查询
    PG-->>DB: 结果集
    DB-->>GW: 业务响应
    GW-->>C: JSON 响应
```

## 权限系统设计

```mermaid
graph TD
    subgraph "权限模型"
        USER["User 用户"]
        TEAM["Team 团队"]
        ROLE["Role 角色"]
    end

    subgraph "权限粒度"
        PROJECT["Project 项目级"]
        COLLECTION["Collection 集合级"]
        DOCUMENT["Document 文档级"]
        FIELD["Field 字段级"]
    end

    USER --> TEAM
    TEAM --> ROLE
    ROLE --> PROJECT
    ROLE --> COLLECTION
    ROLE --> DOCUMENT
    ROLE --> FIELD

    subgraph "内置角色"
        ADMIN["admin"]
        DEVELOPER["developer"]
        VIEWER["viewer"]
        GUEST["guest"]
    end
```

## 要点总结

- **微服务架构**：各服务独立部署，通过事件和队列通信
- **API Gateway 统一入口**：认证、路由、限流集中处理
- **三级权限模型**：Team → Role → Resource 细粒度控制

## 思考题

1. Appwrite 如何保证各微服务之间的事务一致性？
2. 权限系统如何支持自定义角色？