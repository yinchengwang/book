# Spec: docs-diagrams-distributed

## ADDED Requirements

### Requirement: Phase 9 分布式模块图表文件结构

`docs/diagrams/level1-distributed/` 目录 SHALL 包含以下 Excalidraw 图表文件：

- `L1-001-distributed-overview.excalidraw.json` - 分布式模块全景图
- `L1-002-data-structures.excalidraw.json` - 核心数据结构关系图
- `L1-003-shard-routing-flow.excalidraw.json` - 分片路由流程图
- `L1-004-raft-state-machine.excalidraw.json` - Raft 状态机图
- `L1-005-raft-replication-seq.excalidraw.json` - Raft 日志复制时序图
- `L1-006-raft-election-seq.excalidraw.json` - Raft 领导者选举时序图
- `L1-007-2pc-transaction-flow.excalidraw.json` - 2PC 事务流程图
- `L1-008-2pc-recovery-flow.excalidraw.json` - 2PC 故障恢复流程图
- `L1-009-coordinator-arch.excalidraw.json` - 节点协调器架构图
- `L1-010-coordinator-seq.excalidraw.json` - 节点协调流程时序图

#### Scenario: 目录结构完整性检查

- **WHEN** 用户在 `docs/diagrams/level1-distributed/` 目录下列出所有文件
- **THEN** 应当包含上述十个 `.excalidraw.json` 文件

### Requirement: 分布式模块全景图内容

`L1-001-distributed-overview.excalidraw.json` SHALL 包含以下层次结构：

- 应用层（SQL 查询、跨分片查询、分布式事务）
- 协调层（节点注册发现、全局锁服务、领导者选举、配置管理）
- 高可用层（Raft 共识、日志复制、成员变更、故障检测）
- 事务层（2PC、SAGA、TSO、MVCC）
- 分片层（Hash 分片、Range 分片、一致性 Hash、动态扩缩容）
- 存储层（Buffer Pool、Heap AM、BTree AM、WAL）

#### Scenario: 层次结构完整性

- **WHEN** 用户打开分布式模块全景图
- **THEN** 可以看到从应用层到存储层的完整层次
- **AND** 可以了解各层之间的调用关系

### Requirement: 核心数据结构关系图内容

`L1-002-data-structures.excalidraw.json` SHALL 包含以下数据结构的关系：

- NodeRegistry（节点注册表）与节点元数据的关联
- ShardTopology（分片拓扑）包含 ShardNode、ShardInfo、VirtualNode
- ShardRouter（路由器）与 ShardTopology 的关联
- RaftServer 的状态变量（role、currentTerm、log[]、nextIndex[]、matchIndex[]）
- DistTxn（分布式事务）与 TxnParticipant（参与者）的关系

#### Scenario: 数据结构关系清晰性

- **WHEN** 用户查看核心数据结构关系图
- **THEN** 可以清晰理解各数据结构之间的组成和依赖关系

### Requirement: 分片路由流程图内容

`L1-003-shard-routing-flow.excalidraw.json` SHALL 展示完整的分片路由流程：

- 客户端请求解析分片键
- MurmurHash3 计算 Hash 值
- 定位目标分片和主副本节点
- 写入主副本 + 异步复制到从副本

#### Scenario: Hash 分片路由流程

- **WHEN** 执行 `INSERT WHERE user_id = 'u123'`
- **THEN** 可以看到 MurmurHash3 计算 → 定位分片 → 写入主副本 → 复制到从副本的完整流程

### Requirement: Raft 状态机图内容

`L1-004-raft-state-machine.excalidraw.json` SHALL 包含：

- 三个状态：Follower（跟随者）、Candidate（候选者）、Leader（领导者）
- 状态转换条件：选举超时、心跳超时、赢得选举、收到更高任期
- Leader 特有行为：发送心跳、复制日志、等待多数派确认

#### Scenario: Raft 状态转换正确性

- **WHEN** 用户查看 Raft 状态机图
- **THEN** 可以清晰看到所有状态和转换路径
- **AND** 可以了解每个转换的触发条件

### Requirement: Raft 日志复制时序图内容

`L1-005-raft-replication-seq.excalidraw.json` SHALL 展示 Leader 处理写请求的完整流程：

- 客户端 write 请求
- Leader 创建日志条目并写入本地
- 并行发送 AppendEntries 到所有 Followers
- 收集多数派 ACK
- 提交日志并返回客户端

#### Scenario: 日志复制时序完整性

- **WHEN** 用户查看 Raft 日志复制时序图
- **THEN** 可以清晰看到 Leader 和 Followers 之间的消息交互顺序

### Requirement: Raft 领导者选举时序图内容

`L1-006-raft-election-seq.excalidraw.json` SHALL 展示完整的领导者选举流程：

- 选举超时触发 Candidate
- 发起 RequestVote 请求
- 其他节点投票
- 赢得多数票后成为 Leader
- 发送心跳维持领导地位

#### Scenario: 领导者选举时序完整性

- **WHEN** 用户查看 Raft 领导者选举时序图
- **THEN** 可以看到从选举超时到新 Leader 产生的完整过程

### Requirement: 2PC 事务流程图内容

`L1-007-2pc-transaction-flow.excalidraw.json` SHALL 包含：

- 事务状态机：ACTIVE → PREPARING → PREPARED → COMMITTING → COMMITTED
- Phase 1 Prepare：协调者向所有参与者发送 Prepare，参与者获取锁并投票
- Phase 2 Commit：协调者收集全部 YES 后发送 Commit，参与者提交并释放锁
- 参与者节点（Node-A、Node-B、Node-C）与协调者的交互

#### Scenario: 2PC 两阶段流程完整性

- **WHEN** 用户查看 2PC 事务流程图
- **THEN** 可以清晰看到 Prepare 和 Commit 两个阶段的完整流程

### Requirement: 2PC 故障恢复流程图内容

`L1-008-2pc-recovery-flow.excalidraw.json` SHALL 包含：

- Coordinator 在 Commit 阶段宕机的场景
- 重启后查询活跃 PREPARED 事务
- 询问参与者该事务的最终状态
- 根据多数派决定继续 Commit 还是回滚

#### Scenario: 2PC 故障恢复流程完整性

- **WHEN** 用户查看 2PC 故障恢复流程图
- **THEN** 可以了解协调者宕机后的恢复机制

### Requirement: 节点协调器架构图内容

`L1-009-coordinator-arch.excalidraw.json` SHALL 包含：

- NodeRegistry（节点注册表）结构
- GlobalLockService（全局锁服务）组件
- TwoPCoordinator（两阶段协调器）组件
- 各组件之间的关系

#### Scenario: 协调器架构完整性

- **WHEN** 用户查看节点协调器架构图
- **THEN** 可以理解协调器的整体架构和各组件职责

### Requirement: 节点协调流程时序图内容

`L1-010-coordinator-seq.excalidraw.json` SHALL 包含：

- 节点注册流程（Node-A → Registry）
- 健康检查流程（Coordinator → Heartbeat → Node-A → ACK）
- 全局锁获取/释放流程
- 锁等待队列管理

#### Scenario: 协调流程时序完整性

- **WHEN** 用户查看节点协调流程时序图
- **THEN** 可以看到各种协调操作的完整时序
