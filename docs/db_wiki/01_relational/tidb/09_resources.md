# TiDB 学习资源

## 学习目标

- 掌握 TiDB 的官方文档和核心论文
- 理解 TiDB 的源码阅读路径
- 规划 TiDB 的学习路线图

## 官方文档

### 官方网站

- **TiDB 官网**：https://pingcap.com
- **TiDB 文档**：https://docs.pingcap.com/tidb/stable
- **TiDB GitHub**：https://github.com/pingcap/tidb

### 文档结构

```
TiDB 文档结构：
├── 概览
│   ├── TiDB 简介
│   ├── 架构设计
│   └── 使用场景
├── 快速上手
│   ├── 本地部署（TiUP）
│   ├── K8s 部署（TiDB Operator）
│   └── 云上部署（TiDB Cloud）
├── 开发指南
│   ├── SQL 语法（MySQL 兼容）
│   ├── 事务模型（Percolator）
│   ├── 索引设计
│   └── 性能调优
├── 运维指南
│   ├── 集群部署
│   ├── 监控告警
│   ├── 备份恢复
│   └── 故障排查
└── 内核原理
    ├── TiDB Server（SQL 层）
    ├── TiKV（存储层）
    ├── PD（调度层）
    └── TiFlash（列存）
```

## 核心论文

### 1. Percolator 事务模型

**论文**：Large-scale Incremental Processing Using Distributed Transactions and Notifications (OSDI 2010)

**关键内容**：

- Percolator 事务模型
- 两阶段提交（Prewrite + Commit）
- Lock + Write + Data 三组件

**阅读建议**：

- 重点阅读 Section 2（Transaction）和 Section 3（Notifications）
- 理解 TiDB 如何实现 Percolator 事务

### 2. TiDB SQL 优化器

**论文**：TiDB: A Raft-based HTAP Database (VLDB 2020)

**关键内容**：

- TiDB 架构设计
- SQL 优化器实现
- HTAP 混合负载

**阅读建议**：

- 重点阅读 Section 3（Architecture）和 Section 4（Query Processing）
- 理解 TiDB 如何实现 HTAP

### 3. TiKV 事务和存储

**论文**：TiKV: A Raft-based KV Database for Transactions (SIGMOD 2020)

**关键内容**：

- TiKV Raft 实现
- MVCC 版本存储
- Region 分片和调度

**阅读建议**：

- 重点阅读 Section 2（System Overview）和 Section 3（Transaction）
- 理解 TiKV 如何实现分布式事务

## 源码阅读路径

### TiDB Server（Go）

```
TiDB Server 源码路径：
├── parser/          # SQL 解析器
│   ├── lexer.go     # 词法分析
│   └── parser.go    # 语法分析
├── planner/         # 查询优化器
│   ├── core/        # 逻辑计划
│   └── rule/        # 优化规则
├── executor/        # 执行器
│   ├── distsql.go   # 分布式执行
│   └── join.go      # Join 实现
└── session/         # 会话管理
    └── txn.go       # 事务管理
```

**阅读顺序**：

1. `parser/parser.go`：理解 SQL 解析流程
2. `planner/core/logical_plan.go`：理解逻辑计划构建
3. `executor/distsql.go`：理解分布式执行
4. `session/txn.go`：理解事务管理

### TiKV（Rust）

```
TiKV 源码路径：
├── src/storage/     # 存储引擎
│   ├── mvcc/        # MVCC 实现
│   ├── txn/         # 事务实现
│   └── engine/      # RocksDB 封装
├── src/raftstore/   # Raft 实现
│   ├── store.rs     # Raft 状态机
│   └── peer.rs      # Raft Peer
├── src/coprocessor/ # Coprocessor
│   └── dag/         # 向量化执行
└── src/server/      # gRPC 服务
    └── service.rs   # TiKV 服务
```

**阅读顺序**：

1. `src/storage/mvcc/reader.rs`：理解 MVCC 读取
2. `src/raftstore/store.rs`：理解 Raft 实现
3. `src/coprocessor/dag/`：理解向量化执行

### PD（Go）

```
PD 源码路径：
├── server/          # PD 服务
│   ├── tso.go       # TSO 授时
│   └── scheduler.go # Region 调度
├── server/schedule/ # 调度策略
│   ├── balance.go   # 负载均衡
│   └── hot_region.go # 热点调度
└── server/api/      # REST API
```

**阅读顺序**：

1. `server/tso.go`：理解 TSO 实现
2. `server/scheduler.go`：理解 Region 调度
3. `server/schedule/balance.go`：理解负载均衡

## 学习路线图

### 第一阶段：基础概念（1 周）

- TiDB 架构设计（TiDB Server + TiKV + PD）
- MySQL 兼容性测试
- 本地集群部署（TiUP）

### 第二阶段：事务和存储（2 周）

- Percolator 事务模型
- Region 分片和 Raft 复制
- TiKV MVCC 实现

### 第三阶段：查询执行（2 周）

- SQL 解析和优化
- 分布式执行（DistSQL）
- HTAP 混合负载

### 第四阶段：运维和调优（1 周）

- 监控告警（Grafana）
- 性能调优
- 故障排查

## 要点总结

- 官方文档：TiDB 文档 + GitHub 仓库
- 核心论文：Percolator、TiDB VLDB 2020、TiKV SIGMOD 2020
- 源码阅读：TiDB Server（Go）、TiKV（Rust）、PD（Go）
- 学习路线：基础概念 → 事务存储 → 查询执行 → 运维调优

## 思考题

1. Percolator 论文中的两阶段提交如何避免分布式事务的阻塞问题？TiDB 如何优化 Percolator 的性能？
2. TiKV 的 Raft 实现相比 etcd 的 Raft 实现，在性能和可靠性上有何差异？
3. TiDB 的 SQL 优化器相比 PostgreSQL 的优化器，在规则化和代价模型上有何差异？