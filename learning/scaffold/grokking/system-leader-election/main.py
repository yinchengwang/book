"""
领导者选举 — Paxos/Raft 共识 + 租约机制 + 脑裂防护

演示内容:
1. Raft: 领导者选举 + 日志复制
2. Paxos: Prepare/Accept 两阶段提交
3. 脑裂防护: 多数派原则 + 租约机制
"""

import time
from typing import Optional

# ============================================================================
# 1. Raft 领导者选举
# ============================================================================

class RaftNode:
    """Raft 节点模拟"""

    FOLLOWER = 0
    CANDIDATE = 1
    LEADER = 2

    def __init__(self, node_id: int, all_nodes: list):
        self.node_id = node_id
        self.all_nodes = all_nodes
        self.state = RaftNode.FOLLOWER
        self.current_term = 0
        self.voted_for = None
        self.votes_received = 0
        self.log = []
        self.commit_index = 0
        self.leader_id = None

    def start_election(self) -> str:
        """发起选举"""
        self.state = RaftNode.CANDIDATE
        self.current_term += 1
        self.voted_for = self.node_id
        self.votes_received = 1
        return f"节点 {self.node_id} 发起选举, term={self.current_term}"

    def request_vote(self, candidate_id: int, term: int) -> bool:
        """请求投票"""
        if term < self.current_term:
            return False
        if term > self.current_term:
            self.current_term = term
            self.state = RaftNode.FOLLOWER
            self.voted_for = None
        if self.voted_for is None or self.voted_for == candidate_id:
            self.voted_for = candidate_id
            return True
        return False

    def grant_vote(self, to: int) -> None:
        """投票给候选者"""
        self.voted_for = to

    def become_leader(self) -> str:
        """成为领导者"""
        self.state = RaftNode.LEADER
        self.leader_id = self.node_id
        return f"节点 {self.node_id} 成为领导者, term={self.current_term}"

    def append_entries(self, entry: str) -> bool:
        """领导者追加日志（简化）"""
        if self.state != RaftNode.LEADER:
            return False
        self.log.append({"term": self.current_term, "entry": entry})
        return True

    def __repr__(self):
        states = {0: "FOLLOWER", 1: "CANDIDATE", 2: "LEADER"}
        return (f"Node {self.node_id} [{states[self.state]}] "
                f"term={self.current_term}")


class RaftCluster:
    """Raft 集群模拟"""

    def __init__(self, node_count: int = 5):
        self.nodes = [RaftNode(i, list(range(node_count)))
                      for i in range(node_count)]

    def hold_election(self) -> RaftNode:
        """模拟选举过程"""
        nodes = self.nodes
        candidate = nodes[0]  # 简化: 节点 0 发起选举

        print(f"  {candidate.start_election()}")

        # 其他节点投票
        votes = 1  # 自己的一票
        for node in nodes[1:]:
            if node.request_vote(candidate.node_id, candidate.current_term):
                votes += 1
                print(f"  节点 {node.node_id} 投票给 {candidate.node_id}")

        # 检查多数派
        majority = len(nodes) // 2 + 1
        if votes >= majority:
            print(f"  {candidate.become_leader()}")
            print(f"  获得 {votes}/{len(nodes)} 票 (多数={majority})")
            return candidate
        else:
            print(f"  选举失败: {votes}/{len(nodes)} 票 (多数={majority})")
            return None

    def simulate_network_partition(self) -> None:
        """模拟网络分区导致脑裂"""
        print("\n  --- 网络分区模拟 ---")
        # 假设 5 节点分成了 2 组: {0,1} 和 {2,3,4}
        print(f"  分区: {[0,1]} 和 {[2,3,4]}")
        print(f"  分区1 (2 节点) 少于多数 (3) → 无法选举")
        print(f"  分区2 (3 节点) 达到多数 → 可正常选举")
        print(f"  结论: 多数派原则天然防止脑裂")


# ============================================================================
# 2. Paxos 两阶段提交
# ============================================================================

class PaxosAcceptor:
    """Paxos 接受者"""

    def __init__(self, node_id: int):
        self.node_id = node_id
        self.promised_n = 0
        self.accepted_n = 0
        self.accepted_value = None

    def prepare(self, proposal_n: int) -> dict:
        """Prepare 阶段"""
        if proposal_n > self.promised_n:
            self.promised_n = proposal_n
            return {
                "ok": True,
                "promised_n": proposal_n,
                "previous_accepted_n": self.accepted_n,
                "previous_accepted_value": self.accepted_value,
            }
        return {"ok": False, "promised_n": self.promised_n}

    def accept(self, proposal_n: int, value: str) -> dict:
        """Accept 阶段"""
        if proposal_n >= self.promised_n:
            self.promised_n = proposal_n
            self.accepted_n = proposal_n
            self.accepted_value = value
            return {"ok": True, "accepted_n": proposal_n}
        return {"ok": False}


class PaxosDemo:
    """Paxos 演示"""

    def __init__(self, acceptor_count: int = 3):
        self.acceptors = [PaxosAcceptor(i) for i in range(acceptor_count)]

    def run_round(self, value: str) -> dict:
        """运行一轮 Paxos"""
        proposal_n = int(time.time() * 1000) % 100000

        # Phase 1: Prepare
        print(f"  Phase 1 (Prepare): n={proposal_n}")
        promises = []
        for acc in self.acceptors:
            result = acc.prepare(proposal_n)
            if result["ok"]:
                print(f"    接受者 {acc.node_id}: 承诺 n={proposal_n}")
                promises.append(result)
            else:
                print(f"    接受者 {acc.node_id}: 拒绝 (已承诺 n={result['promised_n']})")

        if len(promises) < len(self.acceptors) // 2 + 1:
            return {"status": "failed", "reason": "未达到多数派"}

        # 如果有已接受的值，选择最大的 proposal
        chosen_value = value
        max_n = 0
        for p in promises:
            if p["previous_accepted_n"] > max_n:
                max_n = p["previous_accepted_n"]
                chosen_value = p["previous_accepted_value"] or value

        # Phase 2: Accept
        print(f"  Phase 2 (Accept): n={proposal_n}, value={chosen_value}")
        accepts = 0
        for acc in self.acceptors:
            result = acc.accept(proposal_n, chosen_value)
            if result["ok"]:
                print(f"    接受者 {acc.node_id}: 接受 value={chosen_value}")
                accepts += 1

        return {
            "status": "success" if accepts >= len(self.acceptors) // 2 + 1 else "failed",
            "value": chosen_value,
            "accepts": accepts,
        }


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("    领导者选举演示 — Consensus Algorithms")
    print("=" * 56)

    print("\n--- 1. Raft 选举 ---")
    cluster = RaftCluster(5)
    leader = cluster.hold_election()
    if leader:
        leader.append_entries("SET x=42")
        print(f"  日志已复制: {leader.log}")

    print("\n--- 2. Raft 脑裂防护 ---")
    cluster.simulate_network_partition()

    print("\n--- 3. Paxos 共识 ---")
    paxos = PaxosDemo(3)
    result = paxos.run_round("transaction-1001: pay 100$")
    print(f"  Paxos 结果: {result}")

    # 第二轮：如果有新提案，必须接受之前的决议
    print(f"\n  --- 第二轮提案（模拟冲突） ---")
    result2 = paxos.run_round("transaction-1001: pay 200$")
    print(f"  Paxos 结果: {result2}")

    print("\n--- 4. 租约机制 ---")
    print(f"  领导者续租: 每 500ms 续租一次")
    print(f"  租约到期: 1000ms 无续租则触发新选举")
    print(f"  租约机制确保: 同一时刻最多只有一个有效领导者")

    print("\n" + "=" * 56)
    print("提示: Raft 比 Paxos 更易理解，是工程首选")
    print("=" * 56)
