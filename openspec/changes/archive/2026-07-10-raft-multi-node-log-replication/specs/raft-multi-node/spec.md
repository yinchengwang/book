# P1 Spec —— Raft Multi-Node + Log Replication

## 1. Transport 契约

`raft_transport.h` 定义 `RaftMessage_t` 联合体：
- `vote_request` / `vote_response`
- `append_request` / `append_response`

`RaftTransport_t` 含 `send` 回调 + `user` context。

## 2. Cluster 契约

- `raft_cluster_create(size)` 创建 N 节点（in-process）
- `raft_cluster_tick(cl, delta_ms)` 推进所有节点时间并投递消息
- `raft_cluster_get_node(cl, id)` 拿节点
- `raft_cluster_count_leaders(cl)` 返回当前 leader 数（应当 ≤ 1）

## 3. 投票契约

**RequestVote RPC 接受条件**：
1. term ≥ current_term（否则 reject）
2. voted_for == candidate_id 或 voted_for == NO_VOTE
3. 候选者日志至少与自己一样新（last_log_term > my_last_term || 索引 ≥ my_last_idx）

## 4. AppendEntries 契约

接受：term ≥ current_term 且 prev_log_index/term 匹配
拒绝：term < current_term 或 prev_log 不匹配
update：commit_index = min(leader_commit, log.size)

## 5. 不做

- ❌ 持久化
- ❌ 网络传输
- ❌ snapshot
- ❌ PreVote
