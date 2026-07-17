# Phase11 Spec —— Engineering Raft

## 1. 公共 ABI 契约

`engineering/include/db/consensus/raft.h`：

- `RaftRole` 枚举（3 值）
- `RaftServerConfig` 结构（含 node_id / cluster_size / 计时参数）
- `RaftServer` 不透明类型（forward decl）
- 8 个公开函数（create/destroy/start/stop/get_role/get_term/get_leader/is_leader/tick/submit/get_committed）

## 2. 实现约束

- 内部使用 pthread_mutex 做并发保护
- 最小可工作：tick 推进选举、submit 接受 leader 日志
- 集群通信**不实现**——单节点 smoke 测试足够

## 3. ctest 契约

3 个 smoke：
- `CreateAndDestroy`
- `InitialRoleFollower`
- `TickTriggersElection`

## 4. CMake 集成

- `db/consensus/CMakeLists.txt`：声明 `raft_consensus` 静态库
- `engineering/src/db/CMakeLists.txt`：add_subdirectory(consensus)
- `engineering/test/CMakeLists.txt`：add_subdirectory(db/consensus)

## 5. 不做（明确范围外）

- ❌ 集群网络通信
- ❌ Log 持久化
- ❌ Joint Consensus / Snapshot 完整功能
- ❌ PreVote
- ❌ 学习层副本（phase10+）
