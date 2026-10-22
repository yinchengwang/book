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

## 二十四、NGT 索引内部结构与建图搜索流程

> 本节与 §八 的定位差异：§八 讲「Vald 选了什么算法」，本节讲「NGT 索引在磁盘和内存里长什么样、建图与搜索各调用哪个 CGO 原语、参数默认值是多少」。
> 版本基准：`internal/core/algorithm/ngt/doc.go:15` 声明 Go 包实现的 **Go API for `https://github.com/NGT-labs/NGT`**（注意是 NGT-labs，不是 yahoojapan）。

### 24.1 「图 + 树 + 对象空间」三层结构

NGT 不是纯图索引。Go 侧把索引类型建模为 `indexType` 枚举，只有三个取值（`internal/core/algorithm/ngt/ngt.go:377`）：`IndexTypeNone`、`GraphAndTree`、`Graph`。实际创建时**恒定走 GraphAndTree**：

| 场景 | CGO 原语 | 位置 |
|------|----------|------|
| 落盘模式创建 | `C.ngt_create_graph_and_tree(path, n.prop, ne.err)` | `internal/core/algorithm/ngt/ngt.go:659` |
| 创建后立即落盘 | `C.ngt_save_index` | `internal/core/algorithm/ngt/ngt.go:663` |
| 纯内存模式创建 | `C.ngt_create_graph_and_tree_in_memory` | `internal/core/algorithm/ngt/ngt.go:667` |
| 重新打开索引 | `C.ngt_open_index` | `internal/core/algorithm/ngt/ngt.go:688` |
| 回读索引属性 | `C.ngt_get_property` | `internal/core/algorithm/ngt/ngt.go:693` |
| 取向量本体空间 | `C.ngt_get_object_space` | `internal/core/algorithm/ngt/ngt.go:707` |

三层结构在磁盘上落到**四个文件**。最硬的证据来自仓库自带的测试夹具 —— 目录 `internal/core/algorithm/ngt/assets/index/` 下恰好只有 `grp`、`obj`、`prf`、`tre` 四个文件；agent 的坏索引检测函数注释里也直接写明了这一点（`pkg/agent/core/ngt/service/ngt.go:587`）：

```go
// pkg/agent/core/ngt/service/ngt.go:586
func needsBackup(backupPath string) bool {
	// Initial state where there's only grp, obj, prf, tre -> false
```

| 文件 | 内容 |
|------|------|
| `grp` | 图结构（邻接表） |
| `tre` | 树结构（导航树） |
| `obj` | 向量本体（ObjectSpace） |
| `prf` | 索引属性（纯文本键值对） |

`prf` 是纯文本，仓库夹具 `internal/core/algorithm/ngt/assets/index/prf` 可直接读出一次 6 维 NGT 索引的完整参数：`IndexType	GraphAndTree`（`internal/core/algorithm/ngt/assets/index/prf:11`）、`GraphType	ANNG`（`internal/core/algorithm/ngt/assets/index/prf:9`）、`SeedType	RandomNodes`（`internal/core/algorithm/ngt/assets/index/prf:14`）。

### 24.2 图算法族 `graphType`：只读，无法从 Go 侧设置

`graphType` 枚举（`internal/core/algorithm/ngt/ngt.go:416`）列出 NGT 论文系列的 8 种图构建算法：

| 常量 | 行号 | 含义 |
|------|------|------|
| `ANNG` | `internal/core/algorithm/ngt/ngt.go:417` | Adjacency Navigable Small World Graph，NGT 默认 |
| `KNNG` | `internal/core/algorithm/ngt/ngt.go:418` | k-Navigable Small World Graph |
| `BKNNG` | `internal/core/algorithm/ngt/ngt.go:419` | Bidirectional k-Navigable |
| `ONNG` | `internal/core/algorithm/ngt/ngt.go:420` | Optimized Navigable Small World Graph |
| `IANNG` | `internal/core/algorithm/ngt/ngt.go:421` | Improved ANNG |
| `DNNG` | `internal/core/algorithm/ngt/ngt.go:422` | Dynamic Nearest Neighbor Graph |
| `RANNG` | `internal/core/algorithm/ngt/ngt.go:423` | Relaxed ANNG |
| `RIANNG` | `internal/core/algorithm/ngt/ngt.go:424` | Relaxed Improved ANNG |

⚠️ `graphType` **只能读不能写**：Go 侧没有对应 Option，唯一读取点是 `GetProperty()`（`internal/core/algorithm/ngt/ngt.go:1346`）。算法选择发生在 NGT C++ 库内部，Vald 侧只能观测 —— 两份夹具的实际取值都是 `ANNG`。

同理 `seedType`（入口种子策略，`internal/core/algorithm/ngt/ngt.go:405`：`RandomNodes` / `FixedNodes` / `FirstNode` / `AllLeafNodes`）与 `databaseType`（`internal/core/algorithm/ngt/ngt.go:386`：`Memory` / `MemoryMappedFile`）也都是只读属性。

### 24.3 建图参数全景（`Property` 35 字段）

`Property` 结构体（`internal/core/algorithm/ngt/ngt.go:209`）通过 `C.ngt_get_property_info`（`internal/core/algorithm/ngt/ngt.go:1314`）一次性反读 NGT C++ 侧的完整索引属性，其中与图质量直接相关的字段：

| Go 字段 | 行号 | 作用 |
|---|---|---|
| `EdgeSizeForCreation` | `internal/core/algorithm/ngt/ngt.go:253` | 建图时每节点出边数 |
| `EdgeSizeForSearch` | `internal/core/algorithm/ngt/ngt.go:255` | 搜索时每跳展开的边数 |
| `EdgeSizeLimitForCreation` | `internal/core/algorithm/ngt/ngt.go:257` | 建图出边数上限 |
| `InsertionRadiusCoefficient` | `internal/core/algorithm/ngt/ngt.go:259` | 插入搜索半径系数 |
| `SeedSize` | `internal/core/algorithm/ngt/ngt.go:261` | 种子节点数 |
| `BatchSizeForCreation` | `internal/core/algorithm/ngt/ngt.go:267` | 建图批大小 |
| `DynamicEdgeSizeBase` | `internal/core/algorithm/ngt/ngt.go:271` | 动态出边基数 |
| `DynamicEdgeSizeRate` | `internal/core/algorithm/ngt/ngt.go:273` | 动态出边增长率 |
| `BuildTimeLimit` | `internal/core/algorithm/ngt/ngt.go:275` | 建图时间上限 |
| `OutgoingEdge` / `IncomingEdge` | `internal/core/algorithm/ngt/ngt.go:277` / `:279` | 出边 / 入边总数 |

一组 784 维夹具 `internal/test/data/agent/ngt/validIndex/prf` 给出了真实参数组合：`EdgeSizeForCreation 20`、`EdgeSizeForSearch 10`、`DynamicEdgeSizeBase 30`、`DynamicEdgeSizeRate 20`、`OutgoingEdge 10`、`IncomingEdge 80`。

### 24.4 默认值：Go binding 与生产 Helm 完全不同

Go binding 的包级默认值（`internal/core/algorithm/ngt/option.go:41` 的 `defaultOptions`）：

| 参数 | Go 默认 | 行号 | Helm 生产默认 | 行号 |
|---|---|---|---|---|
| `creation_edge_size` | **10** | `internal/core/algorithm/ngt/option.go:47` | **50** | `charts/vald/values.yaml:2330` |
| `search_edge_size` | **40** | `internal/core/algorithm/ngt/option.go:48` | **50** | `charts/vald/values.yaml:2333` |
| `default_pool_size` | **10000** | `internal/core/algorithm/ngt/option.go:36` | **16** | `charts/vald/values.yaml:2339` |
| `default_epsilon` | **0.1** | `internal/core/algorithm/ngt/option.go:38` | **0.05** | `charts/vald/values.yaml:2345` |
| `epsilon_for_creation` | **0.1** | `internal/core/algorithm/ngt/option.go:52` | **0.05** | `charts/vald/values.yaml:2348` |
| `default_radius` | -1.0 | `internal/core/algorithm/ngt/option.go:37` | -1.0 | `charts/vald/values.yaml:2342` |
| `bulk_insert_chunk_size` | **100** | `internal/core/algorithm/ngt/option.go:51` | **10** | `charts/vald/values.yaml:2316` |
| `distance_type` | `L2` | `internal/core/algorithm/ngt/option.go:50` | `l2` | `charts/vald/values.yaml:2322` |
| `object_type` | `Float` | `internal/core/algorithm/ngt/option.go:49` | `float` | `charts/vald/values.yaml:2327` |

生产的 50/50 明显高于 binding 的 10/40：出边越多图越稠密、召回越高，但内存与建图时间随之线性上升；`search_edge_size` 决定每跳的分支因子。`default_pool_size` 方向相反 —— binding 的 10000 是单次建图的候选池上限，Helm 降到 16 表示生产更倾向「小批量频繁建图」而非「大批量稀疏建图」。

### 24.5 写入与建图的 CGO 边界

关键设计：**`Insert` 只把向量塞进 ObjectSpace，不碰图**。接口注释写得很直白（`internal/core/algorithm/ngt/ngt.go:62`：「This only stores not indexing, you must call CreateIndex and SaveIndex.」）：

| 操作 | CGO 原语 | 位置 |
|------|----------|------|
| `Insert(vec)` | `C.ngt_insert_index_as_float` | `internal/core/algorithm/ngt/ngt.go:897` |
| `BulkInsert(vecs)` | 循环调用 `Insert` | `internal/core/algorithm/ngt/ngt.go:931` |
| `CreateIndex(poolSize)` | `C.ngt_create_index` | `internal/core/algorithm/ngt/ngt.go:1001` |
| `SaveIndex()` | `C.ngt_save_index` | `internal/core/algorithm/ngt/ngt.go:1020` |
| `InsertCommit(v, p)` | `Insert` + `CreateIndex` + `SaveIndex` | `internal/core/algorithm/ngt/ngt.go:911` |

`BulkInsertCommit`（`internal/core/algorithm/ngt/ngt.go:950`）是这条链路的批量化版本：累计到 `n.bulkInsertChunkSize` 条（默认 100）就调一次 `CreateAndSaveIndex`（`internal/core/algorithm/ngt/ngt.go:962`），把「N 次 O(N·E) 建图」摊薄成「N/chunk 次」。

```go
// 简化自 internal/core/algorithm/ngt/ngt.go:950
for _, vec := range vecs {
    id, err = n.Insert(vec)                          // 仅入 ObjectSpace
    idx++
    if idx >= n.bulkInsertChunkSize {
        err = n.CreateAndSaveIndex(poolSize)         // 建图 + 落盘
        idx = 0
    }
}
```

`poolSize == 0` 时回落到实例默认 `n.poolSize`（`internal/core/algorithm/ngt/ngt.go:994`）。

⚠️ **CGO 边界说明**：Vald 没有暴露 NGT C++ 侧的 `createEdges` / `unrollEdge` / `setEntryPoint` / `searchGraph` 这些原语（全仓 grep 零命中）。小世界图的邻接边优化全部封在 `ngt_insert_index_as_float` 与 `ngt_create_index` 内部。想读这部分必须去看 NGT 上游（`NGT-labs/NGT`）的 C++ 源码，Vald 侧只能观测、不能干预。

### 24.6 搜索：八参数签名与读写锁

```go
// internal/core/algorithm/ngt/ngt.go:721
func (n *ngt) Search(
    ctx context.Context, vec []float32, size int, epsilon, radius float32, edgeSize int32,
) (result []algorithm.SearchResult, err error)
```

落到 C 的调用是 8 个位置参数（`internal/core/algorithm/ngt/ngt.go:747`）：

```
C.ngt_search_index_as_float(index, vec, dimension, k, epsilon, radius, results, edgeSize, err)
```

| 参数 | 语义 | 为 0 时的回退 |
|---|---|---|
| `k`（`size`） | 返回近邻个数 | — |
| `epsilon` | 搜索展开系数，越小越精确越慢 | `n.epsilon`（默认 0.1），`internal/core/algorithm/ngt/ngt.go:738` |
| `radius` | 搜索半径 | `n.radius`（默认 -1.0 = 不限），`internal/core/algorithm/ngt/ngt.go:742` |
| `edgeSize` | **per-request** 参数，不走 property，默认 40 | — |

并发模型是标准读写锁：`Search` 持 `rLock`（`internal/core/algorithm/ngt/ngt.go:746`）允许并发搜索，`Insert`（`internal/core/algorithm/ngt/ngt.go:896`）与 `Remove`（`internal/core/algorithm/ngt/ngt.go:1054`）持写锁互斥。传入的 `vec` 由 `pinVec`（`internal/core/algorithm/ngt/ngt.go:43`）用 `runtime.Pinner` 钉住（调用点 `internal/core/algorithm/ngt/ngt.go:728`）防止 Go GC 在 CGO 调用期间搬移底层数组 —— 这是 Go 与 C 共享内存的必备动作。

基线对照是 `LinearSearch`（`internal/core/algorithm/ngt/ngt.go:806`），走 `C.ngt_linear_search_index_as_float`（`internal/core/algorithm/ngt/ngt.go:824`）做暴力扫描，复杂度 O(N·d)，用于回归验证图索引的召回率。

### 24.7 图质量可观测：`GraphStatistics`

`GetGraphStatistics`（`internal/core/algorithm/ngt/ngt.go:1263`）默认传 `cnt=10`，内部调 `C.ngt_get_graph_statistics(n.index, mode, n.ces, ne.err)`（`internal/core/algorithm/ngt/ngt.go:1285`）—— 注意第三个参数复用的是**建图出边数 `n.ces`**，作为统计采样参数。

转换函数 `fromCGraphStatistics`（`internal/core/algorithm/ngt/ngt.go:1199`）搬运 30 个字段。判断「图是否退化」最有用的是 `MeanNumberOfEdgesPerNode`、`MedianIndegree` / `MedianOutdegree`、`VarianceOfIndegree` / `VarianceOfOutdegree`、`NumberOfNodesWithoutEdges`、`NumberOfNodesWithoutIndegree` —— 后两者非零说明存在孤立节点，导航树再强也搜不到它们。

### 24.8 内核选择：NGT ↔ Faiss（**没有 HNSW**）

一个必须纠正的常见误解：**Vald 仓库里根本没有 HNSW**（全仓 `grep -i hnsw` 零命中）。ANN 内核只有两个：

| 内核 | 目录 | 默认 |
|---|---|---|
| NGT | `internal/core/algorithm/ngt/` | ✅ 生产默认（Helm `agent.ngt.*` 配置块） |
| Faiss | `internal/core/algorithm/faiss/` | `IVFPQ`（`internal/core/algorithm/faiss/option.go:99`） |

Faiss 侧的 `methodType` 只有 `IVFPQ` 与 `BinaryIndex`（`internal/core/algorithm/faiss/faiss.go:79`），真正的 factory switch 在 C++ 层（`internal/core/algorithm/faiss/Capi.cpp:48`）：

```cpp
// internal/core/algorithm/faiss/Capi.cpp:46
switch (method_type) {
  case IVFPQ:     return faiss_create_index_ivfpq(d, nlist, m, nbits_per_idx, metric_type);  // :48
  case BINARYIVF: return faiss_create_index_binaryivf(d*8, nlist);                           // :50
}
```

因此 Helm 里 `agent.ngt.*` 与 `agent.faiss.*` 是两套**平行**配置块，**不存在「auto 模式自动在 NGT/HNSW 间切换」这回事** —— 要换内核是换 agent 部署形态，不是改一个开关。

## 二十五、Agent 写入路径与索引生命周期

### 25.1 插入队列的真身：vqueue（时间序双缓冲，无容量控制）

`Insert` 的 gRPC handler 最终落到 `pkg/agent/core/ngt/service/ngt.go:1049`：

```go
return n.vq.PushInsert(uuid, vec, t)   // 只入队即返回，不建图
```

`vqueue` 定义在 `pkg/agent/internal/vqueue/queue.go`，本质是**两个 `sync.Map` + 原子计数器**（`pkg/agent/internal/vqueue/queue.go:47`）：

```go
type vqueue struct {
    il, dl sync.Map[string, *index]   // il = insert 队列，dl = delete 队列
    ic, dc uint64
}
type index struct {
    uuid      string
    vector    []float32
    timestamp int64
}
```

它的语义**不是优先级队列，而是事件时间序的写缓冲**，冲突消解规则是 last-write-wins：

```go
// 简化自 pkg/agent/internal/vqueue/queue.go:75
func (v *vqueue) PushInsert(uuid string, vector []float32, timestamp int64) error {
    if timestamp == 0 { timestamp = time.Now().UnixNano() }        // :79
    dts, ok := v.loadDVQ(uuid)
    if ok && newer(dts, timestamp) { return nil }                   // :83  删除更新则丢弃插入
    oidx, loaded := v.il.LoadOrStore(uuid, &idx)
    if loaded {
        if newer(timestamp, oidx.timestamp) { v.il.Store(uuid, &idx) }  // :93 仅当新数据更新
    } else {
        atomic.AddUint64(&v.ic, 1)                                 // :97
    }
}
```

关键细节：**跨 uuid 的排序推迟到排空时才做**（`pkg/agent/internal/vqueue/queue.go:231`），按时间戳**倒序**（新的先写）：

```go
slices.SortFunc(uii, func(left, right index) int {
    return cmp.Compare(right.timestamp, left.timestamp)
})
```

而且 `RangePopInsert(ctx, now, f)`（`pkg/agent/internal/vqueue/queue.go:207`）只放行 `timestamp <= now` 的条目（`pkg/agent/internal/vqueue/queue.go:215`）—— 即**按事件时间而非到达时间消费**，可容忍乱序到达。`RangePopDelete` 处理完删除后，若删除时间戳更新还会联动弹出对应的 insert（`pkg/agent/internal/vqueue/queue.go:273`）。

⚠️ 配置里的 `vqueue.insert_buffer_pool_size`（`internal/config/ngt.go:100`）与 `delete_buffer_pool_size`（`internal/config/ngt.go:103`）是**死配置**：`vqueue.New()` 在 `pkg/agent/core/ngt/service/ngt.go:231` 以**无参**形式调用，全仓无任何 Go 代码读取这两个字段。所以 vqueue **无界、无背压、无过载拒绝** —— 这是高写入压测时需要留意的一点（内存增长只受上游 RPC 限流约束）。

### 25.2 建图三阶段：delete → insert → CreateIndex

`CreateIndex`（`pkg/agent/core/ngt/service/ngt.go:1288`）把 vqueue 一次性灌进 NGT：

```go
// 简化自 pkg/agent/core/ngt/service/ngt.go:1288
func (n *ngt) CreateIndex(ctx context.Context, poolSize uint32) error {
    ic := n.vq.IVQLen() + n.vq.DVQLen()
    if ic == 0 { return errors.ErrUncommittedIndexNotFound }        // :1304

    wf := atomic.AddUint64(&n.wfci, 1)                              // :1307
    if wf > 1 { return nil }                                        // :1309 并发请求直接忽略（非阻塞拒绝）

    for n.IsIndexing() || n.IsSaving() || n.IsFlushing() {          // :1317 等前一轮收工（100ms 轮询）
        runtime.Gosched(); <-ticker.C
    }
    n.cimu.Lock(); n.indexing.Store(true)                           // :1336
    defer n.indexing.Store(false)

    // 阶段 1：delete —— kvs 与 NGT 双删
    n.vq.RangePopDelete(ctx, now, func(uuid string) bool {         // :1347
        oid := n.kvs.Delete(uuid)
        n.core.Remove(uint(oid))
    })
    // 阶段 2：insert —— 失败重试一次
    n.vq.RangePopInsert(ctx, now, func(uuid string, vec []float32, ts int64) bool {  // :1370
        oid, err = n.core.Insert(vec)
        oid, err = n.core.Insert(vec)                              // :1379 重试
        n.kvs.Set(uuid, uint32(oid), ts)                           // uuid ↔ oid 映射落 KV
    })
    // 阶段 3：建图（真正连边）
    err = n.core.CreateIndex(poolSize)                              // :1407
}
```

`poolSize` 在调用方未指定时回落到 `n.poolSize`，若实例池也未设则取实际插入条数（`pkg/agent/core/ngt/service/ngt.go:1401`）。

**复杂度**：设本次灌入 `m` 条、全库 `N` 条、建图出边 `E`、候选池 `P`。建图本身是 `O(m · log N · E · P)` 量级（每条新向量在已有图上做一次带池的搜索并连 `E` 条边）；阶段 1/2 的 vqueue 排空是 `O(m log m)`（排序主导）。所以 `auto_index_length`（水位）与 `auto_index_duration_limit`（时间）是**吞吐-延迟的直接旋钮**：水位调大 → 单次建图摊薄、但新数据的可搜窗口变长。

### 25.3 四 ticker 驱动的自动建图调度

agent 的自动索引是一个单 goroutine 的 4 ticker `select`（`pkg/agent/core/ngt/service/ngt.go:859`）：

| ticker | 绑定配置 | 生产默认 | 触发动作 | 位置 |
|---|---|---|---|---|
| `tick`（`n.dur`） | `auto_index_check_duration` | 30m（`charts/vald/values.yaml:2298`） | `IVQLen() >= alen` 时 `CreateIndex` | `pkg/agent/core/ngt/service/ngt.go:911` |
| `limit`（`n.lim`） | `auto_index_duration_limit` | 24h（`charts/vald/values.yaml:2295`） | `CreateAndSaveIndex` | `pkg/agent/core/ngt/service/ngt.go:915` |
| `sTick`（`n.sdur`） | `auto_save_index_duration` | 35m（`charts/vald/values.yaml:2304`） | `SaveIndex` | `pkg/agent/core/ngt/service/ngt.go:917` |
| `exportTick` | `export_index_info_duration` | — | 导出索引信息到 K8s CR | `pkg/agent/core/ngt/service/ngt.go:918` |

水位 `alen` = `auto_index_length`，生产默认 100（`charts/vald/values.yaml:2301`）。启动前还有一个可配初始延迟 `initial_delay_max_duration`（默认 3m，`charts/vald/values.yaml:2310`；代码 `pkg/agent/core/ngt/service/ngt.go:881`），让副本先就绪再开始建图。

三个开关值得注意：

- 任一 duration ≤ 0 就被替换为 `math.MaxInt64` 视为禁用（`pkg/agent/core/ngt/service/ngt.go:870`）
- `n.dur == 0 || n.alen == 0` 时置 `n.dcd = true`（`pkg/agent/core/ngt/service/ngt.go:227`），整个自动索引循环不启动
- `is_readreplica` 的 agent 同样禁用（`pkg/agent/core/ngt/service/ngt.go:863`）

### 25.4 两条关机排空路径

数据安全依赖两条独立排空路径，二者都保证「最后一次未建图的数据不丢」：

1. **主循环 ctx 取消**：`case <-ctx.Done(): n.CreateIndex(ctx, n.poolSize)`（`pkg/agent/core/ngt/service/ngt.go:904`）
2. **显式关闭**：`Close(ctx)`（`pkg/agent/core/ngt/service/ngt.go:1941`）→ `CreateIndex`（`pkg/agent/core/ngt/service/ngt.go:1958`）→ `SaveIndex`（`pkg/agent/core/ngt/service/ngt.go:1966`）。只读副本直接跳过（`pkg/agent/core/ngt/service/ngt.go:1954`）

同时，索引未收敛时 `Search` 直接拒绝而不是返回陈旧结果（`pkg/agent/core/ngt/service/ngt.go:936`）：

```go
if n.IsFlushing() { return nil, errors.ErrFlushingIsInProgress }        // :936
if n.IsIndexing() { return nil, errors.ErrCreateIndexingIsInProgress } // :939
```

### 25.5 「reindex」的真正形态：rebuild + Copy-On-Write

Vald 当前版本**没有 `reindex` job**（`Job_JobType` / `JobStatus` / `index.jobTimeout` 全仓零命中）。与「重建索引」对应的是 agent 的 `rebuild()`（`pkg/agent/core/ngt/service/ngt.go:634`），承担了旧 `reindex` 的全部职责：

```go
// pkg/agent/core/ngt/service/ngt.go:634
// path 在 CoW 关闭时等于 n.path，CoW 开启时是临时路径
func (n *ngt) rebuild(ctx context.Context, path string, opts ...core.Option) error {
    if needsBackup(n.path) { n.backupBroken(ctx) }               // 先把坏索引挪走
    for _, f := range file.ListInDir(path) { os.RemoveAll(f) }   // 清空
    n.core, err = core.New(append(opts, core.WithIndexPath(path))...)  // 干净重建
}
```

`initNGT` 在启动时按**四级回退**依次尝试加载，这是「reindex 迁移」的实际形态：

1. 主路径 `n.path` —— 失败则 `rebuild(ctx, n.path, ...)`（`pkg/agent/core/ngt/service/ngt.go:691`）
2. `n.oldPath`（CoW 影子副本）：`n.load(ctx, n.oldPath, opts...)`（`pkg/agent/core/ngt/service/ngt.go:714`）
3. `n.basePath`（非 CoW 向后兼容路径）并 `file.CopyDir` 回迁（`pkg/agent/core/ngt/service/ngt.go:733` 与 `pkg/agent/core/ngt/service/ngt.go:735`）
4. `n.tmpPath` —— 全新空索引（`pkg/agent/core/ngt/service/ngt.go:738`）

坏索引备份目录用纳秒时间戳命名（`name := time.Now().UnixNano()`，`pkg/agent/core/ngt/service/ngt.go:560`），超过 `broken_index_history_limit` 后删最旧。

超时控制不是 `index.jobTimeout`，而是加载超时的三元组（`internal/config/ngt.go:32` / `internal/config/ngt.go:44` / `internal/config/ngt.go:48`）：`min_load_index_timeout` 3m、`max_load_index_timeout` 10m、`load_index_timeout_factor` 1ms（默认值见 `pkg/agent/core/ngt/service/option.go:43`；Helm `charts/vald/values.yaml:2351` / `charts/vald/values.yaml:2354` / `charts/vald/values.yaml:2358`）—— 即**超时 = 待加载索引数 × factor**，再夹在 min/max 之间。

### 25.6 备份快照：sidecar + Bucket 抽象

blob 存储只有 4 个方法的极简接口（`internal/db/storage/blob/blob.go:23`）：

```go
type Bucket interface {
    Open(ctx context.Context) error
    Close() error
    Reader(ctx context.Context, key string) (io.ReadCloser, error)
    Writer(ctx context.Context, key string) (io.WriteCloser, error)
}
```

实现只有两个插件：**S3**（`internal/db/storage/blob/s3/`）与 **CloudStorage**（`internal/db/storage/blob/cloudstorage/`，走 URL opener，覆盖 GCS 等）；类型枚举在 `internal/config/blob.go:20`（`S3` 在 `internal/config/blob.go:24`，`CloudStorage` 在 `internal/config/blob.go:25`）。

sidecar 的备份循环 `startBackupLoop`（`pkg/agent/sidecar/service/observer/observer.go:279`）用容量 1 的 channel 做合并触发（`requestBackup` 满了直接丢弃，`pkg/agent/sidecar/service/observer/observer.go:382`），真正的落盘是 `backup(ctx)`（`pkg/agent/sidecar/service/observer/observer.go:392`），周期默认 10m（`pkg/agent/sidecar/service/observer/option.go:29`）。

**快照格式 = `<pod 名>.tar.gz`**，不是 `.ngt`：文件名来自 `filename`（默认 `_MY_POD_NAME_`，`charts/vald/values.yaml:2597`）+ `filename_suffix`（默认 `.tar.gz`，`charts/vald/values.yaml:2600`），写入时外面再包一层压缩器：

```go
// 简化自 pkg/agent/sidecar/service/storage/storage.go:190
func (b *bs) Writer(ctx context.Context) (io.WriteCloser, error) {
    w, err := b.bucket.Writer(ctx, b.filename+b.suffix)   // :191
    return b.compressor.Writer(w)                          // tar 流再过压缩器
}
```

压缩算法四选一（`internal/config/compress.go:24` 起）：`GOB` / `GZIP` / `LZ4` / `ZSTD`，Helm 默认 `gzip`（`charts/vald/values.yaml:2701`）。

⚠️ 这里容易误解：`internal/compress/` **不是向量量化器**（没有 flat / PQ / ScalarQuantizer），它是通用字节流压缩，只服务于备份 tar 流。真正的向量压缩旋钮见 §二十六 的 `object_type`。

### 25.7 索引维护作业：四张 K8s Job 模板

老版本的进程内 Job 状态机已被替换为 **Kubernetes Job 模板**（`internal/config/index_operator.go:40`）：

```go
type IndexJobTemplates struct {
    Rotate     *k8s.Job `json:"rotate"`      // 只读副本轮转
    Creation   *k8s.Job `json:"creation"`    // 强制建图
    Save       *k8s.Job `json:"save"`        // 强制落盘
    Correction *k8s.Job `json:"correction"`  // 副本一致性纠偏
}
```

其中纠偏作业（corrector）语义最值得注意：它逐 Agent 比对 uuid 的时间戳，**从其它副本拉取更新的版本补齐缺口**，副本过多时回收（`pkg/index/job/correction/service/corrector.go:277` 起）。所以 Vald 的副本一致性是**最终一致 + 周期性纠偏**，而非写入期强同步 —— 这与 §十一 的结论一致，但给出了具体实现路径。

## 二十六、距离度量、类型系统与 Gateway 读写副本分布

### 26.1 13 种距离度量：字符串解析而非 proto 枚举

这是本版本相对旧文档的一个结构性变化：**`DistanceType` / `ObjectType` 已从 proto 枚举降级为字符串**。proto 里只剩（`apis/proto/v1/payload/payload.proto:612`）：

```proto
string distance_type = 4;   // 与 object_type / index_type / seed_type / graph_type 同为 string
```

枚举本体下沉到 Go 侧的 `distanceType`（`internal/core/algorithm/ngt/ngt.go:305`），共 13 种（`internal/core/algorithm/ngt/ngt.go:346` 起）：

| 家族 | 取值 |
|---|---|
| 范数 | `L1`、`L2`、`NormalizedL2` |
| 角度 | `Angle`、`NormalizedAngle`、`Cosine`、`NormalizedCosine` |
| 二值 | `Hamming`、`Jaccard`、`SparseJaccard` |
| 乘积 | `InnerProduct` |
| 双曲 | `Poincare`、`Lorentz` |

字符串别名表在 `WithDistanceTypeByString`（`internal/core/algorithm/ngt/option.go:109`），例如 `cosine|cos` → `Cosine`（`internal/core/algorithm/ngt/option.go:120`）、`dotproduct|dp|innerproduct|ip` → `InnerProduct`（`internal/core/algorithm/ngt/option.go:136`）。⚠️ 没有单独的 `Dot` —— 点积即 `InnerProduct`。Helm 的 CRD 值域有 26 个字符串别名（`internal/k8s/vald/operator/api/valdrelease/values.gen.go:67` 起）。

每个枚举走一个独立的 C setter（`WithDistanceType` 的 13 分支 switch，`internal/core/algorithm/ngt/option.go:143`），例如 `Cosine` → `C.ngt_set_property_distance_type_cosine`（`internal/core/algorithm/ngt/option.go:168`）。

**距离类型是 per-agent 的索引属性，不是 per-request 参数** —— `Search.Config` 里没有距离字段，所以同一集群内每个索引的距离度量各自固定，改度量必须重建索引。

### 26.2 `Cosine` 与 `NormalizedCosine` 是两个不同度量

这是本节最容易踩坑的一点。`Cosine`（`internal/core/algorithm/ngt/option.go:167`）与 `NormalizedCosine`（`internal/core/algorithm/ngt/option.go:202`）映射到**两个不同的 C setter**（`ngt_set_property_distance_type_cosine` vs `..._normalized_cosine`），同理还有 `L2` / `NormalizedL2`（`internal/core/algorithm/ngt/option.go:152` / `internal/core/algorithm/ngt/option.go:192`）与 `Angle` / `NormalizedAngle`。

**Go 侧不存在任何 `NormalizeVector` 之类的归一化函数** —— grep `normalize` 只命中上述常量名。也就是说 Vald **既不替用户归一化，也不做转换**；归一化与否完全交给 NGT C++ 库和用户在写入前自行决定：

- 选 `Cosine` / `NormalizedCosine`：库内每次比较时现算余弦（含除法，慢）
- 选 `InnerProduct` + 自行预归一化：`cos(a,b) = 1 - a·b`，退化为纯点积（快）

**实践含义**：想要「点积速度 + 余弦语义」，必须自己在客户端归一化后再选 `innerproduct`；否则选 `cosine` 会持续付出库内归一化的开销。默认配置是 `l2`（`charts/vald/values.yaml:2322` / `internal/core/algorithm/ngt/option.go:50`）。

### 26.3 `ObjectType`：3 种存储精度

`objectType`（`internal/core/algorithm/ngt/ngt.go:302`）只有三个有效取值：

| 常量 | 行号 | 字符串别名 | 内存/维 |
|---|---|---|---|
| `Uint8` | `internal/core/algorithm/ngt/ngt.go:331` | `uint8` / `ui8` / `u8` | 1 B |
| `Float` | `internal/core/algorithm/ngt/ngt.go:333` | `double` / `float` / `float32` / `f` / `f32` / `fp32` | 4 B |
| `HalfFloat` | `internal/core/algorithm/ngt/ngt.go:335` | `float16` / `halffloat` / `hf` / `f16` / `fp16` | 2 B |

⚠️ 没有 `Bit` 变体。这是 Vald 实现「内存压缩」的主要旋钮：`float` → `float16` 内存直接减半，`uint8` 减到 1/4，代价是召回率。字符串解析在 `WithObjectTypeByString`（`internal/core/algorithm/ngt/option.go:223`），默认 `Float`（`internal/core/algorithm/ngt/option.go:49`；Helm `charts/vald/values.yaml:2327`）。

取回向量时按 `objectType` 分派到不同的 C 函数（分派点在 `internal/core/algorithm/ngt/ngt.go:1083`）：`Float` → `C.ngt_get_object_as_float`、`HalfFloat` → `C.ngt_get_allocated_object_as_float`、`Uint8` → `C.ngt_get_object_as_integer`。

### 26.4 Gateway 的读写不对称：写 N 份、读全量

副本数由 Helm 的 `gateway.lb.gateway_config.index_replica` 给出（默认 **3**，`charts/vald/values.yaml:1311`），一路传到 handler 的 `s.replica` 字段（`pkg/gateway/lb/handler/grpc/handler.go:32`）：

```
charts/vald/values.yaml:1311              index_replica: 3
  → internal/config/gateway.go:38         IndexReplica int
  → pkg/gateway/lb/usecase/vald.go:118    handler.WithReplicationCount(cfg.Gateway.IndexReplica)
  → pkg/gateway/lb/handler/grpc/option.go:91  s.replica = rep
```

`DoMulti`（`pkg/gateway/lb/service/gateway.go:105`）用于写：

```go
limit = min(num, len(addrs))                                  // pkg/gateway/lb/service/gateway.go:119
err = g.client.GetClient().OrderedRange(sctx, addrs, func(...) error {
    if atomic.LoadUint32(&cur) < limit { f(...); cur++ }       // pkg/gateway/lb/service/gateway.go:130
})
```

即**按地址顺序取前 N 个 Agent**（非随机、非最少连接）；成功数不足 `limit` 时第二轮 `OrderedRange` 补齐（`pkg/gateway/lb/service/gateway.go:141`）。`Insert` 走这条路（`pkg/gateway/lb/handler/grpc/insert.go:132`），并把实际落点回填进 `Object_Location.Ips`（`pkg/gateway/lb/handler/grpc/insert.go:117`）。

`BroadCast`（`pkg/gateway/lb/service/gateway.go:69`）用于读，是**全量广播**：

```go
switch kind {
case READ:  client = g.client.GetReadClient()   // pkg/gateway/lb/service/gateway.go:84
case WRITE: client = g.client.GetClient()       // pkg/gateway/lb/service/gateway.go:86
}
return client.RangeConcurrent(fctx, -1, ...)     // pkg/gateway/lb/service/gateway.go:89  -1 = 无并发上限
```

搜索聚合即走 READ 全量广播（`pkg/gateway/lb/handler/grpc/aggregation.go:86`）。**没有任何路由策略枚举** —— 没有 random / roundRobin / leastConnections / weighted 的可配置项；连接级 retry 由 `grpc.Client.RoundRobin` 兜底。

### 26.5 副本数如何放大召回：`calculateNum`

因为读是全量广播，聚合前必须放大每个 Agent 的取回量，否则 N 个副本各只回 k 条、去重后可能凑不满 k。`calculateNum`（`pkg/gateway/lb/handler/grpc/search.go:151`）按副本占比缩放：

```
min = replica / agentCount                                 # pkg/gateway/lb/handler/grpc/search.go:152

ratio <= 0:   n = ceil(num * min)                          # pkg/gateway/lb/handler/grpc/search.go:154
               （无过滤：按副本占比线性放大）

ratio >  0:   n = ceil(num * (min + (1-min)*ratio))         # 有过滤：按过滤强度在 [min, 1] 插值
              sn = ceil(num * min)
              return max(n-1, sn)                          # 且不低于下限
```

聚合器因此按 `num * replica` 预分配缓冲（`newStd`，`pkg/gateway/lb/handler/grpc/aggregation.go:317`，缓冲区 `pkg/gateway/lb/handler/grpc/aggregation.go:321`；另有 `newSlice` 与 `newPoolSlice`），算法选择在 `selectAggregator`（`pkg/gateway/lb/handler/grpc/search.go:207`）。默认走配对堆 `PairingHeap`（`pkg/gateway/lb/handler/grpc/pairing_heap.go:16`）做 top-k，避免每次插入都排序 —— 这是标准的多路归并建堆思路。

**复杂度**：设 `N` 个 Agent、每 Agent 图搜索代价 `T(N/e)`、聚合 top-k 代价 `O(N·k·log(N·k))`。副本数调大同时抬高**读放大**（网络 + 每 Agent 的图搜索）与**写放大**（`DoMulti` 要 `replica` 次 RPC），但换取可用性 —— 单 Agent 挂掉时仍有 `replica-1` 份可读。

### 26.6 读副本轮询与服务发现

只读副本与主副本之间有一处真正的轮询（`internal/client/v1/client/discoverer/discover.go:210`）：

```go
for {
    cur := c.roundRobin.Load()
    next = (cur + 1) % (c.readReplicaReplicas + 1)   // internal/client/v1/client/discoverer/discover.go:219
    if c.roundRobin.CompareAndSwap(cur, next) { break }
}
if next == 0 { return c.client }                     // internal/client/v1/client/discoverer/discover.go:224
return c.readClient                                  // internal/client/v1/client/discoverer/discover.go:227
```

即按 `readReplicaReplicas : 1` 的比例把读流量打到只读副本，无权重、无最少连接。

**服务发现既不是 etcd 也不是 informer**：主路径是调 discoverer 的 gRPC `Nodes` 接口（`internal/client/v1/client/discoverer/discover.go:339`）并按周期重新拉取；失败则回退到 **K8s headless Service 的 DNS A 记录解析**（`dnsDiscovery`，`internal/client/v1/client/discoverer/discover.go:253`）。

### 26.7 健康检查的真相：HTTP 端点不做实际探活

`HealthServerOpts`（`internal/servers/server/option.go:49`）注册的 liveness / readiness 端点（路径由 server 名字小写生成，见 `internal/servers/starter/starter.go:140`）对任何 GET 一律返回 200：

```go
// internal/servers/server/option.go:56
mux.HandleFunc(path, func(w http.ResponseWriter, r *http.Request) {
    if r.Method == http.MethodGet {
        w.WriteHeader(http.StatusOK)
        fmt.Fprint(w, http.StatusText(http.StatusOK))   // 不做任何实际检查
    }
})
```

gRPC 侧同理：`health.Register`（`internal/net/grpc/health/health.go:25`）在注册时把**所有已注册 service 一律标记为 SERVING**，不做 Watch。

所以**真正的健康判定不在探针里** —— K8s liveness 只保证进程活着；索引是否就绪要看 `IndexDetail` 报告的 `stored` / `uncommitted` / `indexing` 三个数（`pkg/agent/core/ngt/handler/grpc/index.go:150`），或等 `Search` 不再返回 `ErrCreateIndexingIsInProgress`。

## 二十七、参考资料

- [Vald GitHub](https://github.com/vdaas/vald)
- [Vald 官方文档](https://vald.vdaas.org/)
- [NGT GitHub](https://github.com/yahoojapan/NGT)
- [NGT 论文](https://arxiv.org/abs/1908.06100)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [Kubernetes CRD](https://kubernetes.io/docs/concepts/extend-kubernetes/api-extension/custom-resources/)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)
- [Buf Schema Registry](https://buf.build/)