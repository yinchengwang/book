# Vald 整体架构

## 学习目标

- 理解 Vald 的 Kubernetes 原生架构设计
- 掌握各组件职责和数据流转
- 了解 NGT 引擎的集成方式

## 核心概念

- **Kubernetes 原生**：以 CRD（Custom Resource Definition）和 Operator 为核心的自动化运维
- **分布式架构**：Gateway、Agent、Index Manager 三层分离
- **NGT 引擎**：Neighborhood Graph Tree，高精度近似最近邻搜索算法
- **自动运维**：索引构建、备份、恢复全部自动化

## 架构总览

```mermaid
graph TB
    subgraph "客户端层"
        CLIENT[gRPC 客户端<br/>Vald SDK]
    end

    subgraph "网关层"
        GATEWAY[Vald Gateway<br/>请求路由/负载均衡]
    end

    subgraph "协调层"
        IMGR[Index Manager<br/>索引生命周期管理]
        BMGR[Backup Manager<br/>自动备份恢复]
    end

    subgraph "Agent 层"
        AGENT1[Vald Agent 01<br/>NGT 引擎]
        AGENT2[Vald Agent 02<br/>NGT 引擎]
        AGENT3[Vald Agent N<br/>NGT 引擎]
    end

    subgraph "Kubernetes 层"
        K8S[Kubernetes 集群]
        CRD[Vald CRD<br/>自定义资源定义]
        OPERATOR[Vald Operator<br/>控制器]
    end

    CLIENT --> GATEWAY
    GATEWAY --> AGENT1
    GATEWAY --> AGENT2
    GATEWAY --> AGENT3

    IMGR --> AGENT1
    IMGR --> AGENT2
    IMGR --> AGENT3

    OPERATOR --> CRD
    CRD --> IMGR
    CRD --> BMGR

    K8S --> OPERATOR
```

## Kubernetes 原生架构

```mermaid
graph LR
    subgraph "Vald Operator 模式"
        A1[管理员创建 Vald CRD] --> A2[Operator 监听 CRD 变化]
        A2 --> A3[自动创建 Deployment/Service]
        A3 --> A4[Agent/Gateway 组件就绪]
        A4 --> A5[索引自动构建]
    end
```

| 组件 | Kubernetes 资源 | 说明 |
|------|----------------|------|
| Gateway | Deployment + Service | 无状态，可水平扩展 |
| Agent | StatefulSet | 有状态，持久化索引数据 |
| Index Manager | Deployment | 索引生命周期管理 |
| Backup Manager | CronJob | 定时备份任务 |

## 组件详解

### Gateway

```mermaid
graph TD
    GW[Vald Gateway] --> ROUTE[请求路由]
    GW --> LB[负载均衡]
    GW --> AUTH[鉴权（可选）]

    ROUTE --> AGENTS[Agent 集群]
    LB --> AGENTS
```

- **请求路由**：将查询分发到正确的 Agent
- **负载均衡**：Round-robin 或一致性哈希
- **协议转换**：gRPC 接口处理

### Agent

```mermaid
graph TD
    AGENT[Vald Agent] --> NGT[NGT 索引引擎]
    AGENT --> STORAGE[本地存储<br/>PVC 持久化]
    AGENT --> REGISTRY[服务注册<br/>Kubernetes Endpoints]

    NGT --> SEARCH[向量搜索]
    NGT --> INSERT[向量插入]
```

- **NGT 引擎**：核心向量搜索能力
- **本地存储**：索引数据持久化到 PVC
- **服务注册**：自动注册到 Kubernetes Endpoints

### Index Manager

```mermaid
graph LR
    IM[Index Manager] --> CREATE[自动创建索引]
    IM --> SAVE[索引保存]
    IM --> LOAD[索引加载]
    IM --> REBALANCE[数据再平衡]
```

- **索引生命周期**：创建 → 更新 → 删除
- **数据再平衡**：Agent 扩缩容时自动迁移数据

## 写入流程

```mermaid
sequenceDiagram
    participant C as Client
    participant GW as Gateway
    participant IM as Index Manager
    participant A as Agent
    participant K8S as Kubernetes

    C->>GW: Insert 向量请求
    GW->>GW: 路由到目标 Agent
    GW->>A: 转发插入请求
    A->>A: 写入 NGT 索引
    A-->>GW: 确认成功
    GW-->>C: 返回结果

    A->>IM: 异步通知索引更新
    IM->>K8S: 更新 CRD 状态
```

## 查询流程

```mermaid
sequenceDiagram
    participant C as Client
    participant GW as Gateway
    participant A1 as Agent 1
    participant A2 as Agent 2
    participant AN as Agent N

    C->>GW: Search 向量查询
    GW->>A1: 并行查询
    GW->>A2: 并行查询
    GW->>AN: 并行查询

    A1-->>GW: 局部 Top-K
    A2-->>GW: 局部 Top-K
    AN-->>GW: 局部 Top-K

    GW->>GW: 合并全局 Top-K
    GW-->>C: 返回最终结果
```

## 自动索引构建

```mermaid
graph TD
    START[索引构建触发] --> CHECK{数据量检查}
    CHECK -->|达到阈值| BUILD[NGT 索引构建]
    CHECK -->|未达阈值| WAIT[继续等待]
    WAIT --> CHECK

    BUILD --> SAVE[保存到 PVC]
    SAVE --> NOTIFY[通知 Index Manager]
    NOTIFY --> READY[索引就绪]
```

## 要点总结

- Vald 是 Kubernetes 原生的向量检索系统，以 Operator 模式实现自动化运维
- Gateway 无状态可扩展，Agent 有状态存储索引数据
- NGT 引擎提供高精度近似最近邻搜索
- 索引生命周期由 Index Manager 自动管理

## 思考题

1. Vald 的 Gateway 无状态设计有什么优势？如何保证 Agent 故障时的数据安全？
2. NGT 索引构建时，Vald 如何保证服务的可用性（写入继续进行）？
3. 如果要实现跨集群的 Vald 部署，需要考虑哪些架构调整？