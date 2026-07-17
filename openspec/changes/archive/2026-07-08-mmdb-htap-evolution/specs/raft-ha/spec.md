# 规格：raft-ha

## ADDED Requirements

### Requirement: Raft 共识算法

系统 SHALL 实现 Raft 共识算法。

#### Scenario: Leader 选举
- **WHEN** 集群启动或 Leader 故障
- **WHEN** 节点 SHALL 成为 Candidate
- **THEN** 请求其他节点投票
- **THEN** 获得多数票的节点 SHALL 成为 Leader

#### Scenario: 任期（Term）管理
- **WHEN** 节点通信
- **WHEN** 任期 SHALL 被交换和比较
- **THEN** 更高任期的节点 SHALL 更新自己的任期
- **THEN** 过期 Leader SHALL 被拒绝

#### Scenario: 心跳机制
- **WHEN** Leader 正常运行
- **WHEN** 定期发送心跳到 Follower
- **THEN** 保持 Leader 地位
- **THEN** 防止新的选举

---

### Requirement: 日志复制

系统 SHALL 实现 Raft 日志复制。

#### Scenario: 日志条目追加
- **WHEN** Leader 收到客户端请求
- **WHEN** 日志条目 SHALL 被追加到本地日志
- **THEN** 条目 SHALL 包含任期号和命令

#### Scenario: 日志复制到 Follower
- **WHEN** Leader 追加日志后
- **WHEN** 并行复制到所有 Follower
- **THEN** Follower SHALL 追加日志
- **THEN** 返回复制成功

#### Scenario: 日志提交
- **WHEN** 日志被复制到多数节点
- **WHEN** Leader SHALL 提交日志
- **THEN** 提交状态 SHALL 被应用到状态机
- **THEN** 后续日志 SHALL 基于此日志

---

### Requirement: 成员变更

系统 SHALL 支持 Raft 成员变更。

#### Scenario: 新节点加入
- **WHEN** 新节点请求加入集群
- **WHEN** 使用 Joint Consensus
- **THEN** 配置变更 SHALL 被逐步应用
- **THEN** 集群 SHALL 保持可用

#### Scenario: 节点移除
- **WHEN** 节点被移除
- **WHEN** 使用 Joint Consensus
- **THEN** 配置变更 SHALL 被逐步应用
- **THEN** 被移除节点 SHALL 停止服务

---

### Requirement: 故障检测与切换

系统 SHALL 实现故障检测和自动切换。

#### Scenario: Follower 故障检测
- **WHEN** Follower 故障
- **WHEN** 心跳超时
- **THEN** Leader SHALL 标记该节点为不可用
- **THEN** 集群 SHALL 继续服务

#### Scenario: Leader 故障检测
- **WHEN** Leader 故障
- **WHEN** 心跳超时
- **THEN** 新选举 SHALL 被触发
- **THEN** 新 Leader SHALL 被选出

#### Scenario: 自动切换
- **WHEN** Leader 故障
- **WHEN** 新 Leader 被选出
- **THEN** VIP/域名 SHALL 自动漂移
- **THEN** 客户端 SHALL 自动重连

---

### Requirement: 线性一致性读

系统 SHALL 支持线性一致性读取。

#### Scenario: ReadIndex 读取
- **WHEN** 客户端发起只读请求
- **WHEN** Leader SHALL 记录提交索引
- **THEN** 等待日志应用到该索引
- **THEN** 读取 SHALL 返回最新数据

#### Scenario: LeaseRead
- **WHEN** 使用 LeaseRead 优化
- **WHEN** Lease 有效期内
- **THEN** 直接读取本地状态
- **THEN** 减少网络开销

---

### Requirement: 快照与恢复

系统 SHALL 实现快照和恢复。

#### Scenario: 快照创建
- **WHEN** 日志增长到阈值
- **WHEN** Leader SHALL 创建快照
- **THEN** 状态机状态 SHALL 被快照
- **THEN** 日志 SHALL 被压缩

#### Scenario: 快照复制
- **WHEN** Follower 落后太多
- **WHEN** Leader SHALL 发送快照
- **THEN** Follower SHALL 安装快照
- **THEN** 然后继续复制日志
