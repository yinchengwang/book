"""
网络拓扑设计 — 星型/环形/网状拓扑 + 负载均衡

演示内容:
1. 网络拓扑: 星型/环形/全网状/部分网状
2. 负载均衡: 轮询/加权/最少连接
3. 故障模拟: 单点故障对拓扑的影响
"""

import random
from typing import List

random.seed(42)

# ============================================================================
# 1. 网络拓扑
# ============================================================================

class NetworkNode:
    def __init__(self, name: str):
        self.name = name
        self.connections = []

    def connect(self, node: 'NetworkNode') -> None:
        if node not in self.connections:
            self.connections.append(node)

    def __repr__(self):
        return self.name


class Topology:
    """拓扑基类"""

    def __init__(self, name: str):
        self.name = name
        self.nodes = {}

    def add_node(self, name: str) -> NetworkNode:
        node = NetworkNode(name)
        self.nodes[name] = node
        return node

    def build(self) -> None:
        raise NotImplementedError

    def count_links(self) -> int:
        """计算总连接数（避免重复计数）"""
        seen = set()
        for node in self.nodes.values():
            for conn in node.connections:
                pair = tuple(sorted([node.name, conn.name]))
                seen.add(pair)
        return len(seen)

    def fault_tolerance(self) -> str:
        """评估容错能力"""
        raise NotImplementedError


class StarTopology(Topology):
    """星型拓扑: 中心节点连接所有叶子"""

    def __init__(self, leaf_count: int = 5):
        super().__init__("星型拓扑")
        self.leaf_count = leaf_count

    def build(self) -> None:
        center = self.add_node("中心交换机")
        for i in range(self.leaf_count):
            leaf = self.add_node(f"服务器-{i+1}")
            leaf.connect(center)
            center.connect(leaf)

    def fault_tolerance(self) -> str:
        return "叶子节点故障影响自身；中心节点故障→全网瘫痪（单点故障）"


class RingTopology(Topology):
    """环形拓扑: 每个节点连接前后邻居"""

    def __init__(self, count: int = 5):
        super().__init__("环形拓扑")
        self.count = count

    def build(self) -> None:
        nodes = [self.add_node(f"节点-{i+1}") for i in range(self.count)]
        for i in range(self.count):
            nodes[i].connect(nodes[(i + 1) % self.count])
            nodes[i].connect(nodes[(i - 1 + self.count) % self.count])

    def fault_tolerance(self) -> str:
        return "单节点故障→双环可自愈（反方向绕行）；多节点故障→分段"


class MeshTopology(Topology):
    """全网状拓扑: 所有节点两两相连"""

    def __init__(self, count: int = 5):
        super().__init__("全网状拓扑")
        self.count = count

    def build(self) -> None:
        nodes = [self.add_node(f"节点-{i+1}") for i in range(self.count)]
        for i in range(self.count):
            for j in range(i + 1, self.count):
                nodes[i].connect(nodes[j])
                nodes[j].connect(nodes[i])

    def fault_tolerance(self) -> str:
        return "极高容错：N-2 故障仍可连通，代价是 O(n²) 连接数"


# ============================================================================
# 2. 负载均衡
# ============================================================================

class LoadBalancer:
    """负载均衡器"""

    def __init__(self, servers: List[str]):
        self.servers = servers
        self.weights = {s: 1 for s in servers}
        self.connections = {s: 0 for s in servers}
        self.idx = 0

    def set_weights(self, weights: dict) -> None:
        self.weights.update(weights)

    def round_robin(self) -> str:
        """轮询"""
        s = self.servers[self.idx]
        self.idx = (self.idx + 1) % len(self.servers)
        return s

    def weighted_round_robin(self) -> str:
        """加权轮询"""
        pool = []
        for s in self.servers:
            pool.extend([s] * self.weights.get(s, 1))
        # 简化: 用固定序列
        s = pool[self.idx % len(pool)]
        self.idx += 1
        return s

    def least_connections(self) -> str:
        """最少连接"""
        return min(self.connections, key=self.connections.get)

    def request_handled(self, server: str) -> None:
        self.connections[server] += 1

    def stats(self) -> dict:
        return dict(self.connections)


# ============================================================================
# 演示
# ============================================================================

if __name__ == "__main__":
    print("=" * 56)
    print("      网络拓扑设计演示 — Network Topologies")
    print("=" * 56)

    print("\n--- 1. 网络拓扑对比 ---")
    topologies = [
        StarTopology(5),
        RingTopology(5),
        MeshTopology(5),
    ]
    for topo in topologies:
        topo.build()
        print(f"\n  {topo.name}:")
        print(f"    连接数: {topo.count_links()}")
        print(f"    容错: {topo.fault_tolerance()}")

    print("\n--- 2. 负载均衡策略 ---")
    servers = ["web-a", "web-b", "web-c", "web-d"]
    lb = LoadBalancer(servers)
    lb.set_weights({"web-a": 3, "web-b": 2, "web-c": 1, "web-d": 1})

    print(f"  轮询策略 (10 请求):")
    for i in range(10):
        s = lb.round_robin()
        lb.request_handled(s)
    for s, c in lb.stats().items():
        print(f"    {s}: {c} 请求")

    # 重置
    lb.connections = {s: 0 for s in servers}
    print(f"\n  最少连接策略 (模拟不同负载后):")
    lb.connections["web-a"] = 10
    lb.connections["web-b"] = 5
    lb.connections["web-c"] = 2
    lb.connections["web-d"] = 0
    for i in range(5):
        s = lb.least_connections()
        lb.request_handled(s)
        print(f"    请求 {i+1} → {s}")

    print("\n--- 3. 故障模拟 ---")
    print(f"  星型拓扑中心交换机故障 → 所有服务器下线")
    print(f"  环形拓扑单节点故障 → 数据绕行")
    print(f"  全网状拓扑单节点故障 → 其他节点仍连通")
    print(f"  负载均衡器本身可能成为 SPOF → 需要主备 LB")

    print("\n" + "=" * 56)
    print("提示: 生产环境一般用多级 LB + 混合拓扑")
    print("=" * 56)
