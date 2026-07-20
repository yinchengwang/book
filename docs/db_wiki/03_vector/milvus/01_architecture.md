# Milvus 整体架构

## 学习目标

- 理解 Milvus 的云原生分层架构
- 掌握各组件职责和数据流转

## 核心概念

- **计算存储分离**：查询/索引/数据节点独立扩缩容
- **日志中心**：Pulsar 作为变更日志中心
- **元数据存储**：etcd 存储集群元数据
- **对象存储**：MinIO/S3 存储数据文件和索引
- **Segment**：数据管理单元，类似 LSM-Tree 的 SSTable

## 架构总览

```mermaid
graph TB
    subgraph "接入层"
        LB[负载均衡] --> PROXY[Proxy 代理<br/>请求路由/鉴权]
    end

    subgraph "协调层"
        RC[RootCoord<br/>集群管理/DDL]
        DC[DataCoord<br/>数据分配/Segment 管理]
        QC[QueryCoord<br/>查询调度/负载均衡]
        IC[IndexCoord<br/>索引任务管理]
    end

    subgraph "执行层"
        DN[DataNode<br/>数据持久化转发]
        QN[QueryNode<br/>向量搜索执行]
        IN[IndexNode<br/>索引构建]
    end

    subgraph "存储层"
        ETCD[etcd<br/>元数据存储]
        PULSAR[Pulsar<br/>日志中心]
        OS[S3/MinIO<br/>对象存储]
    end

    PROXY --> RC
    PROXY --> DC
    PROXY --> QC
    RC --> ETCD
    DC --> PULSAR
    DC --> DN
    QC --> QN
    IC --> IN
    DN --> OS
    QN --> OS
    IN --> OS
```

## 数据写入流程

```mermaid
sequenceDiagram
    participant C as Client
    participant P as Proxy
    participant DC as DataCoord
    participant DN as DataNode
    participant OS as ObjectStore
    participant PULSAR as Pulsar

    C->>P: Insert 请求
    P->>PULSAR: 写入日志
    P-->>C: 确认写入成功
    PULSAR->>DN: 消费日志
    DN->>OS: 构建 Segment(列存) 写入
    DN->>DC: 报告 Segment 完成
    DC->>DC: 记录 Segment 元数据
```

## 查询流程

```mermaid
sequenceDiagram
    participant C as Client
    participant P as Proxy
    participant QC as QueryCoord
    participant QN as QueryNode
    participant OS as ObjectStore

    C->>P: Search 请求
    P->>QC: 查询路由
    QC->>QN: 分发查询到各节点
    QN->>OS: 加载索引（如未缓存）
    QN->>QN: 向量搜索 + 标量过滤
    QN-->>P: 返回局部 Top-K
    P->>P: 合并全局 Top-K
    P-->>C: 返回结果
```

## Segment 管理

```mermaid
graph LR
    subgraph "Segment 状态机"
        S1[Growing<br/>可写入] -->|flush| S2[Sealed<br/>只读]
        S2 -->|索引构建| S3[Indexed<br/>已建索引]
        S3 -->|合并| S4[合并后的大 Segment]
    end

    S1 --> S5[写入 Buffer]
    S5 --> S6[Row-Based 写入<br/>行存优先]
    S6 --> S7[转换为 Column-Based<br/>列存落盘]
```

## 要点总结

- Milvus 采用四层架构：接入层 → 协调层 → 执行层 → 存储层
- Pulsar 作为日志中心，解耦写入和索引构建
- 数据以 Segment 为单位管理，支持冷热分层
- 各组件可独立扩缩容，实现资源隔离

## 思考题

1. Pulsar 在 Milvus 中扮演什么角色？如果替换为 Kafka 会有什么影响？
2. QueryNode 如何管理内存中的索引？当索引超过内存大小时怎么办？
3. Segment 的 Growing → Sealed → Indexed 状态转换由谁触发？