"""
cap_theorem — CAP 定理演示

演示一致性（Consistency）/ 可用性（Availability）/ 分区容错（Partition Tolerance）。
通过模拟网络分区场景展示 CAP 取舍。
"""

import threading
import time
import random
from typing import Dict, List, Optional, Callable
from enum import Enum


class NodeStatus(Enum):
    UP = "UP"
    PARTITIONED = "PARTITIONED"
    DOWN = "DOWN"


class DBNode:
    """数据库节点模拟"""

    def __init__(self, node_id: str):
        self.node_id = node_id
        self.data: Dict[str, str] = {}
        self.status = NodeStatus.UP
        self.connected_nodes: List['DBNode'] = []
        self.read_repair = False

    def write(self, key: str, value: str) -> bool:
        if self.status != NodeStatus.UP:
            return False
        self.data[key] = value

        # 复制到其他节点
        for node in self.connected_nodes:
            if node.status == NodeStatus.UP:
                node.data[key] = value
        return True

    def read(self, key: str) -> Optional[str]:
        if self.status != NodeStatus.UP and not self.read_repair:
            return None  # CP 模式：分区时不可用
        return self.data.get(key)


class SystemCoordinator:
    """模拟系统在不同 CAP 策略下的行为"""

    def __init__(self, nodes: List[DBNode], cap_strategy: str):
        """
        cap_strategy: "CP" = 放弃可用性, "AP" = 放弃一致性, "CA" = 放弃分区容忍
        """
        self.nodes = nodes
        self.cap_strategy = cap_strategy
        for n in nodes:
            n.connected_nodes = [x for x in nodes if x != n]
        self.last_written: Dict[str, str] = {}

    def simulate_partition(self) -> str:
        """模拟网络分区"""
        # 把节点分成两组
        split_point = len(self.nodes) // 2
        partitioned = self.nodes[split_point:]
        for n in partitioned:
            n.status = NodeStatus.PARTITIONED
        other = self.nodes[:split_point]

        # 断连
        for n in partitioned:
            n.connected_nodes = []
        for n in other:
            n.connected_nodes = [
                x for x in other if x != n]

        return f"分区：节点 {[n.node_id for n in partitioned]} 与 "
        f"{[n.node_id for n in other]} 隔离"

    def heal_partition(self):
        """恢复分区"""
        for n in self.nodes:
            n.status = NodeStatus.UP
            n.connected_nodes = [x for x in self.nodes if x != n]

    def write(self, key: str, value: str):
        """在不同 CAP 策略下执行写入"""
        self.last_written[key] = value

        if self.cap_strategy == "CP":
            # 放弃可用性：所有节点必须写入成功
            successes = sum(1 for n in self.nodes if n.write(key, value))
            if successes < len(self.nodes):
                raise Exception(
                    f"CP: 只有 {successes}/{len(self.nodes)} 写入成功，当前视图不可用")
            print(f"  [CP] {key}={value} — 所有节点写入 [OK]")

        elif self.cap_strategy == "AP":
            # 放弃一致性：只要有一个节点可写就返回
            written = False
            for n in self.nodes:
                if n.status == NodeStatus.UP:
                    n.write(key, value)
                    written = True
            if not written:
                raise Exception("AP: 没有可用节点")
            print(f"  [AP] {key}={value} — 可写入的节点已更新（可能不一致）")

        elif self.cap_strategy == "CA":
            # 放弃分区容忍：假设无分区，所有节点写入
            for n in self.nodes:
                n.write(key, value)
            print(f"  [CA] {key}={value} — 所有节点同步写入")

    def read(self, key: str) -> Dict[str, Optional[str]]:
        """读取所有节点的数据（展示一致性状态）"""
        results = {}
        for n in self.nodes:
            val = n.read(key)
            results[n.node_id] = val
        return results


# ════════════════════════════════════════════════════════════
# 1. CP 策略：放弃可用性
# ════════════════════════════════════════════════════════════
def demo_cp():
    print("═══ 1. CP — 放弃可用性，保证一致性 + 分区容忍 ═══")
    nodes = [DBNode("A"), DBNode("B"), DBNode("C")]
    coord = SystemCoordinator(nodes, "CP")

    # 正常情况
    coord.write("x", "1")
    print(f"  正常读取: {coord.read('x')}")

    # 模拟分区
    print(f"\n  → {coord.simulate_partition()}")
    try:
        coord.write("y", "2")
    except Exception as e:
        print(f"  CP 写入拒绝: {e}")

    vals = coord.read("x")
    print(f"  分区后读取: {vals}")

    coord.heal_partition()
    print(f"  分区恢复后读取: {coord.read('x')}\n")


# ════════════════════════════════════════════════════════════
# 2. AP 策略：放弃一致性
# ════════════════════════════════════════════════════════════
def demo_ap():
    print("═══ 2. AP — 放弃一致性，保证可用性 + 分区容忍 ═══")
    nodes = [DBNode("A"), DBNode("B"), DBNode("C")]
    coord = SystemCoordinator(nodes, "AP")

    coord.write("x", "1")

    print(f"  → {coord.simulate_partition()}")
    # 分区两边的节点各自写入
    coord.write("x", "A_value")  # 分区中可写的节点写入

    vals = coord.read("x")
    print(f"  分区后读取: {vals}")
    has_stale = any(v != vals.get(nodes[0].node_id) for n, v in vals.items())
    print(f"  {'[OK]' if not has_stale else '[NO]'} 所有节点一致")
    print(f"  → 写入时只要有一个可用节点就成功，但分区间数据不一致\n")
    coord.heal_partition()


# ════════════════════════════════════════════════════════════
# 3. CA 策略：放弃分区容忍
# ════════════════════════════════════════════════════════════
def demo_ca():
    print("═══ 3. CA — 放弃分区容忍（单机/单集群） ═══")
    nodes = [DBNode("A"), DBNode("B"), DBNode("C")]
    coord = SystemCoordinator(nodes, "CA")

    coord.write("x", "1")
    print(f"  正常读取: {coord.read('x')}")

    print(f"\n  → 假设无分区，CA 系统正常运作")
    print(f"  但在分布式环境中，网络分区不可避免")
    print(f"  一旦发生分区，CA 系统只能牺牲可用性或一致性\n")


# ════════════════════════════════════════════════════════════
# 4. PACELC 扩展
# ════════════════════════════════════════════════════════════
def demo_pacelc():
    print("═══ 4. PACELC 定理 ═══")
    print("  CAP 的扩展：在有分区（P）时在 A 和 C 间取舍")
    print("  在无分区（E，Else）时在延迟（L）和一致性（C）间取舍")
    print()
    print("  | 系统       | 分区时 | 正常时 |")
    print("  |------------|--------|--------|")
    print("  | 传统 RDBMS | CP     | CA     |")
    print("  | Cassandra  | AP     | AL     |")
    print("  | MongoDB    | CP     | CA     |")
    print("  | DynamoDB   | AP     | AL     |")
    print()


def main():
    print("=" * 50)
    print("  CAP 定理演示 — 一致性/可用性/分区容忍")
    print("=" * 50 + "\n")

    demo_cp()
    demo_ap()
    demo_ca()
    demo_pacelc()

    print("所有 CAP 演示完成。")


if __name__ == "__main__":
    main()
