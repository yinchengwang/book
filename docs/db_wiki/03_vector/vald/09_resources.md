# Vald 学习资源

## 学习目标

- 获取 Vald 的优质学习资源
- 了解 NGT 算法的原理和应用

## 官方资源

- **官方文档**：[https://vald.vda.ai/docs/](https://vald.vda.ai/docs/)
- **GitHub**：[https://github.com/vdaas/vald](https://github.com/vdaas/vald)
- **Helm Charts**：[https://github.com/vdaas/vald-helm](https://github.com/vdaas/vald-helm)

## 源码研读路径

```mermaid
graph TD
    SRC[Vald 源码] --> L1[internal/gateway: 网关层]
    SRC --> L2[internal/agent: Agent 核心实现]
    SRC --> L3[internal/k8s: K8s Operator]
    SRC --> L4[internal/ngt: NGT 引擎接口]

    L1 --> G1[gRPC 处理]
    L1 --> G2[路由策略]

    L2 --> A1[索引管理]
    L2 --> A2[向量操作]

    L3 --> K1[Controller 实现]
    L3 --> K2[CRD 定义]

    L4 --> N1[NGT API 封装]
    L4 --> N2[索引序列化]
```

## NGT 算法资料

NGT 是 Vald 的核心索引算法：

- **NGT GitHub**：[https://github.com/yahoojapan/NGT](https://github.com/yahoojapan/NGT)
- **论文**："ANNGT: Approximate Nearest Neighbor Search using Graph and Tree"

```mermaid
graph TD
    NGT[NGT 算法] --> ANNGT[ANNGT<br/>Approximate NN Graph Tree]
    NGT --> QG[Query Graph<br/>动态查询优化]

    ANNGT --> TREE[树结构<br/>快速粗筛]
    ANNGT --> GRAPH[邻接图<br/>精搜]

    TREE --> R1[空间划分]
    TREE --> R2[剪枝搜索]

    GRAPH --> R3[贪婪搜索]
    GRAPH --> R4[路径优化]
```

**关键论文**：
1. Yahoo Japan Research, "NGT: Neighborhood Graph Tree for Approximate Nearest Neighbor Search"
2. "On the Effectiveness of Graph-based Algorithms for Approximate Nearest Neighbor Search"

## K8s Operator 开发资源

Vald 的 Operator 是学习 K8s Controller 开发的好案例：

- **Kubebuilder**：[https://book.kubebuilder.io/](https://book.kubebuilder.io/)
- **Operator SDK**：[https://sdk.operatorframework.io/](https://sdk.operatorframework.io/)

```mermaid
graph TD
    OP[K8s Operator 开发] --> CRD1[CRD 定义]
    OP --> CTRL[Controller 逻辑]
    OP --> RBAC[RBAC 权限]
    OP --> WEBHOOK[Admission Webhook]

    CRD1 --> TYPE[类型定义]
    CRD1 --> VALIDATE[验证逻辑]

    CTRL --> RECONCILE[Reconcile 循环]
    CTRL --> EVENT[事件处理]
```

## 学习路径

```mermaid
graph LR
    S1[入门] --> S2[进阶] --> S3[深入] --> S4[生产]

    S1 --> A1[Helm 部署 Vald]
    S1 --> A2[Python SDK 使用]

    S2 --> B1[Operator 模式理解]
    S2 --> B2[NGT 算法原理]
    S2 --> B3[Agent/Gateway 配置]

    S3 --> C1[源码阅读: Operator]
    S3 --> C2[NGT 源码研究]
    S3 --> C3[自定义引擎]

    S4 --> D1[集群部署调优]
    S4 --> D2[备份恢复方案]
    S4 --> D3[监控告警集成]
```

| 阶段 | 内容 | 时间 |
|------|------|------|
| 入门 | Helm 部署 + SDK 使用 | 1-2 天 |
| 进阶 | Operator 模式 + NGT 原理 | 1 周 |
| 深入 | 源码阅读 + 自定义开发 | 2 周 |
| 生产 | 集群调优 + 监控 | 持续 |

## 项目启发

Vald 的设计对项目有诸多启发：

### 1. 自动化运维模式

Vald 的 Operator 模式展示了如何在 K8s 上实现完全自动化的数据库运维。项目中如需支持 K8s 部署，可借鉴其 CRD 设计。

### 2. NGT 索引算法

NGT 的树+图混合索引策略，对项目中向量索引的设计有参考价值。可考虑引入 ANNGT 思想优化检索精度。

### 3. 索引生命周期管理

Vald 的自动索引构建、保存、加载机制，可借鉴到项目的索引管理模块。

## 要点总结

- Vald 官方文档是最佳学习入口
- 源码核心在 internal/agent 和 internal/k8s 目录
- NGT 算法结合树和图的优点，适合高精度检索
- K8s Operator 开发资源丰富，Vald 是优秀学习案例