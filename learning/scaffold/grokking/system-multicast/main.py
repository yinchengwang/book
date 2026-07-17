"""
Gossip 多播 — 流言协议 + 反熵 + 最终一致性

演示内容:
1. Gossip 协议: 传播模型（反熵/Push/Pull）
2. 收敛性: 传播轮数与节点数的关系
3. 最终一致性: 版本向量 + 冲突解决
"""

import math
import random
from typing import Set

random.seed(42)

# ============================================================================
# 1. Gossip 传播模型
# ============================================================================

class GossipNode:
    """Gossip 协议节点"""

    def __init__(self, node_id: int):
        self.node_id = node_id
        self.messages = set()
        self.peers = []
        self.rounds_to_receive = None

    def add_peer(self, peer: 'GossipNode') -> None:
        if peer not in self.peers and peer != self:
            self.peers.append(peer)

    def receive(self, msg: str, round_num: int) -> None:
        if msg not in self.messages:
            self.messages.add(msg)
            if self.rounds_to_receive is None:
                self.rounds_to_receive = round_num

    def gossip(self, msg: str, fanout: int = 3) -> list:
        """向 fanout 个随机对等点传播消息"""
        if not self.peers:
            return []
        recipients = random.sample(
            self.peers, min(fanout, len(self.peers)))
        return [(r, msg) for r in recipients]


class GossipSimulator:
    """Gossip 传播模拟"""

    def __init__(self, node_count: int = 100):
        self.nodes = [GossipNode(i) for i in range(node_count)]
        # 每个节点随机连接 5-15 个对等点（不超过总节点数）
        max_peers = max(5, min(15, node_count - 1))
        for node in self.nodes:
            others = [n for n in self.nodes if n != node]
            peer_count = min(random.randint(5, max_peers), len(others))
            peers = random.sample(others, peer_count)
            for p in peers:
                node.add_peer(p)

    def simulate(self, fanout: int = 3, max_rounds: int = 20) -> dict:
        """模拟消息传播"""
        # 节点 0 初始感染
        source = self.nodes[0]
        msg = "hello-gossip"
        source.receive(msg, 0)

        for r in range(1, max_rounds + 1):
            # 本轮传播: 每个已感染的节点向 fanout 个对等点传播
            infected = [n for n in self.nodes if msg in n.messages]
            deliveries = []
            for node in infected:
                deliveries.extend(node.gossip(msg, fanout))

            # 传递消息
            for target, m in deliveries:
                target.receive(m, r)

            # 统计
            infected_count = sum(1 for n in self.nodes if msg in n.messages)
            if infected_count == len(self.nodes):
                return {
                    "rounds": r,
                    "infected": infected_count,
                    "total": len(self.nodes),
                    "convergence": True,
                }

        infected_count = sum(1 for n in self.nodes if msg in n.messages)
        return {
            "rounds": max_rounds,
            "infected": infected_count,
            "total": len(self.nodes),
            "convergence": infected_count == len(self.nodes),
        }


# ============================================================================
# 2. 最终一致性 — 版本向量
# ============================================================================

class VersionVector:
    """版本向量（检测冲突）"""

    def __init__(self, node_id: int, total_nodes: int):
        self.node_id = node_id
        self.vector = [0] * total_nodes

    def increment(self) -> None:
        self.vector[self.node_id] += 1

    def merge(self, other: 'VersionVector') -> bool:
        """合并版本向量，返回是否有冲突"""
        has_conflict = False
        for i in range(len(self.vector)):
            if other.vector[i] > self.vector[i]:
                self.vector[i] = other.vector[i]
            elif other.vector[i] != self.vector[i]:
                # 我大 别人小 或反之，但已经处理过大的
                pass
        # 检测冲突: 互相有对方没有的更新
        self_greater = any(self.vector[i] > other.vector[i]
                          for i in range(len(self.vector)))
        other_greater = any(other.vector[i] > self.vector[i]
                           for i in range(len(self.vector)))
        return self_greater and other_greater

    def __repr__(self):
        return str(self.vector)


class CRDTData:
    """基于版本向量的最终一致数据"""

    def __init__(self, node_id: int, total_nodes: int):
        self.node_id = node_id
        self.vv = VersionVector(node_id, total_nodes)
        self.data = {}
        self.conflicts = 0

    def update(self, key: str, value) -> None:
        self.vv.increment()
        self.data[key] = (value, list(self.vv.vector))

    def merge(self, other: 'CRDTData') -> int:
        """合并另一节点的数据，返回冲突数"""
        conflict = self.vv.merge(other.vv)
        # 简单合并: 后写入覆盖（Last-Write-Wins）
        for key, (val, _) in other.data.items():
            if key not in self.data:
                self.data[key] = (val, [])
        return 1 if conflict else 0


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("     Gossip 多播演示 — Gossip Protocol")
    print("=" * 56)

    print("\n--- 1. Gossip 传播收敛 ---")
    for total in [10, 50, 100, 500]:
        gs = GossipSimulator(total)
        result = gs.simulate(fanout=3)
        theoretical = math.ceil(math.log(total) / math.log(3 + 1))
        print(f"  节点数={total:>3}: 收敛需要 {result['rounds']} 轮"
              f" (理论 ~{theoretical} 轮)")

    print("\n--- 2. Gossip 传播模型对比 ---")
    models = {
        "Push": "已感染节点主动推送 → 新节点",
        "Pull": "未感染节点主动拉取 → 已感染节点",
        "Push-Pull": "双方交换摘要 → 差异同步（最高效）",
        "Anti-Entropy": "定期全量比对修复所有差异",
    }
    for model, desc in models.items():
        print(f"  {model:<14} {desc}")

    print("\n--- 3. 最终一致性：版本向量 ---")
    # 3 节点，模拟分区后合并
    nodes_data = [CRDTData(i, 3) for i in range(3)]

    # 节点 0 离线时，节点 1 和 2 各自修改
    nodes_data[1].update("key_a", "value_from_1")
    nodes_data[2].update("key_a", "value_from_2")
    print(f"  节点 1 数据: {nodes_data[1].data}")
    print(f"  节点 2 数据: {nodes_data[2].data}")

    # 合并
    conflicts = nodes_data[1].merge(nodes_data[2])
    print(f"  合并后数据: {nodes_data[1].data}")
    print(f"  版本向量: {nodes_data[1].vv}")
    print(f"  冲突数: {conflicts}")
    print(f"  提示: 实际系统需 CRDT 或自定义冲突解决策略")

    print("\n--- 4. 反熵修复 ---")
    print(f"  场景: 3 个副本中各有一个未同步的写入")
    print(f"  Anti-Entropy 定期比对 Merkle Tree")
    print(f"  发现差异后按范围拉取缺失数据")
    print(f"  修复后 3 副本数据完全一致")

    print("\n--- 5. Gossip vs 其他协议 ---")
    print(f"{'属性':<16} {'Gossip':<20} {'Paxos/Raft':<20}")
    print("-" * 56)
    print(f"{'一致性模型':<16} {'最终一致性':<20} {'强一致性':<20}")
    print(f"{'收敛速度':<16} {'O(log N) 轮':<20} {'O(N) 消息':<20}")
    print(f"{{'容错性':<16}} {'任意节点故障可自愈':<20} {'多数派存活':<20}")
    print(f"{'适用场景':<16} {'成员管理/监控':<20} {'状态机复制':<20}")

    print("\n" + "=" * 56)
    print("提示: Gossip 是大规模分布式系统的基础通信模式")
    print("=" * 56)
