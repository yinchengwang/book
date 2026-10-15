# Vald Wiki（Go 实现的分布式向量搜索引擎）

> 参考版本：Vald v1.x（Go + gRPC + Kubernetes 原生），由 Yahoo! JAPAN（KDDI 子公司）开源。**Kubernetes 原生 + 分布式向量检索 + NGT 算法** 是其差异化。本 wiki 重点覆盖向量类型特有章节（NGT 算法、分布式架构、K8s Operator、向量压缩）。

## 一、项目定位

Vald（**V**ector **A**nnotaion **L**ist **D**atabase）是由 **Yahoo! JAPAN / KDDI** 开源的 **Kubernetes 原生分布式向量搜索引擎**，提供 **gRPC API、NGT 高性能索引、向量压缩、自动水平扩展、CRD / Operator**，是当前 K8s 生态下生产级向量检索的代表项目之一。

- **痛点**：
  - 传统向量库（Milvus / Qdrant）部署需要复杂 YAML + 集群管理；
  - 缺乏 Kubernetes-native 的向量数据库（CRD 自动扩缩容）；
  - NGT（Yahoo! JAPAN 自研）算法性能优于通用 HNSW；
  - 需要 **异步索引 + 压缩 + 多租户** 的云原生向量检索；
  - 跨语言 gRPC + 自动服务发现；
- **典型用户**：电商推荐、图像搜索、内容平台、广告 RAG、生物制药、AI 客服、IoT 数据；
- **核心口号**：**"Highly scalable distributed vector search engine"** + **"Kubernetes-native"** + **"Powered by NGT"**。

## 二、发展历史与社区

- **起源**：2019 年由 Yahoo! JAPAN 启动，源自其内部大规模推荐系统需求；
- **里程碑**：
  - 2019-12：v0.1 开源，基础 NGT 索引；
  - 2020-09：v0.5 引入 **异步索引（Indexing）**、**gRPC API**；
  - 2020-12：v1.0 GA，**生产可用**；
  - 2021-06：v1.1 引入 **Operator**、**CRD**；
  - 2021-12：v1.2 引入 **多向量压缩**（PQ、SQ）、**Helm Chart**；
  - 2022-06：v1.5 引入 **Egress 过滤**、**Discoverer**；
  - 2023-06：v1.6 引入 **向量压缩（int8）**、**异步复制**；
  - 2024-08：v1.7+ 持续优化 **NGT 性能**、**K8s 集成**；
- **维护状态**：活跃，Yahoo! JAPAN + 社区投入；
- **社区**：GitHub 1.5k+ star、Slack、Forum、日本 + 国际社区。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**；
- **NGT 算法（Yahoo! JAPAN 自研）**：**Apache License 2.0**；
- **生态关系**：
  - 与 **NGT**（Yahoo! JAPAN 自研 ANN 库，紧耦合）；
  - 与 **Kubernetes** 深度集成（CRD / Operator / Helm）；
  - 与 **LangChain / LlamaIndex** 默认向量后端之一；
  - 与 **Istio / Envoy** 学习推进服务网格。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Yahoo! JAPAN**：电商推荐、商品搜索、广告 RAG；
  - **KDDI**：电信业务；
  - 国内：阿里、字节、网易等试点；
- **行业**：电商搜索与推荐、内容平台、广告 RAG、生物制药、AI 客服；
- **典型场景**：商品搜索、内容 RAG、推荐召回、图像检索、多租户向量检索。

## 五、一句话对比定位

> Vald 是 **"Go + gRPC + NGT + Kubernetes 原生 + 异步索引 + 向量压缩 + Operator + CRD + 多租户"** 的分布式向量搜索引擎；与 **Milvus（Go + C++ + 分布式强）**、**Qdrant（Rust 云原生）**、**Weaviate（Go 模块化）**、**Chroma（Python 嵌入式）**、**Vespa（Java/C++ 平台）**、**Pinecone（商业全托管）**、**FAISS（算法库）**、**USearch（嵌入式 C++）** 构成向量库阵营。Vald 的差异化是 **"Kubernetes 原生 + NGT 高性能 + 异步索引 + 多租户 + Yahoo! JAPAN 工业验证"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层                                     │
│  - Python: vald-client-python                                       │
│  - Go / Java / Node.js / Rust / C++：官方 SDK                      │
│  - gRPC 客户端（任何支持 gRPC 的语言）                             │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ gRPC
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Gateway 层（流量入口）                               │
│  cmd/gateway/                                                        │
│  ├─ gRPC 接收                                                       │
│  ├─ 负载均衡                                                        │
│  ├─ TLS / Auth                                                      │
│  ├─ 限流 / 监控                                                    │
│  └─ 多集群路由                                                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Filter（Egress 过滤层）                              │
│  cmd/discoverer/                                                     │
│  ├─ 数据预处理                                                      │
│  ├─ Payload 过滤                                                    │
│  ├─ 缓存（命中常用查询）                                            │
│  └─ 查询路由                                                        │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Index Manager（索引调度）                            │
│  cmd/index-manager/                                                  │
│  ├─ 索引创建 / 删除                                                │
│  ├─ 索引副本管理                                                    │
│  ├─ 数据迁移                                                        │
│  └─ CRD / Operator 集成                                            │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Agent（向量检索节点，可水平扩展）                    │
│  cmd/agent/                                                          │
│  internal/core/algorithm/ngt/                                       │
│  ├─ NGT 索引（CGO 绑定 Yahoo! JAPAN NGT）                         │
│  ├─ 向量压缩（int8 / PQ）                                          │
│  ├─ 并发检索                                                        │
│  ├─ gRPC API（Search / Insert / Update / Delete）                  │
│  └─ 健康检查 / 监控                                                │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Indexer（异步索引节点）                              │
│  cmd/indexer/                                                        │
│  ├─ 从 MQ / Kafka 消费                                              │
│  ├─ 构建 NGT 索引                                                     │
│  ├─ 持久化到 S3 / MySQL                                              │
│  └─ 触发副本同步                                                    │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  存储层                                               │
│  - Cassandra / ScyllaDB（备份）                                     │
│  - S3 / MinIO / GCS（索引快照）                                    │
│  - Redis（缓存）                                                   │
│  - Kafka / Pulsar（异步 MQ）                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **Kubernetes 原生**：通过 CRD + Operator + Helm 实现自动化；
- **NGT 算法**：Yahoo! JAPAN 自研，性能优于通用 HNSW；
- **异步索引**：写入与索引分离（Indexer 节点），降低写入延迟；
- **多角色**：Gateway / Filter / Index Manager / Agent / Indexer 各司其职；
- **向量压缩**：int8 / PQ 默认开启，降低内存与网络传输；
- **多租户**：通过 Namespace + CRD 隔离；
- **向量压缩通信**：Agent 间传输压缩向量，节省网络带宽。

## 七、存储引擎

### 7.1 向量存储

- **NGT（Neighborhood Graph and Tree）**：Yahoo! JAPAN 自研 ANN 库，核心数据结构；
- **存储**：NGT 自有格式 `.ngt`，通过 CGO 访问；
- **压缩**：int8（1/4）+ PQ（1/8 ~ 1/32）。

### 7.2 元数据

- **Cassandra / ScyllaDB**：存原始向量（备份用）；
- **MySQL**：元数据（CRD 配置等）；
- **S3 / MinIO**：索引快照；
- **Redis**：缓存。

### 7.3 WAL / MQ

- **Kafka / NATS / Pulsar**：异步消息队列，写入路径；
- **MySQL**：备份 WAL。

### 7.4 备份恢复

- 通过 Cassandra 持久化原始向量；
- 通过 S3 持久化索引快照；
- 恢复时从 S3 拉取索引 + Cassandra 拉取最新数据。

## 八、索引机制（**向量类型核心**）

### 8.1 NGT（Neighborhood Graph and Tree）详解

NGT 是 Yahoo! JAPAN 自研的 ANN 算法，比 HNSW 在大规模（亿级）下更高效：

- **核心结构**：
  - **VPT（Vantage Point Tree）**：树形索引，加速近邻搜索；
  - **PANGraph（Path Adjacency Graph）**：图索引，类似 NSW / HNSW；
  - **SNG（Small World Graph）**：图构建算法；
- **参数**：
  - `numOfEdges`：每节点连接数；
  - `searchRadiusCoeff`：搜索范围；
  - `epsilon`：精度参数；
  - `poolSize`：候选池大小；
- **优势**：
  - 高维（>1024）性能优于 HNSW；
  - 内存占用低（每节点连接数可配置，典型 10-20）；
  - 支持在线增量更新；
- **源码**：
  - Yahoo! JAPAN 自研 C++：`https://github.com/yahoojapan/NGT`；
  - Vald 通过 CGO 绑定：`internal/core/algorithm/ngt/`；
  - 操作接口：`internal/core/algorithm/ngt/ngt.go`（Search / Insert / Remove / Save / Load）。

### 8.2 向量压缩

| 压缩 | 压缩比 | 召回率 | 适用 |
|------|--------|--------|------|
| **int8** | 1/4 | 99%+ | 默认 |
| **PQ** | 1/8 ~ 1/32 | 90-95% | 大规模 |
| **无压缩** | 1/1 | 100% | 小数据 / 高精度 |

**实现位置**：`internal/core/algorithm/ngt/`、`internal/core/algorithm/pq/`。

### 8.3 NGT 与 HNSW 对比

| 维度 | NGT | HNSW |
|------|------|------|
| **高维性能** | 优秀 | 中等 |
| **构建速度** | 中等 | 快 |
| **查询延迟** | 优秀 | 优秀 |
| **内存占用** | 较低 | 中等 |
| **增量更新** | 支持 | 支持 |
| **维护者** | Yahoo! JAPAN | hnswlib 社区 |

### 8.4 异步索引（Indexer）

```
数据流：
Client → Gateway → Insert → MQ（Kafka / Pulsar）
   ↓
Indexer（消费 MQ）
   ↓
构建 NGT 索引 + 压缩
   ↓
持久化到 S3 + 分发到 Agent 副本
```

**优势**：
- 写入延迟低（仅入 MQ）；
- 索引构建异步，可并行；
- 故障隔离（Indexer 崩溃不影响查询）。

### 8.5 标量过滤

- **Egress Filter**：`cmd/discoverer/`，在 Agent 之前过滤不符合的向量；
- **Payload 索引**：基于外部存储（Cassandra）；
- **性能影响**：高选择性过滤会显著降低召回率。

## 九、查询/计算/接口层

### 9.1 gRPC API

```protobuf
service Vald {
  rpc Search(SearchRequest) returns (SearchResponse);
  rpc SearchByID(SearchByIDRequest) returns (SearchResponse);
  rpc Insert(InsertRequest) returns (InsertResponse);
  rpc Update(UpdateRequest) returns (UpdateResponse);
  rpc Delete(DeleteRequest) returns (DeleteResponse);
  rpc Upsert(UpsertRequest) returns (UpsertResponse);
}
```

- **proto 定义**：`apis/proto/vald/v1/`；
- **buf**：使用 Buf Schema Registry 管理 proto。

### 9.2 SDK 生态

| 语言 | 包名 |
|------|------|
| Go | `github.com/vdaas/vald-client-go` |
| Python | `vald-client-python` |
| Java | `vald-client-java` |
| Node.js / TS | `@vdaas/vald-client-node` |
| Rust | `vald-client-rust`（社区）|
| C++ | vald 内置 SDK |
| Ruby | `vald-client-ruby`（社区）|

### 9.3 多租户

- **Kubernetes Namespace**：基于 K8s 原生隔离；
- **CRD**：`Vector` CRD 管理向量索引；
- **Agent Pool**：不同租户不同 Agent Pool；
- **RBAC**：K8s 原生 RBAC。

### 9.4 高级查询

- **SearchByID**：基于已有向量 ID 检索邻居；
- **Search by Vector**：基于原始向量检索；
- **Multi-vector**：多个向量字段独立检索 + 融合；
- **Filter**：基于 Payload 预过滤。

## 十、事务与一致性

- **写入**：异步 MQ + 异步索引，最终一致；
- **读一致性**：默认 **最终一致**（索引可能有秒级延迟）；
- **强一致**：通过禁用异步索引实现；
- **崩溃恢复**：MQ 重放 + S3 索引快照恢复。

## 十一、复制与高可用

- **副本同步**：Indexer → Agent 副本同步；
- **K8s 原生**：Deployment + Service 自动管理；
- **Operator**：自动扩缩容；
- **Cassandra 备份**：原始向量多副本；
- **S3 索引备份**：跨区域复制。

## 十二、扩展性

- **水平扩展**：
  - **Agent Pool**：增加 Pod 副本；
  - **Operator**：自动扩缩容（基于 CPU / 内存 / QPS）；
  - **索引分片**：向量 ID 哈希分片到不同 Agent；
- **垂直扩展**：增加 Pod 资源；
- **冷热分层**：
  - 向量压缩（int8 / PQ）；
  - 索引冷存到 S3，热数据留在 Agent。

## 十三、性能与基准

- **NGT 性能**：Yahoo! JAPAN 官方 benchmark 显示 NGT 在亿级向量下 **recall@10 > 99%**, 延迟 < 10ms；
- **ann-benchmarks**：Vald（NGT）在 glove-100、gist-960 上排名优秀；
- **异步索引优势**：写入延迟从秒级降到毫秒级；
- **向量压缩收益**：内存降低 4-32x，召回率损失可控。

## 十四、运维与可观测性

- **Prometheus**：内置 metrics 端点；
- **OpenTelemetry**：tracing 支持；
- **Grafana Dashboard**：官方；
- **Helm Chart**：`charts/vald/`；
- **Operator**：`charts/vald-operator/`；
- **Backup**：Cassandra + S3 + 索引快照。

## 十五、生态与集成

- **Kubernetes**：原生（CRD + Operator）；
- **AI 框架**：LangChain、LlamaIndex 默认后端；
- **gRPC**：多语言 SDK；
- **服务网格**：Istio 集成示例；
- **监控**：Prometheus + Grafana；
- **部署**：Helm + Kustomize。

## 十六、核心论文与算法原理

- **NGT 论文**：Yasui et al., "NGT: Graph and Tree for Billion-scale Approximate Nearest Neighbor Search", 2019；
- **HNSW**：Malkov & Yashunin, 2016；
- **PQ**：Jégou et al., 2011；
- **PANGraph**：Yasui et al., 2019（NGT 子算法）；
- **SNG**：Yasui et al., 2019（NGT 子算法）；
- **CRD / Operator**：Kubernetes 生态；
- **Kafka**：流式消息系统。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | Agent | `cmd/agent/main.go` | 检索节点 |
| P0 | NGT 绑定 | `internal/core/algorithm/ngt/` | 算法核心 |
| P0 | Gateway | `cmd/gateway/main.go` | 流量入口 |
| P1 | Indexer | `cmd/indexer/main.go` | 异步索引 |
| P1 | Index Manager | `cmd/index-manager/main.go` | 索引调度 |
| P1 | Filter | `cmd/discoverer/main.go` | 过滤 |
| P1 | Vector 压缩 | `internal/core/algorithm/pq/` | PQ 压缩 |
| P2 | Operator | `charts/vald-operator/` | K8s Operator |
| P2 | SDK | `apis/` | gRPC API |
| P2 | 存储 | `internal/core/storage/` | Cassandra / S3 |

## 十八、源码规模与开发语言

- **Go**：约 90% LOC（核心 + SDK）；
- **C++**：约 5% LOC（NGT 算法）；
- **YAML / Helm**：约 3%（部署配置）；
- **Proto**：约 2%（API 定义）；
- **构建系统**：Go modules + Bazel（NGT）；
- **CI**：GitHub Actions；
- **LOC 总量**：约 30 万行（含测试）。

## 十九、学习资源

- **官方文档**：https://vald.vdaas.org/
- **GitHub**：https://github.com/vdaas/vald
- **博客**：https://vald.vdaas.org/blog/
- **Slack**：社区入口；
- **NGT 文档**：https://github.com/yahoojapan/NGT
- **KubeCon 演讲**：Kubernetes-native 向量检索；
- **日本社区活动**：Yahoo! JAPAN Meetup。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Kubernetes | 原生（推荐）|
| Docker Compose | 开发模式 |
| 本地单节点 | 实验 |

## 二十一、变体与衍生项目

- **NGT**：Yahoo! JAPAN 自研 ANN 库（独立项目）；
- **Vald Operator**：K8s Operator（独立）；
- **Helm Charts**：官方支持；
- **多语言 SDK**：Go / Python / Java / Node.js / C++。

## 二十二、风险与陷阱

1. **Kubernetes 强依赖**：非 K8s 部署支持薄弱；
2. **NGT 学习成本**：相比 HNSW 算法，NGT 资料较少；
3. **异步索引延迟**：写入到查询可见默认有 1 秒延迟，强一致需禁用异步；
4. **Cassandra 运维**：Cassandra 集群运维复杂；
5. **NGT CGO 调试**：崩溃调试依赖 NGT 调试符号；
6. **gRPC 调试**：二进制协议，调试需要工具支持；
7. **CRD 学习曲线**：K8s CRD / Operator 需要 K8s 知识；
8. **多语言 SDK 成熟度**：Java / Node.js SDK 较新；
10. **Egress Filter 召回影响**：高选择性过滤会降低召回率。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 已实现 HNSW，可参考 NGT 的图算法改进；
- **`engineering/rag/`** 中 RAG 系统可对接 Vald 作为 Kubernetes-native 后端；
- **学习价值**：K8s-native 设计 + 异步索引 + NGT 算法；
- **MiniVecDB**：参考 Vald 的 Agent + Indexer 分离设计；
- **`learning/notes/`** 可沉淀 K8s Operator + CRD 设计经验。

## 二十四、参考资料

- [Vald GitHub](https://github.com/vdaas/vald)
- [Vald 官方文档](https://vald.vdaas.org/)
- [NGT GitHub](https://github.com/yahoojapan/NGT)
- [NGT 论文](https://arxiv.org/abs/1908.06100)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [Kubernetes CRD](https://kubernetes.io/docs/concepts/extend-kubernetes/api-extension/custom-resources/)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)
- [Buf Schema Registry](https://buf.build/)